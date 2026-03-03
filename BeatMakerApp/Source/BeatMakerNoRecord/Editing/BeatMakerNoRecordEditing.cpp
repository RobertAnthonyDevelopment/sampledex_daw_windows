#include "../BeatMakerNoRecord.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace
{
constexpr const char* trackRolePropertyName = "sampledexTrackRole";

struct WaitCursorScope
{
    WaitCursorScope()  { juce::MouseCursor::showWaitCursor(); }
    ~WaitCursorScope() { juce::MouseCursor::hideWaitCursor(); }
};

te::ArrangerClip* getSelectedArrangerClip (te::SelectionManager& selectionManager)
{
    return dynamic_cast<te::ArrangerClip*> (selectionManager.getSelectedObject (0));
}

te::ArrangerClip* getArrangerClipAtPlayhead (te::ArrangerTrack& track, te::TimePosition now)
{
    for (auto* clip : track.getClips())
        if (auto* arranger = dynamic_cast<te::ArrangerClip*> (clip))
            if (arranger->getEditTimeRange().contains (now))
                return arranger;

    return nullptr;
}

bool isVst3FormatName (const juce::String& formatName)
{
    return formatName.equalsIgnoreCase ("VST3") || formatName.containsIgnoreCase ("VST3");
}

bool isAudioUnitFormatName (const juce::String& formatName)
{
    return formatName.equalsIgnoreCase ("AU")
        || formatName.equalsIgnoreCase ("AudioUnit")
        || formatName.containsIgnoreCase ("AUDIOUNIT");
}

bool isRemovedBuiltInPluginType (const juce::String& pluginType)
{
    const auto type = pluginType.trim().toLowerCase();
    return type == "4osc"
        || type == "sampler"
        || type == "4bandeq"
        || type == "compressor"
        || type == "reverb"
        || type == "delay";
}

bool isRemovedBuiltInPlugin (te::Plugin* plugin)
{
    if (plugin == nullptr)
        return false;

    // Only remove legacy internal plugins. External AU/VST3 plugins may use
    // generic type names (e.g. "reverb") and must stay in user chains.
    if (dynamic_cast<te::ExternalPlugin*> (plugin) != nullptr)
        return false;

    return isRemovedBuiltInPluginType (plugin->getPluginType());
}

bool isExternalInstrumentPlugin (te::Plugin* plugin)
{
    return plugin != nullptr
        && plugin->isSynth()
        && dynamic_cast<te::ExternalPlugin*> (plugin) != nullptr;
}

int removeRemovedBuiltInPluginsFromTrack (te::AudioTrack& track, juce::UndoManager* undoManager)
{
    int removed = 0;
    auto plugins = track.pluginList.getPlugins();

    for (int i = plugins.size(); --i >= 0;)
    {
        auto* plugin = plugins[i].get();
        if (! isRemovedBuiltInPlugin (plugin))
            continue;

        auto state = plugin->state;
        auto parent = state.getParent();
        if (! parent.isValid())
            continue;

        parent.removeChild (state, undoManager);
        ++removed;
    }

    return removed;
}

std::optional<juce::PluginDescription> findPreferredExternalInstrument (const juce::Array<juce::PluginDescription>& knownTypes,
                                                                        bool allowAuFallback)
{
    std::vector<juce::PluginDescription> vst3Candidates;
    std::vector<juce::PluginDescription> auCandidates;

    for (const auto& desc : knownTypes)
    {
        if (! desc.isInstrument)
            continue;

        if (isVst3FormatName (desc.pluginFormatName))
            vst3Candidates.push_back (desc);
        else if (isAudioUnitFormatName (desc.pluginFormatName))
            auCandidates.push_back (desc);
    }

    auto isBundledNova = [] (const juce::PluginDescription& desc)
    {
        return desc.name.containsIgnoreCase ("Sampledex Nova Synth")
            || desc.descriptiveName.containsIgnoreCase ("Sampledex Nova Synth");
    };

    for (const auto& desc : vst3Candidates)
        if (isBundledNova (desc))
            return desc;

    if (allowAuFallback)
        for (const auto& desc : auCandidates)
            if (isBundledNova (desc))
                return desc;

    auto byNameThenManufacturer = [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        const auto byName = a.name.compareIgnoreCase (b.name);
        if (byName != 0)
            return byName < 0;

        return a.manufacturerName.compareIgnoreCase (b.manufacturerName) < 0;
    };

    std::sort (vst3Candidates.begin(), vst3Candidates.end(), byNameThenManufacturer);
    std::sort (auCandidates.begin(), auCandidates.end(), byNameThenManufacturer);

    if (! vst3Candidates.empty())
        return vst3Candidates.front();

    if (allowAuFallback && ! auCandidates.empty())
        return auCandidates.front();

    return std::nullopt;
}

template<typename SequenceType>
int clearMidiSequenceNotes (SequenceType& sequence, juce::UndoManager* undoManager)
{
    int removed = 0;
    auto notes = sequence.getNotes();
    for (int i = notes.size(); --i >= 0;)
    {
        if (auto* note = notes.getUnchecked (i))
        {
            sequence.removeNote (*note, undoManager);
            ++removed;
        }
    }

    return removed;
}

constexpr double minimumMidiNoteLengthBeats = 1.0 / 128.0;

template<typename SequenceType>
bool addClampedMidiNote (SequenceType& sequence,
                         int noteNumber,
                         double startBeat,
                         double lengthBeats,
                         int velocity,
                         juce::UndoManager* undoManager,
                         double clipLengthBeats,
                         int colour)
{
    const double safeClipLength = juce::jmax (minimumMidiNoteLengthBeats, clipLengthBeats);
    const double maxStartBeat = juce::jmax (0.0, safeClipLength - minimumMidiNoteLengthBeats);
    const double clampedStartBeat = juce::jlimit (0.0, maxStartBeat, startBeat);
    const double maxLengthBeats = juce::jmax (minimumMidiNoteLengthBeats, safeClipLength - clampedStartBeat);
    const double clampedLengthBeats = juce::jlimit (minimumMidiNoteLengthBeats, maxLengthBeats, lengthBeats);

    return sequence.addNote (juce::jlimit (0, 127, noteNumber),
                             te::BeatPosition::fromBeats (clampedStartBeat),
                             te::BeatDuration::fromBeats (clampedLengthBeats),
                             juce::jlimit (1, 127, velocity),
                             colour,
                             undoManager) != nullptr;
}

te::Plugin* getPreferredEnabledInstrumentPlugin (te::AudioTrack& track)
{
    te::Plugin* fallback = nullptr;

    for (auto* plugin : track.pluginList.getPlugins())
    {
        if (plugin == nullptr || ! plugin->isSynth() || ! plugin->isEnabled())
            continue;

        if (plugin->getName().containsIgnoreCase ("Sampledex Nova Synth"))
            return plugin;

        if (fallback == nullptr)
            fallback = plugin;
    }

    return fallback;
}

enum class DirectoryVoicing
{
    triads,
    sevenths,
    wideTriads,
    arpPulse
};

int getRootSemitoneFromId (int comboId)
{
    return juce::jlimit (0, 11, comboId - 1);
}

std::vector<int> getScaleIntervalsFromId (int comboId)
{
    switch (comboId)
    {
        case 2:  return { 0, 2, 3, 5, 7, 8, 10 };      // Natural Minor (Aeolian)
        case 3:  return { 0, 2, 3, 5, 7, 9, 10 };      // Dorian
        case 4:  return { 0, 1, 3, 5, 7, 8, 10 };      // Phrygian
        case 5:  return { 0, 2, 4, 6, 7, 9, 11 };      // Lydian
        case 6:  return { 0, 2, 4, 5, 7, 9, 10 };      // Mixolydian
        case 7:  return { 0, 1, 3, 5, 6, 8, 10 };      // Locrian
        case 8:  return { 0, 2, 3, 5, 7, 8, 11 };      // Harmonic Minor
        case 9:  return { 0, 2, 3, 5, 7, 9, 11 };      // Melodic Minor
        case 10: return { 0, 2, 4, 7, 9 };             // Major Pentatonic
        case 11: return { 0, 3, 5, 7, 10 };            // Minor Pentatonic
        case 12: return { 0, 3, 5, 6, 7, 10 };         // Blues Minor
        case 1:
        default: return { 0, 2, 4, 5, 7, 9, 11 };      // Major (Ionian)
    }
}

std::vector<int> getProgressionDegreesFromId (int comboId)
{
    switch (comboId)
    {
        case 2:  return { 6, 4, 1, 5 };                      // vi-IV-I-V
        case 3:  return { 1, 6, 3, 7 };                      // i-VI-III-VII
        case 4:  return { 1, 6, 4, 5 };                      // I-vi-IV-V
        case 5:  return { 2, 5, 1, 6 };                      // ii-V-I-vi
        case 6:  return { 1, 7, 6, 7 };                      // i-VII-VI-VII
        case 7:  return { 1, 4, 6, 5 };                      // i-iv-VI-V
        case 8:  return { 1, 4, 7, 3, 6, 2, 5, 1 };          // circle
        case 9:  return { 1, 2, 3, 4, 5, 6, 7, 1 };          // diatonic walk
        case 10: return { 1, 5 };                            // two-chord pump
        case 1:
        default: return { 1, 5, 6, 4 };                      // I-V-vi-IV
    }
}

int getBarsFromId (int comboId)
{
    switch (comboId)
    {
        case 1:  return 8;
        case 2:  return 12;
        case 3:  return 16;
        case 4:  return 24;
        case 5:  return 32;
        case 6:  return 48;
        case 7:  return 64;
        default: return 16;
    }
}

std::pair<int, int> getTimeSignatureFromId (int comboId)
{
    switch (comboId)
    {
        case 2:  return { 3, 4 };
        case 3:  return { 5, 4 };
        case 4:  return { 6, 8 };
        case 5:  return { 7, 8 };
        case 6:  return { 12, 8 };
        case 1:
        default: return { 4, 4 };
    }
}

DirectoryVoicing getDirectoryVoicingFromId (int comboId)
{
    switch (comboId)
    {
        case 2:  return DirectoryVoicing::sevenths;
        case 3:  return DirectoryVoicing::wideTriads;
        case 4:  return DirectoryVoicing::arpPulse;
        case 1:
        default: return DirectoryVoicing::triads;
    }
}

int getChordChangesPerBarFromId (int comboId)
{
    switch (comboId)
    {
        case 2:  return 2;
        case 3:  return 4;
        case 4:  return 8;
        case 1:
        default: return 1;
    }
}

int getScaleSemitoneForDegree (const std::vector<int>& scaleSemitones, int degreeIndex)
{
    if (scaleSemitones.empty())
        return 0;

    const int scaleSize = (int) scaleSemitones.size();
    const int safeIndex = juce::jmax (0, degreeIndex);
    const int octave = safeIndex / scaleSize;
    const int index = safeIndex % scaleSize;
    return scaleSemitones[(size_t) index] + octave * 12;
}

std::vector<int> buildDiatonicChordNotes (int tonicMidi, const std::vector<int>& scaleSemitones, int degreeIndex, DirectoryVoicing voicing)
{
    const int voiceCount = (voicing == DirectoryVoicing::sevenths) ? 4 : 3;
    std::vector<int> notes;
    notes.reserve ((size_t) voiceCount + 2);

    for (int i = 0; i < voiceCount; ++i)
    {
        const int semitone = getScaleSemitoneForDegree (scaleSemitones, degreeIndex + i * 2);
        notes.push_back (juce::jlimit (24, 108, tonicMidi + semitone));
    }

    if (voicing == DirectoryVoicing::wideTriads && notes.size() >= 3)
    {
        notes[1] = juce::jlimit (24, 108, notes[1] + 12);
        notes[2] = juce::jlimit (24, 108, notes[2] + 12);
    }

    std::sort (notes.begin(), notes.end());
    notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
    return notes;
}

juce::String makeSafeExportName (juce::String name)
{
    name = name.trim();
    if (name.isEmpty())
        name = "DirectoryExport";

    for (const auto c : juce::String ("\\/:*?\"<>|"))
        name = name.replaceCharacter (c, '_');

    name = name.replace (" ", "_");
    name = name.replace ("#", "sharp");
    name = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
    return name.isEmpty() ? juce::String ("DirectoryExport") : name;
}
}

void BeatMakerNoRecord::configureTrackRoleAsAudio (te::AudioTrack& track, int trackNumber)
{
    track.state.setProperty (trackRolePropertyName, "audio", nullptr);
    track.setName ("Audio " + juce::String (juce::jmax (1, trackNumber)));
    track.setColour (juce::Colour::fromRGB (82, 154, 226));

    for (auto* plugin : track.pluginList.getPlugins())
        if (plugin != nullptr && plugin->isSynth())
            plugin->setEnabled (false);
}

void BeatMakerNoRecord::configureTrackRoleAsMidi (te::AudioTrack& track, int trackNumber, bool openInstrumentUi)
{
    track.state.setProperty (trackRolePropertyName, "midi", nullptr);
    track.setName ("MIDI " + juce::String (juce::jmax (1, trackNumber)));
    track.setColour (juce::Colour::fromRGB (231, 148, 84));

    ensureTrackHasInstrumentForMidiPlayback (track);

    if (openInstrumentUi)
    {
        for (auto* plugin : track.pluginList.getPlugins())
        {
            if (plugin != nullptr && plugin->isSynth() && plugin->isEnabled())
            {
                plugin->showWindowExplicitly();
                break;
            }
        }
    }
}

te::AudioTrack* BeatMakerNoRecord::appendTrackWithRole (bool midiRole, bool openInstrumentUi)
{
    if (edit == nullptr)
        return nullptr;

    const int nextTrackNumber = te::getAudioTracks (*edit).size() + 1;
    edit->ensureNumberOfAudioTracks (nextTrackNumber);

    auto tracks = te::getAudioTracks (*edit);
    auto* track = tracks.isEmpty() ? nullptr : tracks.getLast();
    if (track == nullptr)
        return nullptr;

    if (midiRole)
        configureTrackRoleAsMidi (*track, nextTrackNumber, openInstrumentUi);
    else
        configureTrackRoleAsAudio (*track, nextTrackNumber);

    selectionManager.selectOnly (track);
    markPlaybackRoutingNeedsPreparation();
    syncViewControlsFromState();
    updateTrackControlsFromSelection();
    updateButtonsFromState();
    return track;
}

void BeatMakerNoRecord::addTrack()
{
    if (auto* track = appendTrackWithRole (false, false))
        setStatus ("Added audio track: " + track->getName());
}

void BeatMakerNoRecord::addMidiTrack()
{
    if (auto* track = appendTrackWithRole (true, false))
        setStatus ("Added MIDI track: " + track->getName() + " (instrument ready).");
}

void BeatMakerNoRecord::openMidiClipInPianoRoll (te::MidiClip& midiClip, bool floatWindow)
{
    if (edit == nullptr)
        return;

    // Keep MIDI clip as active context before any plugin mutations.
    selectionManager.selectOnly (&midiClip);
    activeMidiClipID = midiClip.itemID;

    auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip.getTrack());
    bool addedPlaybackInstrument = false;

    if (ownerTrack != nullptr)
    {
        const bool hadInstrumentBefore = trackHasInstrumentPlugin (*ownerTrack);
        ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);
        addedPlaybackInstrument = (! hadInstrumentBefore) && trackHasInstrumentPlugin (*ownerTrack);
    }

    // Instrument insertion can move selection to the inserted plugin; re-pin clip context.
    selectionManager.selectOnly (&midiClip);
    focusPianoRollViewportOnClip (midiClip, true);
    syncPianoRollViewportToSelection (false);

    if (floatWindow)
    {
        if (! isSectionFloating (FloatSection::piano))
            setSectionFloating (FloatSection::piano, true);

        if (pianoFloatingWindow != nullptr)
        {
            pianoFloatingWindow->setVisible (true);
            pianoFloatingWindow->toFront (true);
        }
    }
    else if (isSectionFloating (FloatSection::piano))
    {
        setSectionFloating (FloatSection::piano, false);
    }

    te::Plugin* openedInstrument = nullptr;
    if (floatWindow && ownerTrack != nullptr)
    {
        openedInstrument = getPreferredEnabledInstrumentPlugin (*ownerTrack);
        if (openedInstrument != nullptr)
        {
            if (const int pluginIndex = ownerTrack->pluginList.indexOf (openedInstrument); pluginIndex >= 0)
                fxChainBox.setSelectedId (pluginIndex + 1, juce::dontSendNotification);

            openedInstrument->showWindowExplicitly();
        }
    }

    refreshSelectedTrackPluginList();
    updateTrackControlsFromSelection();
    updateButtonsFromState();
    stepSequencer.repaint();
    midiPianoRoll.repaint();

    juce::String status = "Opened piano roll for: " + midiClip.getName();
    if (openedInstrument != nullptr)
        status << " | Instrument UI: " << openedInstrument->getName();
    else if (ownerTrack != nullptr && ! trackHasInstrumentPlugin (*ownerTrack))
        status << " | No instrument available.";

    if (addedPlaybackInstrument)
        status << " (auto-added external instrument for playback)";

    setStatus (status);
}

