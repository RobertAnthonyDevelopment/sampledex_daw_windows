#include "../BeatMakerNoRecord.h"
#include "SharedLayoutSystem.h"

namespace
{
beatmaker::layout::DensityMode getLayoutDensityModeForSession (int densityMode)
{
    if (densityMode == 0)
        return beatmaker::layout::DensityMode::compact;
    if (densityMode == 2)
        return beatmaker::layout::DensityMode::accessible;
    return beatmaker::layout::DensityMode::comfortable;
}
}

void BeatMakerNoRecord::setupSessionHeaderCallbacks()
{
    newEditButton.onClick = [this] { createNewEdit (false); };
    openEditButton.onClick = [this] { openEdit(); };
    saveButton.onClick = [this] { saveEdit(); };
    saveAsButton.onClick = [this] { saveEditAs(); };
    undoButton.onClick = [this] { if (edit != nullptr) edit->getUndoManager().undo(); };
    redoButton.onClick = [this] { if (edit != nullptr) edit->getUndoManager().redo(); };
    helpButton.onClick = [this] { showShortcutOverlay(); };
    beatmakerSpaceButton.onClick = [this] { applyBeatmakerTrackAreaFocusLayout (true, true); };
    startBeatQuickButton.onClick = [this]
    {
        if (edit == nullptr)
            return;

        auto shouldCreateFreshMidiTrack = [this] (te::AudioTrack* track) -> bool
        {
            if (track == nullptr)
                return true;

            if (trackHasMidiContent (*track))
                return false;

            // Preserve existing audio-focused tracks by creating a dedicated
            // MIDI/instrument track for quick-start workflow.
            for (auto* clip : track->getClips())
                if (clip != nullptr && dynamic_cast<te::MidiClip*> (clip) == nullptr)
                    return true;

            return false;
        };

        auto* targetTrack = getSelectedTrackOrFirst();
        if (shouldCreateFreshMidiTrack (targetTrack))
            addMidiTrack();

        targetTrack = getSelectedTrackOrFirst();
        if (targetTrack != nullptr)
            selectionManager.selectOnly (targetTrack);

        applyBeatmakerTrackAreaFocusLayout (true, false);
        addExternalInstrumentPluginToSelectedTrack();

        targetTrack = getSelectedTrackOrFirst();
        if (targetTrack != nullptr && ! trackHasInstrumentPlugin (*targetTrack))
            ensureTrackHasInstrumentForMidiPlayback (*targetTrack);

        if (targetTrack != nullptr && ! trackHasMidiContent (*targetTrack))
            createMidiClip();

        if (auto* midiClip = getSelectedMidiClip())
            openMidiClipInPianoRoll (*midiClip, false);
    };
    focusSelectionButton.onClick = [this] { focusSelectedClipInView(); };
    centerPlayheadButton.onClick = [this] { centerPlayheadInView(); };
    fitProjectButton.onClick = [this] { fitProjectInView(); };

    playPauseButton.onClick = [this]
    {
        if (edit != nullptr)
        {
            if (! edit->getTransport().isPlaying())
                prepareEditForPluginPlayback (true);

            EngineHelpers::togglePlay (*edit);
        }
    };
    stopButton.onClick = [this]
    {
        if (edit != nullptr)
            edit->getTransport().stop (false, false);
    };
    returnToStartButton.onClick = [this]
    {
        if (edit != nullptr)
            edit->getTransport().setPosition (te::TimePosition::fromSeconds (0.0));
    };
    transportLoopButton.onClick = [this] { toggleTransportLooping(); };
    setLoopToSelectionButton.onClick = [this] { setTransportLoopToSelectedClip(); };
    jumpPrevBarButton.onClick = [this] { jumpByBar (false); };
    jumpNextBarButton.onClick = [this] { jumpByBar (true); };
    zoomInButton.onClick = [this] { zoomTimeline (0.7); };
    zoomOutButton.onClick = [this] { zoomTimeline (1.4); };
    zoomResetButton.onClick = [this] { resetZoom(); };
}