void BeatMakerNoRecord::addFloatingInstrumentTrack()
{
    if (edit == nullptr)
        return;

    auto* track = appendTrackWithRole (true, false);
    if (track == nullptr)
    {
        setStatus ("Failed to create floating instrument track.");
        return;
    }

    selectionManager.selectOnly (track);

    const int instrumentCountBefore = [&track]
    {
        int count = 0;
        for (auto* plugin : track->pluginList.getPlugins())
            if (plugin != nullptr && plugin->isSynth() && plugin->isEnabled())
                ++count;
        return count;
    }();

    addExternalInstrumentPluginToSelectedTrack();
    ensureTrackHasInstrumentForMidiPlayback (*track);

    createMidiClip();

    te::MidiClip* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr || midiClip->getTrack() != track)
    {
        for (auto* clip : track->getClips())
            if (auto* candidate = dynamic_cast<te::MidiClip*> (clip))
            {
                midiClip = candidate;
                break;
            }
    }

    if (midiClip != nullptr)
        openMidiClipInPianoRoll (*midiClip, true);
    else
        setSectionFloating (FloatSection::piano, true);

    const int instrumentCountAfter = [&track]
    {
        int count = 0;
        for (auto* plugin : track->pluginList.getPlugins())
            if (plugin != nullptr && plugin->isSynth() && plugin->isEnabled())
                ++count;
        return count;
    }();

    auto* instrument = getPreferredEnabledInstrumentPlugin (*track);
    juce::String status = "Added floating instrument track: " + track->getName();
    if (instrument != nullptr)
        status << " | Instrument: " << instrument->getName();
    if (instrumentCountAfter > instrumentCountBefore)
        status << " (new instrument loaded)";

    setStatus (status + ".");
}

void BeatMakerNoRecord::moveSelectedTrackVertically (bool moveDown)
{
    if (edit == nullptr)
        return;

    auto* selectedTrack = getSelectedTrackOrFirst();
    if (selectedTrack == nullptr)
        return;

    auto* neighbour = selectedTrack->getSiblingTrack (moveDown ? 1 : -1, true);
    if (neighbour == nullptr)
    {
        setStatus (moveDown ? "Track is already at the bottom." : "Track is already at the top.");
        return;
    }

    te::Track::Ptr movableTrack (selectedTrack);
    edit->moveTrack (movableTrack, te::TrackInsertPoint (*neighbour, ! moveDown));
    selectionManager.selectOnly (selectedTrack);
    syncViewControlsFromState();
    setStatus (moveDown ? "Moved track down." : "Moved track up.");
}

void BeatMakerNoRecord::duplicateSelectedTrack()
{
    if (edit == nullptr)
        return;

    auto* track = getSelectedTrackOrFirst();
    if (track == nullptr)
        return;

    te::Track::Ptr sourceTrack (track);
    auto duplicated = edit->copyTrack (sourceTrack, te::TrackInsertPoint (*track, false));
    if (duplicated != nullptr)
    {
        duplicated->setName (track->getName() + " Copy");
        selectionManager.selectOnly (duplicated.get());
        markPlaybackRoutingNeedsPreparation();
        syncViewControlsFromState();
        setStatus ("Duplicated track: " + duplicated->getName());
    }
    else
    {
        setStatus ("Failed to duplicate selected track.");
    }
}

void BeatMakerNoRecord::cycleSelectedTrackColour()
{
    auto* track = getSelectedTrackOrFirst();
    if (track == nullptr)
        return;

    const juce::Colour palette[] =
    {
        juce::Colour::fromRGB (242, 99, 90),
        juce::Colour::fromRGB (244, 158, 76),
        juce::Colour::fromRGB (242, 215, 84),
        juce::Colour::fromRGB (88, 198, 122),
        juce::Colour::fromRGB (77, 189, 206),
        juce::Colour::fromRGB (90, 144, 234),
        juce::Colour::fromRGB (222, 112, 190)
    };
    constexpr int paletteSize = (int) (sizeof (palette) / sizeof (palette[0]));

    int nearestIndex = 0;
    float nearestDistance = 1.0e9f;
    const auto current = track->getColour();

    for (int i = 0; i < paletteSize; ++i)
    {
        const auto c = palette[i];
        const float dr = (float) c.getFloatRed()   - (float) current.getFloatRed();
        const float dg = (float) c.getFloatGreen() - (float) current.getFloatGreen();
        const float db = (float) c.getFloatBlue()  - (float) current.getFloatBlue();
        const float distance = dr * dr + dg * dg + db * db;

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestIndex = i;
        }
    }

    const int nextIndex = (nearestIndex + 1) % paletteSize;
    track->setColour (palette[nextIndex]);
    setStatus ("Updated track colour.");
}

void BeatMakerNoRecord::renameSelectedTrack()
{
    auto* track = getSelectedTrackOrFirst();
    if (track == nullptr)
        return;

    juce::AlertWindow w ("Rename Track", "Enter a new track name", juce::AlertWindow::NoIcon);
    w.addTextEditor ("name", track->getName());
    w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    if (w.runModalLoop() == 1)
    {
        const auto newName = w.getTextEditorContents ("name").trim();
        if (newName.isNotEmpty())
        {
            track->setName (newName);
            selectedTrackLabel.setText ("Track: " + newName, juce::dontSendNotification);
            setStatus ("Renamed track to: " + newName);
        }
    }
}

void BeatMakerNoRecord::toggleMarkerTrackVisibility()
{
    if (edit == nullptr || editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const bool showMarkers = ! viewState.showMarkerTrack.get();

    if (showMarkers)
        edit->ensureMarkerTrack();

    viewState.showMarkerTrack = showMarkers;
    updateTrackModeButtons();
    syncViewControlsFromState();
    setStatus (showMarkers ? "Marker track shown." : "Marker track hidden.");
}

void BeatMakerNoRecord::toggleArrangerTrackVisibility()
{
    if (edit == nullptr || editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const bool showArranger = ! viewState.showArrangerTrack.get();

    if (showArranger)
        edit->ensureArrangerTrack();

    viewState.showArrangerTrack = showArranger;
    updateTrackModeButtons();
    syncViewControlsFromState();
    setStatus (showArranger ? "Arranger track shown." : "Arranger track hidden.");
}

void BeatMakerNoRecord::addMarkerAtPlayhead()
{
    if (edit == nullptr)
        return;

    edit->ensureMarkerTrack();

    const auto now = edit->getTransport().getPosition();
    auto& markerManager = edit->getMarkerManager();
    const int markerID = markerManager.getNextUniqueID();

    if (auto marker = markerManager.createMarker (markerID, now, getBarDurationAt (now), &selectionManager))
    {
        marker->setName ("Marker " + juce::String (markerID));
        selectionManager.selectOnly (marker.get());
        timelineRuler.repaint();
        setStatus ("Created marker " + juce::String (markerID) + ".");
    }
    else
    {
        setStatus ("Failed to create marker.");
    }
}

void BeatMakerNoRecord::jumpToMarker (bool forward)
{
    if (edit == nullptr)
        return;

    auto& markerManager = edit->getMarkerManager();
    auto* marker = forward ? markerManager.getNextMarker (edit->getTransport().getPosition())
                           : markerManager.getPrevMarker (edit->getTransport().getPosition());

    if (marker == nullptr)
    {
        auto markers = markerManager.getMarkers();
        if (markers.isEmpty())
        {
            setStatus ("No markers in edit.");
            return;
        }

        marker = (forward ? markers.getFirst() : markers.getLast()).get();
    }

    edit->getTransport().setPosition (marker->getPosition().getStart());
    selectionManager.selectOnly (marker);
    timelineRuler.repaint();
    setStatus (forward ? "Jumped to next marker." : "Jumped to previous marker.");
}

void BeatMakerNoRecord::setLoopBetweenNearestMarkers()
{
    if (edit == nullptr)
        return;

    auto markers = edit->getMarkerManager().getMarkers();
    if (markers.size() < 2)
    {
        setStatus ("Need at least two markers to loop between markers.");
        return;
    }

    const auto now = edit->getTransport().getPosition();
    const auto epsilon = te::TimeDuration::fromSeconds (0.0001);

    int nextIndex = -1;
    for (int i = 0; i < markers.size(); ++i)
    {
        if (markers[i]->getPosition().getStart() - now > epsilon)
        {
            nextIndex = i;
            break;
        }
    }

    int startIndex = 0;
    int endIndex = 1;

    if (nextIndex > 0)
    {
        startIndex = nextIndex - 1;
        endIndex = nextIndex;
    }
    else if (nextIndex < 0)
    {
        startIndex = markers.size() - 2;
        endIndex = markers.size() - 1;
    }

    auto start = markers[startIndex]->getPosition().getStart();
    auto end = markers[endIndex]->getPosition().getStart();

    if (end <= start)
        end = markers[endIndex]->getPosition().getEnd();

    if (end <= start)
        end = start + getBarDurationAt (start);

    edit->getTransport().setLoopRange ({ start, end });
    edit->getTransport().looping = true;
    updateTransportLoopButton();
    timelineRuler.repaint();
    setStatus ("Loop set between nearest markers.");
}

void BeatMakerNoRecord::addArrangerSectionAtPlayhead()
{
    if (edit == nullptr)
        return;

    edit->ensureArrangerTrack();
    auto* arrangerTrack = edit->getArrangerTrack();
    if (arrangerTrack == nullptr)
        return;

    int sectionNumber = 1;
    for (auto* clip : arrangerTrack->getClips())
        if (dynamic_cast<te::ArrangerClip*> (clip) != nullptr)
            ++sectionNumber;

    const auto start = edit->getTransport().getPosition();
    const auto sectionLength = te::TimeDuration::fromSeconds (getBarDurationAt (start).inSeconds() * 4.0);
    const auto sectionEnd = start + sectionLength;

    if (auto* clip = dynamic_cast<te::ArrangerClip*> (arrangerTrack->insertNewClip (te::TrackItem::Type::arranger,
                                                                                     "Section " + juce::String (sectionNumber),
                                                                                     { start, sectionEnd },
                                                                                     &selectionManager)))
    {
        selectionManager.selectOnly (clip);
        timelineRuler.repaint();
        setStatus ("Created arranger section: " + clip->getName());
    }
    else
    {
        setStatus ("Failed to create arranger section.");
    }
}

void BeatMakerNoRecord::jumpToArrangerSection (bool forward)
{
    if (edit == nullptr)
        return;

    auto* arrangerTrack = edit->getArrangerTrack();
    if (arrangerTrack == nullptr)
    {
        setStatus ("No arranger sections in edit.");
        return;
    }

    const auto now = edit->getTransport().getPosition();
    const auto epsilon = te::TimeDuration::fromSeconds (0.0001);

    te::ArrangerClip* target = nullptr;
    te::ArrangerClip* boundary = nullptr;
    te::TimeDuration bestDelta = te::TimeDuration::fromSeconds (1.0e9);

    for (auto* clip : arrangerTrack->getClips())
    {
        auto* section = dynamic_cast<te::ArrangerClip*> (clip);
        if (section == nullptr)
            continue;

        const auto delta = section->getPosition().getStart() - now;

        if (forward)
        {
            if (delta > epsilon && delta < bestDelta)
            {
                bestDelta = delta;
                target = section;
            }

            if (boundary == nullptr || section->getPosition().getStart() < boundary->getPosition().getStart())
                boundary = section;
        }
        else
        {
            if (delta < -epsilon && -delta < bestDelta)
            {
                bestDelta = -delta;
                target = section;
            }

            if (boundary == nullptr || section->getPosition().getStart() > boundary->getPosition().getStart())
                boundary = section;
        }
    }

    if (target == nullptr)
        target = boundary;

    if (target == nullptr)
    {
        setStatus ("No arranger sections in edit.");
        return;
    }

    edit->getTransport().setPosition (target->getPosition().getStart());
    selectionManager.selectOnly (target);
    timelineRuler.repaint();
    setStatus (forward ? "Jumped to next arranger section." : "Jumped to previous arranger section.");
}

void BeatMakerNoRecord::setLoopToCurrentArrangerSection()
{
    if (edit == nullptr)
        return;

    auto* arrangerTrack = edit->getArrangerTrack();
    if (arrangerTrack == nullptr)
    {
        setStatus ("No arranger sections in edit.");
        return;
    }

    auto* target = getSelectedArrangerClip (selectionManager);

    if (target == nullptr)
    {
        const auto now = edit->getTransport().getPosition();
        target = getArrangerClipAtPlayhead (*arrangerTrack, now);

        if (target == nullptr)
        {
            te::ArrangerClip* latestBeforeCursor = nullptr;
            for (auto* clip : arrangerTrack->getClips())
            {
                auto* section = dynamic_cast<te::ArrangerClip*> (clip);
                if (section == nullptr)
                    continue;

                if (section->getPosition().getStart() <= now
                    && (latestBeforeCursor == nullptr
                        || section->getPosition().getStart() > latestBeforeCursor->getPosition().getStart()))
                {
                    latestBeforeCursor = section;
                }
            }

            target = latestBeforeCursor;
        }
    }

    if (target == nullptr)
    {
        setStatus ("No arranger section found at playhead.");
        return;
    }

    auto sectionRange = target->getEditTimeRange();
    if (sectionRange.getEnd() <= sectionRange.getStart())
        sectionRange = { sectionRange.getStart(), sectionRange.getStart() + getBarDurationAt (sectionRange.getStart()) };

    edit->getTransport().setLoopRange (sectionRange);
    edit->getTransport().looping = true;
    updateTransportLoopButton();
    selectionManager.selectOnly (target);
    timelineRuler.repaint();
    setStatus ("Loop set to arranger section: " + target->getName());
}

void BeatMakerNoRecord::importAudioClip()
{
    if (edit == nullptr)
        return;

    auto* selectedTrack = getSelectedTrackOrFirst();
    if (selectedTrack == nullptr)
        return;

    const auto targetTrackID = selectedTrack->itemID;
    juce::Component::SafePointer<BeatMakerNoRecord> safeThis (this);

    EngineHelpers::browseForAudioFile (engine,
                                       [safeThis, targetTrackID] (const juce::File& file)
                                       {
                                           auto* owner = safeThis.getComponent();
                                           if (owner == nullptr || owner->edit == nullptr || ! file.existsAsFile())
                                               return;

                                           auto* targetTrack = dynamic_cast<te::AudioTrack*> (te::findTrackForID (*owner->edit, targetTrackID));
                                           if (targetTrack == nullptr)
                                               return;

                                           te::AudioFile audioFile (owner->engine, file);
                                           if (! audioFile.isValid())
                                           {
                                               owner->setStatus ("Unable to load audio file: " + file.getFileName());
                                               return;
                                           }

                                           auto clipStart = owner->edit->getTransport().getPosition();
                                           if (auto* selectedClip = owner->getSelectedClip())
                                               if (selectedClip->getTrack() == targetTrack && selectedClip->getPosition().getEnd() > clipStart)
                                                   clipStart = selectedClip->getPosition().getEnd();

                                           for (int iteration = 0; iteration < 128; ++iteration)
                                           {
                                               bool movedStart = false;
                                               for (auto* existingClip : targetTrack->getClips())
                                               {
                                                   if (existingClip == nullptr)
                                                       continue;

                                                   const auto range = existingClip->getEditTimeRange();
                                                   if (range.contains (clipStart))
                                                   {
                                                       clipStart = range.getEnd();
                                                       movedStart = true;
                                                       break;
                                                   }
                                               }

                                               if (! movedStart)
                                                   break;
                                           }

                                           const double beatsPerBar = juce::jmax (1.0, owner->getBeatsPerBarAt (clipStart));
                                           const double startBeat = owner->edit->tempoSequence.toBeats (clipStart).inBeats();
                                           const double snappedBeat = std::ceil ((startBeat - 1.0e-9) / beatsPerBar) * beatsPerBar;
                                           clipStart = owner->edit->tempoSequence.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, snappedBeat)));

                                           const te::ClipPosition position { { clipStart, te::TimeDuration::fromSeconds (audioFile.getLength()) }, {} };

                                           if (auto clip = targetTrack->insertWaveClip (file.getFileNameWithoutExtension(), file, position, false))
                                           {
                                               owner->applyHighQualitySettingsToAudioClip (*clip);
                                               owner->selectionManager.selectOnly (clip.get());

                                               const double barSeconds = juce::jmax (0.01, owner->getBarDurationAt (clipStart).inSeconds());
                                               const double clipSeconds = juce::jmax (0.01, audioFile.getLength());
                                               const int detectedBars = juce::jlimit (1, 32, juce::roundToInt (clipSeconds / barSeconds));
                                               const double fitErrorBars = std::abs (clipSeconds - (double) detectedBars * barSeconds) / barSeconds;
                                               const bool looksLikeLoop = detectedBars <= 16 && fitErrorBars <= 0.12;

                                               if (looksLikeLoop)
                                               {
                                                   clip->setAutoTempo (true);
                                                   clip->setWarpTime (true);
                                                   owner->edit->getTransport().setLoopRange ({ clipStart, te::TimeDuration::fromSeconds ((double) detectedBars * barSeconds) });
                                                   owner->edit->getTransport().looping = true;
                                                   owner->updateTransportLoopButton();
                                                   owner->timelineRuler.repaint();
                                                   owner->setStatus ("Imported audio clip: " + clip->getName()
                                                                     + " | bar-aligned and loop-armed (" + juce::String (detectedBars) + " bars).");
                                               }
                                               else
                                               {
                                                   owner->setStatus ("Imported audio clip: " + clip->getName() + " (bar-aligned placement).");
                                               }
                                           }
                                           else
                                           {
                                               owner->setStatus ("Failed to import audio clip.");
                                           }
                                       });
}

void BeatMakerNoRecord::importMidiClip()
{
    if (edit == nullptr)
        return;

    auto* selectedTrack = getSelectedTrackOrFirst();
    if (selectedTrack == nullptr)
        return;

    const auto trackID = selectedTrack->itemID;
    juce::Component::SafePointer<BeatMakerNoRecord> safeThis (this);
    auto midiDirectory = engine.getPropertyStorage().getDefaultLoadSaveDirectory ("beatMakerMidi");
    if (! midiDirectory.isDirectory())
        midiDirectory = currentEditFile.existsAsFile() ? currentEditFile.getParentDirectory()
                                                       : getProjectsRootDirectory();
    if (! midiDirectory.isDirectory())
        midiDirectory = getProjectsRootDirectory();
    if (midiDirectory.isDirectory())
        engine.getPropertyStorage().setDefaultLoadSaveDirectory ("beatMakerMidi", midiDirectory);

    auto chooser = std::make_shared<juce::FileChooser> ("Select a MIDI file",
                                                         midiDirectory,
                                                         "*.mid;*.midi");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis, chooser, trackID] (const juce::FileChooser&)
                          {
                              auto* owner = safeThis.getComponent();
                              if (owner == nullptr || owner->edit == nullptr)
                                  return;

                              const auto file = chooser->getResult();
                              if (! file.existsAsFile())
                                  return;

                              owner->engine.getPropertyStorage().setDefaultLoadSaveDirectory ("beatMakerMidi", file.getParentDirectory());

                              auto* targetTrack = dynamic_cast<te::AudioTrack*> (te::findTrackForID (*owner->edit, trackID));
                              if (targetTrack == nullptr)
                                  return;

                              if (auto clip = te::createClipFromFile (file, *targetTrack, false))
                              {
                                  const auto start = owner->edit->getTransport().getPosition();
                                  auto position = clip->getPosition();
                                  clip->setPosition ({ { start, position.getLength() }, position.getOffset() });
                                  const bool addedInstrument = owner->ensureTrackHasInstrumentForMidiPlayback (*targetTrack);
                                  owner->selectionManager.selectOnly (clip.get());
                                  if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip.get()))
                                  {
                                      owner->openMidiClipInPianoRoll (*midiClip, false);
                                  }
                                  else
                                  {
                                      owner->setStatus ("Imported MIDI clip: " + clip->getName()
                                                        + (addedInstrument ? " (auto-added instrument for playback)" : ""));
                                  }
                              }
                              else
                              {
                                  owner->setStatus ("Failed to import MIDI file.");
                              }
                          });
}

double BeatMakerNoRecord::getMidiClipLengthBeats (const te::MidiClip& midiClip) const
{
    const auto clipStart = midiClip.getPosition().getStart();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
    const double sequenceLengthBeats = midiClip.getSequence().getLastBeatNumber().inBeats() + (1.0 / 32.0);

    double clipLengthBeats = beatsPerBar;
    if (edit != nullptr)
    {
        const double bps = juce::jmax (1.0e-4, edit->tempoSequence.getBeatsPerSecondAt (clipStart, true));
        clipLengthBeats = juce::jmax (1.0 / 32.0, midiClip.getPosition().getLength().inSeconds() * bps);
    }

    double resolvedLength = juce::jmax (beatsPerBar, clipLengthBeats);
    resolvedLength = juce::jmax (resolvedLength, sequenceLengthBeats);
    return juce::jmax (1.0, resolvedLength);
}

void BeatMakerNoRecord::createMidiClip()
{
    if (edit == nullptr)
        return;

    auto* selectedTrack = getSelectedTrackOrFirst();
    if (selectedTrack == nullptr)
        return;

    const auto start = edit->getTransport().getPosition();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (start));
    const double bps = juce::jmax (1.0e-4, edit->tempoSequence.getBeatsPerSecondAt (start, true));
    const auto oneBar = te::TimeDuration::fromSeconds (beatsPerBar / bps);

    if (auto clip = selectedTrack->insertMIDIClip ("Pattern", { start, oneBar }, &selectionManager))
    {
        auto& sequence = clip->getSequence();
        auto& undoManager = edit->getUndoManager();
        undoManager.beginNewTransaction ("Create MIDI Clip");

        const double clipLengthBeats = getMidiClipLengthBeats (*clip);
        const double stepBeats = juce::jmax (0.125, beatsPerBar / 4.0);
        const double noteLengthBeats = juce::jmax (0.0625, stepBeats * 0.5);

        int created = 0;
        for (int step = 0; step < 4; ++step)
        {
            if (addClampedMidiNote (sequence,
                                    36,
                                    (double) step * stepBeats,
                                    noteLengthBeats,
                                    100,
                                    &undoManager,
                                    clipLengthBeats,
                                    0))
            {
                ++created;
            }
        }

        const bool addedInstrument = ensureTrackHasInstrumentForMidiPlayback (*selectedTrack);
        const bool pianoWasFloating = isSectionFloating (FloatSection::piano);
        // Re-select MIDI clip after any plugin insertion to keep editor context intact.
        selectionManager.selectOnly (clip.get());
        openMidiClipInPianoRoll (*clip, false);

        // Keep the editor host location stable for users building patterns in a floating window.
        if (pianoWasFloating && ! isSectionFloating (FloatSection::piano))
            setSectionFloating (FloatSection::piano, true);

        if (addedInstrument)
            setStatus ("Created MIDI clip (" + juce::String (created) + " note(s)). (auto-added instrument for playback)");
    }
}

bool BeatMakerNoRecord::applyChordScaleDirectoryToClip (te::MidiClip& midiClip,
                                                        bool clearExistingNotes,
                                                        bool previewMode,
                                                        juce::String* outSummary)
{
    if (edit == nullptr)
        return false;

    auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip.getTrack());
    if (ownerTrack == nullptr)
        return false;

    ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);

    const auto timeSignature = getTimeSignatureFromId (chordDirectoryTimeSignatureBox.getSelectedId());
    const int numerator = juce::jmax (1, timeSignature.first);
    const int denominator = juce::jmax (1, timeSignature.second);

    const int bars = juce::jlimit (8, 64, getBarsFromId (chordDirectoryBarsBox.getSelectedId()));
    const int octave = juce::jlimit (1, 7, chordDirectoryOctaveBox.getSelectedId() > 0 ? chordDirectoryOctaveBox.getSelectedId() : 3);
    const int rootSemitone = getRootSemitoneFromId (chordDirectoryRootBox.getSelectedId());
    const auto scaleSemitones = getScaleIntervalsFromId (chordDirectoryScaleBox.getSelectedId());
    const auto progressionDegrees = getProgressionDegreesFromId (chordDirectoryProgressionBox.getSelectedId());
    const auto voicing = getDirectoryVoicingFromId (chordDirectoryVoicingBox.getSelectedId());
    const int chordChangesPerBar = juce::jmax (1, getChordChangesPerBarFromId (chordDirectoryDensityBox.getSelectedId()));
    const int velocityBase = juce::jlimit (1, 127, juce::roundToInt (chordDirectoryVelocitySlider.getValue()));
    const double swingAmount = juce::jlimit (0.0, 0.45, chordDirectorySwingSlider.getValue());
    const bool arpDensity = chordDirectoryDensityBox.getSelectedId() == 4;
    const bool arpMode = arpDensity || voicing == DirectoryVoicing::arpPulse;

    if (scaleSemitones.empty() || progressionDegrees.empty())
        return false;

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction (previewMode ? "Preview Chord Scale Directory" : "Apply Chord Scale Directory");

    auto& sequence = midiClip.getSequence();
    if (clearExistingNotes)
        clearMidiSequenceNotes (sequence, &undoManager);

    const auto clipStart = midiClip.getPosition().getStart();
    const double beatsPerBar = (double) numerator * 4.0 / (double) denominator;
    const double totalBeats = juce::jmax (1.0, beatsPerBar * (double) bars);
    const double bps = juce::jmax (1.0e-4, edit->tempoSequence.getBeatsPerSecondAt (clipStart, true));
    const auto clipLength = te::TimeDuration::fromSeconds (totalBeats / bps);
    const auto clipOffset = midiClip.getPosition().getOffset();
    midiClip.setPosition ({ { clipStart, clipLength }, clipOffset });

    const int tonicMidi = juce::jlimit (24, 96, (octave + 1) * 12 + rootSemitone);
    const int slotCount = juce::jmax (1, bars * chordChangesPerBar);
    const double slotLengthBeats = totalBeats / (double) slotCount;

    int generatedNotes = 0;
    for (int slot = 0; slot < slotCount; ++slot)
    {
        const int degree = progressionDegrees[(size_t) (slot % (int) progressionDegrees.size())];
        const int degreeIndex = juce::jmax (0, degree - 1);
        auto chordNotes = buildDiatonicChordNotes (tonicMidi,
                                                   scaleSemitones,
                                                   degreeIndex,
                                                   arpMode ? DirectoryVoicing::triads : voicing);
        if (chordNotes.empty())
            continue;

        double startBeat = (double) slot * slotLengthBeats;
        if (swingAmount > 0.0 && (slot % 2) == 1)
            startBeat += juce::jmin (slotLengthBeats * 0.48, slotLengthBeats * swingAmount * 0.5);
        startBeat = juce::jmax (0.0, startBeat);

        if (arpMode)
        {
            const int arpSteps = juce::jlimit (2, 16, juce::roundToInt (std::ceil (slotLengthBeats / 0.5)));
            const double arpStepLength = slotLengthBeats / (double) juce::jmax (1, arpSteps);
            for (int step = 0; step < arpSteps; ++step)
            {
                const double beat = startBeat + (double) step * arpStepLength;
                const int note = chordNotes[(size_t) (step % (int) chordNotes.size())];
                const int velocity = juce::jlimit (1, 127, velocityBase + ((step % 2 == 0) ? 8 : -6));
                if (addClampedMidiNote (sequence,
                                        note,
                                        beat,
                                        juce::jmax (1.0 / 64.0, arpStepLength * 0.86),
                                        velocity,
                                        &undoManager,
                                        totalBeats,
                                        0))
                {
                    ++generatedNotes;
                }
            }
        }
        else
        {
            const double chordLength = juce::jmax (1.0 / 32.0, slotLengthBeats * 0.92);
            for (int noteIndex = 0; noteIndex < (int) chordNotes.size(); ++noteIndex)
            {
                const int velocityOffset = noteIndex == 0 ? 6 : (2 - noteIndex * 2);
                if (addClampedMidiNote (sequence,
                                        chordNotes[(size_t) noteIndex],
                                        startBeat,
                                        chordLength,
                                        juce::jlimit (1, 127, velocityBase + velocityOffset),
                                        &undoManager,
                                        totalBeats,
                                        0))
                {
                    ++generatedNotes;
                }
            }
        }
    }

    if (generatedNotes <= 0)
        return false;

    if (outSummary != nullptr)
    {
        juce::String summary;
        summary << chordDirectoryRootBox.getText().trim() << " "
                << chordDirectoryScaleBox.getText().trim()
                << " | " << chordDirectoryProgressionBox.getText().trim()
                << " | " << juce::String (bars) << " bar(s)"
                << " | " << juce::String (numerator) << "/" << juce::String (denominator)
                << " | octave " << juce::String (octave)
                << " | " << juce::String (generatedNotes) << " note(s)";

        if (previewMode && getPreferredEnabledInstrumentPlugin (*ownerTrack) == nullptr)
            summary << " | No instrument available for live audition";
        *outSummary = summary;
    }

    selectionManager.selectOnly (&midiClip);
    return true;
}

void BeatMakerNoRecord::generateMidiChordScaleDirectoryPattern()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    juce::String summary;
    if (! applyChordScaleDirectoryToClip (*midiClip, true, false, &summary))
    {
        setStatus ("Failed to apply chord/scale directory to MIDI clip.");
        return;
    }

    markPlaybackRoutingNeedsPreparation();
    setStatus ("Applied chord/scale directory. " + summary + ".");
}

void BeatMakerNoRecord::previewChordScaleDirectoryPattern()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    juce::String summary;
    if (! applyChordScaleDirectoryToClip (*midiClip, true, true, &summary))
    {
        setStatus ("Failed to generate preview from chord/scale directory.");
        return;
    }

    auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack());
    bool hasInstrument = false;
    if (ownerTrack != nullptr)
    {
        ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);
        hasInstrument = getPreferredEnabledInstrumentPlugin (*ownerTrack) != nullptr;
    }

    const auto clipRange = midiClip->getEditTimeRange();
    if (hasInstrument && ! clipRange.isEmpty())
    {
        edit->getTransport().setLoopRange (clipRange);
        edit->getTransport().looping = true;
        edit->getTransport().setPosition (clipRange.getStart());
        prepareEditForPluginPlayback (false);
        if (! edit->getTransport().isPlaying())
            EngineHelpers::togglePlay (*edit);
    }

    markPlaybackRoutingNeedsPreparation();
    if (hasInstrument)
        setStatus ("Previewing chord/scale directory with external instrument. " + summary + ".");
    else
        setStatus ("Generated chord/scale directory pattern but no instrument is enabled for live preview. " + summary + ".");
}

void BeatMakerNoRecord::exportChordScaleDirectorySelectionAsMidi()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack());
    if (ownerTrack == nullptr)
    {
        setStatus ("Selected MIDI clip is not on an audio track.");
        return;
    }

    juce::String summary;
    if (! applyChordScaleDirectoryToClip (*midiClip, true, false, &summary))
    {
        setStatus ("Failed to prepare chord/scale directory for MIDI export.");
        return;
    }

    auto projectDirectory = currentEditFile.existsAsFile()
                                ? currentEditFile.getParentDirectory()
                                : getProjectsRootDirectory().getChildFile ("Untitled Project");

    if (! projectDirectory.isDirectory() && ! projectDirectory.createDirectory())
    {
        setStatus ("Unable to create project directory for MIDI export.");
        return;
    }

    auto exportRoot = projectDirectory.getChildFile ("Exports")
                                      .getChildFile ("ChordScaleDirectory")
                                      .getChildFile ("MIDI")
                                      .getChildFile (makeSafeExportName (chordDirectoryRootBox.getText()))
                                      .getChildFile (makeSafeExportName (chordDirectoryScaleBox.getText()))
                                      .getChildFile (makeSafeExportName (chordDirectoryTimeSignatureBox.getText()));

    if (! exportRoot.isDirectory() && ! exportRoot.createDirectory())
    {
        setStatus ("Unable to create ChordScaleDirectory/MIDI export directory.");
        return;
    }

    const auto exportStem = makeSafeExportName ("dir_"
                                                + chordDirectoryRootBox.getText() + "_"
                                                + chordDirectoryScaleBox.getText() + "_"
                                                + chordDirectoryProgressionBox.getText() + "_"
                                                + chordDirectoryBarsBox.getText() + "bar_oct"
                                                + chordDirectoryOctaveBox.getText());
    const auto timeStamp = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
    const auto targetFile = exportRoot.getNonexistentChildFile (exportStem + "_" + timeStamp, ".mid", false);

    auto allTracks = te::getAllTracks (*edit);
    const int ownerTrackIndex = allTracks.indexOf (ownerTrack);
    if (ownerTrackIndex < 0)
    {
        setStatus ("Cannot export MIDI: failed to resolve track index.");
        return;
    }

    juce::BigInteger tracksToDo;
    tracksToDo.setBit (ownerTrackIndex);

    te::Renderer::Parameters params (*edit);
    params.edit = edit.get();
    params.destFile = targetFile;
    params.audioFormat = nullptr;
    params.createMidiFile = true;
    params.time = midiClip->getEditTimeRange();
    params.endAllowance = te::TimeDuration::fromSeconds (0.0);
    params.tracksToDo = tracksToDo;
    params.allowedClips.add (midiClip);
    params.usePlugins = false;
    params.useMasterPlugins = false;
    params.canRenderInMono = false;
    params.mustRenderInMono = false;

    setStatus ("Exporting chord/scale directory MIDI...");
    WaitCursorScope waitCursor;
    const auto renderedFile = te::Renderer::renderToFile ("Export Chord Scale Directory MIDI", params);
    if (! renderedFile.existsAsFile())
    {
        setStatus ("Chord/scale directory MIDI export failed.");
        return;
    }

    setStatus ("Exported chord/scale directory MIDI: " + renderedFile.getFileName() + " | " + summary + ".");
}