void BeatMakerNoRecord::layoutSessionHeaderPanel (juce::Rectangle<int> bounds, bool detachedMode)
{
    juce::ignoreUnused (detachedMode);

    const int densityOrdinal = uiDensityMode == UiDensityMode::compact ? 0
                             : (uiDensityMode == UiDensityMode::accessible ? 2 : 1);
    const auto metrics = beatmaker::layout::makeMetrics (bounds,
                                                         getUiDensityScale(),
                                                         getLayoutDensityModeForSession (densityOrdinal),
                                                         false);

    auto sectionBounds = beatmaker::layout::insetSection (bounds, metrics);
    auto sessionArea = beatmaker::layout::groupContent (sessionGroup, sectionBounds, metrics);

    const int sessionInnerWidth = juce::jmax (220, sessionArea.getWidth());
    const int row1TransportWidth = juce::jmax (metrics.denseLayout ? 220 : 300,
                                               sessionInnerWidth / (metrics.denseLayout ? 2 : 3));
    const int row1EditNameWidth = juce::jmax (130, juce::jmax (0, sessionInnerWidth - row1TransportWidth) / 4);
    const int row1ButtonsWidth = juce::jmax (120, sessionInnerWidth - row1TransportWidth - row1EditNameWidth - 8);
    const int row2TempoWidth = juce::jmax (metrics.denseLayout ? 180 : 220,
                                           sessionInnerWidth / (metrics.denseLayout ? 2 : 3));
    const int row2ButtonsWidth = juce::jmax (120, sessionInnerWidth - row2TempoWidth - 8);
    const int row3QuickWidth = juce::jmax (metrics.denseLayout ? 260 : 420, (sessionInnerWidth * 3) / 5);
    const int row3StateWidth = juce::jmax (120, sessionInnerWidth - row3QuickWidth - 8);

    const int row1 = juce::jmax (metrics.defaultRowHeight + 2,
                                 beatmaker::layout::adaptiveButtonRowHeight (metrics, row1ButtonsWidth, 7, 2));
    const int row2 = juce::jmax (metrics.defaultRowHeight + 2,
                                 beatmaker::layout::adaptiveButtonRowHeight (metrics, row2ButtonsWidth, 8, 2));
    const int row3 = juce::jmax (metrics.defaultRowHeight + 2,
                                 juce::jmax (beatmaker::layout::adaptiveButtonRowHeight (metrics, row3QuickWidth, 5, 2),
                                             beatmaker::layout::adaptiveButtonRowHeight (metrics, row3StateWidth, 1, 1)));

    auto rowArea1 = beatmaker::layout::nextRow (sessionArea, metrics, row1);
    auto transportHud = rowArea1.removeFromRight (juce::jmax (metrics.denseLayout ? 220 : 300,
                                                              rowArea1.getWidth() / (metrics.denseLayout ? 2 : 3)));
    transportHud.removeFromLeft (juce::jmin (8, transportHud.getWidth()));
    auto editNameArea = rowArea1.removeFromRight (juce::jmax (130, rowArea1.getWidth() / 4));
    if (rowArea1.getWidth() > 6)
        rowArea1.removeFromRight (6);
    beatmaker::layout::layoutButtonRow (rowArea1, metrics,
                                        { &newEditButton, &openEditButton, &saveButton, &saveAsButton,
                                          &undoButton, &redoButton, &helpButton });
    editNameLabel.setBounds (editNameArea.reduced (0, 1));
    transportInfoLabel.setBounds (transportHud.reduced (0, 1));

    auto rowArea2 = beatmaker::layout::nextRow (sessionArea, metrics, row2);
    auto tempoArea = rowArea2.removeFromRight (juce::jmax (metrics.denseLayout ? 180 : 220,
                                                           rowArea2.getWidth() / (metrics.denseLayout ? 2 : 3)));
    tempoArea.removeFromLeft (juce::jmin (10, tempoArea.getWidth()));
    beatmaker::layout::layoutButtonRow (rowArea2, metrics,
                                        { &playPauseButton, &stopButton, &returnToStartButton, &transportLoopButton,
                                          &setLoopToSelectionButton, &zoomInButton, &zoomOutButton, &zoomResetButton });
    tempoLabel.setBounds (tempoArea.removeFromLeft (56).reduced (0, 1));
    tempoArea.removeFromLeft (4);
    tempoSlider.setBounds (tempoArea.reduced (0, 1));

    auto rowArea3 = beatmaker::layout::nextRow (sessionArea, metrics, row3);
    auto quickActionArea = rowArea3.removeFromRight (juce::jmax (metrics.denseLayout ? 260 : 420,
                                                                 (rowArea3.getWidth() * 3) / 5));
    quickActionArea.removeFromLeft (juce::jmin (8, quickActionArea.getWidth()));
    workflowStateLabel.setBounds (rowArea3.reduced (0, 1));
    beatmaker::layout::layoutButtonRow (quickActionArea, metrics,
                                        { &beatmakerSpaceButton, &startBeatQuickButton,
                                          &focusSelectionButton, &centerPlayheadButton, &fitProjectButton });
}