void BeatMakerNoRecord::exportChordScaleDirectorySelectionAsWav()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack());
    if (ownerTrack == nullptr)
    {
        setStatus ("Selected MIDI clip is not on an audio track.");
        return;
    }

    juce::String summary;
    if (! applyChordScaleDirectoryToClip (*midiClip, true, false, &summary))
    {
        setStatus ("Failed to prepare chord/scale directory for WAV export.");
        return;
    }

    ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);
    if (! trackHasInstrumentPlugin (*ownerTrack))
    {
        setStatus ("Cannot export WAV: no instrument available on selected MIDI track.");
        return;
    }

    auto projectDirectory = currentEditFile.existsAsFile()
                                ? currentEditFile.getParentDirectory()
                                : getProjectsRootDirectory().getChildFile ("Untitled Project");

    if (! projectDirectory.isDirectory() && ! projectDirectory.createDirectory())
    {
        setStatus ("Unable to create project directory for WAV export.");
        return;
    }

    auto exportRoot = projectDirectory.getChildFile ("Exports")
                                      .getChildFile ("ChordScaleDirectory")
                                      .getChildFile ("WAV")
                                      .getChildFile (makeSafeExportName (chordDirectoryRootBox.getText()))
                                      .getChildFile (makeSafeExportName (chordDirectoryScaleBox.getText()))
                                      .getChildFile (makeSafeExportName (chordDirectoryTimeSignatureBox.getText()));

    if (! exportRoot.isDirectory() && ! exportRoot.createDirectory())
    {
        setStatus ("Unable to create ChordScaleDirectory/WAV export directory.");
        return;
    }

    const auto exportStem = makeSafeExportName ("dir_"
                                                + chordDirectoryRootBox.getText() + "_"
                                                + chordDirectoryScaleBox.getText() + "_"
                                                + chordDirectoryProgressionBox.getText() + "_"
                                                + chordDirectoryBarsBox.getText() + "bar_oct"
                                                + chordDirectoryOctaveBox.getText());
    const auto timeStamp = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
    const auto targetFile = exportRoot.getNonexistentChildFile (exportStem + "_" + timeStamp, ".wav", false);

    auto allTracks = te::getAllTracks (*edit);
    const int ownerTrackIndex = allTracks.indexOf (ownerTrack);
    if (ownerTrackIndex < 0)
    {
        setStatus ("Cannot export WAV: failed to resolve track index.");
        return;
    }

    prepareEditForPluginPlayback (false);

    juce::BigInteger tracksToDo;
    tracksToDo.setBit (ownerTrackIndex);

    juce::WavAudioFormat wavFormat;
    te::Renderer::Parameters params (*edit);
    params.edit = edit.get();
    params.destFile = targetFile;
    params.audioFormat = static_cast<juce::AudioFormat*> (&wavFormat);
    params.bitDepth = 24;
    params.blockSizeForAudio = juce::jmax (128, engine.getDeviceManager().getBlockSize());
    params.sampleRateForAudio = juce::jmax (8000.0, engine.getDeviceManager().getSampleRate());
    params.time = midiClip->getEditTimeRange();
    params.endAllowance = te::TimeDuration::fromSeconds (1.5);
    params.tracksToDo = tracksToDo;
    params.allowedClips.add (midiClip);
    params.usePlugins = true;
    params.useMasterPlugins = false;
    params.canRenderInMono = false;
    params.mustRenderInMono = false;

    setStatus ("Exporting chord/scale directory WAV...");
    WaitCursorScope waitCursor;
    const auto renderedFile = te::Renderer::renderToFile ("Export Chord Scale Directory WAV", params);
    if (! renderedFile.existsAsFile())
    {
        setStatus ("Chord/scale directory WAV export failed.");
        return;
    }

    setStatus ("Exported chord/scale directory WAV: " + renderedFile.getFileName() + " | " + summary + ".");
}

void BeatMakerNoRecord::generateMidiChordProgression()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
        ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);

    auto& sequence = midiClip->getSequence();
    auto* undoManager = &edit->getUndoManager();
    undoManager->beginNewTransaction ("Generate MIDI Chords");
    clearMidiSequenceNotes (sequence, undoManager);

    const auto clipStart = midiClip->getPosition().getStart();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    const int barCount = juce::jlimit (1, 32, juce::jmax (1, juce::roundToInt (std::ceil (clipLengthBeats / beatsPerBar))));
    const int octave = juce::jlimit (1, 7, chordDirectoryOctaveBox.getSelectedId() > 0 ? chordDirectoryOctaveBox.getSelectedId() : 3);
    const int tonicMidi = juce::jlimit (24, 96, (octave + 1) * 12 + getRootSemitoneFromId (chordDirectoryRootBox.getSelectedId()));
    const auto scaleSemitones = getScaleIntervalsFromId (chordDirectoryScaleBox.getSelectedId());
    const auto progressionDegrees = getProgressionDegreesFromId (chordDirectoryProgressionBox.getSelectedId());

    if (scaleSemitones.empty() || progressionDegrees.empty())
    {
        setStatus ("Generator settings are invalid. Check scale/progression.");
        return;
    }

    int generatedNotes = 0;

    for (int bar = 0; bar < barCount; ++bar)
    {
        const int degree = progressionDegrees[(size_t) (bar % (int) progressionDegrees.size())];
        const int degreeIndex = juce::jmax (0, degree - 1);
        const auto chordNotes = buildDiatonicChordNotes (tonicMidi, scaleSemitones, degreeIndex, DirectoryVoicing::sevenths);
        if (chordNotes.empty())
            continue;

        const double startBeat = (double) bar * beatsPerBar;
        const double noteLength = juce::jmax (0.25, beatsPerBar * 0.92);
        for (int noteIndex = 0; noteIndex < (int) chordNotes.size(); ++noteIndex)
        {
            const int velocity = juce::jlimit (1, 127, 104 - (noteIndex * 6));
            if (addClampedMidiNote (sequence,
                                    chordNotes[(size_t) noteIndex],
                                    startBeat,
                                    noteLength,
                                    velocity,
                                    undoManager,
                                    clipLengthBeats,
                                    0))
            {
                ++generatedNotes;
            }
        }
    }

    if (generatedNotes <= 0)
    {
        setStatus ("No notes generated for chord progression.");
        return;
    }

    selectionManager.selectOnly (midiClip);
    setStatus ("Generated MIDI chords (" + juce::String (barCount) + " bar(s), " + juce::String (generatedNotes) + " notes, "
               + chordDirectoryRootBox.getText() + " " + chordDirectoryScaleBox.getText() + ").");
}

void BeatMakerNoRecord::generateMidiArpeggioPattern()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
        ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);

    auto& sequence = midiClip->getSequence();
    auto* undoManager = &edit->getUndoManager();
    undoManager->beginNewTransaction ("Generate MIDI Arp");
    clearMidiSequenceNotes (sequence, undoManager);

    const auto clipStart = midiClip->getPosition().getStart();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    const double stepBeats = juce::jlimit (1.0 / 16.0, 1.0, getPianoRollGridBeats().inBeats());
    const int stepCount = juce::jlimit (1, 2048, juce::jmax (1, juce::roundToInt (std::ceil (clipLengthBeats / stepBeats))));
    const int octave = juce::jlimit (1, 7, chordDirectoryOctaveBox.getSelectedId() > 0 ? chordDirectoryOctaveBox.getSelectedId() : 3);
    const int tonicMidi = juce::jlimit (24, 96, (octave + 1) * 12 + getRootSemitoneFromId (chordDirectoryRootBox.getSelectedId()));
    const auto scaleSemitones = getScaleIntervalsFromId (chordDirectoryScaleBox.getSelectedId());
    const auto progressionDegrees = getProgressionDegreesFromId (chordDirectoryProgressionBox.getSelectedId());
    constexpr int arpShape[] = { 0, 1, 2, 1, 3, 2, 1, 0 };
    constexpr int arpShapeCount = (int) (sizeof (arpShape) / sizeof (arpShape[0]));

    if (scaleSemitones.empty() || progressionDegrees.empty())
    {
        setStatus ("Generator settings are invalid. Check scale/progression.");
        return;
    }

    int generatedNotes = 0;
    for (int step = 0; step < stepCount; ++step)
    {
        const double beat = (double) step * stepBeats;
        const int barIndex = juce::jmax (0, juce::roundToInt (std::floor (beat / beatsPerBar)));
        const int degree = progressionDegrees[(size_t) (barIndex % (int) progressionDegrees.size())];
        const int degreeIndex = juce::jmax (0, degree - 1);
        const auto chordNotes = buildDiatonicChordNotes (tonicMidi, scaleSemitones, degreeIndex, DirectoryVoicing::sevenths);
        if (chordNotes.empty())
            continue;

        const int noteNumber = chordNotes[(size_t) (arpShape[step % arpShapeCount] % (int) chordNotes.size())];
        const int velocity = (step % 4 == 0) ? 108 : 92;
        const double length = juce::jmax (0.0625, stepBeats * 0.82);
        if (addClampedMidiNote (sequence, noteNumber, beat, length, velocity, undoManager, clipLengthBeats, 0))
            ++generatedNotes;
    }

    if (generatedNotes <= 0)
    {
        setStatus ("No notes generated for arpeggio.");
        return;
    }

    selectionManager.selectOnly (midiClip);
    setStatus ("Generated MIDI arpeggio (" + juce::String (generatedNotes) + " notes, step "
               + juce::String (stepBeats, 3) + " beat).");
}

void BeatMakerNoRecord::generateMidiBasslinePattern()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
        ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);

    auto& sequence = midiClip->getSequence();
    auto* undoManager = &edit->getUndoManager();
    undoManager->beginNewTransaction ("Generate MIDI Bass");
    clearMidiSequenceNotes (sequence, undoManager);

    const auto clipStart = midiClip->getPosition().getStart();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    const int barCount = juce::jlimit (1, 32, juce::jmax (1, juce::roundToInt (std::ceil (clipLengthBeats / beatsPerBar))));
    const double beatStep = juce::jmax (0.125, beatsPerBar / 4.0);
    const int octave = juce::jlimit (1, 7, chordDirectoryOctaveBox.getSelectedId() > 0 ? chordDirectoryOctaveBox.getSelectedId() : 3);
    const int tonicMidi = juce::jlimit (24, 96, (octave + 1) * 12 + getRootSemitoneFromId (chordDirectoryRootBox.getSelectedId()));
    const auto scaleSemitones = getScaleIntervalsFromId (chordDirectoryScaleBox.getSelectedId());
    const auto progressionDegrees = getProgressionDegreesFromId (chordDirectoryProgressionBox.getSelectedId());

    if (scaleSemitones.empty() || progressionDegrees.empty())
    {
        setStatus ("Generator settings are invalid. Check scale/progression.");
        return;
    }

    int generatedNotes = 0;

    for (int bar = 0; bar < barCount; ++bar)
    {
        const int degree = progressionDegrees[(size_t) (bar % (int) progressionDegrees.size())];
        const int degreeIndex = juce::jmax (0, degree - 1);
        const auto chordNotes = buildDiatonicChordNotes (tonicMidi, scaleSemitones, degreeIndex, DirectoryVoicing::triads);
        if (chordNotes.empty())
            continue;

        const int root = juce::jlimit (24, 96, chordNotes.front() - 24);
        const int fifth = juce::jlimit (24, 96, chordNotes.size() > 2 ? chordNotes[2] - 24 : root + 7);
        const double barBeat = (double) bar * beatsPerBar;

        for (int step = 0; step < 4; ++step)
        {
            const double startBeat = barBeat + (double) step * beatStep;
            int note = root;
            int velocity = (step == 0 || step == 2) ? 108 : 92;

            if (step == 2)
                note = fifth;
            else if (step == 3)
                note = root + 12;

            const double lengthBeats = (step == 0 || step == 2) ? beatStep * 0.92 : beatStep * 0.52;
            if (addClampedMidiNote (sequence,
                                    juce::jlimit (24, 96, note),
                                    startBeat,
                                    juce::jmax (0.125, lengthBeats),
                                    velocity,
                                    undoManager,
                                    clipLengthBeats,
                                    0))
            {
                ++generatedNotes;
            }
        }
    }

    if (generatedNotes <= 0)
    {
        setStatus ("No notes generated for bassline.");
        return;
    }

    selectionManager.selectOnly (midiClip);
    setStatus ("Generated MIDI bassline (" + juce::String (barCount) + " bar(s), " + juce::String (generatedNotes) + " notes).");
}

void BeatMakerNoRecord::generateMidiDrumPattern()
{
    if (edit == nullptr)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
    {
        setStatus ("Create or select a MIDI clip first.");
        return;
    }

    if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
        ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);

    auto& sequence = midiClip->getSequence();
    auto* undoManager = &edit->getUndoManager();
    undoManager->beginNewTransaction ("Generate MIDI Drums");
    clearMidiSequenceNotes (sequence, undoManager);

    const auto clipStart = midiClip->getPosition().getStart();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    const int barCount = juce::jlimit (1, 16, juce::jmax (1, juce::roundToInt (std::ceil (clipLengthBeats / beatsPerBar))));
    const double step16 = juce::jmax (0.03125, beatsPerBar / 16.0);

    juce::Random random;
    int generatedNotes = 0;

    auto addDrum = [&sequence, undoManager, &generatedNotes, clipLengthBeats] (int noteNumber,
                                                                                double beat,
                                                                                double lengthBeats,
                                                                                int velocity)
    {
        if (addClampedMidiNote (sequence,
                                noteNumber,
                                beat,
                                juce::jmax (0.03125, lengthBeats),
                                velocity,
                                undoManager,
                                clipLengthBeats,
                                0))
        {
            ++generatedNotes;
        }
    };

    for (int bar = 0; bar < barCount; ++bar)
    {
        const double barBeat = (double) bar * beatsPerBar;
        for (int step = 0; step < 16; ++step)
        {
            const double beat = barBeat + (double) step * step16;
            const bool isKick = (step == 0 || step == 8 || (step == 10 && (bar % 2 == 1)));
            const bool isSnare = (step == 4 || step == 12);
            const bool isHat = (step % 2 == 0);
            const bool isOpenHat = (step == 14);

            if (isKick)
                addDrum (36, beat, step16 * 1.2, 112 + random.nextInt (10));

            if (isSnare)
                addDrum (38, beat, step16 * 0.9, 104 + random.nextInt (9));

            if (isOpenHat)
                addDrum (46, beat, step16 * 1.7, 88 + random.nextInt (12));
            else if (isHat)
                addDrum (42, beat, step16 * 0.68, 74 + random.nextInt (16));
        }
    }

    if (generatedNotes <= 0)
    {
        setStatus ("No hits generated for drum pattern.");
        return;
    }

    selectionManager.selectOnly (midiClip);
    setStatus ("Generated MIDI drums (" + juce::String (barCount) + " bar(s), " + juce::String (generatedNotes) + " hits).");
}

void BeatMakerNoRecord::openPluginScanDialog()
{
    auto& pluginManager = engine.getPluginManager();
    if (pluginManager.pluginFormatManager.getNumFormats() == 0)
        juce::addDefaultFormatsToManager (pluginManager.pluginFormatManager);
    pluginManager.knownPluginList.setCustomScanner (createTimedPluginScanCustomScanner());
    setTimedPluginScanTimeoutMs (5000);

    if (edit != nullptr && edit->getTransport().isPlaying())
        edit->getTransport().stop (false, false);

    const int formatCount = pluginManager.pluginFormatManager.getNumFormats();

    if (formatCount <= 0)
    {
        setStatus ("No plugin formats are available in this build.");
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Plugin Scan";
    options.dialogBackgroundColour = juce::Colour::fromRGB (20, 26, 36);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;
    options.componentToCentreAround = this;

    consumeTimedOutPluginScanEntries();

    auto* pluginList = new juce::PluginListComponent (pluginManager.pluginFormatManager,
                                                      pluginManager.knownPluginList,
                                                      engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                                      &engine.getPropertyStorage().getPropertiesFile(),
                                                      true);
    pluginList->setOptionsButtonText ("Scan...");
    pluginList->setScanDialogText ("Scanning Plugins",
                                   "Searching AU/VST3 plugins with timeout-safe scanning."
                                   "\nPlugins that exceed 5 seconds are skipped and listed for separate rescans.");
    pluginList->setNumberOfThreadsForScanning (1);
    pluginList->setSize (920, 640);

    options.content.setOwned (pluginList);

    juce::StringArray formatNames;
    for (int i = 0; i < formatCount; ++i)
        if (auto* format = pluginManager.pluginFormatManager.getFormat (i))
            formatNames.addIfNotAlreadyThere (format->getName());

    setStatus ("Opened timeout-safe plugin scanner (formats: " + formatNames.joinIntoString (", ") + ").");
    [[maybe_unused]] const int modalResult = options.runModal();

    auto timedOut = consumeTimedOutPluginScanEntries();
    for (const auto& skipped : timedOut)
        skippedPluginScanEntries.addIfNotAlreadyThere (skipped);

    const int knownPlugins = pluginManager.knownPluginList.getNumTypes();
    if (timedOut.isEmpty())
        setStatus ("Plugin scan closed. Known plugins: " + juce::String (knownPlugins));
    else
        setStatus ("Plugin scan closed. Known plugins: " + juce::String (knownPlugins)
                   + " | Skipped (timeout): " + juce::String (timedOut.size())
                   + " | Use 'Scan Skipped' for targeted rescans.");

    updateButtonsFromState();
}

void BeatMakerNoRecord::scanSkippedPlugins()
{
    if (skippedPluginScanEntries.isEmpty())
    {
        setStatus ("No skipped plugins queued for targeted scan.");
        return;
    }

    auto& pluginManager = engine.getPluginManager();
    if (pluginManager.pluginFormatManager.getNumFormats() == 0)
        juce::addDefaultFormatsToManager (pluginManager.pluginFormatManager);
    pluginManager.knownPluginList.setCustomScanner (createTimedPluginScanCustomScanner());

    if (pluginManager.pluginFormatManager.getNumFormats() <= 0)
    {
        setStatus ("Cannot scan skipped plugins: no plugin formats available.");
        return;
    }

    auto pending = skippedPluginScanEntries;
    skippedPluginScanEntries.clear();

    setTimedPluginScanTimeoutMs (15000);
    consumeTimedOutPluginScanEntries();

    int resolved = 0;
    juce::StringArray stillSkipped;

    for (const auto& identifier : pending)
    {
        pluginManager.knownPluginList.removeFromBlacklist (identifier);

        bool attempted = false;
        bool resolvedThisIdentifier = false;

        for (int i = 0; i < pluginManager.pluginFormatManager.getNumFormats(); ++i)
        {
            auto* format = pluginManager.pluginFormatManager.getFormat (i);
            if (format == nullptr || ! format->fileMightContainThisPluginType (identifier))
                continue;

            attempted = true;

            juce::OwnedArray<juce::PluginDescription> found;
            pluginManager.knownPluginList.scanAndAddFile (identifier, false, found, *format);

            if (! found.isEmpty())
            {
                resolved += found.size();
                resolvedThisIdentifier = true;
                break;
            }
        }

        if (! attempted || ! resolvedThisIdentifier)
            stillSkipped.addIfNotAlreadyThere (identifier);
    }

    auto timedOutAgain = consumeTimedOutPluginScanEntries();
    for (const auto& skipped : timedOutAgain)
        stillSkipped.addIfNotAlreadyThere (skipped);

    skippedPluginScanEntries = stillSkipped;
    setTimedPluginScanTimeoutMs (5000);

    setStatus ("Scan skipped complete: resolved " + juce::String (resolved)
               + ", remaining skipped " + juce::String (skippedPluginScanEntries.size()) + ".");

    updateButtonsFromState();
}

void BeatMakerNoRecord::refreshSelectedTrackPluginList()
{
    updatingTrackControls = true;

    const int previousSelection = fxChainBox.getSelectedId();
    fxChainBox.clear (juce::dontSendNotification);

    auto* track = getSelectedTrackOrFirst();
    if (track != nullptr)
    {
        const int removedBuiltIns = removeRemovedBuiltInPluginsFromTrack (*track, edit != nullptr ? &edit->getUndoManager() : nullptr);
        if (removedBuiltIns > 0)
            markPlaybackRoutingNeedsPreparation();

        auto plugins = track->pluginList.getPlugins();
        int selectedID = 0;
        int itemID = 1;

        for (auto* plugin : plugins)
        {
            juce::String name = plugin->getName().trim();
            if (name.isEmpty())
                name = plugin->getPluginType();

            if (! plugin->isEnabled())
                name = "[Bypassed] " + name;

            if (dynamic_cast<te::VolumeAndPanPlugin*> (plugin) != nullptr)
                name << " (Mixer)";
            else if (dynamic_cast<te::LevelMeterPlugin*> (plugin) != nullptr)
                name << " (Meter)";

            fxChainBox.addItem (name, itemID);

            if (auto* selectedPlugin = dynamic_cast<te::Plugin*> (selectionManager.getSelectedObject (0)))
                if (selectedPlugin == plugin)
                    selectedID = itemID;

            ++itemID;
        }

        if (selectedID == 0 && previousSelection >= 1 && previousSelection <= fxChainBox.getNumItems())
            selectedID = previousSelection;

        if (selectedID == 0 && fxChainBox.getNumItems() > 0)
            selectedID = 1;

        if (selectedID > 0)
            fxChainBox.setSelectedId (selectedID, juce::dontSendNotification);
    }

    updatingTrackControls = false;
    updateButtonsFromState();
}

te::Plugin* BeatMakerNoRecord::getSelectedTrackPlugin() const
{
    auto* track = getSelectedTrackOrFirst();
    if (track == nullptr)
        return nullptr;

    auto plugins = track->pluginList.getPlugins();
    const int selectedIndex = fxChainBox.getSelectedId() - 1;

    if (selectedIndex >= 0 && selectedIndex < plugins.size())
        return plugins[selectedIndex].get();

    if (auto* selectedPlugin = dynamic_cast<te::Plugin*> (selectionManager.getSelectedObject (0)))
        if (selectedPlugin->getOwnerTrack() == track)
            return selectedPlugin;

    return nullptr;
}

void BeatMakerNoRecord::openSelectedTrackPluginEditor()
{
    auto* plugin = getSelectedTrackPlugin();
    if (plugin == nullptr)
    {
        setStatus ("Select a plugin in FX chain first.");
        return;
    }

    plugin->showWindowExplicitly();
    if (plugin->windowState != nullptr && plugin->windowState->isWindowShowing())
        setStatus ("Opened plugin UI: " + plugin->getName());
    else
        setStatus ("Plugin UI not available for: " + plugin->getName());
}

bool BeatMakerNoRecord::trackHasInstrumentPlugin (te::AudioTrack& track) const
{
    auto plugins = track.pluginList.getPlugins();
    for (auto* plugin : plugins)
        if (isExternalInstrumentPlugin (plugin) && plugin->isEnabled())
            return true;

    return false;
}

bool BeatMakerNoRecord::trackHasMidiContent (te::AudioTrack& track) const
{
    for (auto* clip : track.getClips())
        if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
            return true;

    return false;
}

bool BeatMakerNoRecord::enableExistingInstrumentForDefaultMode (te::AudioTrack& track,
                                                                DefaultInstrumentMode mode,
                                                                int& enabledInstruments)
{
    if (trackHasInstrumentPlugin (track))
        return false;

    auto plugins = track.pluginList.getPlugins();
    for (auto* plugin : plugins)
    {
        if (plugin == nullptr || ! plugin->isSynth() || plugin->isEnabled())
            continue;

        auto* external = dynamic_cast<te::ExternalPlugin*> (plugin);
        if (external == nullptr)
            continue;

        bool shouldEnable = true;
        if (mode == DefaultInstrumentMode::forceExternalVst3)
            shouldEnable = external->isVST3();

        if (! shouldEnable)
            continue;

        plugin->setEnabled (true);
        ++enabledInstruments;
        return true;
    }

    return false;
}

bool BeatMakerNoRecord::insertInstrumentForDefaultMode (te::AudioTrack& track,
                                                        DefaultInstrumentMode mode,
                                                        int& addedInstruments)
{
    const auto knownTypes = engine.getPluginManager().knownPluginList.getTypes();
    const bool allowAuFallback = (mode == DefaultInstrumentMode::autoPreferExternal);

    if (const auto preferred = findPreferredExternalInstrument (knownTypes, allowAuFallback))
    {
        if (auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, *preferred))
        {
            track.pluginList.insertPlugin (plugin, getPluginInsertIndexForTrack (track, true), nullptr);
            plugin->setEnabled (true);
            ++addedInstruments;
            return true;
        }
    }

    return false;
}

te::Plugin* BeatMakerNoRecord::choosePreferredInstrumentPluginForMode (te::AudioTrack& track, DefaultInstrumentMode mode) const
{
    auto plugins = track.pluginList.getPlugins();

    te::Plugin* preferredEnabled = nullptr;
    te::Plugin* preferredAny = nullptr;
    te::Plugin* enabledFallback = nullptr;
    te::Plugin* anyFallback = nullptr;

    for (auto* plugin : plugins)
    {
        auto* external = dynamic_cast<te::ExternalPlugin*> (plugin);
        if (external == nullptr || ! plugin->isSynth())
            continue;

        if (anyFallback == nullptr)
            anyFallback = plugin;

        const bool enabled = plugin->isEnabled();
        if (enabled && enabledFallback == nullptr)
            enabledFallback = plugin;

        const bool preferredForMode = (mode != DefaultInstrumentMode::forceExternalVst3) || external->isVST3();
        if (! preferredForMode)
            continue;

        if (preferredAny == nullptr)
            preferredAny = plugin;

        if (enabled && preferredEnabled == nullptr)
            preferredEnabled = plugin;
    }

    if (preferredEnabled != nullptr)
        return preferredEnabled;

    if (preferredAny != nullptr)
        return preferredAny;

    if (enabledFallback != nullptr)
        return enabledFallback;

    return anyFallback;
}

bool BeatMakerNoRecord::normalizeTrackInstrumentActivationForMode (te::AudioTrack& track,
                                                                   DefaultInstrumentMode mode,
                                                                   int& enabledInstruments)
{
    auto* preferred = choosePreferredInstrumentPluginForMode (track, mode);
    if (preferred == nullptr)
        return false;

    bool changed = false;
    for (auto* plugin : track.pluginList.getPlugins())
    {
        if (! isExternalInstrumentPlugin (plugin))
            continue;

        const bool shouldEnable = (plugin == preferred);
        if (plugin->isEnabled() == shouldEnable)
            continue;

        plugin->setEnabled (shouldEnable);
        if (shouldEnable)
            ++enabledInstruments;
        changed = true;
    }

    return changed;
}

bool BeatMakerNoRecord::prepareTrackForMidiPlayback (te::AudioTrack& track, int& enabledInstruments, int& addedInstruments)
{
    if (edit == nullptr || ! trackHasMidiContent (track))
        return false;
    bool changed = false;
    const int removedBuiltIns = removeRemovedBuiltInPluginsFromTrack (track, edit != nullptr ? &edit->getUndoManager() : nullptr);
    if (removedBuiltIns > 0)
        changed = true;

    const auto defaultMode = getDefaultInstrumentModeSelection();
    changed = enableExistingInstrumentForDefaultMode (track, defaultMode, enabledInstruments) || changed;

    if (! trackHasInstrumentPlugin (track) && insertInstrumentForDefaultMode (track, defaultMode, addedInstruments))
        changed = true;

    if (trackHasInstrumentPlugin (track))
        changed = normalizeTrackInstrumentActivationForMode (track, defaultMode, enabledInstruments) || changed;

    return changed;
}

bool BeatMakerNoRecord::normalizeTrackPluginOrderForPlayback (te::AudioTrack& track, int& movedPlugins)
{
    auto plugins = track.pluginList.getPlugins();
    if (plugins.size() < 2)
        return false;

    juce::Array<juce::ValueTree> instruments;
    juce::Array<juce::ValueTree> effects;
    juce::Array<juce::ValueTree> mixers;

    for (auto* plugin : plugins)
    {
        if (plugin == nullptr)
            continue;

        if (dynamic_cast<te::VolumeAndPanPlugin*> (plugin) != nullptr
            || dynamic_cast<te::LevelMeterPlugin*> (plugin) != nullptr)
        {
            mixers.add (plugin->state);
        }
        else if (isExternalInstrumentPlugin (plugin))
        {
            instruments.add (plugin->state);
        }
        else
        {
            effects.add (plugin->state);
        }
    }

    juce::Array<juce::ValueTree> ordered;
    ordered.addArray (instruments);
    ordered.addArray (effects);
    ordered.addArray (mixers);

    if (ordered.size() < 2)
        return false;

    auto parent = ordered[0].getParent();
    if (! parent.isValid())
        return false;

    bool changed = false;
    auto* undoManager = edit != nullptr ? &edit->getUndoManager() : nullptr;

    for (int targetIndex = 0; targetIndex < ordered.size(); ++targetIndex)
    {
        auto targetState = ordered.getReference (targetIndex);
        const int currentIndex = parent.indexOf (targetState);
        if (currentIndex >= 0 && currentIndex != targetIndex)
        {
            parent.moveChild (currentIndex, targetIndex, undoManager);
            ++movedPlugins;
            changed = true;
        }
    }

    return changed;
}

void BeatMakerNoRecord::prepareEditForPluginPlayback (bool reorderFxChains)
{
    if (edit == nullptr)
        return;

    if (edit->getTransport().isPlaying())
    {
        setStatus ("Stop playback before preparing routing.");
        return;
    }

    int enabledInstruments = 0;
    int addedInstruments = 0;
    int movedPlugins = 0;
    int missingVst3Tracks = 0;
    bool changed = false;
    const bool forceVst3Mode = getDefaultInstrumentModeSelection() == DefaultInstrumentMode::forceExternalVst3;

    for (auto* track : te::getAudioTracks (*edit))
    {
        if (track == nullptr)
            continue;

        if (prepareTrackForMidiPlayback (*track, enabledInstruments, addedInstruments))
            changed = true;

        if (forceVst3Mode && trackHasMidiContent (*track) && ! trackHasInstrumentPlugin (*track))
            ++missingVst3Tracks;

        if (reorderFxChains && normalizeTrackPluginOrderForPlayback (*track, movedPlugins))
            changed = true;
    }

    if (changed)
    {
        refreshSelectedTrackPluginList();
        updateButtonsFromState();
    }

    if (enabledInstruments > 0 || addedInstruments > 0 || movedPlugins > 0)
    {
        juce::String summary = "Playback prepared";
        summary << ": enabled instruments " << enabledInstruments
                << ", added instruments " << addedInstruments;

        if (reorderFxChains)
            summary << ", reordered plugins " << movedPlugins;

        if (missingVst3Tracks > 0)
            summary << ", tracks missing VST3 instrument " << missingVst3Tracks;

        setStatus (summary + ".");
    }
    else if (missingVst3Tracks > 0)
    {
        setStatus ("Playback prep: no VST3 instruments available for "
                   + juce::String (missingVst3Tracks) + " MIDI track(s) in forced VST3 mode.");
    }

    playbackRoutingNeedsPreparation = false;
}

int BeatMakerNoRecord::getPluginInsertIndexForTrack (te::AudioTrack& track, bool forInstrument) const
{
    auto plugins = track.pluginList.getPlugins();

    int firstMixerPluginIndex = plugins.size();
    for (int i = 0; i < plugins.size(); ++i)
    {
        auto* plugin = plugins[i].get();
        if (dynamic_cast<te::VolumeAndPanPlugin*> (plugin) != nullptr
            || dynamic_cast<te::LevelMeterPlugin*> (plugin) != nullptr)
        {
            firstMixerPluginIndex = juce::jmin (firstMixerPluginIndex, i);
        }
    }

    int insertIndex = 0;
    if (forInstrument)
    {
        for (int i = 0; i < firstMixerPluginIndex; ++i)
        {
            auto* plugin = plugins[i].get();
            if (isExternalInstrumentPlugin (plugin))
                insertIndex = i + 1;
            else
                break;
        }
    }
    else
    {
        // FX inserts append at the end of user chain, just before mixer plugins.
        insertIndex = firstMixerPluginIndex;
    }

    return juce::jlimit (0, firstMixerPluginIndex, insertIndex);
}

bool BeatMakerNoRecord::ensureTrackHasInstrumentForMidiPlayback (te::AudioTrack& track)
{
    if (edit != nullptr && edit->getTransport().isPlaying())
    {
        setStatus ("Stop playback before changing instrument routing.");
        return false;
    }

    int enabledInstruments = 0;
    int addedInstruments = 0;
    bool changed = prepareTrackForMidiPlayback (track, enabledInstruments, addedInstruments);
    const bool hasInstrumentNow = trackHasInstrumentPlugin (track);

    if (changed)
    {
        refreshSelectedTrackPluginList();
        markPlaybackRoutingNeedsPreparation();
    }

    if (! hasInstrumentNow)
    {
        if (getDefaultInstrumentModeSelection() == DefaultInstrumentMode::forceExternalVst3)
            setStatus ("No VST3 instrument available in forced VST3 mode. Scan plugins or switch instrument mode.");
        else
            setStatus ("No external instrument available for MIDI playback. Scan plugins or add an AU/VST3 instrument.");
    }

    return changed;
}

void BeatMakerNoRecord::addExternalInstrumentPluginToSelectedTrack()
{
    if (edit == nullptr)
        return;

    if (edit->getTransport().isPlaying())
    {
        setStatus ("Stop playback before inserting plugins.");
        return;
    }

    auto* track = getSelectedTrackOrFirst();
    if (track == nullptr)
    {
        setStatus ("Select a track first.");
        return;
    }

    auto& pluginManager = engine.getPluginManager();
    auto allPlugins = pluginManager.knownPluginList.getTypes();

    if (allPlugins.isEmpty())
    {
        setStatus ("No scanned plugins found. Click Scan Plugins first.");
        return;
    }

    std::vector<juce::PluginDescription> auInstruments;
    std::vector<juce::PluginDescription> vst3Instruments;

    for (const auto& desc : allPlugins)
    {
        if (! desc.isInstrument)
            continue;

        const auto format = desc.pluginFormatName.toUpperCase();
        const bool isVst3 = format.contains ("VST3");
        const bool isAu = format.contains ("AUDIOUNIT") || format == "AU";

        if (isVst3)
            vst3Instruments.push_back (desc);
        else if (isAu)
            auInstruments.push_back (desc);
    }

    if (auInstruments.empty() && vst3Instruments.empty())
    {
        setStatus ("No AU/VST3 instruments available. Scan plugins and ensure instrument plugins are installed.");
        return;
    }

    auto byNameThenManufacturer = [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        const auto nameCompare = a.name.compareIgnoreCase (b.name);
        if (nameCompare != 0)
            return nameCompare < 0;

        return a.manufacturerName.compareIgnoreCase (b.manufacturerName) < 0;
    };

    std::sort (auInstruments.begin(), auInstruments.end(), byNameThenManufacturer);
    std::sort (vst3Instruments.begin(), vst3Instruments.end(), byNameThenManufacturer);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Add External Instrument");

    std::vector<juce::PluginDescription> choiceById;
    choiceById.reserve (auInstruments.size() + vst3Instruments.size());
    int itemId = 1;

    auto populateSubmenu = [&choiceById, &itemId] (juce::PopupMenu& subMenu,
                                                   const std::vector<juce::PluginDescription>& list)
    {
        for (const auto& desc : list)
        {
            juce::String label = desc.name.trim();
            if (label.isEmpty())
                label = desc.descriptiveName.trim();

            if (desc.manufacturerName.trim().isNotEmpty())
                label << " - " << desc.manufacturerName.trim();

            subMenu.addItem (itemId, label);
            choiceById.push_back (desc);
            ++itemId;
        }
    };

    if (! vst3Instruments.empty())
    {
        juce::PopupMenu vstMenu;
        populateSubmenu (vstMenu, vst3Instruments);
        menu.addSubMenu ("VST3 Instruments", vstMenu, true);
    }

    if (! auInstruments.empty())
    {
        juce::PopupMenu auMenu;
        populateSubmenu (auMenu, auInstruments);
        menu.addSubMenu ("AU Instruments", auMenu, true);
    }

    const int selectedItem = menu.showMenu (juce::PopupMenu::Options().withTargetComponent (&fxAddExternalInstrumentButton)
                                                                      .withMinimumWidth (460));
    if (selectedItem <= 0)
        return;

    const auto selectedIndex = (size_t) (selectedItem - 1);
    if (selectedIndex >= choiceById.size())
        return;

    const auto& desc = choiceById[selectedIndex];
    setStatus ("Loading instrument plugin: " + desc.name + "...");
    const auto loadStartMs = juce::Time::getMillisecondCounterHiRes();
    WaitCursorScope waitCursor;
    auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);
    if (plugin == nullptr)
    {
        setStatus ("Failed to load plugin: " + desc.name);
        return;
    }

    const bool behavesAsInstrument = isExternalInstrumentPlugin (plugin.get());
    track->pluginList.insertPlugin (plugin,
                                    getPluginInsertIndexForTrack (*track, behavesAsInstrument),
                                    &selectionManager);
    plugin->setEnabled (true);

    if (behavesAsInstrument)
    {
        for (auto* other : track->pluginList.getPlugins())
        {
            if (! isExternalInstrumentPlugin (other) || other == plugin.get())
                continue;

            other->setEnabled (false);
        }
    }

    refreshSelectedTrackPluginList();

    const int insertedIndex = track->pluginList.indexOf (plugin.get());
    if (insertedIndex >= 0)
        fxChainBox.setSelectedId (insertedIndex + 1, juce::dontSendNotification);

    markPlaybackRoutingNeedsPreparation();
    updateButtonsFromState();
    const auto loadElapsedMs = juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - loadStartMs);

    if (behavesAsInstrument)
    {
        setStatus ("Added " + desc.pluginFormatName + " instrument: " + desc.name + " on " + track->getName()
                   + " (" + juce::String (loadElapsedMs) + " ms).");
    }
    else
    {
        setStatus ("Loaded " + desc.pluginFormatName + " plugin from instrument menu as FX chain insert: "
                   + desc.name + " on " + track->getName()
                   + " (" + juce::String (loadElapsedMs) + " ms).");
    }
}

void BeatMakerNoRecord::addExternalPluginToSelectedTrack()
{
    if (edit == nullptr)
        return;

    if (edit->getTransport().isPlaying())
    {
        setStatus ("Stop playback before inserting plugins.");
        return;
    }

    auto* track = getSelectedTrackOrFirst();
    if (track == nullptr)
    {
        setStatus ("Select a track first.");
        return;
    }

    auto& pluginManager = engine.getPluginManager();
    auto allPlugins = pluginManager.knownPluginList.getTypes();

    if (allPlugins.isEmpty())
    {
        setStatus ("No scanned plugins found. Click Scan Plugins first.");
        return;
    }

    std::vector<juce::PluginDescription> auEffects;
    std::vector<juce::PluginDescription> vst3Effects;
    std::vector<juce::PluginDescription> auInstrumentClassified;
    std::vector<juce::PluginDescription> vst3InstrumentClassified;

    for (const auto& desc : allPlugins)
    {
        const auto format = desc.pluginFormatName.toUpperCase();
        const bool isVst3 = format.contains ("VST3");
        const bool isAu = format.contains ("AUDIOUNIT") || format == "AU";

        if (! isVst3 && ! isAu)
            continue;

        if (desc.isInstrument)
        {
            if (isVst3)
                vst3InstrumentClassified.push_back (desc);
            else
                auInstrumentClassified.push_back (desc);

            continue;
        }

        if (isVst3)
            vst3Effects.push_back (desc);
        else if (isAu)
            auEffects.push_back (desc);
    }

    if (auEffects.empty() && vst3Effects.empty() && auInstrumentClassified.empty() && vst3InstrumentClassified.empty())
    {
        setStatus ("No AU/VST3 plugins available. Scan plugins and ensure effects are installed.");
        return;
    }

    auto byNameThenManufacturer = [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        const auto nameCompare = a.name.compareIgnoreCase (b.name);
        if (nameCompare != 0)
            return nameCompare < 0;

        return a.manufacturerName.compareIgnoreCase (b.manufacturerName) < 0;
    };

    std::sort (auEffects.begin(), auEffects.end(), byNameThenManufacturer);
    std::sort (vst3Effects.begin(), vst3Effects.end(), byNameThenManufacturer);
    std::sort (auInstrumentClassified.begin(), auInstrumentClassified.end(), byNameThenManufacturer);
    std::sort (vst3InstrumentClassified.begin(), vst3InstrumentClassified.end(), byNameThenManufacturer);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Add External Effect");

    struct FxMenuChoice
    {
        juce::PluginDescription description;
        bool instrumentClassified = false;
    };

    std::vector<FxMenuChoice> choiceById;
    choiceById.reserve (auEffects.size() + vst3Effects.size() + auInstrumentClassified.size() + vst3InstrumentClassified.size());
    int itemId = 1;

    auto populateSubmenu = [&choiceById, &itemId] (juce::PopupMenu& subMenu,
                                                   const std::vector<juce::PluginDescription>& list,
                                                   bool instrumentClassified)
    {
        for (const auto& desc : list)
        {
            juce::String label = desc.name.trim();
            if (label.isEmpty())
                label = desc.descriptiveName.trim();

            if (desc.manufacturerName.trim().isNotEmpty())
                label << " - " << desc.manufacturerName.trim();

            if (instrumentClassified)
                label << " [Inst-flagged]";

            subMenu.addItem (itemId, label);
            choiceById.push_back ({ desc, instrumentClassified });
            ++itemId;
        }
    };

    if (! vst3Effects.empty())
    {
        juce::PopupMenu vstMenu;
        populateSubmenu (vstMenu, vst3Effects, false);
        menu.addSubMenu ("VST3 Effects", vstMenu, true);
    }

    if (! auEffects.empty())
    {
        juce::PopupMenu auMenu;
        populateSubmenu (auMenu, auEffects, false);
        menu.addSubMenu ("AU Effects", auMenu, true);
    }

    if (! vst3InstrumentClassified.empty() || ! auInstrumentClassified.empty())
    {
        menu.addSeparator();
        menu.addSectionHeader ("Instrument-Classified Plugins (Insert As FX)");

        if (! vst3InstrumentClassified.empty())
        {
            juce::PopupMenu vstMenu;
            populateSubmenu (vstMenu, vst3InstrumentClassified, true);
            menu.addSubMenu ("VST3 Inst-Flagged", vstMenu, true);
        }

        if (! auInstrumentClassified.empty())
        {
            juce::PopupMenu auMenu;
            populateSubmenu (auMenu, auInstrumentClassified, true);
            menu.addSubMenu ("AU Inst-Flagged", auMenu, true);
        }
    }

    const int selectedItem = menu.showMenu (juce::PopupMenu::Options().withTargetComponent (&fxAddExternalButton)
                                                                      .withMinimumWidth (460));
    if (selectedItem <= 0)
        return;

    const auto selectedIndex = (size_t) (selectedItem - 1);
    if (selectedIndex >= choiceById.size())
        return;

    const auto& choice = choiceById[selectedIndex];
    const auto& desc = choice.description;
    setStatus ("Loading effect plugin: " + desc.name + "...");
    const auto loadStartMs = juce::Time::getMillisecondCounterHiRes();
    WaitCursorScope waitCursor;
    auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);
    if (plugin == nullptr)
    {
        setStatus ("Failed to load plugin: " + desc.name);
        return;
    }

    track->pluginList.insertPlugin (plugin, getPluginInsertIndexForTrack (*track, false), &selectionManager);
    plugin->setEnabled (true);
    refreshSelectedTrackPluginList();

    const int insertedIndex = track->pluginList.indexOf (plugin.get());
    if (insertedIndex >= 0)
        fxChainBox.setSelectedId (insertedIndex + 1, juce::dontSendNotification);

    markPlaybackRoutingNeedsPreparation();
    updateButtonsFromState();
    const auto loadElapsedMs = juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - loadStartMs);

    juce::String status = "Added " + desc.pluginFormatName + " effect: " + desc.name + " on " + track->getName();
    if (choice.instrumentClassified || plugin->isSynth())
        status << " (forced into FX chain)";

    status << " (" << juce::String (loadElapsedMs) << " ms).";
    setStatus (status);
}

void BeatMakerNoRecord::moveSelectedTrackPlugin (bool moveDown)
{
    if (edit == nullptr)
        return;

    auto* plugin = getSelectedTrackPlugin();
    if (plugin == nullptr)
    {
        setStatus ("Select a plugin in FX chain first.");
        return;
    }

    if (! plugin->canBeMoved())
    {
        setStatus ("Selected plugin cannot be moved.");
        return;
    }

    auto parent = plugin->state.getParent();
    const int currentIndex = parent.indexOf (plugin->state);
    const int nextIndex = currentIndex + (moveDown ? 1 : -1);

    if (currentIndex < 0 || nextIndex < 0 || nextIndex >= parent.getNumChildren())
    {
        setStatus (moveDown ? "Plugin already at end of chain." : "Plugin already at start of chain.");
        return;
    }

    parent.moveChild (currentIndex, nextIndex, &edit->getUndoManager());
    refreshSelectedTrackPluginList();
    fxChainBox.setSelectedId (nextIndex + 1, juce::dontSendNotification);
    markPlaybackRoutingNeedsPreparation();
    updateButtonsFromState();
    setStatus (moveDown ? "Moved plugin down." : "Moved plugin up.");
}

void BeatMakerNoRecord::toggleSelectedTrackPluginBypass()
{
    auto* plugin = getSelectedTrackPlugin();
    if (plugin == nullptr)
    {
        setStatus ("Select a plugin in FX chain first.");
        return;
    }

    if (! plugin->canBeDisabled())
    {
        setStatus ("Selected plugin cannot be bypassed.");
        return;
    }

    const bool wasEnabled = plugin->isEnabled();
    plugin->setEnabled (! wasEnabled);

    if (! wasEnabled && plugin->isSynth())
    {
        if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (plugin->getOwnerTrack()))
        {
            for (auto* other : ownerTrack->pluginList.getPlugins())
            {
                if (other == nullptr || other == plugin || ! other->isSynth())
                    continue;

                other->setEnabled (false);
            }
        }
    }

    refreshSelectedTrackPluginList();
    markPlaybackRoutingNeedsPreparation();
    updateButtonsFromState();
    setStatus (plugin->isEnabled() ? "Plugin enabled: " + plugin->getName()
                                   : "Plugin bypassed: " + plugin->getName());
}

void BeatMakerNoRecord::deleteSelectedTrackPlugin()
{
    auto* plugin = getSelectedTrackPlugin();
    if (plugin == nullptr)
    {
        setStatus ("Select a plugin in FX chain first.");
        return;
    }

    if (dynamic_cast<te::VolumeAndPanPlugin*> (plugin) != nullptr
        || dynamic_cast<te::LevelMeterPlugin*> (plugin) != nullptr)
    {
        setStatus ("Cannot delete core mixer plugins.");
        return;
    }

    const auto pluginName = plugin->getName();
    if (! confirmDestructiveAction ("Delete FX Plugin", "Delete plugin \"" + pluginName + "\" from this track?"))
    {
        setStatus ("Delete plugin cancelled.");
        return;
    }

    plugin->deleteFromParent();
    refreshSelectedTrackPluginList();
    markPlaybackRoutingNeedsPreparation();
    updateButtonsFromState();
    setStatus ("Deleted plugin: " + pluginName);
}

void BeatMakerNoRecord::adjustSelectedAudioClipGain (float deltaDb)
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    const float newGainDb = juce::jlimit (-60.0f, 24.0f, clip->getGainDB() + deltaDb);
    clip->setGainDB (newGainDb);
    updateButtonsFromState();
    setStatus ("Audio gain set to " + juce::String (newGainDb, 1) + " dB.");
}

void BeatMakerNoRecord::setSelectedAudioClipFade (bool fadeIn)
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    const auto fade = getGridDurationAt (clip->getPosition().getStart());
    const bool ok = fadeIn ? clip->setFadeIn (fade) : clip->setFadeOut (fade);

    if (! ok)
    {
        setStatus ("Unable to set clip fade.");
        return;
    }

    updateButtonsFromState();
    setStatus (fadeIn ? "Applied fade-in using current grid." : "Applied fade-out using current grid.");
}

void BeatMakerNoRecord::clearSelectedAudioClipFades()
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    clip->setFadeIn (te::TimeDuration());
    clip->setFadeOut (te::TimeDuration());
    updateButtonsFromState();
    setStatus ("Cleared audio clip fades.");
}

void BeatMakerNoRecord::toggleSelectedAudioClipReverse()
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    const bool shouldReverse = ! clip->getIsReversed();
    clip->setIsReversed (shouldReverse);
    clip->reverseLoopPoints();
    updateButtonsFromState();
    setStatus (shouldReverse ? "Audio clip reverse enabled." : "Audio clip reverse disabled.");
}

void BeatMakerNoRecord::scaleSelectedAudioClipSpeed (double factor)
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    applyHighQualitySettingsToAudioClip (*clip);
    const double newSpeed = juce::jlimit (0.125, 8.0, clip->getSpeedRatio() * factor);
    clip->setSpeedRatio (newSpeed);
    updateButtonsFromState();
    setStatus ("Audio clip speed: " + juce::String (newSpeed, 3) + "x");
}

void BeatMakerNoRecord::adjustSelectedAudioClipPitch (float semitones)
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    applyHighQualitySettingsToAudioClip (*clip);
    const float newPitch = clip->getPitchChange() + semitones;
    clip->setPitchChange (newPitch);
    updateButtonsFromState();
    setStatus ("Audio clip pitch shift: " + juce::String (clip->getPitchChange(), 2) + " semitones.");
}

void BeatMakerNoRecord::toggleSelectedAudioClipAutoTempo()
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    applyHighQualitySettingsToAudioClip (*clip);
    clip->setAutoTempo (! clip->getAutoTempo());
    updateButtonsFromState();
    setStatus (clip->getAutoTempo() ? "Audio clip AutoTempo enabled." : "Audio clip AutoTempo disabled.");
}

void BeatMakerNoRecord::toggleSelectedAudioClipWarp()
{
    auto* clip = getSelectedAudioClip();
    if (clip == nullptr)
    {
        setStatus ("Select an audio clip first.");
        return;
    }

    applyHighQualitySettingsToAudioClip (*clip);
    clip->setWarpTime (! clip->getWarpTime());
    updateButtonsFromState();
    setStatus (clip->getWarpTime() ? "Audio clip warp enabled." : "Audio clip warp disabled.");
}

void BeatMakerNoRecord::alignSelectedClipToBar()
{
    if (edit == nullptr)
        return;

    auto* clip = getSelectedClip();
    if (clip == nullptr)
    {
        setStatus ("Select a clip first.");
        return;
    }

    auto clipPosition = clip->getPosition();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipPosition.getStart()));
    const double startBeat = edit->tempoSequence.toBeats (clipPosition.getStart()).inBeats();
    const double snappedBeat = std::round (startBeat / beatsPerBar) * beatsPerBar;
    const auto snappedStart = edit->tempoSequence.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, snappedBeat)));

    if (std::abs ((snappedStart - clipPosition.getStart()).inSeconds()) < 1.0e-4)
    {
        setStatus ("Selected clip is already aligned to a bar.");
        return;
    }

    clip->setPosition ({ { snappedStart, clipPosition.getLength() }, clipPosition.getOffset() });
    setStatus ("Aligned selected clip to nearest bar.");
}

void BeatMakerNoRecord::makeSelectedClipLoop (int bars)
{
    if (edit == nullptr)
        return;

    auto* clip = getSelectedClip();
    if (clip == nullptr)
    {
        setStatus ("Select a clip first.");
        return;
    }

    const int barsToBuild = juce::jlimit (1, 64, bars);
    auto clipPosition = clip->getPosition();

    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipPosition.getStart()));
    const double startBeat = edit->tempoSequence.toBeats (clipPosition.getStart()).inBeats();
    const double snappedBeat = std::round (startBeat / beatsPerBar) * beatsPerBar;
    const auto snappedStart = edit->tempoSequence.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, snappedBeat)));

    if (std::abs ((snappedStart - clipPosition.getStart()).inSeconds()) > 1.0e-4)
    {
        clip->setPosition ({ { snappedStart, clipPosition.getLength() }, clipPosition.getOffset() });
        clipPosition = clip->getPosition();
    }

    const double barSeconds = juce::jmax (0.01, getBarDurationAt (clipPosition.getStart()).inSeconds());
    const double targetLengthSeconds = juce::jmax (0.01, barSeconds * (double) barsToBuild);
    const double targetStartSeconds = clipPosition.getStart().inSeconds();
    const double targetEndSeconds = targetStartSeconds + targetLengthSeconds;

    double sourceLengthSeconds = juce::jmax (0.01, clipPosition.getLength().inSeconds());
    if (sourceLengthSeconds > targetLengthSeconds)
    {
        clip->setEnd (te::TimePosition::fromSeconds (targetEndSeconds), true);
        clipPosition = clip->getPosition();
        sourceLengthSeconds = juce::jmax (0.01, clipPosition.getLength().inSeconds());
    }

    int segmentCount = 1;
    double nextStartSeconds = clipPosition.getEnd().inSeconds();

    while (nextStartSeconds < targetEndSeconds - 1.0e-4)
    {
        auto duplicatedClip = te::duplicateClip (*clip);
        if (duplicatedClip == nullptr)
            break;

        const double remainingSeconds = targetEndSeconds - nextStartSeconds;
        const double segmentLengthSeconds = juce::jmax (0.01, juce::jmin (sourceLengthSeconds, remainingSeconds));
        const auto duplicatedPosition = duplicatedClip->getPosition();
        duplicatedClip->setPosition ({ { te::TimePosition::fromSeconds (nextStartSeconds),
                                         te::TimeDuration::fromSeconds (segmentLengthSeconds) },
                                       duplicatedPosition.getOffset() });
        nextStartSeconds += segmentLengthSeconds;
        ++segmentCount;
    }

    edit->getTransport().setLoopRange ({ te::TimePosition::fromSeconds (targetStartSeconds),
                                         te::TimeDuration::fromSeconds (targetLengthSeconds) });
    edit->getTransport().looping = true;
    updateTransportLoopButton();
    timelineRuler.repaint();
    selectionManager.selectOnly (clip);
    setStatus ("Built " + juce::String (barsToBuild) + "-bar loop from selected clip (" + juce::String (segmentCount) + " segment(s)).");
}

void BeatMakerNoRecord::fillTransportLoopWithSelectedClip()
{
    if (edit == nullptr)
        return;

    auto* clip = getSelectedClip();
    if (clip == nullptr)
    {
        setStatus ("Select a clip first.");
        return;
    }

    const auto loopRange = edit->getTransport().getLoopRange();
    if (loopRange.isEmpty())
    {
        setStatus ("Set a transport loop range first.");
        return;
    }

    const double loopStartSeconds = loopRange.getStart().inSeconds();
    const double loopEndSeconds = loopRange.getEnd().inSeconds();
    if (loopEndSeconds <= loopStartSeconds + 1.0e-4)
    {
        setStatus ("Transport loop range is too short.");
        return;
    }

    auto clipPosition = clip->getPosition();
    const double sourceLengthSeconds = juce::jmax (0.01, clipPosition.getLength().inSeconds());
    const double firstSegmentLengthSeconds = juce::jmax (0.01, juce::jmin (sourceLengthSeconds, loopEndSeconds - loopStartSeconds));

    clip->setPosition ({ { te::TimePosition::fromSeconds (loopStartSeconds),
                           te::TimeDuration::fromSeconds (firstSegmentLengthSeconds) },
                         clipPosition.getOffset() });

    int segmentCount = 1;
    double nextStartSeconds = loopStartSeconds + firstSegmentLengthSeconds;

    while (nextStartSeconds < loopEndSeconds - 1.0e-4)
    {
        auto duplicatedClip = te::duplicateClip (*clip);
        if (duplicatedClip == nullptr)
            break;

        const double remainingSeconds = loopEndSeconds - nextStartSeconds;
        const double segmentLengthSeconds = juce::jmax (0.01, juce::jmin (sourceLengthSeconds, remainingSeconds));
        const auto duplicatedPosition = duplicatedClip->getPosition();
        duplicatedClip->setPosition ({ { te::TimePosition::fromSeconds (nextStartSeconds),
                                         te::TimeDuration::fromSeconds (segmentLengthSeconds) },
                                       duplicatedPosition.getOffset() });
        nextStartSeconds += segmentLengthSeconds;
        ++segmentCount;
    }

    edit->getTransport().looping = true;
    updateTransportLoopButton();
    timelineRuler.repaint();
    selectionManager.selectOnly (clip);
    setStatus ("Filled transport loop with selected clip (" + juce::String (segmentCount) + " segment(s)).");
}

void BeatMakerNoRecord::copySelection()
{
    if (selectionManager.copySelected())
        setStatus ("Copied selection.");
    else
        setStatus ("Nothing to copy.");
}

void BeatMakerNoRecord::cutSelection()
{
    if (selectionManager.cutSelected())
        setStatus ("Cut selection.");
    else
        setStatus ("Nothing to cut.");
}

void BeatMakerNoRecord::pasteSelection()
{
    if (selectionManager.pasteSelected())
        setStatus ("Pasted.");
    else
        setStatus ("Nothing to paste.");
}

void BeatMakerNoRecord::selectAllEditableItems()
{
    if (edit == nullptr)
        return;

    te::SelectableList all;
    for (auto* track : te::getAudioTracks (*edit))
    {
        all.add (track);
        for (auto* clip : track->getClips())
            all.add (clip);
    }

    if (all.isNotEmpty())
    {
        selectionManager.select (all);
        setStatus ("Selected all clips and tracks.");
    }
}

void BeatMakerNoRecord::duplicateSelectedClip()
{
    if (auto* clip = getSelectedClip())
    {
        if (auto duplicated = te::duplicateClip (*clip))
        {
            const auto sourcePosition = clip->getPosition();
            duplicated->setPosition ({ { sourcePosition.getEnd(), sourcePosition.getLength() }, sourcePosition.getOffset() });
            selectionManager.selectOnly (duplicated.get());
            setStatus ("Duplicated clip.");
        }
    }
}

void BeatMakerNoRecord::splitAllTracksAtPlayhead()
{
    if (edit == nullptr)
        return;

    const auto playhead = edit->getTransport().getPosition();
    for (auto* track : te::getClipTracks (*edit))
        track->splitAt (playhead);

    setStatus ("Split all tracks at playhead.");
}

void BeatMakerNoRecord::splitSelectedClipAtPlayhead()
{
    if (edit == nullptr)
        return;

    auto newSelection = te::splitClips (selectionManager.getSelectedObjects(), edit->getTransport().getPosition());
    if (newSelection.isNotEmpty())
    {
        selectionManager.select (newSelection);
        setStatus ("Split clip(s) at playhead.");
    }
    else
    {
        setStatus ("No clips were split.");
    }
}

void BeatMakerNoRecord::trimSelectedClipStartToPlayhead()
{
    if (edit == nullptr)
        return;

    if (auto* clip = getSelectedClip())
    {
        const auto playhead = edit->getTransport().getPosition();
        const auto range = clip->getEditTimeRange();

        if (playhead > range.getStart() && playhead < range.getEnd())
        {
            clip->setStart (playhead, true, false);
            setStatus ("Trimmed clip start to playhead.");
        }
    }
}

void BeatMakerNoRecord::trimSelectedClipEndToPlayhead()
{
    if (edit == nullptr)
        return;

    if (auto* clip = getSelectedClip())
    {
        const auto playhead = edit->getTransport().getPosition();
        const auto range = clip->getEditTimeRange();

        if (playhead > range.getStart() && playhead < range.getEnd())
        {
            clip->setEnd (playhead, true);
            setStatus ("Trimmed clip end to playhead.");
        }
    }
}

void BeatMakerNoRecord::moveSelectedClipBoundaryToCursor (bool moveStart)
{
    if (edit == nullptr)
        return;

    te::moveSelectedClips (selectionManager.getSelectedObjects(), *edit,
                           moveStart ? te::MoveClipAction::moveStartToCursor
                                     : te::MoveClipAction::moveEndToCursor,
                           false);

    setStatus (moveStart ? "Moved selected clip starts to cursor." : "Moved selected clip ends to cursor.");
}

void BeatMakerNoRecord::nudgeSelectedClip (bool moveRight)
{
    if (edit == nullptr)
        return;

    auto* clip = getSelectedClip();
    if (clip == nullptr)
        return;

    const auto clipPosition = clip->getPosition();
    const auto grid = getGridDurationAt (clipPosition.getStart());
    const auto nudgeSeconds = (moveRight ? 1.0 : -1.0) * grid.inSeconds();
    const auto newStartSeconds = juce::jmax (0.0, (clipPosition.getStart() + te::TimeDuration::fromSeconds (nudgeSeconds)).inSeconds());

    clip->setPosition ({ { te::TimePosition::fromSeconds (newStartSeconds), clipPosition.getLength() }, clipPosition.getOffset() });
    setStatus (moveRight ? "Nudged clip right." : "Nudged clip left.");
}

void BeatMakerNoRecord::slipSelectedClipContent (bool forward)
{
    auto* clip = getSelectedClip();
    if (clip == nullptr || edit == nullptr)
        return;

    const auto delta = getGridDurationAt (clip->getPosition().getStart());
    const auto signedDelta = forward ? delta.inSeconds() : -delta.inSeconds();
    const auto newOffsetSeconds = juce::jmax (0.0, (clip->getPosition().getOffset() + te::TimeDuration::fromSeconds (signedDelta)).inSeconds());
    clip->setOffset (te::TimeDuration::fromSeconds (newOffsetSeconds));
    setStatus (forward ? "Slipped clip content right." : "Slipped clip content left.");
}

void BeatMakerNoRecord::moveSelectedClipToNeighbour (bool moveToNext)
{
    if (edit == nullptr)
        return;

    te::moveSelectedClips (selectionManager.getSelectedObjects(), *edit,
                           moveToNext ? te::MoveClipAction::moveToStartOfNext
                                      : te::MoveClipAction::moveToEndOfLast,
                           false);

    setStatus (moveToNext ? "Moved selected clips to start of next clip." : "Moved selected clips to end of previous clip.");
}

void BeatMakerNoRecord::insertBarAtPlayhead()
{
    if (edit == nullptr)
        return;

    const auto cursor = edit->getTransport().getPosition();
    te::insertSpaceIntoEdit (*edit, { cursor, getBarDurationAt (cursor) });
    setStatus ("Inserted one bar at playhead.");
}

void BeatMakerNoRecord::deleteBarAtPlayhead()
{
    if (edit == nullptr)
        return;

    const auto cursor = edit->getTransport().getPosition();
    if (! confirmDestructiveAction ("Delete Bar", "Delete one bar at the current playhead position?"))
    {
        setStatus ("Delete bar cancelled.");
        return;
    }

    te::deleteRegionOfTracks (*edit,
                              { cursor, getBarDurationAt (cursor) },
                              false,
                              te::CloseGap::yes,
                              &selectionManager);
    setStatus ("Deleted one bar at playhead.");
}

void BeatMakerNoRecord::quantizeSelectedMidiClip()
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr || edit == nullptr)
    {
        setStatus ("Select a MIDI clip to quantize.");
        return;
    }

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Quantize MIDI Clip");

    auto quant = midiClip->getQuantisation();
    quant.setType (quantizeTypeBox.getText());
    quant.setProportion (1.0f);
    quant.setIsQuantisingNoteOffs (false);
    midiClip->setQuantisation (quant);

    int clamped = 0;
    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    auto& sequence = midiClip->getSequence();
    for (auto* note : sequence.getNotes())
    {
        if (note == nullptr)
            continue;

        const double originalStart = note->getStartBeat().inBeats();
        const double originalLength = juce::jmax (minimumMidiNoteLengthBeats, note->getLengthBeats().inBeats());
        const double maxStart = juce::jmax (0.0, clipLengthBeats - minimumMidiNoteLengthBeats);
        const double clampedStart = juce::jlimit (0.0, maxStart, originalStart);
        const double maxLength = juce::jmax (minimumMidiNoteLengthBeats, clipLengthBeats - clampedStart);
        const double clampedLength = juce::jlimit (minimumMidiNoteLengthBeats, maxLength, originalLength);

        if (std::abs (clampedStart - originalStart) > 1.0e-7
            || std::abs (clampedLength - originalLength) > 1.0e-7)
        {
            note->setStartAndLength (te::BeatPosition::fromBeats (clampedStart),
                                     te::BeatDuration::fromBeats (clampedLength),
                                     &undoManager);
            ++clamped;
        }
    }

    setStatus ("Applied MIDI quantize: " + quantizeTypeBox.getText()
               + (clamped > 0 ? " | clamped " + juce::String (clamped) + " note(s) to clip range." : juce::String()));
}

void BeatMakerNoRecord::transposeSelectedMidiNotes (int semitones)
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        setStatus ("Select a MIDI clip to transpose.");
        return;
    }

    auto& sequence = midiClip->getSequence();
    const auto notes = sequence.getNotes();
    if (notes.isEmpty())
    {
        setStatus ("Selected MIDI clip has no notes.");
        return;
    }

    auto* undoManager = edit != nullptr ? &edit->getUndoManager() : nullptr;
    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Transpose MIDI Notes");

    int changed = 0;

    for (auto* note : notes)
    {
        const int original = note->getNoteNumber();
        const int shifted = juce::jlimit (0, 127, original + semitones);
        if (shifted != original)
        {
            note->setNoteNumber (shifted, undoManager);
            ++changed;
        }
    }

    setStatus ("Transposed MIDI notes by " + juce::String (semitones) + " semitone(s): " + juce::String (changed) + " note(s).");
}

void BeatMakerNoRecord::adjustSelectedMidiNoteVelocity (int delta)
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        setStatus ("Select a MIDI clip to edit velocity.");
        return;
    }

    auto& sequence = midiClip->getSequence();
    const auto notes = sequence.getNotes();
    if (notes.isEmpty())
    {
        setStatus ("Selected MIDI clip has no notes.");
        return;
    }

    auto* undoManager = edit != nullptr ? &edit->getUndoManager() : nullptr;
    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Adjust MIDI Velocity");

    int changed = 0;

    for (auto* note : notes)
    {
        const int original = note->getVelocity();
        const int next = juce::jlimit (1, 127, original + delta);
        if (next != original)
        {
            note->setVelocity (next, undoManager);
            ++changed;
        }
    }

    setStatus ("Adjusted MIDI velocity by " + juce::String (delta) + ": " + juce::String (changed) + " note(s).");
}

void BeatMakerNoRecord::humanizeSelectedMidiTiming (double maxJitterBeats)
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        setStatus ("Select a MIDI clip to humanize timing.");
        return;
    }

    auto& sequence = midiClip->getSequence();
    const auto notes = sequence.getNotes();
    if (notes.isEmpty())
    {
        setStatus ("Selected MIDI clip has no notes.");
        return;
    }

    auto* undoManager = edit != nullptr ? &edit->getUndoManager() : nullptr;
    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Humanize MIDI Timing");

    juce::Random random;
    int changed = 0;
    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    const double spreadBeats = std::abs (maxJitterBeats);

    for (auto* note : notes)
    {
        const double jitter = (random.nextDouble() * 2.0 - 1.0) * spreadBeats;
        const auto currentStart = note->getStartBeat().inBeats();
        const double noteLength = juce::jmax (minimumMidiNoteLengthBeats, note->getLengthBeats().inBeats());
        const double maxStart = juce::jmax (0.0, clipLengthBeats - noteLength);
        const auto newStart = juce::jlimit (0.0, maxStart, currentStart + jitter);

        if (std::abs (newStart - currentStart) > 1.0e-7)
        {
            note->setStartAndLength (te::BeatPosition::fromBeats (newStart), note->getLengthBeats(), undoManager);
            ++changed;
        }
    }

    setStatus ("Humanized MIDI timing (" + juce::String (spreadBeats, 3) + " beats max): "
               + juce::String (changed) + " note(s).");
}

void BeatMakerNoRecord::humanizeSelectedMidiVelocity (int maxDelta)
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        setStatus ("Select a MIDI clip to humanize velocity.");
        return;
    }

    auto& sequence = midiClip->getSequence();
    const auto notes = sequence.getNotes();
    if (notes.isEmpty())
    {
        setStatus ("Selected MIDI clip has no notes.");
        return;
    }

    auto* undoManager = edit != nullptr ? &edit->getUndoManager() : nullptr;
    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Humanize MIDI Velocity");

    juce::Random random;
    const int spread = juce::jmax (1, std::abs (maxDelta));
    int changed = 0;

    for (auto* note : notes)
    {
        const int offset = random.nextInt (2 * spread + 1) - spread;
        const int original = note->getVelocity();
        const int next = juce::jlimit (1, 127, original + offset);
        if (next != original)
        {
            note->setVelocity (next, undoManager);
            ++changed;
        }
    }

    setStatus ("Humanized MIDI velocity (+/-" + juce::String (spread) + "): " + juce::String (changed) + " note(s).");
}

void BeatMakerNoRecord::legatoSelectedMidiNotes()
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr || edit == nullptr)
    {
        setStatus ("Select a MIDI clip to apply legato.");
        return;
    }

    auto& sequence = midiClip->getSequence();
    juce::Array<te::MidiNote*> orderedNotes;
    for (auto* note : sequence.getNotes())
        orderedNotes.add (note);

    if (orderedNotes.size() < 2)
    {
        setStatus ("Need at least two MIDI notes for legato.");
        return;
    }

    te::MidiList::sortMidiEventsByTime (orderedNotes);

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Legato MIDI Notes");
    const auto maxEndBeat = sequence.getLastBeatNumber();

    for (auto* note : orderedNotes)
        midiClip->legatoNote (*note, orderedNotes, maxEndBeat, undoManager);

    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    for (auto* note : orderedNotes)
    {
        if (note == nullptr)
            continue;

        const double currentStart = note->getStartBeat().inBeats();
        const double currentLength = juce::jmax (minimumMidiNoteLengthBeats, note->getLengthBeats().inBeats());
        const double maxStart = juce::jmax (0.0, clipLengthBeats - minimumMidiNoteLengthBeats);
        const double clampedStart = juce::jlimit (0.0, maxStart, currentStart);
        const double maxLength = juce::jmax (minimumMidiNoteLengthBeats, clipLengthBeats - clampedStart);
        const double clampedLength = juce::jlimit (minimumMidiNoteLengthBeats, maxLength, currentLength);

        if (std::abs (clampedStart - currentStart) > 1.0e-7
            || std::abs (clampedLength - currentLength) > 1.0e-7)
        {
            note->setStartAndLength (te::BeatPosition::fromBeats (clampedStart),
                                     te::BeatDuration::fromBeats (clampedLength),
                                     &undoManager);
        }
    }

    setStatus ("Applied legato to MIDI notes: " + juce::String (orderedNotes.size()) + " note(s).");
}

void BeatMakerNoRecord::bounceSelectedMidiClipToAudio()
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr || edit == nullptr)
    {
        setStatus ("Select a MIDI clip to bounce.");
        return;
    }

    auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack());
    if (ownerTrack == nullptr)
    {
        setStatus ("Selected MIDI clip is not on an audio track.");
        return;
    }

    const bool addedInstrument = ensureTrackHasInstrumentForMidiPlayback (*ownerTrack);
    if (! trackHasInstrumentPlugin (*ownerTrack))
    {
        setStatus ("Cannot bounce: no enabled instrument on selected MIDI track.");
        return;
    }

    const auto clipRange = midiClip->getEditTimeRange();
    if (clipRange.getLength().inSeconds() <= 1.0e-5)
    {
        setStatus ("Cannot bounce: selected MIDI clip has zero length.");
        return;
    }

    auto projectDirectory = currentEditFile.existsAsFile()
                                ? currentEditFile.getParentDirectory()
                                : getProjectsRootDirectory().getChildFile ("Untitled Project");

    if (! projectDirectory.isDirectory() && ! projectDirectory.createDirectory())
    {
        setStatus ("Unable to create project directory for bounce.");
        return;
    }

    auto exportsDirectory = projectDirectory.getChildFile ("Exports");
    if (! exportsDirectory.isDirectory() && ! exportsDirectory.createDirectory())
    {
        setStatus ("Unable to create Exports directory for bounce.");
        return;
    }

    auto bouncesDirectory = exportsDirectory.getChildFile ("Bounces");
    if (! bouncesDirectory.isDirectory() && ! bouncesDirectory.createDirectory())
    {
        setStatus ("Unable to create Bounces directory.");
        return;
    }

    auto makeSafeName = [] (juce::String name)
    {
        name = name.trim();
        if (name.isEmpty())
            name = "MidiBounce";

        for (const auto c : juce::String ("\\/:*?\"<>|"))
            name = name.replaceCharacter (c, '_');

        name = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        name = name.replaceCharacter (' ', '_').trim();
        return name.isEmpty() ? juce::String ("MidiBounce") : name;
    };

    const auto baseName = makeSafeName (ownerTrack->getName() + "_" + midiClip->getName() + "_bounce");
    const auto timeStamp = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
    const auto targetFile = bouncesDirectory.getNonexistentChildFile (baseName + "_" + timeStamp, ".wav", false);

    auto allTracks = te::getAllTracks (*edit);
    const int ownerTrackIndex = allTracks.indexOf (ownerTrack);
    if (ownerTrackIndex < 0)
    {
        setStatus ("Cannot bounce: failed to resolve track index.");
        return;
    }

    juce::BigInteger tracksToDo;
    tracksToDo.setBit (ownerTrackIndex);

    auto* defaultFormat = engine.getAudioFileFormatManager().getDefaultFormat();
    juce::WavAudioFormat wavFormat;

    te::Renderer::Parameters params (*edit);
    params.edit = edit.get();
    params.destFile = targetFile;
    params.audioFormat = defaultFormat != nullptr ? defaultFormat : static_cast<juce::AudioFormat*> (&wavFormat);
    params.bitDepth = 24;
    params.blockSizeForAudio = juce::jmax (128, engine.getDeviceManager().getBlockSize());
    params.sampleRateForAudio = juce::jmax (8000.0, engine.getDeviceManager().getSampleRate());
    params.time = clipRange;
    params.endAllowance = te::TimeDuration::fromSeconds (1.5);
    params.tracksToDo = tracksToDo;
    params.allowedClips.add (midiClip);
    params.usePlugins = true;
    params.useMasterPlugins = false;
    params.canRenderInMono = false;
    params.mustRenderInMono = false;

    setStatus ("Bouncing MIDI clip to audio...");
    WaitCursorScope waitCursor;
    const auto renderedFile = te::Renderer::renderToFile ("Bounce MIDI to Audio", params);

    if (! renderedFile.existsAsFile())
    {
        setStatus ("MIDI bounce failed.");
        return;
    }

    te::AudioFile renderedAudio (engine, renderedFile);
    if (! renderedAudio.isValid())
    {
        setStatus ("Bounce completed but rendered file could not be loaded.");
        return;
    }

    const auto renderedLength = te::TimeDuration::fromSeconds (juce::jmax (0.01, renderedAudio.getLength()));
    const te::ClipPosition bouncedPosition { { clipRange.getStart(), renderedLength }, {} };
    const juce::String bouncedClipName = midiClip->getName().trim().isNotEmpty() ? midiClip->getName() + " Bounce"
                                                                                  : "MIDI Bounce";

    if (auto bouncedClip = ownerTrack->insertWaveClip (bouncedClipName, renderedFile, bouncedPosition, false))
    {
        applyHighQualitySettingsToAudioClip (*bouncedClip);
        selectionManager.selectOnly (bouncedClip.get());
        setStatus ("Bounced MIDI clip to audio: " + renderedFile.getFileName()
                   + (addedInstrument ? " (auto-added instrument)" : ""));
    }
    else
    {
        setStatus ("Bounce rendered, but adding the audio clip failed.");
    }
}

void BeatMakerNoRecord::toggleSelectedClipLooping()
{
    if (auto* clip = getSelectedClip())
    {
        if (! clip->canLoop())
        {
            setStatus ("Selected clip does not support looping.");
            return;
        }

        if (clip->isLooping())
        {
            clip->disableLooping();
            setStatus ("Clip looping disabled.");
        }
        else
        {
            clip->setNumberOfLoops (4);
            setStatus ("Clip looping enabled (4 loops).");
        }
    }
}

void BeatMakerNoRecord::renameSelectedClip()
{
    auto* clip = getSelectedClip();
    if (clip == nullptr)
        return;

    juce::AlertWindow w ("Rename Clip", "Enter a new clip name", juce::AlertWindow::NoIcon);
    w.addTextEditor ("name", clip->getName());
    w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    if (w.runModalLoop() == 1)
    {
        const auto newName = w.getTextEditorContents ("name").trim();
        if (newName.isNotEmpty())
        {
            clip->setName (newName);
            setStatus ("Renamed clip to: " + newName);
        }
    }
}

void BeatMakerNoRecord::deleteSelectedItem()
{
    auto* selected = selectionManager.getSelectedObject (0);

    if (auto* clip = dynamic_cast<te::Clip*> (selected))
    {
        if (! confirmDestructiveAction ("Delete Clip", "Delete selected clip \"" + clip->getName() + "\"?"))
        {
            setStatus ("Delete clip cancelled.");
            return;
        }

        clip->removeFromParent();
        setStatus ("Deleted clip.");
        syncViewControlsFromState();
        return;
    }

    if (auto* track = dynamic_cast<te::Track*> (selected))
    {
        if (edit != nullptr && ! (track->isMasterTrack() || track->isTempoTrack() || track->isMarkerTrack() || track->isChordTrack()))
        {
            if (! confirmDestructiveAction ("Delete Track", "Delete selected track \"" + track->getName() + "\" and its clips/plugins?"))
            {
                setStatus ("Delete track cancelled.");
                return;
            }

            edit->deleteTrack (track);
            markPlaybackRoutingNeedsPreparation();
            setStatus ("Deleted track.");
            syncViewControlsFromState();
        }
        return;
    }

    if (auto* plugin = dynamic_cast<te::Plugin*> (selected))
    {
        if (! confirmDestructiveAction ("Delete Plugin", "Delete selected plugin \"" + plugin->getName() + "\"?"))
        {
            setStatus ("Delete plugin cancelled.");
            return;
        }

        plugin->deleteFromParent();
        markPlaybackRoutingNeedsPreparation();
        setStatus ("Deleted plugin.");
    }
}

void BeatMakerNoRecord::setTransportLoopToSelectedClip()
{
    if (edit == nullptr)
        return;

    auto* clip = getSelectedClip();
    if (clip == nullptr)
        return;

    edit->getTransport().setLoopRange (clip->getEditTimeRange());
    edit->getTransport().looping = true;
    updateTransportLoopButton();
    setStatus ("Transport loop set to selected clip.");
}

void BeatMakerNoRecord::toggleTransportLooping()
{
    if (edit == nullptr)
        return;

    auto& transport = edit->getTransport();
    transport.looping = ! transport.looping;

    if (transport.looping && transport.getLoopRange().isEmpty())
        transport.setLoopRange ({ te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (4.0) });

    updateTransportLoopButton();
}

void BeatMakerNoRecord::zoomTimeline (double factor)
{
    if (editComponent == nullptr)
        return;

    const auto anchorTime = edit != nullptr ? edit->getTransport().getPosition()
                                            : te::TimePosition::fromSeconds (0.0);
    zoomTimelineAroundTime (factor, anchorTime);
}

void BeatMakerNoRecord::zoomTimelineVertically (double factor)
{
    if (editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const double minTrackHeight = trackHeightSlider.getMinimum();
    const double maxTrackHeight = trackHeightSlider.getMaximum();
    const double nextTrackHeight = juce::jlimit (minTrackHeight,
                                                 maxTrackHeight,
                                                 viewState.trackHeight.get() * factor);
    viewState.trackHeight = nextTrackHeight;
    syncViewControlsFromState();
}

void BeatMakerNoRecord::resetZoom()
{
    if (editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    viewState.viewX1 = te::TimePosition::fromSeconds (0.0);
    viewState.viewX2 = te::TimePosition::fromSeconds (16.0);
    syncViewControlsFromState();
}

void BeatMakerNoRecord::resetVerticalZoom()
{
    if (editComponent == nullptr)
        return;

    editComponent->getEditViewState().trackHeight = 58.0;
    syncViewControlsFromState();
}

void BeatMakerNoRecord::applyTrackMixerFromUI()
{
    if (updatingTrackControls || edit == nullptr)
        return;

    auto* track = getSelectedTrackOrFirst();
    if (track == nullptr)
        return;

    track->setMute (trackMuteButton.getToggleState());
    track->setSolo (trackSoloButton.getToggleState());

    if (auto* volumePlugin = track->getVolumePlugin())
    {
        volumePlugin->setVolumeDb ((float) trackVolumeSlider.getValue());
        volumePlugin->setPan ((float) trackPanSlider.getValue());
    }

    mixerArea.repaint();
}

void BeatMakerNoRecord::applyTempoFromUI()
{
    if (updatingTrackControls || edit == nullptr)
        return;

    if (auto* firstTempo = edit->tempoSequence.getTempo (0))
        firstTempo->setBpm (tempoSlider.getValue());

    updateTransportInfoLabel();
}
