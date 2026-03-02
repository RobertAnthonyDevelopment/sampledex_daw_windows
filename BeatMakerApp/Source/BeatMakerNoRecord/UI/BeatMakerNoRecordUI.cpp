#include "../BeatMakerNoRecord.h"
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace
{
bool isBlackMidiKey (int noteNumber)
{
    switch (noteNumber % 12)
    {
        case 1:
        case 3:
        case 6:
        case 8:
        case 10: return true;
        default: return false;
    }
}

constexpr int stepSequencerStepsPerBar = 16;
const juce::Identifier uiLaneHeightPropertyId ("sampledexLaneHeight");
constexpr int minTrackLaneHeightPx = 28;
constexpr int maxTrackLaneHeightPx = 320;
constexpr int pianoRollMinNote = 24;
constexpr int pianoRollMaxNote = 96;
constexpr int pianoRollDefaultVisibleNotes = 36;
constexpr int pianoRollMinVisibleNotes = 8;
constexpr int pianoRollDefaultLowestNote = 36;

struct StepSequencerLane
{
    int noteNumber = 36;
    const char* label = "";
};

const std::array<StepSequencerLane, 8>& getStepSequencerLanes()
{
    static const std::array<StepSequencerLane, 8> lanes
    {{
        { 36, "Kick" },
        { 38, "Snare" },
        { 42, "Closed HH" },
        { 46, "Open HH" },
        { 41, "Low Tom" },
        { 43, "High Tom" },
        { 49, "Crash" },
        { 51, "Ride" }
    }};

    return lanes;
}

struct StepSequencerGeometry
{
    juce::Rectangle<int> frame;
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> laneArea;
    juce::Rectangle<int> gridArea;
    juce::Rectangle<int> footerArea;
};

StepSequencerGeometry getStepSequencerGeometry (juce::Rectangle<int> localArea)
{
    StepSequencerGeometry geometry;
    geometry.frame = localArea.reduced (2, 2);

    auto inner = geometry.frame.reduced (4, 4);
    const int footerHeight = juce::jlimit (11, 18, inner.getHeight() / 8);
    geometry.footerArea = inner.removeFromBottom (footerHeight);
    if (inner.getHeight() > 4)
        inner.removeFromBottom (4);

    const int headerHeight = juce::jlimit (20, 30, inner.getHeight() / 5);
    geometry.headerArea = inner.removeFromTop (headerHeight);
    if (inner.getHeight() > 4)
        inner.removeFromTop (4);

    const int laneWidth = juce::jlimit (70, 120, inner.getWidth() / 4);
    geometry.laneArea = inner.removeFromLeft (laneWidth);
    if (inner.getWidth() > 4)
        inner.removeFromLeft (4);
    geometry.gridArea = inner;
    return geometry;
}

struct PianoRollGeometry
{
    juce::Rectangle<int> frame;
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> rulerArea;
    juce::Rectangle<int> keyboardArea;
    juce::Rectangle<int> gridArea;
    juce::Rectangle<int> velocityArea;
    juce::Rectangle<int> footerArea;
};

struct PianoRollHeaderTabs
{
    juce::Rectangle<int> pianoTab;
    juce::Rectangle<int> stepTab;
    juce::Rectangle<int> infoArea;
};

PianoRollGeometry getPianoRollGeometry (juce::Rectangle<int> localArea)
{
    PianoRollGeometry geometry;
    geometry.frame = localArea.reduced (2, 2);

    auto inner = geometry.frame.reduced (4, 4);
    const int headerHeight = juce::jlimit (18, 30, inner.getHeight() / 7);
    geometry.headerArea = inner.removeFromTop (headerHeight);
    if (inner.getHeight() > 4)
        inner.removeFromTop (4);

    const int rulerHeight = juce::jlimit (14, 22, inner.getHeight() / 10);
    geometry.rulerArea = inner.removeFromTop (rulerHeight);
    if (inner.getHeight() > 4)
        inner.removeFromTop (4);

    const int footerHeight = juce::jlimit (11, 20, inner.getHeight() / 10);
    geometry.footerArea = inner.removeFromBottom (footerHeight);
    if (inner.getHeight() > 4)
        inner.removeFromBottom (4);

    const int velocityHeight = juce::jlimit (46, 96, inner.getHeight() / 5);
    geometry.velocityArea = inner.removeFromBottom (velocityHeight);
    if (inner.getHeight() > 4)
        inner.removeFromBottom (4);

    const int keyboardWidth = juce::jlimit (62, 106, inner.getWidth() / 6);
    geometry.keyboardArea = inner.removeFromLeft (keyboardWidth);
    if (inner.getWidth() > 4)
        inner.removeFromLeft (4);

    geometry.gridArea = inner;
    return geometry;
}

PianoRollHeaderTabs getPianoRollHeaderTabs (const PianoRollGeometry& geometry)
{
    PianoRollHeaderTabs tabs;
    auto header = geometry.headerArea;
    auto tabsArea = header.removeFromLeft (juce::jlimit (160, 320, geometry.frame.getWidth() / 2));
    tabs.pianoTab = tabsArea.removeFromLeft (juce::jmax (80, tabsArea.getWidth() / 2)).reduced (1, 1);
    tabs.stepTab = tabsArea.reduced (1, 1);
    tabs.infoArea = header;
    return tabs;
}

int getStepSequencerLaneIndexForY (int y, juce::Rectangle<int> gridArea)
{
    const auto laneCount = (int) getStepSequencerLanes().size();
    if (laneCount <= 0 || y < gridArea.getY() || y >= gridArea.getBottom())
        return -1;

    // Match lane hit-testing to paint geometry (fixed lane height + clamped final lane).
    const int laneHeight = juce::jmax (1, gridArea.getHeight() / laneCount);
    const int lane = (y - gridArea.getY()) / laneHeight;
    return juce::jlimit (0, laneCount - 1, lane);
}

int getStepSequencerStepIndexForX (int x, juce::Rectangle<int> gridArea)
{
    if (x < gridArea.getX() || x >= gridArea.getRight())
        return -1;

    const int step = (x - gridArea.getX()) * stepSequencerStepsPerBar / juce::jmax (1, gridArea.getWidth());
    return juce::jlimit (0, stepSequencerStepsPerBar - 1, step);
}

bool isSupportedDroppedMidiExtension (juce::String extension)
{
    extension = extension.toLowerCase();
    return extension == ".mid" || extension == ".midi";
}

bool isSupportedDroppedAudioExtension (juce::String extension)
{
    extension = extension.toLowerCase();
    return extension == ".wav" || extension == ".aif" || extension == ".aiff"
        || extension == ".flac" || extension == ".mp3" || extension == ".ogg"
        || extension == ".m4a" || extension == ".caf" || extension == ".w64"
        || extension == ".bwf";
}

bool isSupportedDroppedFileExtension (juce::String extension)
{
    return isSupportedDroppedMidiExtension (extension) || isSupportedDroppedAudioExtension (extension);
}

te::MidiNote* findStepSequencerNoteByPitchAndBeat (te::MidiClip& midiClip,
                                                   int noteNumber,
                                                   double targetBeat,
                                                   double tolerance)
{
    for (auto* note : midiClip.getSequence().getNotes())
        if (note != nullptr
            && note->getNoteNumber() == noteNumber
            && std::abs (note->getStartBeat().inBeats() - targetBeat) <= tolerance)
            return note;

    return nullptr;
}

bool insertStepSequencerNoteAtCell (te::MidiClip& midiClip,
                                    int noteNumber,
                                    int stepIndex,
                                    double pageStartBeat,
                                    double stepLengthBeats,
                                    double clipLengthBeats,
                                    int velocity,
                                    juce::UndoManager& undoManager)
{
    if (stepIndex < 0 || stepIndex >= stepSequencerStepsPerBar)
        return false;

    constexpr double minimumLengthBeats = 1.0 / 128.0;
    const double unclampedStartBeat = pageStartBeat + (double) stepIndex * stepLengthBeats;
    const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - minimumLengthBeats);
    const double startBeat = juce::jlimit (0.0, maxStartBeat, unclampedStartBeat);
    const double maxLengthBeats = juce::jmax (minimumLengthBeats, clipLengthBeats - startBeat);
    constexpr double stepGate = 0.92;
    const double gatedLengthBeats = juce::jmax (minimumLengthBeats, stepLengthBeats * stepGate);
    const double lengthBeats = juce::jlimit (minimumLengthBeats, maxLengthBeats, gatedLengthBeats);

    return midiClip.getSequence().addNote (noteNumber,
                                           te::BeatPosition::fromBeats (startBeat),
                                           te::BeatDuration::fromBeats (lengthBeats),
                                           juce::jlimit (1, 127, velocity),
                                           0,
                                           &undoManager) != nullptr;
}

bool clearStepSequencerPageLaneNotes (te::MidiClip& midiClip,
                                      const std::array<StepSequencerLane, 8>& lanes,
                                      double pageStartBeat,
                                      te::BeatDuration stepLength,
                                      juce::UndoManager& undoManager)
{
    auto& sequence = midiClip.getSequence();
    const double stepLengthBeats = juce::jmax (1.0 / 64.0, stepLength.inBeats());
    const double tolerance = juce::jmax (1.0 / 128.0, stepLengthBeats * 0.42);
    bool changed = false;

    for (int lane = 0; lane < (int) lanes.size(); ++lane)
    {
        const int noteNumber = lanes[(size_t) lane].noteNumber;
        for (int step = 0; step < stepSequencerStepsPerBar; ++step)
        {
            const double targetBeat = pageStartBeat + (double) step * stepLengthBeats;
            if (auto* note = findStepSequencerNoteByPitchAndBeat (midiClip, noteNumber, targetBeat, tolerance))
            {
                sequence.removeNote (*note, &undoManager);
                changed = true;
            }
        }
    }

    return changed;
}

enum TrackAreaToolbarItemIds
{
    trackAreaToolbarToolSelect = 6201,
    trackAreaToolbarToolPencil,
    trackAreaToolbarToolScissors,
    trackAreaToolbarToolResize,
    trackAreaToolbarSplit,
    trackAreaToolbarDuplicate,
    trackAreaToolbarCopy,
    trackAreaToolbarCut,
    trackAreaToolbarPaste,
    trackAreaToolbarDelete,
    trackAreaToolbarZoomIn,
    trackAreaToolbarZoomOut,
    trackAreaToolbarZoomReset,
    trackAreaToolbarFocusSelection,
    trackAreaToolbarCenterPlayhead,
    trackAreaToolbarFitProject,
    trackAreaToolbarSetLoopSelection,
    trackAreaToolbarAddMarker,
    trackAreaToolbarPrevMarker,
    trackAreaToolbarNextMarker,
    trackAreaToolbarAddSection,
    trackAreaToolbarPrevSection,
    trackAreaToolbarNextSection,
    trackAreaToolbarAddTrack,
    trackAreaToolbarAddMidiTrack
};

enum MixerToolbarItemIds
{
    mixerToolbarMute = 6251,
    mixerToolbarSolo,
    mixerToolbarTrackAdd,
    mixerToolbarTrackAddMidi,
    mixerToolbarFxRefresh,
    mixerToolbarFxOpen,
    mixerToolbarFxScan,
    mixerToolbarFxPrep,
    mixerToolbarFxAddSampler,
    mixerToolbarFxAddSynth,
    mixerToolbarFxAddEq,
    mixerToolbarFxAddComp,
    mixerToolbarFxAddReverb,
    mixerToolbarFxAddDelay,
    mixerToolbarFxAddInstrument,
    mixerToolbarFxAddExternal,
    mixerToolbarFxMoveUp,
    mixerToolbarFxMoveDown,
    mixerToolbarFxBypass,
    mixerToolbarFxDelete
};

enum CommandToolbarItemIds
{
    commandToolbarNewProject = 6281,
    commandToolbarOpenProject,
    commandToolbarSaveProject,
    commandToolbarUndo,
    commandToolbarRedo,
    commandToolbarPlayPause,
    commandToolbarStop,
    commandToolbarReturnStart,
    commandToolbarLoopToggle,
    commandToolbarAddTrack,
    commandToolbarAddMidiTrack,
    commandToolbarImportAudio,
    commandToolbarImportMidi,
    commandToolbarCreateMidiClip,
    commandToolbarToolSelect,
    commandToolbarToolPencil,
    commandToolbarToolScissors,
    commandToolbarToolResize,
    commandToolbarSplitClip,
    commandToolbarDuplicateClip,
    commandToolbarDeleteSelection,
    commandToolbarFocusSelection,
    commandToolbarCenterPlayhead,
    commandToolbarFitProject
};

enum PianoRollToolbarItemIds
{
    pianoRollToolbarToolSelect = 6101,
    pianoRollToolbarToolPencil,
    pianoRollToolbarToolScissors,
    pianoRollToolbarToolResize,
    pianoRollToolbarQuantize,
    pianoRollToolbarTransposeDown,
    pianoRollToolbarTransposeUp,
    pianoRollToolbarVelocityDown,
    pianoRollToolbarVelocityUp,
    pianoRollToolbarHumanizeTiming,
    pianoRollToolbarHumanizeVelocity,
    pianoRollToolbarLegato,
    pianoRollToolbarCopy,
    pianoRollToolbarCut,
    pianoRollToolbarPaste,
    pianoRollToolbarDelete,
    pianoRollToolbarGenerateChords,
    pianoRollToolbarGenerateArp,
    pianoRollToolbarGenerateBass,
    pianoRollToolbarGenerateDrums,
    pianoRollToolbarFocus,
    pianoRollToolbarResetView,
    pianoRollToolbarZoomTimeIn,
    pianoRollToolbarZoomTimeOut,
    pianoRollToolbarZoomPitchIn,
    pianoRollToolbarZoomPitchOut
};

enum StepSequencerToolbarItemIds
{
    stepSequencerToolbarCreateMidi = 6131,
    stepSequencerToolbarClearPage,
    stepSequencerToolbarPatternFourOnFloor,
    stepSequencerToolbarRandomizePage,
    stepSequencerToolbarShiftLeft,
    stepSequencerToolbarShiftRight,
    stepSequencerToolbarVaryVelocity,
    stepSequencerToolbarQuantize,
    stepSequencerToolbarGenerateDrums
};

enum AppCommandIds
{
    appCommandSaveProject = 22001,
    appCommandSaveProjectAs,
    appCommandUndo,
    appCommandRedo,
    appCommandPlayPause,
    appCommandStop,
    appCommandReturnToStart,
    appCommandToggleLoop,
    appCommandCreateMidiClip,
    appCommandQuantizeMidi,
    appCommandFocusSelection,
    appCommandCenterPlayhead,
    appCommandFitProject,
    appCommandToolSelect,
    appCommandToolPencil,
    appCommandToolScissors,
    appCommandToolResize,
    appCommandSplitSelection,
    appCommandTransposeDown,
    appCommandTransposeUp,
    appCommandVelocityDown,
    appCommandVelocityUp,
    appCommandBounceMidiToAudio,
    appCommandToggleFloatWorkspace,
    appCommandToggleFloatMixer,
    appCommandToggleFloatPiano,
    appCommandDockAllPanels,
    appCommandStepRandomize,
    appCommandStepFourOnFloor,
    appCommandStepClear,
    appCommandStepShiftLeft,
    appCommandStepShiftRight,
    appCommandStepVaryVelocity
};

struct PianoRollToolbarItemDefinition
{
    int itemId = 0;
    juce::String label;
    juce::String tooltip;
    enum class IconGlyph
    {
        none,
        select,
        pencil,
        scissors,
        resize,
        quantize,
        transposeDown,
        transposeUp,
        velocityDown,
        velocityUp,
        humanizeTiming,
        humanizeVelocity,
        legato,
        copy,
        cut,
        paste,
        remove,
        generateChords,
        generateArp,
        generateBass,
        generateDrums,
        focus,
        reset,
        zoomTimeIn,
        zoomTimeOut,
        zoomPitchIn,
        zoomPitchOut
    };
    bool isToggleButton = false;
    bool iconOnly = false;
    IconGlyph iconGlyph = IconGlyph::none;
    int preferredWidth = 60;
    int minWidth = 44;
    int maxWidth = 110;
    std::function<void()> onClick;
    std::function<bool()> isEnabled;
    std::function<bool()> isToggled;
};

class ToolbarIconButton final : public juce::Button
{
public:
    explicit ToolbarIconButton (const PianoRollToolbarItemDefinition& definitionToUse)
        : juce::Button (definitionToUse.label), definition (definitionToUse)
    {
        setClickingTogglesState (definition.isToggleButton);
    }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.75f);
        const bool toggled = definition.isToggleButton && getToggleState();

        auto fill = toggled ? juce::Colour::fromRGB (52, 141, 210)
                            : juce::Colour::fromRGB (24, 36, 51);

        if (isMouseOverButton)
            fill = fill.brighter (0.10f);
        if (isButtonDown)
            fill = fill.brighter (0.18f);
        if (! isEnabled())
            fill = fill.withAlpha (0.35f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour ((toggled ? juce::Colours::white : juce::Colour::fromRGB (103, 142, 184)).withAlpha (isEnabled() ? 0.92f : 0.45f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        auto iconBounds = bounds.reduced (7.0f, 6.0f);
        const auto iconColour = (toggled ? juce::Colour::fromRGB (9, 20, 30)
                                         : juce::Colours::white).withAlpha (isEnabled() ? 0.95f : 0.48f);
        drawIconGlyph (g, definition.iconGlyph, iconBounds, iconColour);
    }

private:
    static void drawPlus (juce::Graphics& g, juce::Point<float> center, float size)
    {
        g.drawLine ({ center.x - size, center.y, center.x + size, center.y }, 1.6f);
        g.drawLine ({ center.x, center.y - size, center.x, center.y + size }, 1.6f);
    }

    static void drawArrow (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float thickness)
    {
        g.drawLine ({ from.x, from.y, to.x, to.y }, thickness);
        auto direction = to - from;
        if (direction.getDistanceFromOrigin() < 0.01f)
            return;

        direction = direction / direction.getDistanceFromOrigin();
        auto normal = juce::Point<float> (-direction.y, direction.x);
        const float head = 4.2f;
        const auto base = to - direction * head;
        g.drawLine ({ to.x, to.y, base.x + normal.x * 2.6f, base.y + normal.y * 2.6f }, thickness);
        g.drawLine ({ to.x, to.y, base.x - normal.x * 2.6f, base.y - normal.y * 2.6f }, thickness);
    }

    static void drawNoteHead (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.fillEllipse (bounds);
        g.drawLine (bounds.getRight() - 0.8f, bounds.getCentreY(), bounds.getRight() - 0.8f, bounds.getY() - 5.0f, 1.6f);
    }

    static void drawIconGlyph (juce::Graphics& g,
                               PianoRollToolbarItemDefinition::IconGlyph icon,
                               juce::Rectangle<float> b,
                               juce::Colour colour)
    {
        g.setColour (colour);

        switch (icon)
        {
            case PianoRollToolbarItemDefinition::IconGlyph::select:
            {
                juce::Path p;
                p.startNewSubPath (b.getX() + 2.0f, b.getY() + 1.5f);
                p.lineTo (b.getCentreX() + 0.6f, b.getCentreY() + 1.0f);
                p.lineTo (b.getX() + 5.0f, b.getBottom() - 1.5f);
                p.lineTo (b.getX() + 2.0f, b.getBottom() - 1.5f);
                p.closeSubPath();
                g.fillPath (p);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::pencil:
            {
                g.drawLine (b.getX() + 2.0f, b.getBottom() - 2.0f, b.getRight() - 2.0f, b.getY() + 2.0f, 2.0f);
                g.fillEllipse (juce::Rectangle<float> (b.getRight() - 4.0f, b.getY() + 0.6f, 3.4f, 3.4f));
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::scissors:
            case PianoRollToolbarItemDefinition::IconGlyph::cut:
            {
                g.drawLine (b.getX() + 2.0f, b.getY() + 2.5f, b.getRight() - 2.0f, b.getBottom() - 2.5f, 1.6f);
                g.drawLine (b.getX() + 2.0f, b.getBottom() - 2.5f, b.getRight() - 2.0f, b.getY() + 2.5f, 1.6f);
                g.drawEllipse (juce::Rectangle<float> (b.getX() + 1.0f, b.getY() + 4.0f, 3.4f, 3.4f), 1.3f);
                g.drawEllipse (juce::Rectangle<float> (b.getX() + 1.0f, b.getBottom() - 7.4f, 3.4f, 3.4f), 1.3f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::resize:
            {
                drawArrow (g, { b.getX() + 1.6f, b.getCentreY() }, { b.getRight() - 1.6f, b.getCentreY() }, 1.5f);
                drawArrow (g, { b.getRight() - 1.6f, b.getCentreY() }, { b.getX() + 1.6f, b.getCentreY() }, 1.5f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::quantize:
            {
                const float cellW = b.getWidth() / 3.0f;
                const float cellH = b.getHeight() / 3.0f;
                for (int x = 0; x < 4; ++x)
                    g.drawLine (b.getX() + cellW * (float) x, b.getY(), b.getX() + cellW * (float) x, b.getBottom(), 1.0f);
                for (int y = 0; y < 4; ++y)
                    g.drawLine (b.getX(), b.getY() + cellH * (float) y, b.getRight(), b.getY() + cellH * (float) y, 1.0f);
                g.drawLine (b.getX() + 1.6f, b.getBottom() - 2.2f, b.getRight() - 1.6f, b.getY() + 2.2f, 1.5f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::transposeDown:
            case PianoRollToolbarItemDefinition::IconGlyph::transposeUp:
            {
                drawNoteHead (g, { b.getX() + 1.0f, b.getY() + 4.5f, 4.8f, 3.8f });
                const bool up = icon == PianoRollToolbarItemDefinition::IconGlyph::transposeUp;
                drawArrow (g,
                           { b.getRight() - 3.0f, up ? b.getBottom() - 2.0f : b.getY() + 2.0f },
                           { b.getRight() - 3.0f, up ? b.getY() + 2.0f : b.getBottom() - 2.0f },
                           1.5f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::velocityDown:
            case PianoRollToolbarItemDefinition::IconGlyph::velocityUp:
            {
                const bool up = icon == PianoRollToolbarItemDefinition::IconGlyph::velocityUp;
                const float barW = b.getWidth() / 6.0f;
                g.fillRect (b.getX() + barW * 0.7f, b.getBottom() - 4.0f, barW, 3.0f);
                g.fillRect (b.getX() + barW * 2.2f, b.getBottom() - 6.5f, barW, 5.5f);
                g.fillRect (b.getX() + barW * 3.7f, b.getBottom() - 9.0f, barW, 8.0f);
                drawArrow (g,
                           { b.getRight() - 2.0f, up ? b.getBottom() - 1.8f : b.getY() + 1.8f },
                           { b.getRight() - 2.0f, up ? b.getY() + 1.8f : b.getBottom() - 1.8f },
                           1.4f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::humanizeTiming:
            {
                juce::Path p;
                p.startNewSubPath (b.getX() + 1.5f, b.getCentreY());
                p.cubicTo (b.getX() + b.getWidth() * 0.28f, b.getY() + 1.0f,
                           b.getX() + b.getWidth() * 0.42f, b.getBottom() - 1.0f,
                           b.getX() + b.getWidth() * 0.58f, b.getCentreY());
                p.cubicTo (b.getX() + b.getWidth() * 0.72f, b.getY() + 1.0f,
                           b.getRight() - 1.5f, b.getBottom() - 1.0f,
                           b.getRight() - 1.5f, b.getCentreY());
                g.strokePath (p, juce::PathStrokeType (1.6f));
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::humanizeVelocity:
            {
                const float barW = b.getWidth() / 7.0f;
                g.fillRect (b.getX() + barW * 0.5f, b.getBottom() - 3.0f, barW, 2.0f);
                g.fillRect (b.getX() + barW * 2.0f, b.getBottom() - 7.0f, barW, 6.0f);
                g.fillRect (b.getX() + barW * 3.5f, b.getBottom() - 4.5f, barW, 3.5f);
                g.fillRect (b.getX() + barW * 5.0f, b.getBottom() - 9.5f, barW, 8.5f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::legato:
            {
                g.drawRoundedRectangle (juce::Rectangle<float> (b.getX() + 1.0f, b.getCentreY() - 2.4f, b.getWidth() * 0.40f, 4.8f), 1.6f, 1.3f);
                g.drawRoundedRectangle (juce::Rectangle<float> (b.getCentreX() - 1.2f, b.getCentreY() - 2.4f, b.getWidth() * 0.40f, 4.8f), 1.6f, 1.3f);
                g.drawLine (b.getCentreX() - 2.0f, b.getCentreY(), b.getCentreX() + 2.4f, b.getCentreY(), 1.3f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::copy:
            {
                g.drawRect (juce::Rectangle<int> (roundToInt (b.getX() + 2.0f), roundToInt (b.getY() + 3.0f),
                                                  roundToInt (b.getWidth() - 6.0f), roundToInt (b.getHeight() - 6.0f)), 1);
                g.drawRect (juce::Rectangle<int> (roundToInt (b.getX() + 4.5f), roundToInt (b.getY() + 1.0f),
                                                  roundToInt (b.getWidth() - 6.0f), roundToInt (b.getHeight() - 6.0f)), 1);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::paste:
            {
                g.drawRoundedRectangle (juce::Rectangle<float> (b.getX() + 2.0f, b.getY() + 3.0f, b.getWidth() - 4.0f, b.getHeight() - 4.5f), 2.2f, 1.4f);
                g.fillRoundedRectangle (juce::Rectangle<float> (b.getCentreX() - 3.2f, b.getY() + 1.0f, 6.4f, 2.6f), 1.2f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::remove:
            {
                g.drawRoundedRectangle (juce::Rectangle<float> (b.getX() + 2.6f, b.getY() + 3.0f, b.getWidth() - 5.2f, b.getHeight() - 5.5f), 1.8f, 1.4f);
                g.drawLine (b.getX() + 5.0f, b.getY() + 3.0f, b.getRight() - 5.0f, b.getY() + 3.0f, 1.4f);
                g.drawLine (b.getX() + 5.5f, b.getY() + 1.6f, b.getRight() - 5.5f, b.getBottom() - 1.6f, 1.5f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::generateChords:
            {
                g.fillEllipse (juce::Rectangle<float> (b.getX() + 1.0f, b.getBottom() - 4.0f, 3.2f, 3.2f));
                g.fillEllipse (juce::Rectangle<float> (b.getCentreX() - 1.6f, b.getY() + 1.5f, 3.2f, 3.2f));
                g.fillEllipse (juce::Rectangle<float> (b.getRight() - 4.2f, b.getBottom() - 4.0f, 3.2f, 3.2f));
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::generateArp:
            {
                g.drawLine (b.getX() + 1.8f, b.getBottom() - 2.0f, b.getX() + 1.8f, b.getY() + 4.0f, 1.5f);
                g.drawLine (b.getX() + 4.8f, b.getBottom() - 2.0f, b.getX() + 4.8f, b.getY() + 1.8f, 1.5f);
                g.drawLine (b.getX() + 7.8f, b.getBottom() - 2.0f, b.getX() + 7.8f, b.getY() + 6.2f, 1.5f);
                drawArrow (g, { b.getX() + 1.2f, b.getBottom() - 1.2f }, { b.getRight() - 1.2f, b.getY() + 1.2f }, 1.2f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::generateBass:
            {
                juce::Path p;
                p.startNewSubPath (b.getX() + 1.4f, b.getCentreY());
                p.cubicTo (b.getX() + b.getWidth() * 0.22f, b.getBottom() - 2.0f,
                           b.getX() + b.getWidth() * 0.35f, b.getY() + 2.0f,
                           b.getX() + b.getWidth() * 0.52f, b.getCentreY());
                p.cubicTo (b.getX() + b.getWidth() * 0.67f, b.getBottom() - 2.0f,
                           b.getX() + b.getWidth() * 0.84f, b.getY() + 2.0f,
                           b.getRight() - 1.4f, b.getCentreY());
                g.strokePath (p, juce::PathStrokeType (1.6f));
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::generateDrums:
            {
                g.drawEllipse (juce::Rectangle<float> (b.getX() + 1.5f, b.getY() + 2.0f, b.getWidth() - 3.0f, b.getHeight() - 4.5f), 1.6f);
                g.drawLine (b.getX() + 2.2f, b.getY() + 3.8f, b.getRight() - 2.2f, b.getY() + 3.8f, 1.4f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::focus:
            {
                g.drawEllipse (juce::Rectangle<float> (b.getCentreX() - 4.2f, b.getCentreY() - 4.2f, 8.4f, 8.4f), 1.4f);
                g.drawLine (b.getCentreX(), b.getY() + 1.0f, b.getCentreX(), b.getY() + 3.4f, 1.4f);
                g.drawLine (b.getCentreX(), b.getBottom() - 1.0f, b.getCentreX(), b.getBottom() - 3.4f, 1.4f);
                g.drawLine (b.getX() + 1.0f, b.getCentreY(), b.getX() + 3.4f, b.getCentreY(), 1.4f);
                g.drawLine (b.getRight() - 1.0f, b.getCentreY(), b.getRight() - 3.4f, b.getCentreY(), 1.4f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::reset:
            {
                juce::Path p;
                p.addCentredArc (b.getCentreX(), b.getCentreY(), b.getWidth() * 0.34f, b.getHeight() * 0.34f, 0.0f,
                                 juce::MathConstants<float>::pi * 0.14f,
                                 juce::MathConstants<float>::pi * 1.72f,
                                 true);
                g.strokePath (p, juce::PathStrokeType (1.5f));
                drawArrow (g, { b.getX() + 2.4f, b.getY() + 3.2f }, { b.getX() + 1.4f, b.getY() + 1.8f }, 1.4f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::zoomTimeIn:
            case PianoRollToolbarItemDefinition::IconGlyph::zoomTimeOut:
            {
                g.drawEllipse (juce::Rectangle<float> (b.getX() + 1.4f, b.getY() + 1.4f, b.getWidth() * 0.56f, b.getHeight() * 0.56f), 1.5f);
                g.drawLine (b.getCentreX() + 1.6f, b.getCentreY() + 1.6f, b.getRight() - 1.2f, b.getBottom() - 1.2f, 1.5f);
                const auto center = juce::Point<float> (b.getX() + b.getWidth() * 0.29f, b.getY() + b.getHeight() * 0.29f);
                if (icon == PianoRollToolbarItemDefinition::IconGlyph::zoomTimeIn)
                    drawPlus (g, center, 2.0f);
                else
                    g.drawLine ({ center.x - 2.0f, center.y, center.x + 2.0f, center.y }, 1.6f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::zoomPitchIn:
            case PianoRollToolbarItemDefinition::IconGlyph::zoomPitchOut:
            {
                drawArrow (g, { b.getCentreX(), b.getBottom() - 1.6f }, { b.getCentreX(), b.getY() + 1.6f }, 1.4f);
                drawArrow (g, { b.getCentreX(), b.getY() + 1.6f }, { b.getCentreX(), b.getBottom() - 1.6f }, 1.4f);
                if (icon == PianoRollToolbarItemDefinition::IconGlyph::zoomPitchIn)
                    drawPlus (g, { b.getRight() - 3.2f, b.getY() + 3.2f }, 1.5f);
                else
                    g.drawLine ({ b.getRight() - 5.0f, b.getY() + 3.2f, b.getRight() - 1.4f, b.getY() + 3.2f }, 1.6f);
                break;
            }
            case PianoRollToolbarItemDefinition::IconGlyph::none:
            default:
            {
                g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                g.drawFittedText ("?", b.toNearestInt(), juce::Justification::centred, 1);
                break;
            }
        }
    }

    PianoRollToolbarItemDefinition definition;
};

class PianoRollToolbarTextItem final : public juce::ToolbarItemComponent
{
public:
    explicit PianoRollToolbarTextItem (const PianoRollToolbarItemDefinition& definitionToUse)
        : juce::ToolbarItemComponent (definitionToUse.itemId, definitionToUse.label, false),
          definition (definitionToUse)
    {
        if (definition.iconOnly)
            button = std::make_unique<ToolbarIconButton> (definition);
        else
            button = std::make_unique<juce::TextButton> (definition.label);

        addAndMakeVisible (*button);
        button->setWantsKeyboardFocus (false);
        button->setClickingTogglesState (definition.isToggleButton);
        button->onClick = [this]
        {
            if ((bool) definition.onClick)
                definition.onClick();
        };

        if (definition.tooltip.isNotEmpty())
        {
            setTooltip (definition.tooltip);
            button->setTooltip (definition.tooltip);
        }

        refreshStateFromOwner();
    }

    void refreshStateFromOwner()
    {
        const bool enabled = (bool) definition.isEnabled ? definition.isEnabled() : true;
        button->setEnabled (enabled);

        const bool toggled = definition.isToggleButton && (bool) definition.isToggled && definition.isToggled();
        button->setToggleState (toggled, juce::dontSendNotification);
    }

    bool getToolbarItemSizes (int,
                              bool,
                              int& preferredSize,
                              int& minSize,
                              int& maxSize) override
    {
        preferredSize = definition.preferredWidth;
        minSize = definition.minWidth;
        maxSize = definition.maxWidth;
        return true;
    }

    void paintButtonArea (juce::Graphics&, int, int, bool, bool) override {}

    void contentAreaChanged (const juce::Rectangle<int>& newBounds) override
    {
        button->setBounds (newBounds.reduced (1, 1));
    }

private:
    PianoRollToolbarItemDefinition definition;
    std::unique_ptr<juce::Button> button;
};

class PianoRollToolbarFactory final : public juce::ToolbarItemFactory
{
public:
    void addToolbarItem (PianoRollToolbarItemDefinition definition)
    {
        definitions.push_back (std::move (definition));
    }

    void setDefaultItems (std::vector<int> itemIds)
    {
        defaultItems = std::move (itemIds);
    }

    void getAllToolbarItemIds (juce::Array<int>& ids) override
    {
        ids.clearQuick();

        for (const auto& definition : definitions)
            ids.add (definition.itemId);

        ids.add (juce::ToolbarItemFactory::separatorBarId);
        ids.add (juce::ToolbarItemFactory::spacerId);
        ids.add (juce::ToolbarItemFactory::flexibleSpacerId);
    }

    void getDefaultItemSet (juce::Array<int>& ids) override
    {
        ids.clearQuick();

        for (const int itemId : defaultItems)
            ids.add (itemId);
    }

    juce::ToolbarItemComponent* createItem (int itemId) override
    {
        for (const auto& definition : definitions)
            if (definition.itemId == itemId)
                return new PianoRollToolbarTextItem (definition);

        return nullptr;
    }

private:
    std::vector<PianoRollToolbarItemDefinition> definitions;
    std::vector<int> defaultItems;
};

enum MenuCommandIds
{
    menuFileNew = 1001,
    menuFileOpen,
    menuFileSave,
    menuFileSaveAs,

    menuEditUndo,
    menuEditRedo,
    menuEditCopy,
    menuEditCut,
    menuEditPaste,
    menuEditDuplicate,
    menuEditSplit,
    menuEditDelete,
    menuEditSelectAll,
    menuEditDeselectAll,
    menuEditToolSelect,
    menuEditToolPencil,
    menuEditToolScissors,
    menuEditToolResize,
    menuEditStepRandomize,
    menuEditStepFourOnFloor,
    menuEditStepClearPage,
    menuEditStepShiftLeft,
    menuEditStepShiftRight,
    menuEditStepVaryVelocity,
    menuEditAlignClipToBar,
    menuEditLoopClip2Bars,
    menuEditLoopClip4Bars,
    menuEditFillTransportLoop,

    menuTransportPlayPause,
    menuTransportStop,
    menuTransportReturnStart,
    menuTransportLoopToggle,
    menuTransportLoopSelection,
    menuTransportPrevBar,
    menuTransportNextBar,
    menuTransportAudioQuality,

    menuTrackAdd,
    menuTrackAddMidi,
    menuTrackAddFloatingSynth,
    menuTrackDuplicate,
    menuTrackRename,
    menuTrackImportAudio,
    menuTrackImportMidi,
    menuTrackCreateMidi,
    menuTrackBounceMidi,
    menuTrackGenChords,
    menuTrackGenArp,
    menuTrackGenBass,
    menuTrackGenDrums,
    menuTrackPreviewDirectory,
    menuTrackApplyDirectory,
    menuTrackExportDirectoryMidi,
    menuTrackExportDirectoryWav,

    menuPluginsScan,
    menuPluginsScanSkipped,
    menuPluginsPrepPlayback,
    menuPluginsOpenUi,
    menuPluginsAddBundledNova,
    menuPluginsAddInstrument,
    menuPluginsAddFx,
    menuPluginsAddSampler,
    menuPluginsAdd4Osc,
    menuPluginsAddEq,
    menuPluginsAddComp,
    menuPluginsAddReverb,
    menuPluginsAddDelay,
    menuPluginsSynthInit,
    menuPluginsSynthWarmPad,
    menuPluginsSynthPunchBass,
    menuPluginsSynthBrightPluck,

    menuViewMarkers,
    menuViewArranger,
    menuViewZoomIn,
    menuViewZoomOut,
    menuViewZoomReset,
    menuViewFocusSelection,
    menuViewCenterPlayhead,
    menuViewFitProject,
    menuViewFloatWorkspace,
    menuViewFloatMixer,
    menuViewFloatPiano,
    menuViewDockAllPanels,
    menuViewPianoModeSplit,
    menuViewPianoModePiano,
    menuViewPianoModeSteps,
    menuViewLayoutMidiFocus,
    menuViewLayoutAudioFocus,
    menuViewLayoutHybridFocus,
    menuViewUiCompact,
    menuViewUiComfortable,
    menuViewUiAccessible,
    menuViewPanelsAll,
    menuViewPanelsProject,
    menuViewPanelsEditing,
    menuViewPanelsSound,

    menuHelpShortcuts
};

struct MixerStripLayout
{
    juce::Rectangle<int> strip;
    juce::Rectangle<int> header;
    juce::Rectangle<int> muteButton;
    juce::Rectangle<int> soloButton;
    juce::Rectangle<int> faderTrack;
    juce::Rectangle<int> panTrack;
};

struct MixerRoutingLayout
{
    juce::Rectangle<int> stripsArea;
    juce::Rectangle<int> busArea;
    int railX = 0;
    juce::Rectangle<int> meterArea;
};

MixerRoutingLayout getMixerRoutingLayout (juce::Rectangle<int> localArea)
{
    MixerRoutingLayout layout;
    auto inner = localArea.reduced (6, 5);

    const int busWidth = inner.getWidth() < 360
                             ? juce::jlimit (92, 128, inner.getWidth() / 3)
                             : juce::jlimit (120, 176, inner.getWidth() / 4);

    layout.busArea = inner.removeFromRight (busWidth);
    inner.removeFromRight (8);
    layout.stripsArea = inner;
    layout.busArea = layout.busArea.reduced (2, 2);

    layout.railX = layout.busArea.getX() + juce::jlimit (14, 24, layout.busArea.getWidth() / 6);
    layout.meterArea = layout.busArea.withTrimmedTop (34).withTrimmedBottom (26).removeFromRight (24).reduced (2, 0);
    return layout;
}

MixerStripLayout getMixerStripLayout (juce::Rectangle<int> area, int trackIndex, int trackCount)
{
    MixerStripLayout layout;
    if (trackCount <= 0)
        return layout;

    constexpr int stripGap = 8;
    const int availableWidth = area.getWidth() - juce::jmax (0, trackCount - 1) * stripGap;
    const int stripWidth = juce::jlimit (82, 148, availableWidth / juce::jmax (1, trackCount));
    const int totalWidth = stripWidth * trackCount + stripGap * juce::jmax (0, trackCount - 1);
    const int startX = area.getX() + juce::jmax (0, (area.getWidth() - totalWidth) / 2);

    layout.strip = { startX + trackIndex * (stripWidth + stripGap), area.getY() + 2, stripWidth, juce::jmax (26, area.getHeight() - 4) };

    auto inner = layout.strip.reduced (6, 6);
    layout.header = inner.removeFromTop (20);

    auto buttons = inner.removeFromTop (20);
    layout.muteButton = buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 2);
    layout.soloButton = buttons.reduced (1, 2);

    inner.removeFromTop (4);
    auto panZone = inner.removeFromBottom (30);
    inner.removeFromBottom (5);

    layout.faderTrack = { inner.getCentreX() - 4, inner.getY(), 8, juce::jmax (18, inner.getHeight()) };
    layout.panTrack = panZone.reduced (4, 11);

    return layout;
}

double getMixerVolumeNormalised (double volumeDb)
{
    return juce::jlimit (0.0, 1.0, juce::jmap (volumeDb, -60.0, 12.0, 0.0, 1.0));
}

double getMixerVolumeDbFromY (int y, juce::Rectangle<int> faderTrack)
{
    const double ratioFromTop = juce::jlimit (0.0, 1.0,
                                              (double) (y - faderTrack.getY())
                                              / (double) juce::jmax (1, faderTrack.getHeight()));
    return juce::jmap (1.0 - ratioFromTop, -60.0, 12.0);
}

double getMixerPanFromX (int x, juce::Rectangle<int> panTrack)
{
    const double ratio = juce::jlimit (0.0, 1.0,
                                       (double) (x - panTrack.getX())
                                       / (double) juce::jmax (1, panTrack.getWidth()));
    return juce::jmap (ratio, -1.0, 1.0);
}

double getVisibleTrackContentHeight (const te::Edit& edit, const EditViewState& viewState)
{
    const int globalTrackHeight = juce::jlimit (minTrackLaneHeightPx, maxTrackLaneHeightPx, roundToInt (viewState.trackHeight.get()));
    double contentHeight = 0.0;

    for (auto* track : te::getAllTracks (edit))
    {
        if (track == nullptr)
            continue;

        bool shouldInclude = true;

        if (track->isMasterTrack())
            shouldInclude = viewState.showMasterTrack.get();
        else if (track->isTempoTrack())
            shouldInclude = viewState.showGlobalTrack.get();
        else if (track->isMarkerTrack())
            shouldInclude = viewState.showMarkerTrack.get();
        else if (track->isChordTrack())
            shouldInclude = viewState.showChordTrack.get();
        else if (track->isArrangerTrack())
            shouldInclude = viewState.showArrangerTrack.get();

        if (! shouldInclude)
            continue;

        int laneHeight = (int) track->state.getProperty (uiLaneHeightPropertyId, globalTrackHeight);
        laneHeight = juce::jlimit (minTrackLaneHeightPx, maxTrackLaneHeightPx, laneHeight);
        contentHeight += (double) laneHeight + 2.0;
    }

    return contentHeight;
}

juce::Colour getRoutingWireColour (bool isSelected, bool isMuted, bool isSolo)
{
    if (isMuted)
        return juce::Colour::fromRGB (95, 108, 128).withAlpha (0.28f);

    if (isSelected)
        return juce::Colour::fromRGB (132, 212, 255).withAlpha (0.92f);

    if (isSolo)
        return juce::Colour::fromRGB (240, 194, 86).withAlpha (0.88f);

    return juce::Colour::fromRGB (92, 158, 232).withAlpha (0.62f);
}

bool isMixerUtilityPlugin (const te::Plugin& plugin)
{
    return dynamic_cast<const te::VolumeAndPanPlugin*> (&plugin) != nullptr
        || dynamic_cast<const te::LevelMeterPlugin*> (&plugin) != nullptr;
}

struct MixerTrackUiState
{
    float volumeDb = 0.0f;
    float pan = 0.0f;
    bool hasInstrument = false;
    int userFxCount = 0;
};

te::AudioTrack* getMidiClipOwnerTrack (te::MidiClip* midiClip)
{
    return midiClip != nullptr ? dynamic_cast<te::AudioTrack*> (midiClip->getTrack()) : nullptr;
}

te::MidiClip* getPreferredMidiClipOnTrack (te::AudioTrack& track, te::TimePosition playhead)
{
    te::MidiClip* fallback = nullptr;

    for (auto* clip : track.getClips())
    {
        auto* midiClip = dynamic_cast<te::MidiClip*> (clip);
        if (midiClip == nullptr)
            continue;

        if (fallback == nullptr)
            fallback = midiClip;

        if (midiClip->getEditTimeRange().contains (playhead))
            return midiClip;
    }

    return fallback;
}

te::Plugin* getFirstEnabledInstrumentPlugin (te::AudioTrack& track)
{
    for (auto* plugin : track.pluginList.getPlugins())
    {
        if (plugin != nullptr && plugin->isSynth() && plugin->isEnabled())
            return plugin;
    }

    return nullptr;
}

juce::String getInstrumentWorkflowLabel (te::Plugin* plugin)
{
    if (plugin == nullptr)
        return "None";

    juce::String label = plugin->getName().trim();
    if (label.isEmpty())
        label = plugin->getPluginType();

    if (auto* external = dynamic_cast<te::ExternalPlugin*> (plugin))
    {
        juce::String format;
        if (external->isVST3())
            format = "VST3";
        else if (external->isAU())
            format = "AU";
        else
            format = "External";

        if (format.isNotEmpty())
            label << " (" << format << ")";
    }
    else if (plugin->getPluginType().equalsIgnoreCase ("4osc"))
    {
        label << " (Built-in)";
    }

    return label;
}

MixerTrackUiState getMixerTrackUiState (te::AudioTrack& track)
{
    MixerTrackUiState state;

    if (auto* volumePlugin = track.getVolumePlugin())
    {
        state.volumeDb = volumePlugin->getVolumeDb();
        state.pan = volumePlugin->getPan();
    }

    for (auto* plugin : track.pluginList.getPlugins())
    {
        if (plugin == nullptr)
            continue;

        if (plugin->isSynth() && plugin->isEnabled())
            state.hasInstrument = true;

        if (! isMixerUtilityPlugin (*plugin))
            ++state.userFxCount;
    }

    return state;
}

void drawMixerModeButton (juce::Graphics& g,
                          juce::Rectangle<int> bounds,
                          const juce::String& text,
                          bool isOn,
                          juce::Colour onColour)
{
    const auto fill = isOn ? onColour.withAlpha (0.86f) : juce::Colour::fromRGB (39, 50, 67);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (isOn ? 0.98f : 0.80f));
    g.setFont (juce::Font (juce::FontOptions (10.2f, juce::Font::bold)));
    g.drawText (text, bounds, juce::Justification::centred, false);
}
}

BeatMakerNoRecord::SectionContainer::SectionContainer (BeatMakerNoRecord& ownerToUse, FloatSection sectionToUse)
    : owner (ownerToUse), section (sectionToUse)
{
    setOpaque (false);
}

void BeatMakerNoRecord::SectionContainer::resized()
{
    owner.layoutSectionContent (section, getLocalBounds());
}

void BeatMakerNoRecord::SectionContainer::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (section != FloatSection::workspace || owner.editComponent == nullptr)
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    const auto mods = e.mods;
    const double primaryDelta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;

    if (mods.isCommandDown() || mods.isCtrlDown())
    {
        const double strength = juce::jlimit (0.2, 2.0, std::abs (primaryDelta) * 2.5);
        const double factor = primaryDelta >= 0.0 ? std::pow (0.86, strength) : std::pow (1.16, strength);
        auto anchor = owner.edit != nullptr ? owner.edit->getTransport().getPosition() : te::TimePosition::fromSeconds (0.0);
        owner.zoomTimelineAroundTime (factor, anchor);
        return;
    }

    if (mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        auto& viewState = owner.editComponent->getEditViewState();
        const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
        owner.moveTimelineViewportBySeconds (-primaryDelta * visibleSeconds * 0.28);
        return;
    }

    const double next = juce::jlimit (0.0, 1.0, owner.verticalScrollSlider.getValue() - wheel.deltaY * 0.26);
    owner.verticalScrollSlider.setValue (next);
}

BeatMakerNoRecord::FloatingSectionWindow::FloatingSectionWindow (const juce::String& windowTitle,
                                                                 std::function<void()> closeHandler)
    : juce::DocumentWindow (windowTitle,
                            juce::Colour::fromRGB (11, 16, 24),
                            juce::DocumentWindow::closeButton
                              | juce::DocumentWindow::minimiseButton
                              | juce::DocumentWindow::maximiseButton),
      onClosePressed (std::move (closeHandler))
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (520, 320, 4096, 4096);
    setBackgroundColour (juce::Colour::fromRGB (11, 16, 24));
}

void BeatMakerNoRecord::FloatingSectionWindow::closeButtonPressed()
{
    if (onClosePressed)
        onClosePressed();
}

BeatMakerNoRecord::LayoutSplitter::LayoutSplitter (bool isVerticalToUse)
    : isVertical (isVerticalToUse)
{
    setOpaque (false);
    setAlwaysOnTop (true);
    setMouseCursor (isVertical ? juce::MouseCursor::LeftRightResizeCursor
                               : juce::MouseCursor::UpDownResizeCursor);
}

void BeatMakerNoRecord::LayoutSplitter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colours::transparentBlack);

    const float center = isVertical ? bounds.getWidth() * 0.5f : bounds.getHeight() * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    if (isVertical)
        g.fillRect (juce::Rectangle<float> (center - 0.5f, 0.0f, 1.0f, bounds.getHeight()));
    else
        g.fillRect (juce::Rectangle<float> (0.0f, center - 0.5f, bounds.getWidth(), 1.0f));

    g.setColour (juce::Colour::fromRGB (101, 168, 233).withAlpha (isMouseOverOrDragging() ? 0.75f : 0.35f));
    if (isVertical)
    {
        const float gripY = bounds.getCentreY();
        g.fillRoundedRectangle ({ center - 2.2f, gripY - 18.0f, 4.4f, 36.0f }, 2.0f);
    }
    else
    {
        const float gripX = bounds.getCentreX();
        g.fillRoundedRectangle ({ gripX - 18.0f, center - 2.2f, 36.0f, 4.4f }, 2.0f);
    }
}

void BeatMakerNoRecord::LayoutSplitter::mouseDown (const juce::MouseEvent& e)
{
    dragStartScreen = e.getScreenPosition();
}

void BeatMakerNoRecord::LayoutSplitter::mouseDrag (const juce::MouseEvent& e)
{
    const auto current = e.getScreenPosition();
    const int delta = isVertical ? (current.x - dragStartScreen.x) : (current.y - dragStartScreen.y);
    if (delta == 0)
        return;

    dragStartScreen = current;
    if (onDeltaDrag != nullptr)
        onDeltaDrag (delta);
}

BeatMakerNoRecord::TimelineRulerComponent::TimelineRulerComponent (BeatMakerNoRecord& ownerToUse)
    : owner (ownerToUse)
{
    setOpaque (true);
    startTimerHz (10);
}

void BeatMakerNoRecord::TimelineRulerComponent::paint (juce::Graphics& g)
{
    owner.paintTimelineRuler (g, getLocalBounds());
}

void BeatMakerNoRecord::TimelineRulerComponent::mouseDown (const juce::MouseEvent& e)
{
    owner.handleTimelineRulerMouseDown (e, getWidth());
}

void BeatMakerNoRecord::TimelineRulerComponent::mouseDrag (const juce::MouseEvent& e)
{
    owner.handleTimelineRulerMouseDrag (e, getWidth());
}

void BeatMakerNoRecord::TimelineRulerComponent::mouseUp (const juce::MouseEvent& e)
{
    owner.handleTimelineRulerMouseUp (e, getWidth());
}

void BeatMakerNoRecord::TimelineRulerComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    owner.handleTimelineRulerMouseWheel (e, wheel, getWidth());
}

void BeatMakerNoRecord::TimelineRulerComponent::timerCallback()
{
    owner.runTransportPlaybackSafetyCheck();
    owner.updateTransportInfoLabel();

    if (owner.shouldAnimateTimelineRuler())
        repaint();
}

BeatMakerNoRecord::MidiPianoRollComponent::MidiPianoRollComponent (BeatMakerNoRecord& ownerToUse)
    : owner (ownerToUse)
{
    setOpaque (true);
    startTimerHz (8);
}

void BeatMakerNoRecord::MidiPianoRollComponent::paint (juce::Graphics& g)
{
    owner.paintMidiPianoRoll (g, getLocalBounds());
}

void BeatMakerNoRecord::MidiPianoRollComponent::mouseDown (const juce::MouseEvent& e)
{
    owner.handleMidiPianoRollMouseDown (e, getWidth(), getHeight());
}

void BeatMakerNoRecord::MidiPianoRollComponent::mouseDrag (const juce::MouseEvent& e)
{
    owner.handleMidiPianoRollMouseDrag (e, getWidth(), getHeight());
}

void BeatMakerNoRecord::MidiPianoRollComponent::mouseUp (const juce::MouseEvent& e)
{
    owner.handleMidiPianoRollMouseUp (e);
}

void BeatMakerNoRecord::MidiPianoRollComponent::mouseMove (const juce::MouseEvent& e)
{
    owner.handleMidiPianoRollMouseMove (e, getWidth(), getHeight());
}

void BeatMakerNoRecord::MidiPianoRollComponent::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void BeatMakerNoRecord::MidiPianoRollComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    owner.handleMidiPianoRollMouseWheel (e, wheel, getWidth(), getHeight());
}

void BeatMakerNoRecord::MidiPianoRollComponent::timerCallback()
{
    if (owner.shouldAnimateMidiPianoRoll())
        repaint();
}

BeatMakerNoRecord::StepSequencerComponent::StepSequencerComponent (BeatMakerNoRecord& ownerToUse)
    : owner (ownerToUse)
{
    setOpaque (true);
    startTimerHz (8);
}

void BeatMakerNoRecord::StepSequencerComponent::paint (juce::Graphics& g)
{
    owner.paintStepSequencer (g, getLocalBounds());
}

void BeatMakerNoRecord::StepSequencerComponent::mouseDown (const juce::MouseEvent& e)
{
    owner.handleStepSequencerMouseDown (e, getWidth(), getHeight());
}

void BeatMakerNoRecord::StepSequencerComponent::mouseDrag (const juce::MouseEvent& e)
{
    owner.handleStepSequencerMouseDrag (e, getWidth(), getHeight());
}

void BeatMakerNoRecord::StepSequencerComponent::mouseUp (const juce::MouseEvent& e)
{
    owner.handleStepSequencerMouseUp (e);
}

void BeatMakerNoRecord::StepSequencerComponent::timerCallback()
{
    if (! owner.isShowing() || ! isShowing())
        return;

    const bool playing = owner.edit != nullptr && owner.edit->getTransport().isPlaying();
    if (playing && owner.getSelectedMidiClip() != nullptr && owner.stepSequencerDragMode == StepSequencerDragMode::none)
    {
        owner.updateStepSequencerScrollbarFromPageContext();
        repaint();
        return;
    }

    juce::String stateSignature;
    if (auto* midiClip = owner.getSelectedMidiClip())
    {
        stateSignature << "clip=" << midiClip->getName()
                       << "|page=" << juce::String (owner.getStepSequencerPageStartBeat (*midiClip), 3);

        juce::uint32 hash = 2166136261u;
        for (auto* note : midiClip->getSequence().getNotes())
        {
            if (note == nullptr)
                continue;

            hash ^= (juce::uint32) note->getNoteNumber();
            hash *= 16777619u;
            hash ^= (juce::uint32) juce::roundToInt (note->getStartBeat().inBeats() * 960.0);
            hash *= 16777619u;
            hash ^= (juce::uint32) juce::roundToInt (note->getLengthBeats().inBeats() * 960.0);
            hash *= 16777619u;
            hash ^= (juce::uint32) note->getVelocity();
            hash *= 16777619u;
        }

        stateSignature << "|hash=" << juce::String ((int64) hash);
    }
    else
    {
        stateSignature = "no-midi";
    }

    bool shouldRepaint = owner.shouldAnimateStepSequencer()
                      || owner.stepSequencerDragMode != StepSequencerDragMode::none;

    if (stateSignature != lastStateSignature)
    {
        lastStateSignature = stateSignature;
        shouldRepaint = true;
    }

    if (shouldRepaint)
    {
        if (owner.stepSequencerDragMode == StepSequencerDragMode::none)
            owner.updateStepSequencerScrollbarFromPageContext();
        repaint();
    }
}

BeatMakerNoRecord::MixerAreaComponent::MixerAreaComponent (BeatMakerNoRecord& ownerToUse)
    : owner (ownerToUse)
{
    setOpaque (true);
    startTimerHz (6);
}

void BeatMakerNoRecord::MixerAreaComponent::paint (juce::Graphics& g)
{
    owner.paintMixerArea (g, getLocalBounds());
}

void BeatMakerNoRecord::MixerAreaComponent::mouseDown (const juce::MouseEvent& e)
{
    owner.handleMixerAreaMouseDown (e, getWidth(), getHeight());
}

void BeatMakerNoRecord::MixerAreaComponent::mouseDrag (const juce::MouseEvent& e)
{
    owner.handleMixerAreaMouseDrag (e, getWidth(), getHeight());
}

void BeatMakerNoRecord::MixerAreaComponent::mouseUp (const juce::MouseEvent& e)
{
    owner.handleMixerAreaMouseUp (e);
}

void BeatMakerNoRecord::MixerAreaComponent::timerCallback()
{
    if (! owner.isShowing() || ! isShowing())
        return;

    const bool playing = owner.edit != nullptr && owner.edit->getTransport().isPlaying();
    if (playing && owner.mixerDragMode == MixerDragMode::none)
    {
        repaint();
        return;
    }

    juce::String stateSignature;
    if (owner.edit == nullptr)
    {
        stateSignature = "no-edit";
    }
    else
    {
        auto tracks = te::getAudioTracks (*owner.edit);
        stateSignature << "tracks=" << tracks.size();

        if (auto* selectedTrack = owner.getSelectedTrackOrFirst())
            stateSignature << "|selected=" << selectedTrack->getName();
        else
            stateSignature << "|selected=none";

        stateSignature << "|playing=" << (owner.edit->getTransport().isPlaying() ? "1" : "0");

        for (int i = 0; i < tracks.size(); ++i)
        {
            auto* track = tracks.getUnchecked (i);
            if (track == nullptr)
                continue;

            float volumeDb = 0.0f;
            float pan = 0.0f;
            if (auto* vp = track->getVolumePlugin())
            {
                volumeDb = vp->getVolumeDb();
                pan = vp->getPan();
            }

            stateSignature << ";"
                           << i
                           << ":"
                           << track->getName()
                           << ":"
                           << (track->isMuted (false) ? "1" : "0")
                           << ":"
                           << (track->isSolo (false) ? "1" : "0")
                           << ":"
                           << juce::String (volumeDb, 2)
                           << ":"
                           << juce::String (pan, 2);
        }
    }

    bool shouldRepaint = owner.mixerDragMode != MixerDragMode::none;

    if (stateSignature != lastStateSignature)
    {
        lastStateSignature = stateSignature;
        shouldRepaint = true;
    }

    if (owner.edit != nullptr && owner.edit->getTransport().isPlaying())
        shouldRepaint = true;

    if (shouldRepaint)
        repaint();
}

void BeatMakerNoRecord::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient base (juce::Colour::fromRGB (11, 15, 22), 0.0f, 0.0f,
                               juce::Colour::fromRGB (7, 9, 14), 0.0f, bounds.getBottom(), false);
    g.setGradientFill (base);
    g.fillAll();

    juce::ColourGradient ambience (juce::Colour::fromRGB (29, 58, 86).withAlpha (0.16f), bounds.getWidth() * 0.22f, bounds.getHeight() * 0.03f,
                                   juce::Colour::fromRGB (10, 13, 19).withAlpha (0.0f), bounds.getWidth() * 0.86f, bounds.getHeight() * 0.82f, true);
    g.setGradientFill (ambience);
    g.fillEllipse (bounds.getWidth() * 0.06f, bounds.getHeight() * -0.06f, bounds.getWidth() * 0.86f, bounds.getHeight() * 0.72f);

    juce::ColourGradient sideGlow (juce::Colour::fromRGB (43, 82, 118).withAlpha (0.09f), bounds.getWidth() * 0.95f, bounds.getHeight() * 0.32f,
                                   juce::Colour::fromRGB (14, 18, 24).withAlpha (0.0f), bounds.getWidth() * 0.72f, bounds.getHeight() * 0.92f, true);
    g.setGradientFill (sideGlow);
    g.fillEllipse (bounds.getWidth() * 0.58f, bounds.getHeight() * 0.12f, bounds.getWidth() * 0.54f, bounds.getHeight() * 0.78f);

    g.setColour (juce::Colours::white.withAlpha (0.013f));
    constexpr int rowStep = 36;
    for (int y = 0; y < getHeight(); y += rowStep)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    g.setColour (juce::Colours::white.withAlpha (0.008f));
    constexpr int colStep = 70;
    for (int x = 0; x < getWidth(); x += colStep)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());

    auto drawPanelShell = [&g] (juce::Rectangle<int> area,
                                juce::Colour tint,
                                float cornerSize)
    {
        if (area.isEmpty())
            return;

        auto shell = area.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.24f));
        g.fillRoundedRectangle (shell.translated (0.0f, 2.0f), cornerSize);

        juce::ColourGradient fill (juce::Colour::fromRGB (21, 29, 40).withAlpha (0.90f), shell.getX(), shell.getY(),
                                   juce::Colour::fromRGB (14, 19, 28).withAlpha (0.84f), shell.getX(), shell.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (shell, cornerSize);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (shell, cornerSize, 1.0f);

        auto accent = shell.removeFromTop (2.0f).reduced (14.0f, 0.0f);
        juce::ColourGradient accentFill (tint.withAlpha (0.66f), accent.getX(), accent.getY(),
                                         tint.withAlpha (0.0f), accent.getRight(), accent.getY(), false);
        g.setGradientFill (accentFill);
        g.fillRoundedRectangle (accent, 1.0f);
    };

    auto boundsInEditor = [this] (juce::Component& c)
    {
        auto componentBounds = c.getBounds();
        auto* parent = c.getParentComponent();
        while (parent != nullptr && parent != this)
        {
            componentBounds = componentBounds.translated (parent->getX(), parent->getY());
            parent = parent->getParentComponent();
        }

        return parent == this ? componentBounds : juce::Rectangle<int>();
    };

    juce::Rectangle<int> leftUnion;
    bool hasLeftUnion = false;
    for (auto* panel : { static_cast<juce::Component*> (&arrangementGroup),
                         static_cast<juce::Component*> (&trackGroup),
                         static_cast<juce::Component*> (&clipEditGroup),
                         static_cast<juce::Component*> (&midiEditGroup),
                         static_cast<juce::Component*> (&audioEditGroup),
                         static_cast<juce::Component*> (&fxGroup),
                         static_cast<juce::Component*> (&mixerGroup) })
    {
        if (panel->isVisible() && ! panel->getBounds().isEmpty())
        {
            const auto panelBounds = boundsInEditor (*panel);
            leftUnion = hasLeftUnion ? leftUnion.getUnion (panelBounds) : panelBounds;
            hasLeftUnion = true;
        }
    }

    juce::Rectangle<int> rightUnion;
    bool hasRightUnion = false;
    for (auto* panel : { static_cast<juce::Component*> (&workspaceGroup),
                         static_cast<juce::Component*> (&mixerAreaGroup),
                         static_cast<juce::Component*> (&channelRackGroup),
                         static_cast<juce::Component*> (&inspectorGroup),
                         static_cast<juce::Component*> (&stepSequencerGroup),
                         static_cast<juce::Component*> (&pianoRollGroup) })
    {
        if (panel->isVisible() && ! panel->getBounds().isEmpty())
        {
            const auto panelBounds = boundsInEditor (*panel);
            rightUnion = hasRightUnion ? rightUnion.getUnion (panelBounds) : panelBounds;
            hasRightUnion = true;
        }
    }

    if (! sessionGroup.getBounds().isEmpty())
        drawPanelShell (sessionGroup.getBounds().expanded (6, 6), juce::Colour::fromRGB (97, 167, 233), 10.0f);

    if (hasLeftUnion)
        drawPanelShell (leftUnion.expanded (6, 6), juce::Colour::fromRGB (92, 156, 219), 12.0f);

    if (hasRightUnion)
        drawPanelShell (rightUnion.expanded (6, 6), juce::Colour::fromRGB (102, 172, 231), 12.0f);

    if (leftDockUsesDualColumn && ! leftDockViewportBounds.isEmpty())
    {
        const int dividerX = juce::jlimit (leftDockViewportBounds.getX() + 12,
                                           leftDockViewportBounds.getRight() - 12,
                                           leftDockDualColumnDividerX);
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawVerticalLine (dividerX,
                            (float) leftDockViewportBounds.getY() + 3.0f,
                            (float) leftDockViewportBounds.getBottom() - 3.0f);
    }

    if (leftDockScrollSlider.isVisible() && leftDockScrollSlider.isEnabled() && ! leftDockViewportBounds.isEmpty())
    {
        const auto value = leftDockScrollSlider.getValue();
        const auto minValue = leftDockScrollSlider.getMinimum();
        const auto maxValue = leftDockScrollSlider.getMaximum();
        const int fadeHeight = 14;

        if (value > minValue + 0.5)
        {
            auto topFade = leftDockViewportBounds.withHeight (juce::jmin (fadeHeight, leftDockViewportBounds.getHeight()));
            juce::ColourGradient fade (juce::Colour::fromRGB (10, 14, 22).withAlpha (0.74f), (float) topFade.getX(), (float) topFade.getY(),
                                       juce::Colour::fromRGB (10, 14, 22).withAlpha (0.0f), (float) topFade.getX(), (float) topFade.getBottom(), false);
            g.setGradientFill (fade);
            g.fillRect (topFade);
        }

        if (value < maxValue - 0.5)
        {
            auto bottomFade = leftDockViewportBounds.withY (leftDockViewportBounds.getBottom() - juce::jmin (fadeHeight, leftDockViewportBounds.getHeight()));
            juce::ColourGradient fade (juce::Colour::fromRGB (10, 14, 22).withAlpha (0.0f), (float) bottomFade.getX(), (float) bottomFade.getY(),
                                       juce::Colour::fromRGB (10, 14, 22).withAlpha (0.74f), (float) bottomFade.getX(), (float) bottomFade.getBottom(), false);
            g.setGradientFill (fade);
            g.fillRect (bottomFade);
        }
    }

    auto titleArea = getLocalBounds().reduced (16).removeFromTop (24);
    auto titleTextArea = titleArea.removeFromLeft (235);
    g.setColour (juce::Colours::white.withAlpha (0.98f));
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("TheSampledexWorkflow", titleTextArea, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour::fromRGB (145, 196, 244).withAlpha (0.78f));
    g.setFont (juce::Font (juce::FontOptions (10.8f, juce::Font::plain)));
    g.drawText ("Playback/editing workstation", titleArea.removeFromLeft (176), juce::Justification::centredLeft, false);

    auto drawTopBadge = [&g] (juce::Rectangle<int> area, juce::Colour tint, const juce::String& text)
    {
        if (area.isEmpty() || text.isEmpty())
            return;

        auto badge = area.toFloat().reduced (0.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.24f));
        g.fillRoundedRectangle (badge.translated (0.0f, 1.0f), 5.0f);

        juce::ColourGradient fill (tint.withAlpha (0.30f), badge.getX(), badge.getY(),
                                   juce::Colour::fromRGB (18, 24, 34).withAlpha (0.12f), badge.getX(), badge.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (badge, 5.0f);

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (badge, 5.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.94f));
        g.setFont (juce::Font (juce::FontOptions (10.3f, juce::Font::bold)));
        g.drawFittedText (text, badge.toNearestInt().reduced (8, 0), juce::Justification::centred, 1);
    };

    const bool isPlaying = edit != nullptr && edit->getTransport().isPlaying();
    const auto playbackBadgeTint = isPlaying ? juce::Colour::fromRGB (69, 184, 130)
                                             : juce::Colour::fromRGB (118, 138, 166);
    auto playbackBadge = titleArea.removeFromLeft (isPlaying ? 62 : 66);
    drawTopBadge (playbackBadge, playbackBadgeTint, isPlaying ? "PLAY" : "STOP");
    titleArea.removeFromLeft (8);
    drawTopBadge (titleArea.removeFromLeft (122),
                  juce::Colour::fromRGB (76, 130, 186),
                  "REC DISABLED");

    auto drawInfoChip = [&g] (juce::Rectangle<int> area, juce::Colour tint)
    {
        if (area.isEmpty())
            return;

        auto chip = area.expanded (4, 2).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.24f));
        g.fillRoundedRectangle (chip.translated (0.0f, 1.0f), 6.0f);

        juce::ColourGradient fill (tint.withAlpha (0.16f), chip.getX(), chip.getY(),
                                   juce::Colour::fromRGB (18, 24, 34).withAlpha (0.08f), chip.getX(), chip.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (chip, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (chip, 6.0f, 1.0f);
    };

    drawInfoChip (editNameLabel.getBounds(), juce::Colour::fromRGB (76, 126, 182));
    drawInfoChip (transportInfoLabel.getBounds(), juce::Colour::fromRGB (64, 144, 205));
    drawInfoChip (workflowStateLabel.getBounds(), juce::Colour::fromRGB (78, 156, 211));
    drawInfoChip (selectedTrackLabel.getBounds(), juce::Colour::fromRGB (72, 132, 191));
    drawInfoChip (statusLabel.getBounds(), juce::Colour::fromRGB (72, 150, 120));
    drawInfoChip (contextHintLabel.getBounds(), juce::Colour::fromRGB (88, 140, 201));

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    const int topChromeBottom = juce::jmax (topMenuBar.getBottom(), commandToolbar.getBottom());
    g.drawHorizontalLine (topChromeBottom + 5, 14.0f, (float) getWidth() - 14.0f);
    g.drawHorizontalLine (statusLabel.getY() - 5, 14.0f, (float) getWidth() - 14.0f);

    if (fileDragOverlayActive)
    {
        auto overlay = getLocalBounds().reduced (20).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.fillRoundedRectangle (overlay, 12.0f);

        juce::ColourGradient tint (juce::Colour::fromRGB (70, 138, 208).withAlpha (0.36f), overlay.getX(), overlay.getY(),
                                   juce::Colour::fromRGB (28, 58, 88).withAlpha (0.18f), overlay.getRight(), overlay.getBottom(), true);
        g.setGradientFill (tint);
        g.fillRoundedRectangle (overlay, 12.0f);

        g.setColour (juce::Colour::fromRGB (136, 198, 246).withAlpha (0.94f));
        g.drawRoundedRectangle (overlay.reduced (1.0f), 12.0f, 2.0f);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawFittedText ("Drop Audio or MIDI Files to Import",
                          overlay.toNearestInt().reduced (18, 20),
                          juce::Justification::centred,
                          2);

        g.setColour (juce::Colours::white.withAlpha (0.82f));
        g.setFont (juce::Font (juce::FontOptions (11.2f, juce::Font::plain)));
        g.drawFittedText ("Supported: WAV, AIFF, FLAC, MP3, OGG, M4A, CAF, MID, MIDI",
                          overlay.toNearestInt().reduced (18, 52),
                          juce::Justification::centredBottom,
                          2);
    }
}

void BeatMakerNoRecord::resized()
{
    const auto densityMode = uiDensityMode;
    const float uiScale = getUiDensityScale();
    const bool compactLayout = getWidth() < roundToInt (1540.0f * uiScale)
                               || getHeight() < roundToInt (860.0f * uiScale);
    const bool denseLayout = getWidth() < roundToInt (1360.0f * uiScale)
                             || getHeight() < roundToInt (760.0f * uiScale);
    int defaultRowHeight = denseLayout ? 27 : (compactLayout ? 28 : 30);
    int rowGap = denseLayout ? 4 : 5;
    int groupInset = denseLayout ? 8 : 10;
    int groupTitleInset = denseLayout ? 18 : 20;

    if (densityMode == UiDensityMode::compact)
    {
        defaultRowHeight = juce::jmax (24, defaultRowHeight - 2);
        rowGap = juce::jmax (3, rowGap - 1);
        groupInset = juce::jmax (7, groupInset - 1);
    }
    else if (densityMode == UiDensityMode::accessible)
    {
        defaultRowHeight += 4;
        rowGap += 1;
        groupInset += 1;
        groupTitleInset += 2;
    }

    auto setBoundsOrHide = [] (juce::Component& component, juce::Rectangle<int> bounds)
    {
        if (bounds.getWidth() < 12 || bounds.getHeight() < 12)
            component.setBounds ({});
        else
            component.setBounds (bounds);
    };

    auto layoutButtonRow = [&setBoundsOrHide, denseLayout, defaultRowHeight, densityMode] (juce::Rectangle<int> row, std::initializer_list<juce::Button*> buttons)
    {
        const int count = (int) buttons.size();
        if (count == 0 || row.getWidth() <= 0 || row.getHeight() <= 0)
        {
            for (auto* button : buttons)
                button->setBounds ({});
            return;
        }

        const int gapX = row.getWidth() < 460 ? 4 : 6;
        int minButtonWidth = denseLayout ? 58 : 68;
        if (densityMode == UiDensityMode::compact)
            minButtonWidth = juce::jmax (50, minButtonWidth - 8);
        else if (densityMode == UiDensityMode::accessible)
            minButtonWidth += 16;

        const int columns = juce::jmax (1, juce::jmin (count, (row.getWidth() + gapX) / (minButtonWidth + gapX)));

        const int gapY = row.getHeight() >= (defaultRowHeight * 2) ? 3 : 2;
        const int maxRowsByHeight = juce::jmax (1, (row.getHeight() + gapY) / juce::jmax (1, defaultRowHeight + gapY));
        const int requiredRows = juce::jmax (1, (count + columns - 1) / columns);
        const int maxRowsByMode = (densityMode == UiDensityMode::accessible ? 3 : 2);
        const int usedRows = juce::jmax (1, juce::jmin (requiredRows, juce::jmin (maxRowsByHeight, maxRowsByMode)));
        const int visibleCount = juce::jmin (count, columns * usedRows);

        juce::Array<juce::Button*> visibleButtons;
        visibleButtons.ensureStorageAllocated (visibleCount);

        int index = 0;
        for (auto* button : buttons)
        {
            if (index < visibleCount)
                visibleButtons.add (button);
            else
                button->setBounds ({});

            ++index;
        }

        const int totalGapY = gapY * juce::jmax (0, usedRows - 1);
        const int availableHeight = juce::jmax (0, row.getHeight() - totalGapY);
        const int baseRowHeight = usedRows > 0 ? availableHeight / usedRows : 0;
        int rowHeightRemainder = usedRows > 0 ? availableHeight % usedRows : 0;

        int buttonIndex = 0;
        for (int rowIndex = 0; rowIndex < usedRows && buttonIndex < visibleCount; ++rowIndex)
        {
            int thisRowHeight = baseRowHeight;
            if (rowHeightRemainder > 0)
            {
                ++thisRowHeight;
                --rowHeightRemainder;
            }

            auto line = row.removeFromTop (juce::jmax (0, thisRowHeight));
            if (rowIndex < usedRows - 1 && row.getHeight() > 0)
                row.removeFromTop (juce::jmin (gapY, row.getHeight()));

            const int rowButtonCount = juce::jmin (columns, visibleCount - buttonIndex);
            const int availableWidth = juce::jmax (0, line.getWidth() - gapX * juce::jmax (0, rowButtonCount - 1));
            const int baseWidth = rowButtonCount > 0 ? availableWidth / rowButtonCount : 0;
            int widthRemainder = rowButtonCount > 0 ? availableWidth % rowButtonCount : 0;

            for (int i = 0; i < rowButtonCount; ++i)
            {
                int width = baseWidth;
                if (widthRemainder > 0)
                {
                    ++width;
                    --widthRemainder;
                }

                setBoundsOrHide (*visibleButtons.getUnchecked (buttonIndex), line.removeFromLeft (juce::jmax (0, width)).reduced (0, 1));
                ++buttonIndex;

                if (i < rowButtonCount - 1 && line.getWidth() > 0)
                    line.removeFromLeft (juce::jmin (gapX, line.getWidth()));
            }
        }
    };

    auto getAdaptiveButtonRowHeight = [denseLayout, defaultRowHeight, densityMode] (int availableWidth, int buttonCount, int maxRows = 2)
    {
        if (availableWidth <= 0 || buttonCount <= 0)
            return defaultRowHeight;

        const int gap = availableWidth < 460 ? 4 : 6;
        int minButtonWidth = denseLayout ? 58 : 68;
        if (densityMode == UiDensityMode::compact)
            minButtonWidth = juce::jmax (50, minButtonWidth - 8);
        else if (densityMode == UiDensityMode::accessible)
            minButtonWidth += 16;

        const int columns = juce::jmax (1, juce::jmin (buttonCount, (availableWidth + gap) / (minButtonWidth + gap)));
        const int requiredRows = juce::jmax (1, (buttonCount + columns - 1) / columns);
        const int densityMaxRows = densityMode == UiDensityMode::accessible ? juce::jmax (2, maxRows)
                                                                            : juce::jmax (1, maxRows);
        const int rows = juce::jlimit (1, densityMaxRows, requiredRows);
        return rows * defaultRowHeight + juce::jmax (0, rows - 1) * 3;
    };

    auto nextRow = [defaultRowHeight, rowGap] (juce::Rectangle<int>& area, int height = 0)
    {
        const int targetHeight = juce::jmax (0, juce::jmin (height > 0 ? height : defaultRowHeight, area.getHeight()));
        auto row = area.removeFromTop (targetHeight);
        if (area.getHeight() > 0)
            area.removeFromTop (juce::jmin (rowGap, area.getHeight()));
        return row;
    };

    auto getGroupContent = [groupInset, groupTitleInset] (juce::GroupComponent& group, juce::Rectangle<int> bounds)
    {
        group.setBounds (bounds);

        if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
            return juce::Rectangle<int>();

        auto inner = bounds.reduced (groupInset);
        if (inner.getHeight() <= groupTitleInset)
            return juce::Rectangle<int>();

        inner.removeFromTop (groupTitleInset);
        return inner;
    };

    auto estimateGroupHeight = [groupInset, groupTitleInset, rowGap] (std::initializer_list<int> rowHeights)
    {
        int contentHeight = 0;
        int rowCount = 0;
        for (const int height : rowHeights)
        {
            if (height <= 0)
                continue;

            contentHeight += height;
            ++rowCount;
        }

        if (rowCount > 1)
            contentHeight += rowGap * (rowCount - 1);

        return contentHeight + groupInset * 2 + groupTitleInset;
    };

    auto bounds = getLocalBounds().reduced (12);
    const int footerHeight = (denseLayout ? 30 : 34) + (densityMode == UiDensityMode::accessible ? 4 : (densityMode == UiDensityMode::compact ? -2 : 0));
    auto footer = bounds.removeFromBottom (juce::jmax (26, footerHeight)).reduced (2, 2);
    statusLabel.setBounds (footer.removeFromLeft ((footer.getWidth() * 57) / 100).reduced (0, 1));
    footer.removeFromLeft (8);
    contextHintLabel.setBounds (footer.reduced (0, 1));

    const int menuBarHeight = densityMode == UiDensityMode::accessible ? 34 : (denseLayout ? 28 : 32);
    auto menuBarArea = bounds.removeFromTop (menuBarHeight);
    topMenuBar.setBounds (menuBarArea.reduced (1, 2));
    const int commandToolbarHeight = densityMode == UiDensityMode::accessible ? 32 : (denseLayout ? 27 : 30);
    auto commandToolbarArea = bounds.removeFromTop (commandToolbarHeight);
    commandToolbar.setBounds (commandToolbarArea.reduced (1, 1));
    bounds.removeFromTop (denseLayout ? 6 : 8);

    const int sessionInnerWidth = juce::jmax (220, bounds.getWidth() - groupInset * 2);
    const int row1TransportWidth = juce::jmax (denseLayout ? 220 : 300, sessionInnerWidth / (denseLayout ? 2 : 3));
    const int row1EditNameWidth = juce::jmax (130, juce::jmax (0, sessionInnerWidth - row1TransportWidth) / 4);
    const int row1ButtonsWidth = juce::jmax (120, sessionInnerWidth - row1TransportWidth - row1EditNameWidth - 8);
    const int row2TempoWidth = juce::jmax (denseLayout ? 180 : 220, sessionInnerWidth / (denseLayout ? 2 : 3));
    const int row2ButtonsWidth = juce::jmax (120, sessionInnerWidth - row2TempoWidth - 8);
    const int row3QuickWidth = juce::jmax (denseLayout ? 220 : 320, sessionInnerWidth / 2);
    const int row3StateWidth = juce::jmax (120, sessionInnerWidth - row3QuickWidth - 8);

    const int sessionRowHeight1 = juce::jmax (defaultRowHeight + 2, getAdaptiveButtonRowHeight (row1ButtonsWidth, 7, 2));
    const int sessionRowHeight2 = juce::jmax (defaultRowHeight + 2, getAdaptiveButtonRowHeight (row2ButtonsWidth, 8, 2));
    const int sessionRowHeight3 = juce::jmax (defaultRowHeight + 2,
                                              juce::jmax (getAdaptiveButtonRowHeight (row3QuickWidth, 3, 2),
                                                          getAdaptiveButtonRowHeight (row3StateWidth, 1, 1)));
    const int sessionHeight = estimateGroupHeight ({ sessionRowHeight1, sessionRowHeight2, sessionRowHeight3 });
    auto sessionBounds = bounds.removeFromTop (juce::jlimit (92, 188, sessionHeight));
    bounds.removeFromTop (10);
    auto sessionArea = getGroupContent (sessionGroup, sessionBounds);

    auto sessionRow1 = nextRow (sessionArea, sessionRowHeight1);
    auto transportHud = sessionRow1.removeFromRight (juce::jmax (denseLayout ? 220 : 300,
                                                                 sessionRow1.getWidth() / (denseLayout ? 2 : 3)));
    transportHud.removeFromLeft (juce::jmin (8, transportHud.getWidth()));

    auto editNameArea = sessionRow1.removeFromRight (juce::jmax (130, sessionRow1.getWidth() / 4));
    if (sessionRow1.getWidth() > 6)
        sessionRow1.removeFromRight (6);
    layoutButtonRow (sessionRow1, { &newEditButton, &openEditButton, &saveButton, &saveAsButton,
                                    &undoButton, &redoButton, &helpButton });
    editNameLabel.setBounds (editNameArea.reduced (0, 1));
    transportInfoLabel.setBounds (transportHud.reduced (0, 1));

    auto sessionRow2 = nextRow (sessionArea, sessionRowHeight2);
    auto tempoArea = sessionRow2.removeFromRight (juce::jmax (denseLayout ? 180 : 220,
                                                              sessionRow2.getWidth() / (denseLayout ? 2 : 3)));
    tempoArea.removeFromLeft (juce::jmin (10, tempoArea.getWidth()));
    layoutButtonRow (sessionRow2, { &playPauseButton, &stopButton, &returnToStartButton, &transportLoopButton,
                                    &setLoopToSelectionButton, &zoomInButton, &zoomOutButton, &zoomResetButton });
    tempoLabel.setBounds (tempoArea.removeFromLeft (56).reduced (0, 1));
    tempoArea.removeFromLeft (4);
    tempoSlider.setBounds (tempoArea.reduced (0, 1));

    auto sessionRow3 = nextRow (sessionArea, sessionRowHeight3);
    auto quickActionArea = sessionRow3.removeFromRight (juce::jmax (denseLayout ? 220 : 320, sessionRow3.getWidth() / 2));
    quickActionArea.removeFromLeft (juce::jmin (8, quickActionArea.getWidth()));
    workflowStateLabel.setBounds (sessionRow3.reduced (0, 1));
    layoutButtonRow (quickActionArea, { &focusSelectionButton, &centerPlayheadButton, &fitProjectButton });

    auto body = bounds;
    const auto fullBodyBounds = body;
    currentBodyWidthForResize = body.getWidth();
    currentRightDockWidthForResize = 0;
    currentRightDockHeightForResize = 0;
    currentBottomDockHeightForResize = 0;

    const int splitterThickness = 8;
    const int splitGap = 10;

    leftDockSplitter.setBounds ({});
    workspaceMixerSplitter.setBounds ({});
    workspaceBottomSplitter.setBounds ({});
    mixerPianoSplitter.setBounds ({});

    const int hardMinLeftDockWidth = denseLayout ? 170 : 200;
    const int preferredMinLeftDockWidth = denseLayout ? 260 : 300;
    const int preferredMinRightDockWidth = denseLayout ? 420 : 520;
    const int minRightDockVisibleWidth = denseLayout ? 380 : 480;
    const int preferredMaxLeftDockWidth = juce::jmax (preferredMinLeftDockWidth,
                                                      body.getWidth() - preferredMinRightDockWidth);
    const int maxLeftDockWidth = juce::jmax (0, body.getWidth() - splitGap - minRightDockVisibleWidth);

    int leftDockWidth = roundToInt ((float) body.getWidth() * leftDockWidthRatio);

    if (maxLeftDockWidth >= hardMinLeftDockWidth)
    {
        const int upperBound = juce::jmax (hardMinLeftDockWidth,
                                           juce::jmin (preferredMaxLeftDockWidth, maxLeftDockWidth));
        leftDockWidth = juce::jlimit (hardMinLeftDockWidth, upperBound, leftDockWidth);
    }
    else
    {
        // When the window is very narrow, protect the right dock so timeline/piano don't disappear.
        leftDockWidth = juce::jmax (0, maxLeftDockWidth);
    }

    leftDockWidthRatio = (float) leftDockWidth / (float) juce::jmax (1, body.getWidth());

    auto leftDock = body.removeFromLeft (leftDockWidth);
    const int leftDockSplitX = leftDock.getRight();
    body.removeFromLeft (splitGap);
    auto rightDock = body;

    const int leftSectionGap = denseLayout ? 6 : 8;
    const auto panelMode = getLeftDockPanelModeSelection();
    const bool showProjectPanelSet = panelMode == LeftDockPanelMode::all || panelMode == LeftDockPanelMode::project;
    const bool showEditingPanelSet = panelMode == LeftDockPanelMode::all || panelMode == LeftDockPanelMode::editing;
    const bool showSoundPanelSet = panelMode == LeftDockPanelMode::all || panelMode == LeftDockPanelMode::sound;

    const bool showArrangementPanel = showProjectPanelSet;
    const bool showTrackPanel = showProjectPanelSet;
    const bool showClipTools = clipEditGroup.isVisible() && showEditingPanelSet;
    const bool showMidiTools = midiEditGroup.isVisible() && showEditingPanelSet;
    const bool showAudioTools = audioEditGroup.isVisible() && showEditingPanelSet;
    const bool showFxPanel = showSoundPanelSet;
    const bool showMixerPanel = showProjectPanelSet || showSoundPanelSet;
    const int panelSelectorHeight = defaultRowHeight + 8;
    const int dualColumnGap = denseLayout ? 6 : 8;
    const int leftDockScrollTrackWidth = denseLayout ? 14 : 16;
    const int leftDockScrollPaddingWidth = 4;

    struct LeftDockMetrics
    {
        bool useDualColumn = false;
        int arrangementPrefHeight = 0;
        int trackPrefHeight = 0;
        int clipPrefHeight = 0;
        int midiPrefHeight = 0;
        int audioPrefHeight = 0;
        int fxPrefHeight = 0;
        int mixerPrefHeight = 0;
        int leftColumnContentHeight = 0;
        int rightColumnContentHeight = 0;
        int singleColumnContentHeight = 0;
        int totalLeftContentHeight = 0;
    };

    auto computeLeftDockMetrics = [&] (int dockWidth)
    {
        LeftDockMetrics metrics;
        metrics.useDualColumn = ! denseLayout && dockWidth >= 620;

        const int leftColumnWidthForCalc = metrics.useDualColumn
                                               ? juce::jmax (140, (dockWidth - dualColumnGap) / 2)
                                               : dockWidth;
        const int rightColumnWidthForCalc = metrics.useDualColumn
                                                ? juce::jmax (140, dockWidth - leftColumnWidthForCalc - dualColumnGap)
                                                : dockWidth;

        auto getSectionContentWidth = [groupInset] (int sectionWidth)
        {
            return juce::jmax (72, sectionWidth - groupInset * 2);
        };

        const int arrangementContentWidth = getSectionContentWidth (metrics.useDualColumn ? leftColumnWidthForCalc : dockWidth);
        const int trackContentWidth = getSectionContentWidth (metrics.useDualColumn ? rightColumnWidthForCalc : dockWidth);
        const int clipContentWidth = arrangementContentWidth;
        const int midiContentWidth = trackContentWidth;
        const int audioContentWidth = arrangementContentWidth;
        const int fxContentWidth = trackContentWidth;

        metrics.arrangementPrefHeight = estimateGroupHeight ({
            getAdaptiveButtonRowHeight (arrangementContentWidth, 6, 2),
            getAdaptiveButtonRowHeight (arrangementContentWidth, 6, 2)
        });

        metrics.trackPrefHeight = estimateGroupHeight ({
            getAdaptiveButtonRowHeight (trackContentWidth, 8, 2),
            getAdaptiveButtonRowHeight (trackContentWidth, 6, 2),
            defaultRowHeight,
            defaultRowHeight
        });

        metrics.clipPrefHeight = estimateGroupHeight ({
            getAdaptiveButtonRowHeight (clipContentWidth, 8, 2),
            getAdaptiveButtonRowHeight (clipContentWidth, 6, 2),
            getAdaptiveButtonRowHeight (clipContentWidth, 6, 2)
        });

        const int midiTabRowHeight = defaultRowHeight + 2;
        const bool midiDirectoryPage = midiToolsTabs.getCurrentTabIndex() == 1;
        if (! midiDirectoryPage)
        {
            const bool midiUsesExtraLegatoRow = midiContentWidth < 420;
            metrics.midiPrefHeight = estimateGroupHeight ({
                midiTabRowHeight,
                defaultRowHeight + 2,
                defaultRowHeight + 2,
                midiUsesExtraLegatoRow ? getAdaptiveButtonRowHeight (midiContentWidth, 2, 2) : 0,
                getAdaptiveButtonRowHeight (midiContentWidth, 6, 2),
                getAdaptiveButtonRowHeight (midiContentWidth, 2, 2),
                getAdaptiveButtonRowHeight (midiContentWidth, 4, 2)
            });
        }
        else
        {
            const bool compactDirectoryRows = midiContentWidth < 520;
            const bool compactVoicingRow = midiContentWidth < 430;
            const bool compactActionRows = midiContentWidth < 460;
            metrics.midiPrefHeight = estimateGroupHeight ({
                midiTabRowHeight,
                defaultRowHeight + 2,
                compactDirectoryRows ? (defaultRowHeight * 2 + rowGap) : (defaultRowHeight + 2),
                compactDirectoryRows ? (defaultRowHeight + 2) : 0,
                defaultRowHeight + 2,
                compactVoicingRow ? (defaultRowHeight * 2 + rowGap) : (defaultRowHeight + 2),
                compactVoicingRow ? (defaultRowHeight + 2) : 0,
                defaultRowHeight + 2,
                compactActionRows ? (defaultRowHeight * 2 + rowGap) : getAdaptiveButtonRowHeight (midiContentWidth, 4, 2)
            });
        }

        metrics.audioPrefHeight = estimateGroupHeight ({
            getAdaptiveButtonRowHeight (audioContentWidth, 5, 2),
            getAdaptiveButtonRowHeight (audioContentWidth, 7, 2),
            getAdaptiveButtonRowHeight (audioContentWidth, 4, 2)
        });

        metrics.fxPrefHeight = estimateGroupHeight ({
            defaultRowHeight,
            getAdaptiveButtonRowHeight (fxContentWidth, 8, 2),
            getAdaptiveButtonRowHeight (fxContentWidth, 5, 2)
        });

        metrics.mixerPrefHeight = estimateGroupHeight ({
            defaultRowHeight + 2,
            defaultRowHeight + 2
        });

        auto getColumnHeight = [leftSectionGap] (std::initializer_list<int> heights)
        {
            int total = 0;
            int count = 0;
            for (const int height : heights)
            {
                if (height <= 0)
                    continue;

                total += height;
                ++count;
            }

            return total + juce::jmax (0, count - 1) * leftSectionGap;
        };

        metrics.leftColumnContentHeight = getColumnHeight ({ metrics.arrangementPrefHeight,
                                                             showClipTools ? metrics.clipPrefHeight : 0,
                                                             showAudioTools ? metrics.audioPrefHeight : 0,
                                                             showMixerPanel ? metrics.mixerPrefHeight : 0 });
        metrics.rightColumnContentHeight = getColumnHeight ({ showTrackPanel ? metrics.trackPrefHeight : 0,
                                                              showMidiTools ? metrics.midiPrefHeight : 0,
                                                              showFxPanel ? metrics.fxPrefHeight : 0 });
        metrics.singleColumnContentHeight = getColumnHeight ({ showArrangementPanel ? metrics.arrangementPrefHeight : 0,
                                                               showTrackPanel ? metrics.trackPrefHeight : 0,
                                                               showClipTools ? metrics.clipPrefHeight : 0,
                                                               showMidiTools ? metrics.midiPrefHeight : 0,
                                                               showAudioTools ? metrics.audioPrefHeight : 0,
                                                               showFxPanel ? metrics.fxPrefHeight : 0,
                                                               showMixerPanel ? metrics.mixerPrefHeight : 0 });
        const int sectionContentHeight = metrics.useDualColumn
                                             ? juce::jmax (metrics.leftColumnContentHeight, metrics.rightColumnContentHeight)
                                             : metrics.singleColumnContentHeight;
        const int selectorBlockHeight = panelSelectorHeight + leftSectionGap;
        metrics.totalLeftContentHeight = selectorBlockHeight + sectionContentHeight;
        return metrics;
    };

    auto leftDockMetrics = computeLeftDockMetrics (leftDock.getWidth());
    int maxLeftScrollPx = juce::jmax (0, leftDockMetrics.totalLeftContentHeight - juce::jmax (0, leftDock.getHeight()));
    bool needsLeftDockScroll = maxLeftScrollPx > 0;

    if (needsLeftDockScroll)
    {
        const int scrolledDockWidth = juce::jmax (96, leftDock.getWidth() - leftDockScrollTrackWidth - leftDockScrollPaddingWidth);
        const auto scrolledMetrics = computeLeftDockMetrics (scrolledDockWidth);
        leftDockMetrics = scrolledMetrics;
        maxLeftScrollPx = juce::jmax (0, leftDockMetrics.totalLeftContentHeight - juce::jmax (0, leftDock.getHeight()));
        needsLeftDockScroll = maxLeftScrollPx > 0;
    }

    const bool useDualColumnLeftDock = leftDockMetrics.useDualColumn;
    leftDockUsesDualColumn = useDualColumnLeftDock;
    leftDockDualColumnDividerX = 0;
    const int arrangementPrefHeight = leftDockMetrics.arrangementPrefHeight;
    const int trackPrefHeight = leftDockMetrics.trackPrefHeight;
    const int clipPrefHeight = leftDockMetrics.clipPrefHeight;
    const int midiPrefHeight = leftDockMetrics.midiPrefHeight;
    const int audioPrefHeight = leftDockMetrics.audioPrefHeight;
    const int fxPrefHeight = leftDockMetrics.fxPrefHeight;
    const int mixerPrefHeight = leftDockMetrics.mixerPrefHeight;
    const int leftColumnContentHeight = leftDockMetrics.leftColumnContentHeight;
    const int totalLeftContentHeight = leftDockMetrics.totalLeftContentHeight;

    if (needsLeftDockScroll)
    {
        auto leftDockScrollArea = leftDock.removeFromRight (leftDockScrollTrackWidth);
        if (leftDock.getWidth() > 4)
            leftDock.removeFromRight (leftDockScrollPaddingWidth);
        setBoundsOrHide (leftDockScrollSlider, leftDockScrollArea.reduced (1, 1));
        leftDockScrollSlider.setVisible (true);
    }
    else
    {
        leftDockScrollSlider.setBounds ({});
        leftDockScrollSlider.setVisible (false);
    }

    if (! fullBodyBounds.isEmpty() && ! rightDock.isEmpty())
        leftDockSplitter.setBounds (leftDockSplitX - splitterThickness / 2,
                                    fullBodyBounds.getY(),
                                    splitterThickness,
                                    fullBodyBounds.getHeight());

    leftDockViewportBounds = leftDock;

    leftDockScrollSlider.setRange (0.0, (double) maxLeftScrollPx, 1.0);
    leftDockScrollSlider.setEnabled (needsLeftDockScroll);
    if (! leftDockScrollSlider.isEnabled())
        leftDockScrollSlider.setValue (0.0, juce::dontSendNotification);

    const int leftScrollOffset = juce::jlimit (0, maxLeftScrollPx, roundToInt (leftDockScrollSlider.getValue()));
    if (std::abs ((double) leftScrollOffset - leftDockScrollSlider.getValue()) > 0.5)
        leftDockScrollSlider.setValue ((double) leftScrollOffset, juce::dontSendNotification);

    const int leftViewportTop = leftDock.getY();
    const int leftViewportBottom = leftDock.getBottom();
    juce::Rectangle<int> leftPanelSelectorBounds;
    juce::Rectangle<int> arrangementBounds;
    juce::Rectangle<int> trackBounds;
    juce::Rectangle<int> clipBounds;
    juce::Rectangle<int> midiBounds;
    juce::Rectangle<int> audioBounds;
    juce::Rectangle<int> fxBounds;
    juce::Rectangle<int> mixerControlsBounds;

    if (useDualColumnLeftDock)
    {
        const int columnGap = dualColumnGap;
        const int leftColumnWidth = juce::jmax (140, (leftDock.getWidth() - columnGap) / 2);
        const int rightColumnWidth = juce::jmax (140, leftDock.getWidth() - leftColumnWidth - columnGap);
        const int contentTop = leftViewportTop - leftScrollOffset;
        leftPanelSelectorBounds = { leftDock.getX(), contentTop, leftDock.getWidth(), panelSelectorHeight };
        const int columnsTop = contentTop + panelSelectorHeight + leftSectionGap;
        const int columnsContentHeight = juce::jmax (leftDock.getHeight(),
                                                     juce::jmax (0, totalLeftContentHeight - panelSelectorHeight - leftSectionGap));

        juce::Rectangle<int> leftColumn (leftDock.getX(), columnsTop, leftColumnWidth, columnsContentHeight);
        juce::Rectangle<int> rightColumn (leftColumn.getRight() + columnGap, columnsTop, rightColumnWidth, columnsContentHeight);
        leftDockDualColumnDividerX = leftColumn.getRight() + (columnGap / 2);
        int leftY = leftColumn.getY();
        int rightY = rightColumn.getY();

        auto takeColumnSection = [leftSectionGap] (const juce::Rectangle<int>& column, int& y, int sectionHeight, bool visible)
        {
            if (! visible || sectionHeight <= 0)
                return juce::Rectangle<int>();

            juce::Rectangle<int> section (column.getX(), y, column.getWidth(), sectionHeight);
            y += sectionHeight + leftSectionGap;
            return section;
        };

        const int columnViewportHeight = juce::jmax (0, leftDock.getHeight() - panelSelectorHeight - leftSectionGap);
        const int expandedMixerHeight = mixerPrefHeight
            + juce::jmax (0, juce::jmax (0, columnViewportHeight) - leftColumnContentHeight);

        arrangementBounds = takeColumnSection (leftColumn, leftY, arrangementPrefHeight, showArrangementPanel);
        trackBounds = takeColumnSection (rightColumn, rightY, trackPrefHeight, showTrackPanel);
        clipBounds = takeColumnSection (leftColumn, leftY, clipPrefHeight, showClipTools);
        midiBounds = takeColumnSection (rightColumn, rightY, midiPrefHeight, showMidiTools);
        audioBounds = takeColumnSection (leftColumn, leftY, audioPrefHeight, showAudioTools);
        fxBounds = takeColumnSection (rightColumn, rightY, fxPrefHeight, showFxPanel);
        mixerControlsBounds = takeColumnSection (leftColumn, leftY, expandedMixerHeight, showMixerPanel);
    }
    else
    {
        leftPanelSelectorBounds = { leftDock.getX(), leftViewportTop - leftScrollOffset, leftDock.getWidth(), panelSelectorHeight };
        int leftSectionY = leftPanelSelectorBounds.getBottom() + leftSectionGap;

        auto takeLeftSection = [&leftSectionY, leftSectionGap, &leftDock] (int sectionHeight, bool visible)
        {
            if (! visible || sectionHeight <= 0)
                return juce::Rectangle<int>();

            juce::Rectangle<int> section (leftDock.getX(), leftSectionY, leftDock.getWidth(), sectionHeight);
            leftSectionY += sectionHeight + leftSectionGap;
            return section;
        };

        const int sectionViewportHeight = juce::jmax (0, leftDock.getHeight() - panelSelectorHeight - leftSectionGap);
        const int expandedMixerHeight = mixerPrefHeight
            + juce::jmax (0, juce::jmax (0, sectionViewportHeight)
                             - juce::jmax (0, totalLeftContentHeight - panelSelectorHeight - leftSectionGap));

        arrangementBounds = takeLeftSection (arrangementPrefHeight, showArrangementPanel);
        trackBounds = takeLeftSection (trackPrefHeight, showTrackPanel);
        clipBounds = takeLeftSection (clipPrefHeight, showClipTools);
        midiBounds = takeLeftSection (midiPrefHeight, showMidiTools);
        audioBounds = takeLeftSection (audioPrefHeight, showAudioTools);
        fxBounds = takeLeftSection (fxPrefHeight, showFxPanel);
        mixerControlsBounds = takeLeftSection (expandedMixerHeight, showMixerPanel);
    }

    auto clipToLeftViewport = [leftViewportTop, leftViewportBottom] (juce::Rectangle<int> section)
    {
        if (section.isEmpty())
            return section;

        const int y = juce::jmax (section.getY(), leftViewportTop);
        const int bottom = juce::jmin (section.getBottom(), leftViewportBottom);
        if (bottom <= y)
            return juce::Rectangle<int>();

        section.setY (y);
        section.setHeight (bottom - y);
        return section;
    };

    leftPanelSelectorBounds = clipToLeftViewport (leftPanelSelectorBounds);
    arrangementBounds = clipToLeftViewport (arrangementBounds);
    trackBounds = clipToLeftViewport (trackBounds);
    clipBounds = clipToLeftViewport (clipBounds);
    midiBounds = clipToLeftViewport (midiBounds);
    audioBounds = clipToLeftViewport (audioBounds);
    fxBounds = clipToLeftViewport (fxBounds);
    mixerControlsBounds = clipToLeftViewport (mixerControlsBounds);

    if (! leftPanelSelectorBounds.isEmpty())
    {
        auto selectorRow = leftPanelSelectorBounds.reduced (0, 1);
        leftDockPanelTabs.setBounds (selectorRow);
        leftDockPanelModeLabel.setBounds ({});
        leftDockPanelModeBox.setBounds ({});
    }
    else
    {
        leftDockPanelTabs.setBounds ({});
        leftDockPanelModeLabel.setBounds ({});
        leftDockPanelModeBox.setBounds ({});
    }

    auto arrangementArea = getGroupContent (arrangementGroup, arrangementBounds);
    const int arrangementRowHeight = getAdaptiveButtonRowHeight (arrangementArea.getWidth(), 6, 2);
    layoutButtonRow (nextRow (arrangementArea, arrangementRowHeight), { &showMarkerTrackButton, &showArrangerTrackButton, &addMarkerButton,
                                                   &prevMarkerButton, &nextMarkerButton, &loopMarkersButton });
    layoutButtonRow (nextRow (arrangementArea, arrangementRowHeight), { &addSectionButton, &prevSectionButton, &nextSectionButton,
                                                   &loopSectionButton, &jumpPrevBarButton, &jumpNextBarButton });

    auto trackArea = getGroupContent (trackGroup, trackBounds);
    const int trackManageRowHeight = getAdaptiveButtonRowHeight (trackArea.getWidth(), 8, 2);
    const int trackImportRowHeight = getAdaptiveButtonRowHeight (trackArea.getWidth(), 6, 2);
    layoutButtonRow (nextRow (trackArea, trackManageRowHeight), { &addTrackButton, &addMidiTrackButton, &addFloatingSynthTrackButton,
                                            &moveTrackUpButton, &moveTrackDownButton, &duplicateTrackButton,
                                            &colorTrackButton, &renameTrackButton });
    layoutButtonRow (nextRow (trackArea, trackImportRowHeight), { &importAudioButton, &importMidiButton, &createMidiClipButton,
                                            &splitAllTracksButton, &insertBarButton, &deleteBarButton });
    auto editToolRow = nextRow (trackArea);
    editToolLabel.setBounds (editToolRow.removeFromLeft (66).reduced (0, 1));
    editToolRow.removeFromLeft (6);
    layoutButtonRow (editToolRow, { &editToolSelectButton, &editToolPencilButton, &editToolScissorsButton, &editToolResizeButton });
    auto defaultSynthRow = nextRow (trackArea);
    defaultSynthModeLabel.setBounds (defaultSynthRow.removeFromLeft (96).reduced (0, 1));
    defaultSynthRow.removeFromLeft (6);
    defaultSynthModeBox.setBounds (defaultSynthRow.reduced (0, 1));

    if (showClipTools)
    {
        auto clipArea = getGroupContent (clipEditGroup, clipBounds);
        const int clipRow1Height = getAdaptiveButtonRowHeight (clipArea.getWidth(), 8, 2);
        const int clipRow2Height = getAdaptiveButtonRowHeight (clipArea.getWidth(), 6, 2);
        layoutButtonRow (nextRow (clipArea, clipRow1Height), { &copyButton, &cutButton, &pasteButton, &deleteButton,
                                               &duplicateButton, &splitButton, &selectAllButton, &deselectAllButton });
        layoutButtonRow (nextRow (clipArea, clipRow2Height), { &trimStartButton, &trimEndButton, &moveStartToCursorButton,
                                               &moveEndToCursorButton, &nudgeLeftButton, &nudgeRightButton });
        layoutButtonRow (nextRow (clipArea, clipRow2Height), { &slipLeftButton, &slipRightButton, &moveToPrevButton,
                                               &moveToNextButton, &toggleClipLoopButton, &renameClipButton });
    }
    else
    {
        clipEditGroup.setBounds ({});
    }

    if (showMidiTools)
    {
        auto midiArea = getGroupContent (midiEditGroup, midiBounds);
        midiToolsTabs.setBounds (nextRow (midiArea, defaultRowHeight + 2).reduced (0, 1));

        const bool midiDirectoryPage = midiToolsTabs.getCurrentTabIndex() == 1;
        if (! midiDirectoryPage)
        {
            auto midiRow1 = nextRow (midiArea);
            if (midiRow1.getWidth() < 420)
            {
                auto left = midiRow1.removeFromLeft (juce::jmax (110, midiRow1.getWidth() / 2));
                quantizeTypeBox.setBounds (left.removeFromLeft (juce::jmax (72, left.getWidth() - 76)).reduced (0, 1));
                left.removeFromLeft (4);
                quantizeButton.setBounds (left.reduced (0, 1));

                gridLabel.setBounds (midiRow1.removeFromLeft (34).reduced (0, 1));
                midiRow1.removeFromLeft (4);
                gridBox.setBounds (midiRow1.reduced (0, 1));

                const int midiUtilityRowHeight = getAdaptiveButtonRowHeight (midiArea.getWidth(), 2, 2);
                layoutButtonRow (nextRow (midiArea, midiUtilityRowHeight), { &midiLegatoButton, &midiBounceToAudioButton });
            }
            else
            {
                const int quantizeTypeWidth = juce::jmax (96, midiRow1.getWidth() / 4);
                quantizeTypeBox.setBounds (midiRow1.removeFromLeft (quantizeTypeWidth).reduced (0, 1));
                midiRow1.removeFromLeft (6);
                quantizeButton.setBounds (midiRow1.removeFromLeft (juce::jmax (72, midiRow1.getWidth() / 6)).reduced (0, 1));
                midiRow1.removeFromLeft (8);
                gridLabel.setBounds (midiRow1.removeFromLeft (34).reduced (0, 1));
                midiRow1.removeFromLeft (6);
                gridBox.setBounds (midiRow1.removeFromLeft (juce::jmax (78, midiRow1.getWidth() / 4)).reduced (0, 1));
                midiRow1.removeFromLeft (6);
                const int legatoWidth = juce::jmax (0, (midiRow1.getWidth() - 6) / 2);
                midiLegatoButton.setBounds (midiRow1.removeFromLeft (legatoWidth).reduced (0, 1));
                if (midiRow1.getWidth() > 6)
                    midiRow1.removeFromLeft (6);
                midiBounceToAudioButton.setBounds (midiRow1.reduced (0, 1));
            }

            layoutButtonRow (nextRow (midiArea, getAdaptiveButtonRowHeight (midiArea.getWidth(), 6, 2)), { &midiTransposeDownButton, &midiTransposeUpButton,
                                                   &midiOctaveDownButton, &midiOctaveUpButton,
                                                   &midiVelocityDownButton, &midiVelocityUpButton });
            layoutButtonRow (nextRow (midiArea, getAdaptiveButtonRowHeight (midiArea.getWidth(), 2, 2)), { &midiHumanizeTimingButton, &midiHumanizeVelocityButton });
            layoutButtonRow (nextRow (midiArea, getAdaptiveButtonRowHeight (midiArea.getWidth(), 4, 2)), { &midiGenerateChordsButton, &midiGenerateArpButton,
                                                   &midiGenerateBassButton, &midiGenerateDrumsButton });
        }
        else
        {
            auto layoutLabelCombo = [] (juce::Rectangle<int>& row, juce::Label& label, juce::ComboBox& combo, int labelWidth)
            {
                label.setBounds (row.removeFromLeft (labelWidth).reduced (0, 1));
                row.removeFromLeft (4);
                combo.setBounds (row.reduced (0, 1));
            };

            auto layoutLabelSlider = [] (juce::Rectangle<int>& row, juce::Label& label, juce::Slider& slider, int labelWidth)
            {
                label.setBounds (row.removeFromLeft (labelWidth).reduced (0, 1));
                row.removeFromLeft (4);
                slider.setBounds (row.reduced (0, 1));
            };

            const bool compactDirectoryRows = midiArea.getWidth() < 520;

            if (compactDirectoryRows)
            {
                auto rowA = nextRow (midiArea, defaultRowHeight + 2);
                auto rowALeft = rowA.removeFromLeft (juce::jmax (160, rowA.getWidth() / 2));
                rowA.removeFromLeft (6);
                layoutLabelCombo (rowALeft, chordDirectoryRootLabel, chordDirectoryRootBox, 46);
                layoutLabelCombo (rowA, chordDirectoryScaleLabel, chordDirectoryScaleBox, 46);

                auto rowB = nextRow (midiArea, defaultRowHeight + 2);
                layoutLabelCombo (rowB, chordDirectoryProgressionLabel, chordDirectoryProgressionBox, 90);
            }
            else
            {
                auto rowA = nextRow (midiArea, defaultRowHeight + 2);
                auto keyCell = rowA.removeFromLeft (juce::jmax (130, rowA.getWidth() / 5));
                rowA.removeFromLeft (6);
                auto scaleCell = rowA.removeFromLeft (juce::jmax (170, rowA.getWidth() / 3));
                rowA.removeFromLeft (6);
                auto progressionCell = rowA;
                layoutLabelCombo (keyCell, chordDirectoryRootLabel, chordDirectoryRootBox, 46);
                layoutLabelCombo (scaleCell, chordDirectoryScaleLabel, chordDirectoryScaleBox, 46);
                layoutLabelCombo (progressionCell, chordDirectoryProgressionLabel, chordDirectoryProgressionBox, 92);
            }

            {
                auto rowB = nextRow (midiArea, defaultRowHeight + 2);
                auto barsCell = rowB.removeFromLeft (juce::jmax (112, rowB.getWidth() / 4));
                rowB.removeFromLeft (6);
                auto sigCell = rowB.removeFromLeft (juce::jmax (138, rowB.getWidth() / 3));
                rowB.removeFromLeft (6);
                auto octCell = rowB;
                layoutLabelCombo (barsCell, chordDirectoryBarsLabel, chordDirectoryBarsBox, 42);
                layoutLabelCombo (sigCell, chordDirectoryTimeSignatureLabel, chordDirectoryTimeSignatureBox, 58);
                layoutLabelCombo (octCell, chordDirectoryOctaveLabel, chordDirectoryOctaveBox, 54);
            }

            {
                auto rowC = nextRow (midiArea, defaultRowHeight + 2);
                if (rowC.getWidth() < 430)
                {
                    auto leftCell = rowC.removeFromLeft (juce::jmax (128, rowC.getWidth() / 2));
                    rowC.removeFromLeft (6);
                    layoutLabelCombo (leftCell, chordDirectoryVoicingLabel, chordDirectoryVoicingBox, 58);
                    layoutLabelCombo (rowC, chordDirectoryDensityLabel, chordDirectoryDensityBox, 58);

                    auto rowCPreset = nextRow (midiArea, defaultRowHeight + 2);
                    layoutLabelCombo (rowCPreset, chordDirectoryPreviewPresetLabel, chordDirectoryPreviewPresetBox, 96);
                }
                else
                {
                    auto voicingCell = rowC.removeFromLeft (juce::jmax (132, rowC.getWidth() / 3));
                    rowC.removeFromLeft (6);
                    auto densityCell = rowC.removeFromLeft (juce::jmax (132, rowC.getWidth() / 3));
                    rowC.removeFromLeft (6);
                    auto presetCell = rowC;
                    layoutLabelCombo (voicingCell, chordDirectoryVoicingLabel, chordDirectoryVoicingBox, 58);
                    layoutLabelCombo (densityCell, chordDirectoryDensityLabel, chordDirectoryDensityBox, 58);
                    layoutLabelCombo (presetCell, chordDirectoryPreviewPresetLabel, chordDirectoryPreviewPresetBox, 96);
                }
            }

            {
                auto rowD = nextRow (midiArea, defaultRowHeight + 2);
                auto velocityCell = rowD.removeFromLeft (juce::jmax (150, rowD.getWidth() / 2));
                rowD.removeFromLeft (6);
                auto swingCell = rowD;
                layoutLabelSlider (velocityCell, chordDirectoryVelocityLabel, chordDirectoryVelocitySlider, 64);
                layoutLabelSlider (swingCell, chordDirectorySwingLabel, chordDirectorySwingSlider, 54);
            }

            const bool compactActionRows = midiArea.getWidth() < 460;
            if (compactActionRows)
            {
                layoutButtonRow (nextRow (midiArea, getAdaptiveButtonRowHeight (midiArea.getWidth(), 2, 2)),
                                 { &chordDirectoryPreviewButton, &chordDirectoryApplyButton });
                layoutButtonRow (nextRow (midiArea, getAdaptiveButtonRowHeight (midiArea.getWidth(), 2, 2)),
                                 { &chordDirectoryExportMidiButton, &chordDirectoryExportWavButton });
            }
            else
            {
                layoutButtonRow (nextRow (midiArea, getAdaptiveButtonRowHeight (midiArea.getWidth(), 4, 2)),
                                 { &chordDirectoryPreviewButton, &chordDirectoryApplyButton,
                                   &chordDirectoryExportMidiButton, &chordDirectoryExportWavButton });
            }
        }
    }
    else
    {
        midiEditGroup.setBounds ({});
        midiToolsTabs.setBounds ({});
    }

    if (showAudioTools)
    {
        auto audioArea = getGroupContent (audioEditGroup, audioBounds);
        layoutButtonRow (nextRow (audioArea, getAdaptiveButtonRowHeight (audioArea.getWidth(), 5, 2)), { &audioGainDownButton, &audioGainUpButton, &audioFadeInButton,
                                                &audioFadeOutButton, &audioClearFadesButton });
        layoutButtonRow (nextRow (audioArea, getAdaptiveButtonRowHeight (audioArea.getWidth(), 7, 2)), { &audioReverseButton, &audioSpeedDownButton, &audioSpeedUpButton,
                                                &audioPitchDownButton, &audioPitchUpButton, &audioAutoTempoButton, &audioWarpButton });
        layoutButtonRow (nextRow (audioArea, getAdaptiveButtonRowHeight (audioArea.getWidth(), 4, 2)), { &audioAlignToBarButton, &audioMake2BarLoopButton,
                                                &audioMake4BarLoopButton, &audioFillTransportLoopButton });
    }
    else
    {
        audioEditGroup.setBounds ({});
    }

    auto fxArea = getGroupContent (fxGroup, fxBounds);
    auto fxRow1 = nextRow (fxArea);
    fxChainLabel.setBounds (fxRow1.removeFromLeft (54).reduced (0, 1));
    fxRow1.removeFromLeft (6);
    auto fxActions = fxRow1.removeFromRight (juce::jmax (140, fxRow1.getWidth() / 2));
    fxChainBox.setBounds (fxRow1.reduced (0, 1));
    const int actionGap = 6;
    const int actionWidth = juce::jmax (0, (fxActions.getWidth() - actionGap * 3) / 4);
    fxRefreshButton.setBounds (fxActions.removeFromLeft (actionWidth).reduced (0, 1));
    fxActions.removeFromLeft (actionGap);
    fxScanButton.setBounds (fxActions.removeFromLeft (actionWidth).reduced (0, 1));
    fxActions.removeFromLeft (actionGap);
    fxScanSkippedButton.setBounds (fxActions.removeFromLeft (actionWidth).reduced (0, 1));
    fxActions.removeFromLeft (actionGap);
    fxPrepPlaybackButton.setBounds (fxActions.reduced (0, 1));
    layoutButtonRow (nextRow (fxArea, getAdaptiveButtonRowHeight (fxArea.getWidth(), 8, 2)), { &fxAddSamplerButton, &fxAddSynthButton, &fxAddEQButton,
                                         &fxAddCompButton, &fxAddReverbButton, &fxAddDelayButton,
                                         &fxAddExternalInstrumentButton, &fxAddExternalButton });
    layoutButtonRow (nextRow (fxArea, getAdaptiveButtonRowHeight (fxArea.getWidth(), 5, 2)), { &fxOpenEditorButton, &fxMoveUpButton, &fxMoveDownButton, &fxBypassButton, &fxDeleteButton });

    auto mixerControlsArea = getGroupContent (mixerGroup, mixerControlsBounds);
    auto mixerRow1 = nextRow (mixerControlsArea, defaultRowHeight + 2);
    selectedTrackLabel.setBounds (mixerRow1.removeFromLeft (juce::jmax (120, mixerRow1.getWidth() / 2)).reduced (0, 1));
    mixerRow1.removeFromLeft (6);
    trackMuteButton.setBounds (mixerRow1.removeFromLeft (64).reduced (0, 1));
    mixerRow1.removeFromLeft (6);
    trackSoloButton.setBounds (mixerRow1.removeFromLeft (64).reduced (0, 1));
    mixerRow1.removeFromLeft (10);
    trackHeightLabel.setBounds (mixerRow1.removeFromLeft (58).reduced (0, 1));
    mixerRow1.removeFromLeft (6);
    trackHeightSlider.setBounds (mixerRow1.reduced (0, 1));

    auto mixerRow2 = nextRow (mixerControlsArea, defaultRowHeight + 2);
    trackVolumeLabel.setBounds (mixerRow2.removeFromLeft (34).reduced (0, 1));
    mixerRow2.removeFromLeft (6);
    trackVolumeSlider.setBounds (mixerRow2.removeFromLeft (juce::jmax (96, mixerRow2.getWidth() / 2)).reduced (0, 1));
    mixerRow2.removeFromLeft (6);
    trackPanLabel.setBounds (mixerRow2.removeFromLeft (32).reduced (0, 1));
    mixerRow2.removeFromLeft (6);
    trackPanSlider.setBounds (mixerRow2.reduced (0, 1));

    const bool dockWorkspace = ! isSectionFloating (FloatSection::workspace);
    const bool dockMixer = ! isSectionFloating (FloatSection::mixer);
    const bool dockPiano = ! isSectionFloating (FloatSection::piano);

    juce::Rectangle<int> workspaceBounds;
    juce::Rectangle<int> mixerBounds;
    juce::Rectangle<int> pianoBounds;

    auto rightDockArea = rightDock;
    const bool needsBottomPane = dockMixer || dockPiano;
    const bool separateMixerPane = dockWorkspace && dockMixer
                                   && rightDockArea.getWidth() >= roundToInt (1180.0f * uiScale);

    if (separateMixerPane)
    {
        currentRightDockWidthForResize = rightDockArea.getWidth();
        const int minWorkspaceWidth = juce::jmax (260, rightDockArea.getWidth() / 3);
        const int minMixerWidth = juce::jmax (260, rightDockArea.getWidth() / 3);
        const int maxWorkspaceWidth = juce::jmax (minWorkspaceWidth, rightDockArea.getWidth() - minMixerWidth);
        const int workspaceWidth = juce::jlimit (minWorkspaceWidth,
                                                 maxWorkspaceWidth,
                                                 roundToInt ((float) rightDockArea.getWidth() * workspaceMixerWidthRatio));
        workspaceMixerWidthRatio = (float) workspaceWidth / (float) juce::jmax (1, currentRightDockWidthForResize);

        auto workspaceColumn = rightDockArea.removeFromLeft (workspaceWidth);
        if (rightDockArea.getWidth() > 8)
            rightDockArea.removeFromLeft (8);
        mixerBounds = rightDockArea;

        if (! workspaceColumn.isEmpty() && ! mixerBounds.isEmpty())
            workspaceMixerSplitter.setBounds (workspaceColumn.getRight(),
                                              workspaceColumn.getY(),
                                              8,
                                              workspaceColumn.getHeight());

        if (dockPiano)
        {
            currentRightDockHeightForResize = workspaceColumn.getHeight();
            const int minBottomHeight = juce::jmax (210, workspaceColumn.getHeight() / 3);
            const int minWorkspaceHeight = juce::jmax (200, workspaceColumn.getHeight() / 3);
            const int maxBottomHeight = juce::jmax (minBottomHeight, workspaceColumn.getHeight() - minWorkspaceHeight);
            const int bottomPanelHeight = juce::jlimit (minBottomHeight,
                                                        maxBottomHeight,
                                                        roundToInt ((float) workspaceColumn.getHeight() * workspaceBottomHeightRatio));
            workspaceBottomHeightRatio = (float) bottomPanelHeight / (float) juce::jmax (1, workspaceColumn.getHeight());

            auto bottomWorkspace = workspaceColumn.removeFromBottom (bottomPanelHeight);
            if (workspaceColumn.getHeight() > 8)
                workspaceColumn.removeFromBottom (8);
            workspaceBounds = workspaceColumn;
            pianoBounds = bottomWorkspace;

            if (! workspaceBounds.isEmpty())
                workspaceBottomSplitter.setBounds (workspaceBounds.getX(),
                                                   workspaceBounds.getBottom(),
                                                   workspaceBounds.getWidth(),
                                                   8);
        }
        else
        {
            workspaceBounds = workspaceColumn;
        }
    }
    else if (dockWorkspace && needsBottomPane)
    {
        currentRightDockHeightForResize = rightDockArea.getHeight();
        const int minBottomHeight = juce::jmax (180, rightDockArea.getHeight() / 3);
        const int minWorkspaceHeight = juce::jmax (170, rightDockArea.getHeight() / 3);
        const int maxBottomHeight = juce::jmax (minBottomHeight, rightDockArea.getHeight() - minWorkspaceHeight);
        const int bottomPanelHeight = juce::jlimit (minBottomHeight,
                                                    maxBottomHeight,
                                                    roundToInt ((float) rightDockArea.getHeight() * workspaceBottomHeightRatio));
        workspaceBottomHeightRatio = (float) bottomPanelHeight / (float) juce::jmax (1, rightDockArea.getHeight());
        auto bottomRight = rightDockArea.removeFromBottom (bottomPanelHeight);
        rightDockArea.removeFromBottom (8);
        workspaceBounds = rightDockArea;

        if (! workspaceBounds.isEmpty())
            workspaceBottomSplitter.setBounds (workspaceBounds.getX(),
                                               workspaceBounds.getBottom(),
                                               workspaceBounds.getWidth(),
                                               8);

        if (dockMixer && dockPiano)
        {
            currentBottomDockHeightForResize = bottomRight.getHeight();
            const int splitterHeight = 8;
            const int availableHeight = juce::jmax (0, bottomRight.getHeight() - splitterHeight);
            const int minPianoPriorityHeight = denseLayout ? 170 : 210;
            const int minMixerHeight = juce::jmax (70, juce::jmin (120, availableHeight / 3));
            const int minimumSplitHeight = minPianoPriorityHeight + minMixerHeight;

            if (availableHeight < minimumSplitHeight)
            {
                mixerBounds = {};
                pianoBounds = bottomRight;
                mixerPianoSplitter.setBounds ({});
            }
            else
            {
                const int minPianoHeight = juce::jlimit (120,
                                                         juce::jmax (120, availableHeight - minMixerHeight),
                                                         minPianoPriorityHeight);
                const int maxMixerHeight = juce::jmax (minMixerHeight, availableHeight - minPianoHeight);
                const int mixerHeight = juce::jlimit (minMixerHeight,
                                                      maxMixerHeight,
                                                      roundToInt ((float) availableHeight * mixerPianoHeightRatio));

                mixerBounds = bottomRight.removeFromTop (mixerHeight);
                mixerPianoHeightRatio = (float) mixerBounds.getHeight() / (float) juce::jmax (1, availableHeight);
                if (bottomRight.getHeight() > splitterHeight)
                    bottomRight.removeFromTop (splitterHeight);
                pianoBounds = bottomRight;

                if (! mixerBounds.isEmpty() && ! pianoBounds.isEmpty())
                {
                    mixerPianoSplitter.setBounds (mixerBounds.getX(),
                                                  mixerBounds.getBottom(),
                                                  mixerBounds.getWidth(),
                                                  splitterHeight);
                }
                else
                {
                    mixerPianoSplitter.setBounds ({});
                }
            }
        }
        else if (dockMixer)
        {
            mixerBounds = bottomRight;
        }
        else
        {
            pianoBounds = bottomRight;
        }
    }
    else if (dockWorkspace)
    {
        workspaceBounds = rightDockArea;
    }
    else if (needsBottomPane)
    {
        if (dockMixer && dockPiano)
        {
            currentBottomDockHeightForResize = rightDockArea.getHeight();
            const int splitterHeight = 8;
            const int availableHeight = juce::jmax (0, rightDockArea.getHeight() - splitterHeight);
            const int minPianoPriorityHeight = denseLayout ? 170 : 210;
            const int minMixerHeight = juce::jmax (70, juce::jmin (120, availableHeight / 3));
            const int minimumSplitHeight = minPianoPriorityHeight + minMixerHeight;

            if (availableHeight < minimumSplitHeight)
            {
                mixerBounds = {};
                pianoBounds = rightDockArea;
                mixerPianoSplitter.setBounds ({});
            }
            else
            {
                const int minPianoHeight = juce::jlimit (120,
                                                         juce::jmax (120, availableHeight - minMixerHeight),
                                                         minPianoPriorityHeight);
                const int maxMixerHeight = juce::jmax (minMixerHeight, availableHeight - minPianoHeight);
                const int mixerHeight = juce::jlimit (minMixerHeight,
                                                      maxMixerHeight,
                                                      roundToInt ((float) availableHeight * mixerPianoHeightRatio));

                mixerBounds = rightDockArea.removeFromTop (mixerHeight);
                mixerPianoHeightRatio = (float) mixerBounds.getHeight() / (float) juce::jmax (1, availableHeight);
                if (rightDockArea.getHeight() > splitterHeight)
                    rightDockArea.removeFromTop (splitterHeight);
                pianoBounds = rightDockArea;

                if (! mixerBounds.isEmpty() && ! pianoBounds.isEmpty())
                {
                    mixerPianoSplitter.setBounds (mixerBounds.getX(),
                                                  mixerBounds.getBottom(),
                                                  mixerBounds.getWidth(),
                                                  splitterHeight);
                }
                else
                {
                    mixerPianoSplitter.setBounds ({});
                }
            }
        }
        else if (dockMixer)
        {
            mixerBounds = rightDockArea;
        }
        else
        {
            pianoBounds = rightDockArea;
        }
    }

    auto ensureDockAttachment = [this] (SectionContainer& section, bool shouldBeDocked)
    {
        if (shouldBeDocked)
        {
            if (section.getParentComponent() != this)
                addAndMakeVisible (section);
        }
        else if (section.getParentComponent() == this)
        {
            removeChildComponent (&section);
        }
    };

    ensureDockAttachment (workspaceSection, dockWorkspace);
    ensureDockAttachment (mixerSection, dockMixer);
    ensureDockAttachment (pianoSection, dockPiano);

    if (dockWorkspace)
        workspaceSection.setBounds (workspaceBounds);
    else if (workspaceSection.getParentComponent() == this)
        workspaceSection.setBounds ({});

    if (dockMixer)
        mixerSection.setBounds (mixerBounds);
    else if (mixerSection.getParentComponent() == this)
        mixerSection.setBounds ({});

    if (dockPiano)
        pianoSection.setBounds (pianoBounds);
    else if (pianoSection.getParentComponent() == this)
        pianoSection.setBounds ({});

    if (workspaceSection.getParentComponent() != nullptr && ! workspaceSection.getLocalBounds().isEmpty())
        layoutSectionContent (FloatSection::workspace, workspaceSection.getLocalBounds());

    if (mixerSection.getParentComponent() != nullptr && ! mixerSection.getLocalBounds().isEmpty())
        layoutSectionContent (FloatSection::mixer, mixerSection.getLocalBounds());

    if (pianoSection.getParentComponent() != nullptr && ! pianoSection.getLocalBounds().isEmpty())
        layoutSectionContent (FloatSection::piano, pianoSection.getLocalBounds());

    leftDockSplitter.toFront (false);
    workspaceMixerSplitter.toFront (false);
    workspaceBottomSplitter.toFront (false);
    mixerPianoSplitter.toFront (false);

    syncViewControlsFromState();
}

void BeatMakerNoRecord::layoutSectionContent (FloatSection section, juce::Rectangle<int> bounds)
{
    const float uiScale = getUiDensityScale();
    const auto densityMode = uiDensityMode;
    const bool compactLayout = bounds.getWidth() < roundToInt (980.0f * uiScale)
                               || bounds.getHeight() < roundToInt (520.0f * uiScale);
    const bool denseLayout = bounds.getWidth() < roundToInt (820.0f * uiScale)
                             || bounds.getHeight() < roundToInt (420.0f * uiScale);
    int defaultRowHeight = denseLayout ? 25 : (compactLayout ? 26 : 28);
    int rowGap = denseLayout ? 3 : 5;
    int groupInset = denseLayout ? 8 : 10;
    int groupTitleInset = denseLayout ? 18 : 20;

    if (densityMode == UiDensityMode::compact)
    {
        defaultRowHeight = juce::jmax (23, defaultRowHeight - 2);
        rowGap = juce::jmax (2, rowGap - 1);
        groupInset = juce::jmax (7, groupInset - 1);
    }
    else if (densityMode == UiDensityMode::accessible)
    {
        defaultRowHeight += 3;
        rowGap += 1;
        groupInset += 1;
        groupTitleInset += 2;
    }

    auto getGroupContent = [groupInset, groupTitleInset] (juce::GroupComponent& group, juce::Rectangle<int> area)
    {
        group.setBounds (area);

        if (area.getWidth() <= 0 || area.getHeight() <= 0)
            return juce::Rectangle<int>();

        auto inner = area.reduced (groupInset);
        if (inner.getHeight() <= groupTitleInset)
            return juce::Rectangle<int>();

        inner.removeFromTop (groupTitleInset);
        return inner;
    };

    auto getGroupContentPlain = [groupInset] (juce::GroupComponent& group, juce::Rectangle<int> area)
    {
        group.setBounds (area);

        if (area.getWidth() <= 0 || area.getHeight() <= 0)
            return juce::Rectangle<int>();

        return area.reduced (groupInset);
    };

    auto nextRow = [defaultRowHeight, rowGap] (juce::Rectangle<int>& area, int height = 0)
    {
        const int targetHeight = juce::jmax (0, juce::jmin (height > 0 ? height : defaultRowHeight, area.getHeight()));
        auto row = area.removeFromTop (targetHeight);
        if (area.getHeight() > 0)
            area.removeFromTop (juce::jmin (rowGap, area.getHeight()));
        return row;
    };

    switch (section)
    {
        case FloatSection::workspace:
        {
            auto workspaceArea = getGroupContent (workspaceGroup, bounds);
            auto rulerRow = nextRow (workspaceArea, denseLayout ? 22 : 24);
            timelineRuler.setBounds (rulerRow.reduced (0, 1));
            workspaceArea.removeFromTop (3);

            const int minToolbarHeight = denseLayout ? 24 : 26;
            const int maxToolbarHeight = denseLayout ? 40 : 44;
            const int toolbarHeight = juce::jlimit (minToolbarHeight,
                                                    maxToolbarHeight,
                                                    juce::jmax (minToolbarHeight, workspaceArea.getHeight() / 11));
            auto toolbarRow = nextRow (workspaceArea, toolbarHeight);
            trackAreaToolbar.setBounds (toolbarRow.reduced (0, 1));

            auto editFrame = workspaceArea;
            auto horizontalRow = editFrame.removeFromBottom (12);
            auto verticalCol = editFrame.removeFromRight (12);
            lastEditViewportBounds = editFrame;

            if (editComponent != nullptr)
                editComponent->setBounds (editFrame);

            verticalScrollSlider.setBounds (verticalCol.reduced (1));

            const int headerWidth = getHeaderWidth();
            const int footerWidth = getFooterWidth();
            const int contentX = editFrame.getX() + headerWidth;
            const int contentWidth = juce::jmax (16, editFrame.getWidth() - headerWidth - footerWidth);

            horizontalScrollSlider.setBounds (contentX, horizontalRow.getY(), contentWidth, horizontalRow.getHeight());

            auto rulerBounds = timelineRuler.getBounds();
            rulerBounds.setX (contentX);
            rulerBounds.setWidth (contentWidth);
            timelineRuler.setBounds (rulerBounds);
            break;
        }

        case FloatSection::mixer:
        {
            auto mixerRegion = bounds;
            const int minRackSectionHeight = juce::jmax (120, mixerRegion.getHeight() / 3);
            const int maxRackSectionHeight = juce::jmax (minRackSectionHeight, mixerRegion.getHeight() - 120);
            const int rackSectionHeight = juce::jlimit (minRackSectionHeight, maxRackSectionHeight, roundToInt (mixerRegion.getHeight() * 0.72f));
            auto rackRegion = mixerRegion.removeFromBottom (rackSectionHeight);
            if (mixerRegion.getHeight() > 8)
                mixerRegion.removeFromBottom (8);

            auto mixerContent = getGroupContent (mixerAreaGroup, mixerRegion);
            const int minMixerToolbarHeight = denseLayout ? 24 : 26;
            const int maxMixerToolbarHeight = denseLayout ? 40 : 44;
            const int mixerToolbarHeight = juce::jlimit (minMixerToolbarHeight,
                                                         maxMixerToolbarHeight,
                                                         juce::jmax (minMixerToolbarHeight, mixerContent.getHeight() / 8));
            auto mixerToolbarRow = nextRow (mixerContent, mixerToolbarHeight);
            mixerToolsToolbar.setBounds (mixerToolbarRow.reduced (0, 1));
            mixerArea.setBounds (mixerContent);

            const int minSynthHeight = juce::jmax (132, rackRegion.getHeight() / 3);
            const int maxSynthHeight = juce::jmax (minSynthHeight, rackRegion.getHeight() - 96);
            const int synthHeight = juce::jlimit (minSynthHeight, maxSynthHeight, roundToInt (rackRegion.getHeight() * 0.64f));
            auto synthRegion = rackRegion.removeFromBottom (synthHeight);
            if (rackRegion.getHeight() > 8)
                rackRegion.removeFromBottom (8);

            juce::Rectangle<int> rackLeft;
            juce::Rectangle<int> rackRight;
            if (rackRegion.getWidth() < 520)
            {
                const int minTopHeight = juce::jmax (92, rackRegion.getHeight() / 3);
                const int maxTopHeight = juce::jmax (minTopHeight, rackRegion.getHeight() - 92);
                rackLeft = rackRegion.removeFromTop (juce::jlimit (minTopHeight, maxTopHeight, rackRegion.getHeight() / 2));
                if (rackRegion.getHeight() > 8)
                    rackRegion.removeFromTop (8);
                rackRight = rackRegion;
            }
            else
            {
                const int minRackLeftWidth = juce::jmax (170, rackRegion.getWidth() / 4);
                const int maxRackLeftWidth = juce::jmax (minRackLeftWidth, rackRegion.getWidth() - 170);
                rackLeft = rackRegion.removeFromLeft (juce::jlimit (minRackLeftWidth, maxRackLeftWidth, rackRegion.getWidth() / 2));
                if (rackRegion.getWidth() > 8)
                    rackRegion.removeFromLeft (8);
                rackRight = rackRegion;
            }

            auto channelRackArea = getGroupContent (channelRackGroup, rackLeft);
            auto inspectorArea = getGroupContent (inspectorGroup, rackRight);
            auto synthArea = getGroupContent (builtInSynthGroup, synthRegion);

            auto row1 = nextRow (channelRackArea, defaultRowHeight);
            channelRackTrackLabel.setBounds (row1.removeFromLeft (42).reduced (0, 1));
            row1.removeFromLeft (6);
            channelRackTrackBox.setBounds (row1.reduced (0, 1));

            auto row2 = nextRow (channelRackArea, defaultRowHeight);
            channelRackPluginLabel.setBounds (row2.removeFromLeft (42).reduced (0, 1));
            row2.removeFromLeft (6);
            channelRackPluginBox.setBounds (row2.reduced (0, 1));

            const int compactRackButtonsHeight = defaultRowHeight * 2 + 3;
            auto row3 = nextRow (channelRackArea, channelRackArea.getWidth() < 330 ? compactRackButtonsHeight : (defaultRowHeight + 2));
            const int rackButtonGap = 6;
            if (row3.getHeight() >= compactRackButtonsHeight - 1)
            {
                auto topButtons = row3.removeFromTop ((row3.getHeight() - 3) / 2);
                if (row3.getHeight() > 3)
                    row3.removeFromTop (3);
                auto bottomButton = row3;

                const int topButtonWidth = juce::jmax (0, (topButtons.getWidth() - rackButtonGap) / 2);
                channelRackAddInstrumentButton.setBounds (topButtons.removeFromLeft (topButtonWidth).reduced (0, 1));
                if (topButtons.getWidth() > rackButtonGap)
                    topButtons.removeFromLeft (rackButtonGap);
                channelRackAddFxButton.setBounds (topButtons.reduced (0, 1));
                channelRackOpenPluginButton.setBounds (bottomButton.reduced (0, 1));
            }
            else
            {
                const int rackButtonWidth = juce::jmax (0, (row3.getWidth() - rackButtonGap * 2) / 3);
                channelRackAddInstrumentButton.setBounds (row3.removeFromLeft (rackButtonWidth).reduced (0, 1));
                row3.removeFromLeft (rackButtonGap);
                channelRackAddFxButton.setBounds (row3.removeFromLeft (rackButtonWidth).reduced (0, 1));
                row3.removeFromLeft (rackButtonGap);
                channelRackOpenPluginButton.setBounds (row3.reduced (0, 1));
            }

            inspectorTrackNameLabel.setBounds (nextRow (inspectorArea, 24).reduced (2, 0));
            inspectorRouteLabel.setBounds (nextRow (inspectorArea, 24).reduced (2, 0));
            inspectorPluginLabel.setBounds (nextRow (inspectorArea, 24).reduced (2, 0));
            inspectorMeterLabel.setBounds (nextRow (inspectorArea, 24).reduced (2, 0));

            builtInSynthTargetLabel.setBounds (nextRow (synthArea, 22).reduced (2, 0));

            const bool compactPresetRow = synthArea.getWidth() < 420;
            auto presetRow = nextRow (synthArea, compactPresetRow ? (defaultRowHeight * 2 + 3) : 24);
            const int presetGap = 6;
            if (compactPresetRow && presetRow.getHeight() >= (defaultRowHeight * 2))
            {
                auto topPresets = presetRow.removeFromTop ((presetRow.getHeight() - 3) / 2);
                if (presetRow.getHeight() > 3)
                    presetRow.removeFromTop (3);
                auto bottomPresets = presetRow;

                const int topWidth = juce::jmax (0, (topPresets.getWidth() - presetGap) / 2);
                builtInSynthPresetInitButton.setBounds (topPresets.removeFromLeft (topWidth).reduced (0, 1));
                if (topPresets.getWidth() > presetGap)
                    topPresets.removeFromLeft (presetGap);
                builtInSynthPresetWarmPadButton.setBounds (topPresets.reduced (0, 1));

                const int bottomWidth = juce::jmax (0, (bottomPresets.getWidth() - presetGap) / 2);
                builtInSynthPresetPunchBassButton.setBounds (bottomPresets.removeFromLeft (bottomWidth).reduced (0, 1));
                if (bottomPresets.getWidth() > presetGap)
                    bottomPresets.removeFromLeft (presetGap);
                builtInSynthPresetBrightPluckButton.setBounds (bottomPresets.reduced (0, 1));
            }
            else
            {
                const int presetWidth = juce::jmax (0, (presetRow.getWidth() - presetGap * 3) / 4);
                builtInSynthPresetInitButton.setBounds (presetRow.removeFromLeft (presetWidth).reduced (0, 1));
                presetRow.removeFromLeft (presetGap);
                builtInSynthPresetWarmPadButton.setBounds (presetRow.removeFromLeft (presetWidth).reduced (0, 1));
                presetRow.removeFromLeft (presetGap);
                builtInSynthPresetPunchBassButton.setBounds (presetRow.removeFromLeft (presetWidth).reduced (0, 1));
                presetRow.removeFromLeft (presetGap);
                builtInSynthPresetBrightPluckButton.setBounds (presetRow.reduced (0, 1));
            }

            auto layoutMacroPair = [&nextRow, defaultRowHeight] (juce::Rectangle<int>& area,
                                                                 juce::Label& leftLabel, juce::Slider& leftSlider,
                                                                 juce::Label& rightLabel, juce::Slider& rightSlider)
            {
                const bool compact = area.getWidth() < 360;
                auto row = nextRow (area, compact ? (defaultRowHeight * 2 + 3) : defaultRowHeight);

                if (compact && row.getHeight() >= (defaultRowHeight * 2))
                {
                    auto top = row.removeFromTop ((row.getHeight() - 3) / 2);
                    if (row.getHeight() > 3)
                        row.removeFromTop (3);
                    auto bottom = row;

                    const int topLabelWidth = juce::jlimit (42, 70, top.getWidth() / 3);
                    leftLabel.setBounds (top.removeFromLeft (topLabelWidth).reduced (0, 1));
                    if (top.getWidth() > 4)
                        top.removeFromLeft (4);
                    leftSlider.setBounds (top.reduced (0, 1));

                    const int bottomLabelWidth = juce::jlimit (42, 70, bottom.getWidth() / 3);
                    rightLabel.setBounds (bottom.removeFromLeft (bottomLabelWidth).reduced (0, 1));
                    if (bottom.getWidth() > 4)
                        bottom.removeFromLeft (4);
                    rightSlider.setBounds (bottom.reduced (0, 1));
                }
                else
                {
                    auto left = row.removeFromLeft (row.getWidth() / 2);
                    if (row.getWidth() > 8)
                        row.removeFromLeft (8);
                    auto right = row;

                    const int leftLabelWidth = juce::jlimit (36, 64, left.getWidth() / 3);
                    leftLabel.setBounds (left.removeFromLeft (leftLabelWidth).reduced (0, 1));
                    left.removeFromLeft (4);
                    leftSlider.setBounds (left.reduced (0, 1));

                    const int rightLabelWidth = juce::jlimit (36, 64, right.getWidth() / 3);
                    rightLabel.setBounds (right.removeFromLeft (rightLabelWidth).reduced (0, 1));
                    right.removeFromLeft (4);
                    rightSlider.setBounds (right.reduced (0, 1));
                }
            };

            layoutMacroPair (synthArea,
                             builtInSynthCutoffLabel, builtInSynthCutoffSlider,
                             builtInSynthResonanceLabel, builtInSynthResonanceSlider);
            layoutMacroPair (synthArea,
                             builtInSynthEnvLabel, builtInSynthEnvSlider,
                             builtInSynthDriveLabel, builtInSynthDriveSlider);
            layoutMacroPair (synthArea,
                             builtInSynthAttackLabel, builtInSynthAttackSlider,
                             builtInSynthReleaseLabel, builtInSynthReleaseSlider);
            layoutMacroPair (synthArea,
                             builtInSynthReverbLabel, builtInSynthReverbSlider,
                             builtInSynthDelayLabel, builtInSynthDelaySlider);
            layoutMacroPair (synthArea,
                             builtInSynthOsc1Label, builtInSynthOsc1Slider,
                             builtInSynthOsc2Label, builtInSynthOsc2Slider);
            layoutMacroPair (synthArea,
                             builtInSynthOsc3Label, builtInSynthOsc3Slider,
                             builtInSynthOsc4Label, builtInSynthOsc4Slider);
            layoutMacroPair (synthArea,
                             builtInSynthUnisonLabel, builtInSynthUnisonSlider,
                             builtInSynthWidthLabel, builtInSynthWidthSlider);
            layoutMacroPair (synthArea,
                             builtInSynthChorusLabel, builtInSynthChorusSlider,
                             builtInSynthMasterLabel, builtInSynthMasterSlider);
            break;
        }

        case FloatSection::piano:
        {
            auto topSection = bounds;
            const int minHeaderHeight = denseLayout ? 22 : 24;
            const int maxHeaderHeight = denseLayout ? 34 : 38;
            const int headerHeight = juce::jlimit (minHeaderHeight,
                                                   maxHeaderHeight,
                                                   juce::jmax (minHeaderHeight, bounds.getHeight() / 4));
            auto headerRow = topSection.removeFromTop (headerHeight).reduced (0, 1);
            topSection.removeFromTop (juce::jlimit (2, 6, bounds.getHeight() / 24));

            const int controlGap = denseLayout ? 4 : 6;
            const bool stackControls = headerRow.getWidth() < 760;

            if (stackControls)
            {
                auto tabsRow = headerRow.removeFromTop (juce::jmax (14, headerRow.getHeight() / 2));
                pianoEditorModeTabs.setBounds (tabsRow.reduced (0, 1));
                if (headerRow.getHeight() > 3)
                    headerRow.removeFromTop (3);

                auto controlsRow = headerRow;
                const int half = juce::jmax (0, (controlsRow.getWidth() - controlGap) / 2);
                auto left = controlsRow.removeFromLeft (half);
                if (controlsRow.getWidth() > controlGap)
                    controlsRow.removeFromLeft (controlGap);
                auto right = controlsRow;

                const int leftHalf = juce::jmax (0, (left.getWidth() - controlGap) / 2);
                pianoFloatToggleButton.setBounds (left.removeFromLeft (leftHalf).reduced (0, 1));
                if (left.getWidth() > controlGap)
                    left.removeFromLeft (controlGap);
                pianoAlwaysOnTopButton.setBounds (left.reduced (0, 1));

                const int rightHalf = juce::jmax (0, (right.getWidth() - controlGap) / 2);
                pianoEnsureInstrumentButton.setBounds (right.removeFromLeft (rightHalf).reduced (0, 1));
                if (right.getWidth() > controlGap)
                    right.removeFromLeft (controlGap);
                pianoOpenInstrumentButton.setBounds (right.reduced (0, 1));
            }
            else
            {
                auto tabsArea = headerRow.removeFromLeft (juce::jlimit (210, 320, headerRow.getWidth() / 3));
                pianoEditorModeTabs.setBounds (tabsArea.reduced (0, 1));
                if (headerRow.getWidth() > 8)
                    headerRow.removeFromLeft (8);

                const int controlWidth = juce::jmax (0, (headerRow.getWidth() - controlGap * 3) / 4);
                pianoFloatToggleButton.setBounds (headerRow.removeFromLeft (controlWidth).reduced (0, 1));
                if (headerRow.getWidth() > controlGap)
                    headerRow.removeFromLeft (controlGap);
                pianoAlwaysOnTopButton.setBounds (headerRow.removeFromLeft (controlWidth).reduced (0, 1));
                if (headerRow.getWidth() > controlGap)
                    headerRow.removeFromLeft (controlGap);
                pianoEnsureInstrumentButton.setBounds (headerRow.removeFromLeft (controlWidth).reduced (0, 1));
                if (headerRow.getWidth() > controlGap)
                    headerRow.removeFromLeft (controlGap);
                pianoOpenInstrumentButton.setBounds (headerRow.reduced (0, 1));
            }

            const auto effectiveViewMode = getPianoEditorLayoutModeSelection();
            const int splitGap = denseLayout ? 8 : 10;
            const int availableSplitHeight = juce::jmax (0, topSection.getHeight() - splitGap);
            auto layoutPianoRollEditor = [this, denseLayout] (juce::Rectangle<int> area)
            {
                if (area.isEmpty())
                {
                    pianoRollToolbar.setBounds ({});
                    midiPianoRoll.setBounds ({});
                    pianoRollHorizontalScrollBar.setBounds ({});
                    pianoRollVerticalScrollBar.setBounds ({});
                    return;
                }

                const int minToolbarHeight = denseLayout ? 24 : 27;
                const int maxToolbarHeight = denseLayout ? 40 : 44;
                const int toolbarHeight = juce::jlimit (minToolbarHeight,
                                                        maxToolbarHeight,
                                                        juce::jmax (minToolbarHeight, area.getHeight() / 7));
                auto toolbarRow = area.removeFromTop (juce::jmin (toolbarHeight, area.getHeight()));
                const int toolbarGap = denseLayout ? 3 : 5;
                if (area.getHeight() > toolbarGap)
                    area.removeFromTop (toolbarGap);

                pianoRollToolbar.setBounds (toolbarRow.reduced (0, 1));

                const int scrollThickness = denseLayout ? 12 : 14;
                auto editorArea = area;
                auto horizontalBar = editorArea.removeFromBottom (juce::jmin (scrollThickness, editorArea.getHeight()));
                if (editorArea.getHeight() > 2)
                    editorArea.removeFromBottom (2);

                auto verticalBar = editorArea.removeFromRight (juce::jmin (scrollThickness, editorArea.getWidth()));
                if (editorArea.getWidth() > 2)
                    editorArea.removeFromRight (2);

                auto horizontalBounds = horizontalBar.withTrimmedRight (verticalBar.getWidth());
                auto verticalBounds = verticalBar;
                pianoRollHorizontalScrollBar.setBounds (horizontalBounds);
                pianoRollVerticalScrollBar.setBounds (verticalBounds);
                midiPianoRoll.setBounds (editorArea);
            };
            auto layoutStepSequencerEditor = [this, denseLayout] (juce::Rectangle<int> area)
            {
                if (area.isEmpty())
                {
                    stepSequencerToolbar.setBounds ({});
                    stepSequencer.setBounds ({});
                    stepSequencerHorizontalScrollBar.setBounds ({});
                    return;
                }

                const int minToolbarHeight = denseLayout ? 24 : 27;
                const int maxToolbarHeight = denseLayout ? 40 : 44;
                const int toolbarHeight = juce::jlimit (minToolbarHeight,
                                                        maxToolbarHeight,
                                                        juce::jmax (minToolbarHeight, area.getHeight() / 6));
                auto toolbarRow = area.removeFromTop (juce::jmin (toolbarHeight, area.getHeight()));
                const int toolbarGap = denseLayout ? 3 : 5;
                if (area.getHeight() > toolbarGap)
                    area.removeFromTop (toolbarGap);

                stepSequencerToolbar.setBounds (toolbarRow.reduced (0, 1));

                const int scrollThickness = denseLayout ? 12 : 14;
                auto editorArea = area;
                auto horizontalBar = editorArea.removeFromBottom (juce::jmin (scrollThickness, editorArea.getHeight()));
                if (editorArea.getHeight() > 2)
                    editorArea.removeFromBottom (2);

                stepSequencerHorizontalScrollBar.setBounds (horizontalBar);
                stepSequencer.setBounds (editorArea);
            };

            if (effectiveViewMode == PianoEditorLayoutMode::pianoRoll)
            {
                pianoStepSplitter.setBounds ({});
                stepSequencerGroup.setBounds ({});
                stepSequencerToolbar.setBounds ({});
                stepSequencer.setBounds ({});
                stepSequencerHorizontalScrollBar.setBounds ({});
                auto pianoArea = getGroupContentPlain (pianoRollGroup, topSection);
                layoutPianoRollEditor (pianoArea);
                updatePianoRollScrollbarsFromViewport();
                updateStepSequencerScrollbarFromPageContext();
                break;
            }

            if (effectiveViewMode == PianoEditorLayoutMode::stepSequencer)
            {
                pianoStepSplitter.setBounds ({});
                pianoRollGroup.setBounds ({});
                pianoRollToolbar.setBounds ({});
                midiPianoRoll.setBounds ({});
                pianoRollHorizontalScrollBar.setBounds ({});
                pianoRollVerticalScrollBar.setBounds ({});
                auto stepArea = getGroupContentPlain (stepSequencerGroup, topSection);
                layoutStepSequencerEditor (stepArea);
                updatePianoRollScrollbarsFromViewport();
                updateStepSequencerScrollbarFromPageContext();
                break;
            }

            const int minStepHeight = juce::jmax (denseLayout ? 78 : 92, availableSplitHeight / 4);
            const int minPianoHeight = juce::jmax (denseLayout ? 120 : 140, availableSplitHeight / 3);

            currentPianoSectionHeightForResize = juce::jmax (1, availableSplitHeight);
            int stepHeight = roundToInt ((float) availableSplitHeight * pianoStepHeightRatio);
            stepHeight = juce::jlimit (minStepHeight,
                                       juce::jmax (minStepHeight, availableSplitHeight - minPianoHeight),
                                       stepHeight);
            pianoStepHeightRatio = (float) stepHeight / (float) juce::jmax (1, availableSplitHeight);

            auto stepBounds = topSection.removeFromTop (stepHeight);
            if (topSection.getHeight() > splitGap)
                topSection.removeFromTop (splitGap);

            if (! stepBounds.isEmpty() && ! topSection.isEmpty())
            {
                pianoStepSplitter.setBounds (stepBounds.getX(),
                                             stepBounds.getBottom(),
                                             stepBounds.getWidth(),
                                             splitGap);
                pianoStepSplitter.toFront (false);
            }
            else
            {
                pianoStepSplitter.setBounds ({});
            }

            auto stepArea = getGroupContentPlain (stepSequencerGroup, stepBounds);
            auto pianoArea = getGroupContentPlain (pianoRollGroup, topSection);

            layoutStepSequencerEditor (stepArea);
            layoutPianoRollEditor (pianoArea);
            updatePianoRollScrollbarsFromViewport();
            updateStepSequencerScrollbarFromPageContext();
            break;
        }
    }
}

bool BeatMakerNoRecord::isSectionFloating (FloatSection section) const
{
    switch (section)
    {
        case FloatSection::workspace: return workspaceSectionFloating;
        case FloatSection::mixer:     return mixerSectionFloating;
        case FloatSection::piano:     return pianoSectionFloating;
    }

    return false;
}

juce::String BeatMakerNoRecord::getSectionFloatingTitle (FloatSection section) const
{
    switch (section)
    {
        case FloatSection::workspace: return "Timeline + Track Area";
        case FloatSection::mixer:     return "Mixer Area";
        case FloatSection::piano:     return getPianoFloatingWindowTitle();
    }

    return "Floating Section";
}

juce::String BeatMakerNoRecord::getPianoFloatingWindowTitle() const
{
    juce::String title ("Piano Roll + Step Sequencer");

    if (auto* midiClip = getSelectedMidiClip())
    {
        juce::String trackName ("Unknown Track");
        if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
            trackName = ownerTrack->getName();

        title << " | " << trackName << " - " << midiClip->getName();
    }
    else if (auto* track = getSelectedTrackOrFirst())
    {
        title << " | " << track->getName();
    }

    return title;
}

void BeatMakerNoRecord::refreshPianoFloatingWindowUi()
{
    const bool isFloating = isSectionFloating (FloatSection::piano);
    pianoFloatToggleButton.setButtonText (isFloating ? "Dock Piano" : "Float Piano");
    pianoFloatToggleButton.setToggleState (isFloating, juce::dontSendNotification);
    pianoAlwaysOnTopButton.setEnabled (isFloating);
    pianoAlwaysOnTopButton.setToggleState (pianoFloatingAlwaysOnTop, juce::dontSendNotification);

    if (pianoFloatingWindow != nullptr)
    {
        pianoFloatingWindow->setName (getPianoFloatingWindowTitle());
        pianoFloatingWindow->setAlwaysOnTop (pianoFloatingAlwaysOnTop);
    }
}

void BeatMakerNoRecord::setupCommandToolbar()
{
    if (commandToolbarFactory != nullptr)
    {
        refreshCommandToolbarState();
        return;
    }

    commandToolbar.clear();
    commandToolbar.setVertical (false);
    commandToolbar.setStyle (juce::Toolbar::iconsOnly);
    commandToolbar.setWantsKeyboardFocus (false);
    commandToolbar.setColour (juce::Toolbar::backgroundColourId, juce::Colour::fromRGB (9, 15, 25).withAlpha (0.95f));
    commandToolbar.setColour (juce::Toolbar::separatorColourId, juce::Colour::fromRGB (98, 136, 176).withAlpha (0.76f));
    commandToolbar.setColour (juce::Toolbar::buttonMouseOverBackgroundColourId, juce::Colour::fromRGB (66, 129, 191).withAlpha (0.44f));
    commandToolbar.setColour (juce::Toolbar::buttonMouseDownBackgroundColourId, juce::Colour::fromRGB (79, 173, 229).withAlpha (0.58f));

    auto factory = std::make_unique<PianoRollToolbarFactory>();

    auto addToolbarItem = [factoryPtr = factory.get()] (int itemId,
                                                         const juce::String& label,
                                                         const juce::String& tooltip,
                                                         bool isToggleButton,
                                                         int preferredWidth,
                                                         std::function<void()> onClick,
                                                         std::function<bool()> isEnabled,
                                                         std::function<bool()> isToggled)
    {
        PianoRollToolbarItemDefinition definition;
        definition.itemId = itemId;
        definition.label = label;
        definition.tooltip = tooltip;
        definition.isToggleButton = isToggleButton;
        definition.preferredWidth = juce::jmax (38, preferredWidth);
        definition.minWidth = juce::jmax (34, definition.preferredWidth - 16);
        definition.maxWidth = juce::jmax (definition.preferredWidth, definition.preferredWidth + 30);
        definition.onClick = std::move (onClick);
        definition.isEnabled = std::move (isEnabled);
        definition.isToggled = std::move (isToggled);
        factoryPtr->addToolbarItem (std::move (definition));
    };

    auto hasEdit = [this] { return edit != nullptr; };
    auto hasEditComponent = [this] { return editComponent != nullptr; };
    auto hasSelection = [this] { return selectionManager.getSelectedObjects().isNotEmpty(); };
    auto hasClip = [this] { return getSelectedClip() != nullptr; };
    auto canUndo = [this] { return edit != nullptr && edit->getUndoManager().canUndo(); };
    auto canRedo = [this] { return edit != nullptr && edit->getUndoManager().canRedo(); };
    auto transportPlaying = [this] { return edit != nullptr && edit->getTransport().isPlaying(); };
    auto transportLooping = [this] { return edit != nullptr && edit->getTransport().looping; };

    addToolbarItem (commandToolbarNewProject, "New", "Create new project", false, 48,
                    [this] { createNewEdit (false); },
                    [] { return true; },
                    {});
    addToolbarItem (commandToolbarOpenProject, "Open", "Open project", false, 52,
                    [this] { openEdit(); },
                    [] { return true; },
                    {});
    addToolbarItem (commandToolbarSaveProject, "Save", "Save project", false, 52,
                    [this] { saveEdit(); },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarUndo, "Undo", "Undo", false, 48,
                    [this]
                    {
                        if (edit != nullptr)
                            edit->getUndoManager().undo();
                    },
                    canUndo,
                    {});
    addToolbarItem (commandToolbarRedo, "Redo", "Redo", false, 48,
                    [this]
                    {
                        if (edit != nullptr)
                            edit->getUndoManager().redo();
                    },
                    canRedo,
                    {});

    addToolbarItem (commandToolbarPlayPause, "Play", "Play or pause transport", true, 52,
                    [this]
                    {
                        if (edit != nullptr)
                        {
                            if (! edit->getTransport().isPlaying())
                                prepareEditForPluginPlayback (true);

                            EngineHelpers::togglePlay (*edit);
                        }
                    },
                    hasEdit,
                    transportPlaying);
    addToolbarItem (commandToolbarStop, "Stop", "Stop transport", false, 52,
                    [this]
                    {
                        if (edit != nullptr)
                            edit->getTransport().stop (false, false);
                    },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarReturnStart, "Rtn0", "Return playhead to start", false, 52,
                    [this]
                    {
                        if (edit != nullptr)
                            edit->getTransport().setPosition (te::TimePosition::fromSeconds (0.0));
                    },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarLoopToggle, "Loop", "Toggle transport loop", true, 52,
                    [this] { toggleTransportLooping(); },
                    hasEdit,
                    transportLooping);

    addToolbarItem (commandToolbarAddTrack, "A+Trk", "Add audio track", false, 62,
                    [this] { addTrack(); },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarAddMidiTrack, "M+Trk", "Add MIDI track", false, 62,
                    [this] { addMidiTrack(); },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarImportAudio, "ImpA", "Import audio clip", false, 56,
                    [this] { importAudioClip(); },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarImportMidi, "ImpM", "Import MIDI clip", false, 56,
                    [this] { importMidiClip(); },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarCreateMidiClip, "NewMIDI", "Create MIDI clip on selected track", false, 70,
                    [this] { createMidiClip(); },
                    hasEdit,
                    {});

    addToolbarItem (commandToolbarToolSelect, "Sel", "Select tool", true, 46,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::select); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::select; });
    addToolbarItem (commandToolbarToolPencil, "Pen", "Pencil tool", true, 46,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::pencil); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::pencil; });
    addToolbarItem (commandToolbarToolScissors, "Scis", "Scissors tool", true, 50,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::scissors); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::scissors; });
    addToolbarItem (commandToolbarToolResize, "Rsz", "Resize tool", true, 48,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::resize); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::resize; });

    addToolbarItem (commandToolbarSplitClip, "Split", "Split selected clip at playhead", false, 52,
                    [this] { commandManager.invokeDirectly (appCommandSplitSelection, true); },
                    hasClip,
                    {});
    addToolbarItem (commandToolbarDuplicateClip, "Dup", "Duplicate selected clip", false, 48,
                    [this] { duplicateSelectedClip(); },
                    hasClip,
                    {});
    addToolbarItem (commandToolbarDeleteSelection, "Del", "Delete selected item(s)", false, 44,
                    [this] { deleteSelectedItem(); },
                    hasSelection,
                    {});

    addToolbarItem (commandToolbarFocusSelection, "Focus", "Focus selected clip in timeline", false, 58,
                    [this] { focusSelectedClipInView(); },
                    hasEditComponent,
                    {});
    addToolbarItem (commandToolbarCenterPlayhead, "Center", "Center timeline around playhead", false, 62,
                    [this] { centerPlayheadInView(); },
                    hasEdit,
                    {});
    addToolbarItem (commandToolbarFitProject, "Fit", "Fit timeline to project", false, 44,
                    [this] { fitProjectInView(); },
                    hasEditComponent,
                    {});

    factory->setDefaultItems ({
        commandToolbarNewProject, commandToolbarOpenProject, commandToolbarSaveProject, commandToolbarUndo, commandToolbarRedo,
        juce::ToolbarItemFactory::separatorBarId,
        commandToolbarPlayPause, commandToolbarStop, commandToolbarReturnStart, commandToolbarLoopToggle,
        juce::ToolbarItemFactory::separatorBarId,
        commandToolbarAddTrack, commandToolbarAddMidiTrack, commandToolbarImportAudio, commandToolbarImportMidi, commandToolbarCreateMidiClip,
        juce::ToolbarItemFactory::separatorBarId,
        commandToolbarToolSelect, commandToolbarToolPencil, commandToolbarToolScissors, commandToolbarToolResize,
        juce::ToolbarItemFactory::separatorBarId,
        commandToolbarSplitClip, commandToolbarDuplicateClip, commandToolbarDeleteSelection,
        juce::ToolbarItemFactory::separatorBarId, juce::ToolbarItemFactory::flexibleSpacerId,
        commandToolbarFocusSelection, commandToolbarCenterPlayhead, commandToolbarFitProject
    });

    commandToolbarFactory = std::move (factory);
    commandToolbar.addDefaultItems (*commandToolbarFactory);
    refreshCommandToolbarState();
}

void BeatMakerNoRecord::refreshCommandToolbarState()
{
    for (int i = 0; i < commandToolbar.getNumItems(); ++i)
        if (auto* item = dynamic_cast<PianoRollToolbarTextItem*> (commandToolbar.getItemComponent (i)))
            item->refreshStateFromOwner();
}

void BeatMakerNoRecord::setupTrackAreaToolbar()
{
    if (trackAreaToolbarFactory != nullptr)
    {
        refreshTrackAreaToolbarState();
        return;
    }

    trackAreaToolbar.clear();
    trackAreaToolbar.setVertical (false);
    trackAreaToolbar.setStyle (juce::Toolbar::iconsOnly);
    trackAreaToolbar.setWantsKeyboardFocus (false);
    trackAreaToolbar.setColour (juce::Toolbar::backgroundColourId, juce::Colour::fromRGB (10, 16, 26).withAlpha (0.92f));
    trackAreaToolbar.setColour (juce::Toolbar::separatorColourId, juce::Colour::fromRGB (90, 128, 170).withAlpha (0.72f));
    trackAreaToolbar.setColour (juce::Toolbar::buttonMouseOverBackgroundColourId, juce::Colour::fromRGB (64, 123, 189).withAlpha (0.46f));
    trackAreaToolbar.setColour (juce::Toolbar::buttonMouseDownBackgroundColourId, juce::Colour::fromRGB (58, 164, 228).withAlpha (0.58f));

    auto factory = std::make_unique<PianoRollToolbarFactory>();

    auto addToolbarItem = [factoryPtr = factory.get()] (int itemId,
                                                         const juce::String& label,
                                                         const juce::String& tooltip,
                                                         bool isToggleButton,
                                                         int preferredWidth,
                                                         std::function<void()> onClick,
                                                         std::function<bool()> isEnabled,
                                                         std::function<bool()> isToggled)
    {
        PianoRollToolbarItemDefinition definition;
        definition.itemId = itemId;
        definition.label = label;
        definition.tooltip = tooltip;
        definition.isToggleButton = isToggleButton;
        definition.preferredWidth = juce::jmax (38, preferredWidth);
        definition.minWidth = juce::jmax (34, definition.preferredWidth - 16);
        definition.maxWidth = juce::jmax (definition.preferredWidth, definition.preferredWidth + 30);
        definition.onClick = std::move (onClick);
        definition.isEnabled = std::move (isEnabled);
        definition.isToggled = std::move (isToggled);
        factoryPtr->addToolbarItem (std::move (definition));
    };

    auto hasEdit = [this] { return edit != nullptr; };
    auto hasSelection = [this] { return selectionManager.getSelectedObjects().isNotEmpty(); };
    auto hasClip = [this] { return getSelectedClip() != nullptr; };
    auto hasMarkers = [this]
    {
        return edit != nullptr && edit->getMarkerManager().getMarkers().size() > 0;
    };
    auto hasArrangerSections = [this]
    {
        if (edit == nullptr)
            return false;

        if (auto* arrangerTrack = edit->getArrangerTrack())
            for (auto* clip : arrangerTrack->getClips())
                if (dynamic_cast<te::ArrangerClip*> (clip) != nullptr)
                    return true;

        return false;
    };
    auto hasEditComponent = [this] { return editComponent != nullptr; };

    addToolbarItem (trackAreaToolbarToolSelect, "Sel", "Select tool", true, 46,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::select); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::select; });
    addToolbarItem (trackAreaToolbarToolPencil, "Pen", "Pencil tool", true, 46,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::pencil); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::pencil; });
    addToolbarItem (trackAreaToolbarToolScissors, "Scis", "Scissors tool", true, 50,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::scissors); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::scissors; });
    addToolbarItem (trackAreaToolbarToolResize, "Rsz", "Resize tool", true, 48,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::resize); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::resize; });

    addToolbarItem (trackAreaToolbarSplit, "Split", "Split selected clip at playhead", false, 52,
                    [this] { commandManager.invokeDirectly (appCommandSplitSelection, true); },
                    hasClip,
                    {});
    addToolbarItem (trackAreaToolbarDuplicate, "Dup", "Duplicate selected clip", false, 48,
                    [this] { duplicateSelectedClip(); },
                    hasClip,
                    {});
    addToolbarItem (trackAreaToolbarCopy, "Copy", "Copy selection", false, 52,
                    [this] { copySelection(); },
                    hasSelection,
                    {});
    addToolbarItem (trackAreaToolbarCut, "Cut", "Cut selection", false, 46,
                    [this] { cutSelection(); },
                    hasSelection,
                    {});
    addToolbarItem (trackAreaToolbarPaste, "Paste", "Paste from clipboard", false, 54,
                    [this] { pasteSelection(); },
                    hasEdit,
                    {});
    addToolbarItem (trackAreaToolbarDelete, "Del", "Delete selected items", false, 44,
                    [this] { deleteSelectedItem(); },
                    hasSelection,
                    {});

    addToolbarItem (trackAreaToolbarZoomIn, "Zoom+", "Zoom timeline in", false, 56,
                    [this] { zoomTimeline (0.84); },
                    hasEditComponent,
                    {});
    addToolbarItem (trackAreaToolbarZoomOut, "Zoom-", "Zoom timeline out", false, 56,
                    [this] { zoomTimeline (1.2); },
                    hasEditComponent,
                    {});
    addToolbarItem (trackAreaToolbarZoomReset, "1:1", "Reset timeline zoom", false, 42,
                    [this] { resetZoom(); },
                    hasEditComponent,
                    {});
    addToolbarItem (trackAreaToolbarFocusSelection, "Focus", "Focus selected clip in timeline", false, 58,
                    [this] { focusSelectedClipInView(); },
                    hasEditComponent,
                    {});
    addToolbarItem (trackAreaToolbarCenterPlayhead, "Center", "Center timeline around playhead", false, 62,
                    [this] { centerPlayheadInView(); },
                    hasEdit,
                    {});
    addToolbarItem (trackAreaToolbarFitProject, "Fit", "Fit project in timeline", false, 44,
                    [this] { fitProjectInView(); },
                    hasEditComponent,
                    {});
    addToolbarItem (trackAreaToolbarSetLoopSelection, "LoopSel", "Set transport loop to selected clip", false, 66,
                    [this] { setTransportLoopToSelectedClip(); },
                    hasClip,
                    {});

    addToolbarItem (trackAreaToolbarAddMarker, "M+", "Add marker at playhead", false, 42,
                    [this] { addMarkerAtPlayhead(); },
                    hasEdit,
                    {});
    addToolbarItem (trackAreaToolbarPrevMarker, "M<", "Jump to previous marker", false, 42,
                    [this] { jumpToMarker (false); },
                    hasMarkers,
                    {});
    addToolbarItem (trackAreaToolbarNextMarker, "M>", "Jump to next marker", false, 42,
                    [this] { jumpToMarker (true); },
                    hasMarkers,
                    {});
    addToolbarItem (trackAreaToolbarAddSection, "S+", "Add arranger section at playhead", false, 42,
                    [this] { addArrangerSectionAtPlayhead(); },
                    hasEdit,
                    {});
    addToolbarItem (trackAreaToolbarPrevSection, "S<", "Jump to previous arranger section", false, 42,
                    [this] { jumpToArrangerSection (false); },
                    hasArrangerSections,
                    {});
    addToolbarItem (trackAreaToolbarNextSection, "S>", "Jump to next arranger section", false, 42,
                    [this] { jumpToArrangerSection (true); },
                    hasArrangerSections,
                    {});

    addToolbarItem (trackAreaToolbarAddTrack, "A+Trk", "Add audio track", false, 62,
                    [this] { addTrack(); },
                    hasEdit,
                    {});
    addToolbarItem (trackAreaToolbarAddMidiTrack, "M+Trk", "Add MIDI track", false, 62,
                    [this] { addMidiTrack(); },
                    hasEdit,
                    {});

    factory->setDefaultItems ({
        trackAreaToolbarToolSelect, trackAreaToolbarToolPencil, trackAreaToolbarToolScissors, trackAreaToolbarToolResize,
        juce::ToolbarItemFactory::separatorBarId,
        trackAreaToolbarSplit, trackAreaToolbarDuplicate, trackAreaToolbarCopy, trackAreaToolbarCut, trackAreaToolbarPaste, trackAreaToolbarDelete,
        juce::ToolbarItemFactory::separatorBarId,
        trackAreaToolbarZoomIn, trackAreaToolbarZoomOut, trackAreaToolbarZoomReset,
        trackAreaToolbarFocusSelection, trackAreaToolbarCenterPlayhead, trackAreaToolbarFitProject, trackAreaToolbarSetLoopSelection,
        juce::ToolbarItemFactory::separatorBarId,
        trackAreaToolbarAddMarker, trackAreaToolbarPrevMarker, trackAreaToolbarNextMarker,
        trackAreaToolbarAddSection, trackAreaToolbarPrevSection, trackAreaToolbarNextSection,
        juce::ToolbarItemFactory::separatorBarId, juce::ToolbarItemFactory::flexibleSpacerId,
        trackAreaToolbarAddTrack, trackAreaToolbarAddMidiTrack
    });

    trackAreaToolbarFactory = std::move (factory);
    trackAreaToolbar.addDefaultItems (*trackAreaToolbarFactory);
    refreshTrackAreaToolbarState();
}

void BeatMakerNoRecord::refreshTrackAreaToolbarState()
{
    for (int i = 0; i < trackAreaToolbar.getNumItems(); ++i)
        if (auto* item = dynamic_cast<PianoRollToolbarTextItem*> (trackAreaToolbar.getItemComponent (i)))
            item->refreshStateFromOwner();
}

void BeatMakerNoRecord::setupMixerToolsToolbar()
{
    if (mixerToolsToolbarFactory != nullptr)
    {
        refreshMixerToolsToolbarState();
        return;
    }

    mixerToolsToolbar.clear();
    mixerToolsToolbar.setVertical (false);
    mixerToolsToolbar.setStyle (juce::Toolbar::iconsOnly);
    mixerToolsToolbar.setWantsKeyboardFocus (false);
    mixerToolsToolbar.setColour (juce::Toolbar::backgroundColourId, juce::Colour::fromRGB (10, 17, 24).withAlpha (0.92f));
    mixerToolsToolbar.setColour (juce::Toolbar::separatorColourId, juce::Colour::fromRGB (102, 138, 176).withAlpha (0.72f));
    mixerToolsToolbar.setColour (juce::Toolbar::buttonMouseOverBackgroundColourId, juce::Colour::fromRGB (62, 127, 181).withAlpha (0.45f));
    mixerToolsToolbar.setColour (juce::Toolbar::buttonMouseDownBackgroundColourId, juce::Colour::fromRGB (86, 176, 225).withAlpha (0.58f));

    auto factory = std::make_unique<PianoRollToolbarFactory>();

    auto addToolbarItem = [factoryPtr = factory.get()] (int itemId,
                                                         const juce::String& label,
                                                         const juce::String& tooltip,
                                                         bool isToggleButton,
                                                         int preferredWidth,
                                                         std::function<void()> onClick,
                                                         std::function<bool()> isEnabled,
                                                         std::function<bool()> isToggled)
    {
        PianoRollToolbarItemDefinition definition;
        definition.itemId = itemId;
        definition.label = label;
        definition.tooltip = tooltip;
        definition.isToggleButton = isToggleButton;
        definition.preferredWidth = juce::jmax (38, preferredWidth);
        definition.minWidth = juce::jmax (34, definition.preferredWidth - 16);
        definition.maxWidth = juce::jmax (definition.preferredWidth, definition.preferredWidth + 30);
        definition.onClick = std::move (onClick);
        definition.isEnabled = std::move (isEnabled);
        definition.isToggled = std::move (isToggled);
        factoryPtr->addToolbarItem (std::move (definition));
    };

    auto hasEdit = [this] { return edit != nullptr; };
    auto hasTrack = [this] { return getSelectedTrackOrFirst() != nullptr; };
    auto getSelectedFx = [this] () -> te::Plugin*
    {
        return getSelectedTrackPlugin();
    };
    auto hasSelectedFx = [getSelectedFx]
    {
        return getSelectedFx() != nullptr;
    };
    auto canBypassSelectedFx = [getSelectedFx]
    {
        if (auto* plugin = getSelectedFx())
            return plugin->canBeDisabled();

        return false;
    };
    auto canDeleteSelectedFx = [getSelectedFx]
    {
        auto* plugin = getSelectedFx();
        return plugin != nullptr
            && dynamic_cast<te::VolumeAndPanPlugin*> (plugin) == nullptr
            && dynamic_cast<te::LevelMeterPlugin*> (plugin) == nullptr;
    };

    addToolbarItem (mixerToolbarMute, "Mute", "Toggle mute for selected track", true, 54,
                    [this]
                    {
                        if (trackMuteButton.isEnabled())
                            trackMuteButton.triggerClick();
                    },
                    hasTrack,
                    [this]
                    {
                        if (auto* track = getSelectedTrackOrFirst())
                            return track->isMuted (false);

                        return false;
                    });
    addToolbarItem (mixerToolbarSolo, "Solo", "Toggle solo for selected track", true, 54,
                    [this]
                    {
                        if (trackSoloButton.isEnabled())
                            trackSoloButton.triggerClick();
                    },
                    hasTrack,
                    [this]
                    {
                        if (auto* track = getSelectedTrackOrFirst())
                            return track->isSolo (false);

                        return false;
                    });
    addToolbarItem (mixerToolbarTrackAdd, "A+Trk", "Add audio track", false, 62,
                    [this] { addTrack(); },
                    hasEdit,
                    {});
    addToolbarItem (mixerToolbarTrackAddMidi, "M+Trk", "Add MIDI track", false, 62,
                    [this] { addMidiTrack(); },
                    hasEdit,
                    {});

    addToolbarItem (mixerToolbarFxRefresh, "Ref", "Refresh FX list from selected track", false, 46,
                    [this] { refreshSelectedTrackPluginList(); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxOpen, "Open", "Open selected plugin UI", false, 54,
                    [this] { openSelectedTrackPluginEditor(); },
                    hasSelectedFx,
                    {});
    addToolbarItem (mixerToolbarFxScan, "Scan", "Scan plugins", false, 52,
                    [this] { openPluginScanDialog(); },
                    hasEdit,
                    {});
    addToolbarItem (mixerToolbarFxPrep, "Prep", "Prepare playback routing and plugin ordering", false, 52,
                    [this] { prepareEditForPluginPlayback (true); },
                    hasEdit,
                    {});

    addToolbarItem (mixerToolbarFxAddSampler, "Samp", "Add Sampler plugin", false, 54,
                    [this] { addBuiltInPluginToSelectedTrack ("sampler", "Sampler"); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxAddSynth, "4OSC", "Add 4OSC synth", false, 54,
                    [this] { addBuiltInPluginToSelectedTrack ("4osc", "4OSC"); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxAddEq, "EQ", "Add EQ plugin", false, 44,
                    [this] { addBuiltInPluginToSelectedTrack ("4bandEq", "EQ"); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxAddComp, "Comp", "Add compressor plugin", false, 52,
                    [this] { addBuiltInPluginToSelectedTrack ("compressor", "Compressor"); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxAddReverb, "Rev", "Add reverb plugin", false, 46,
                    [this] { addBuiltInPluginToSelectedTrack ("reverb", "Reverb"); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxAddDelay, "Dly", "Add delay plugin", false, 46,
                    [this] { addBuiltInPluginToSelectedTrack ("delay", "Delay"); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxAddInstrument, "Inst+", "Add external instrument", false, 58,
                    [this] { addExternalInstrumentPluginToSelectedTrack(); },
                    hasTrack,
                    {});
    addToolbarItem (mixerToolbarFxAddExternal, "FX+", "Add external effect", false, 52,
                    [this] { addExternalPluginToSelectedTrack(); },
                    hasTrack,
                    {});

    addToolbarItem (mixerToolbarFxMoveUp, "Up", "Move selected plugin up", false, 42,
                    [this] { moveSelectedTrackPlugin (false); },
                    hasSelectedFx,
                    {});
    addToolbarItem (mixerToolbarFxMoveDown, "Dn", "Move selected plugin down", false, 42,
                    [this] { moveSelectedTrackPlugin (true); },
                    hasSelectedFx,
                    {});
    addToolbarItem (mixerToolbarFxBypass, "Byp", "Toggle bypass for selected plugin", false, 46,
                    [this] { toggleSelectedTrackPluginBypass(); },
                    canBypassSelectedFx,
                    {});
    addToolbarItem (mixerToolbarFxDelete, "Del", "Delete selected plugin", false, 44,
                    [this] { deleteSelectedTrackPlugin(); },
                    canDeleteSelectedFx,
                    {});

    factory->setDefaultItems ({
        mixerToolbarMute, mixerToolbarSolo, mixerToolbarTrackAdd, mixerToolbarTrackAddMidi,
        juce::ToolbarItemFactory::separatorBarId,
        mixerToolbarFxRefresh, mixerToolbarFxOpen, mixerToolbarFxScan, mixerToolbarFxPrep,
        juce::ToolbarItemFactory::separatorBarId,
        mixerToolbarFxAddSampler, mixerToolbarFxAddSynth, mixerToolbarFxAddEq, mixerToolbarFxAddComp,
        mixerToolbarFxAddReverb, mixerToolbarFxAddDelay, mixerToolbarFxAddInstrument, mixerToolbarFxAddExternal,
        juce::ToolbarItemFactory::separatorBarId, juce::ToolbarItemFactory::flexibleSpacerId,
        mixerToolbarFxMoveUp, mixerToolbarFxMoveDown, mixerToolbarFxBypass, mixerToolbarFxDelete
    });

    mixerToolsToolbarFactory = std::move (factory);
    mixerToolsToolbar.addDefaultItems (*mixerToolsToolbarFactory);
    refreshMixerToolsToolbarState();
}

void BeatMakerNoRecord::refreshMixerToolsToolbarState()
{
    for (int i = 0; i < mixerToolsToolbar.getNumItems(); ++i)
        if (auto* item = dynamic_cast<PianoRollToolbarTextItem*> (mixerToolsToolbar.getItemComponent (i)))
            item->refreshStateFromOwner();
}

void BeatMakerNoRecord::setupPianoRollToolbar()
{
    if (pianoRollToolbarFactory != nullptr)
    {
        refreshPianoRollToolbarState();
        return;
    }

    pianoRollToolbar.clear();
    pianoRollToolbar.setVertical (false);
    pianoRollToolbar.setStyle (juce::Toolbar::iconsOnly);
    pianoRollToolbar.setWantsKeyboardFocus (false);
    pianoRollToolbar.setColour (juce::Toolbar::backgroundColourId, juce::Colour::fromRGB (11, 18, 28).withAlpha (0.92f));
    pianoRollToolbar.setColour (juce::Toolbar::separatorColourId, juce::Colour::fromRGB (84, 121, 164).withAlpha (0.74f));
    pianoRollToolbar.setColour (juce::Toolbar::buttonMouseOverBackgroundColourId, juce::Colour::fromRGB (56, 118, 186).withAlpha (0.44f));
    pianoRollToolbar.setColour (juce::Toolbar::buttonMouseDownBackgroundColourId, juce::Colour::fromRGB (49, 157, 224).withAlpha (0.58f));

    auto factory = std::make_unique<PianoRollToolbarFactory>();

    auto addToolbarItem = [factoryPtr = factory.get()] (int itemId,
                                                         const juce::String& label,
                                                         const juce::String& tooltip,
                                                         bool isToggleButton,
                                                         int preferredWidth,
                                                         std::function<void()> onClick,
                                                         std::function<bool()> isEnabled,
                                                         std::function<bool()> isToggled,
                                                         PianoRollToolbarItemDefinition::IconGlyph iconGlyph)
    {
        PianoRollToolbarItemDefinition definition;
        definition.itemId = itemId;
        definition.label = label;
        definition.tooltip = tooltip;
        definition.isToggleButton = isToggleButton;
        definition.iconOnly = true;
        definition.iconGlyph = iconGlyph;

        const int compactPreferredWidth = juce::jlimit (32, 42, preferredWidth - 14);
        definition.preferredWidth = compactPreferredWidth;
        definition.minWidth = juce::jmax (30, compactPreferredWidth - 6);
        definition.maxWidth = juce::jmax (compactPreferredWidth, compactPreferredWidth + 8);
        definition.onClick = std::move (onClick);
        definition.isEnabled = std::move (isEnabled);
        definition.isToggled = std::move (isToggled);
        factoryPtr->addToolbarItem (std::move (definition));
    };

    auto hasEdit = [this] { return edit != nullptr; };
    auto hasSelection = [this] { return selectionManager.getSelectedObjects().isNotEmpty(); };
    auto hasMidiClip = [this] { return getSelectedMidiClip() != nullptr; };

    addToolbarItem (pianoRollToolbarToolSelect, "Sel", "Select and move notes", true, 46,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::select); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::select; },
                    PianoRollToolbarItemDefinition::IconGlyph::select);
    addToolbarItem (pianoRollToolbarToolPencil, "Pen", "Draw new notes", true, 46,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::pencil); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::pencil; },
                    PianoRollToolbarItemDefinition::IconGlyph::pencil);
    addToolbarItem (pianoRollToolbarToolScissors, "Scis", "Split notes", true, 50,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::scissors); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::scissors; },
                    PianoRollToolbarItemDefinition::IconGlyph::scissors);
    addToolbarItem (pianoRollToolbarToolResize, "Rsz", "Resize note lengths", true, 48,
                    [this] { setTimelineEditToolFromUi (TimelineEditTool::resize); },
                    hasEdit,
                    [] { return getTimelineEditTool() == TimelineEditTool::resize; },
                    PianoRollToolbarItemDefinition::IconGlyph::resize);

    addToolbarItem (pianoRollToolbarQuantize, "Qtz", "Quantize selected MIDI notes", false, 48,
                    [this] { commandManager.invokeDirectly (appCommandQuantizeMidi, true); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::quantize);
    addToolbarItem (pianoRollToolbarTransposeDown, "P-", "Transpose selected notes down by one semitone", false, 44,
                    [this] { commandManager.invokeDirectly (appCommandTransposeDown, true); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::transposeDown);
    addToolbarItem (pianoRollToolbarTransposeUp, "P+", "Transpose selected notes up by one semitone", false, 44,
                    [this] { commandManager.invokeDirectly (appCommandTransposeUp, true); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::transposeUp);
    addToolbarItem (pianoRollToolbarVelocityDown, "V-", "Reduce selected note velocity", false, 44,
                    [this] { commandManager.invokeDirectly (appCommandVelocityDown, true); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::velocityDown);
    addToolbarItem (pianoRollToolbarVelocityUp, "V+", "Increase selected note velocity", false, 44,
                    [this] { commandManager.invokeDirectly (appCommandVelocityUp, true); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::velocityUp);
    addToolbarItem (pianoRollToolbarHumanizeTiming, "HumT", "Humanize note timing", false, 54,
                    [this] { humanizeSelectedMidiTiming (0.08); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::humanizeTiming);
    addToolbarItem (pianoRollToolbarHumanizeVelocity, "HumV", "Humanize note velocity", false, 54,
                    [this] { humanizeSelectedMidiVelocity (10); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::humanizeVelocity);
    addToolbarItem (pianoRollToolbarLegato, "Leg", "Legato selected notes", false, 46,
                    [this] { legatoSelectedMidiNotes(); },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::legato);

    addToolbarItem (pianoRollToolbarCopy, "Copy", "Copy current selection", false, 52,
                    [this] { copySelection(); },
                    hasSelection,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::copy);
    addToolbarItem (pianoRollToolbarCut, "Cut", "Cut current selection", false, 46,
                    [this] { cutSelection(); },
                    hasSelection,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::cut);
    addToolbarItem (pianoRollToolbarPaste, "Paste", "Paste clipboard content", false, 54,
                    [this] { pasteSelection(); },
                    hasEdit,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::paste);
    addToolbarItem (pianoRollToolbarDelete, "Del", "Delete current selection", false, 44,
                    [this] { deleteSelectedItem(); },
                    hasSelection,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::remove);

    addToolbarItem (pianoRollToolbarGenerateChords, "Chrd", "Generate chord progression in selected MIDI clip", false, 54,
                    [this] { generateMidiChordProgression(); },
                    hasEdit,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::generateChords);
    addToolbarItem (pianoRollToolbarGenerateArp, "Arp", "Generate arpeggio pattern in selected MIDI clip", false, 48,
                    [this] { generateMidiArpeggioPattern(); },
                    hasEdit,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::generateArp);
    addToolbarItem (pianoRollToolbarGenerateBass, "Bass", "Generate bassline pattern in selected MIDI clip", false, 52,
                    [this] { generateMidiBasslinePattern(); },
                    hasEdit,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::generateBass);
    addToolbarItem (pianoRollToolbarGenerateDrums, "Drm", "Generate drum pattern in selected MIDI clip", false, 48,
                    [this] { generateMidiDrumPattern(); },
                    hasEdit,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::generateDrums);

    addToolbarItem (pianoRollToolbarFocus, "Focus", "Focus view around MIDI note content", false, 58,
                    [this]
                    {
                        if (auto* midiClip = getSelectedMidiClip())
                        {
                            focusPianoRollViewportOnClip (*midiClip, true);
                            midiPianoRoll.repaint();
                        }
                    },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::focus);
    addToolbarItem (pianoRollToolbarResetView, "Reset", "Reset piano roll view to full clip framing", false, 58,
                    [this]
                    {
                        if (auto* midiClip = getSelectedMidiClip())
                        {
                            focusPianoRollViewportOnClip (*midiClip, false);
                            midiPianoRoll.repaint();
                        }
                    },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::reset);
    addToolbarItem (pianoRollToolbarZoomTimeIn, "X+", "Zoom in horizontally", false, 42,
                    [this]
                    {
                        if (getSelectedMidiClip() == nullptr)
                            return;

                        syncPianoRollViewportToSelection (false);
                        const double anchorBeat = pianoRollViewStartBeat + (pianoRollViewLengthBeats * 0.5);
                        zoomPianoRollViewportTime (0.82, anchorBeat);
                    },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::zoomTimeIn);
    addToolbarItem (pianoRollToolbarZoomTimeOut, "X-", "Zoom out horizontally", false, 42,
                    [this]
                    {
                        if (getSelectedMidiClip() == nullptr)
                            return;

                        syncPianoRollViewportToSelection (false);
                        const double anchorBeat = pianoRollViewStartBeat + (pianoRollViewLengthBeats * 0.5);
                        zoomPianoRollViewportTime (1.22, anchorBeat);
                    },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::zoomTimeOut);
    addToolbarItem (pianoRollToolbarZoomPitchIn, "Y+", "Zoom in vertically", false, 42,
                    [this]
                    {
                        if (getSelectedMidiClip() == nullptr)
                            return;

                        syncPianoRollViewportToSelection (false);
                        const int anchorNote = pianoRollViewLowestNote + (juce::jmax (1, pianoRollViewNoteCount) / 2);
                        zoomPianoRollViewportPitch (0.86, anchorNote);
                    },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::zoomPitchIn);
    addToolbarItem (pianoRollToolbarZoomPitchOut, "Y-", "Zoom out vertically", false, 42,
                    [this]
                    {
                        if (getSelectedMidiClip() == nullptr)
                            return;

                        syncPianoRollViewportToSelection (false);
                        const int anchorNote = pianoRollViewLowestNote + (juce::jmax (1, pianoRollViewNoteCount) / 2);
                        zoomPianoRollViewportPitch (1.18, anchorNote);
                    },
                    hasMidiClip,
                    {},
                    PianoRollToolbarItemDefinition::IconGlyph::zoomPitchOut);

    factory->setDefaultItems ({
        pianoRollToolbarToolSelect, pianoRollToolbarToolPencil, pianoRollToolbarToolScissors, pianoRollToolbarToolResize,
        juce::ToolbarItemFactory::separatorBarId,
        pianoRollToolbarQuantize,
        pianoRollToolbarTransposeDown, pianoRollToolbarTransposeUp,
        pianoRollToolbarVelocityDown, pianoRollToolbarVelocityUp,
        pianoRollToolbarHumanizeTiming, pianoRollToolbarHumanizeVelocity, pianoRollToolbarLegato,
        juce::ToolbarItemFactory::separatorBarId,
        pianoRollToolbarCopy, pianoRollToolbarCut, pianoRollToolbarPaste, pianoRollToolbarDelete,
        juce::ToolbarItemFactory::separatorBarId,
        pianoRollToolbarGenerateChords, pianoRollToolbarGenerateArp, pianoRollToolbarGenerateBass, pianoRollToolbarGenerateDrums,
        juce::ToolbarItemFactory::separatorBarId, juce::ToolbarItemFactory::flexibleSpacerId,
        pianoRollToolbarFocus, pianoRollToolbarResetView,
        pianoRollToolbarZoomTimeIn, pianoRollToolbarZoomTimeOut,
        pianoRollToolbarZoomPitchIn, pianoRollToolbarZoomPitchOut
    });

    pianoRollToolbarFactory = std::move (factory);
    pianoRollToolbar.addDefaultItems (*pianoRollToolbarFactory);
    refreshPianoRollToolbarState();
}

void BeatMakerNoRecord::refreshPianoRollToolbarState()
{
    for (int i = 0; i < pianoRollToolbar.getNumItems(); ++i)
        if (auto* item = dynamic_cast<PianoRollToolbarTextItem*> (pianoRollToolbar.getItemComponent (i)))
            item->refreshStateFromOwner();
}

void BeatMakerNoRecord::setupStepSequencerToolbar()
{
    if (stepSequencerToolbarFactory != nullptr)
    {
        refreshStepSequencerToolbarState();
        return;
    }

    stepSequencerToolbar.clear();
    stepSequencerToolbar.setVertical (false);
    stepSequencerToolbar.setStyle (juce::Toolbar::iconsOnly);
    stepSequencerToolbar.setWantsKeyboardFocus (false);
    stepSequencerToolbar.setColour (juce::Toolbar::backgroundColourId, juce::Colour::fromRGB (10, 19, 30).withAlpha (0.92f));
    stepSequencerToolbar.setColour (juce::Toolbar::separatorColourId, juce::Colour::fromRGB (83, 122, 167).withAlpha (0.72f));
    stepSequencerToolbar.setColour (juce::Toolbar::buttonMouseOverBackgroundColourId, juce::Colour::fromRGB (62, 128, 190).withAlpha (0.46f));
    stepSequencerToolbar.setColour (juce::Toolbar::buttonMouseDownBackgroundColourId, juce::Colour::fromRGB (56, 167, 226).withAlpha (0.58f));

    auto factory = std::make_unique<PianoRollToolbarFactory>();

    auto addToolbarItem = [factoryPtr = factory.get()] (int itemId,
                                                         const juce::String& label,
                                                         const juce::String& tooltip,
                                                         bool isToggleButton,
                                                         int preferredWidth,
                                                         std::function<void()> onClick,
                                                         std::function<bool()> isEnabled,
                                                         std::function<bool()> isToggled)
    {
        PianoRollToolbarItemDefinition definition;
        definition.itemId = itemId;
        definition.label = label;
        definition.tooltip = tooltip;
        definition.isToggleButton = isToggleButton;
        definition.preferredWidth = juce::jmax (38, preferredWidth);
        definition.minWidth = juce::jmax (34, definition.preferredWidth - 16);
        definition.maxWidth = juce::jmax (definition.preferredWidth, definition.preferredWidth + 30);
        definition.onClick = std::move (onClick);
        definition.isEnabled = std::move (isEnabled);
        definition.isToggled = std::move (isToggled);
        factoryPtr->addToolbarItem (std::move (definition));
    };

    auto hasEdit = [this] { return edit != nullptr; };
    auto hasMidiClip = [this] { return getSelectedMidiClip() != nullptr; };

    addToolbarItem (stepSequencerToolbarCreateMidi, "NewMIDI", "Create one-bar MIDI clip at playhead", false, 68,
                    [this] { createMidiClip(); },
                    hasEdit,
                    {});
    addToolbarItem (stepSequencerToolbarClearPage, "Clear", "Clear all active steps on current sequencer page", false, 52,
                    [this] { clearStepSequencerPage(); },
                    hasMidiClip,
                    {});
    addToolbarItem (stepSequencerToolbarPatternFourOnFloor, "4/4", "Generate a four-on-the-floor drum pattern on this page", false, 42,
                    [this] { applyStepSequencerFourOnFloorPattern(); },
                    hasMidiClip,
                    {});
    addToolbarItem (stepSequencerToolbarRandomizePage, "Rand", "Randomize drum steps on this page", false, 50,
                    [this] { randomizeStepSequencerPage(); },
                    hasMidiClip,
                    {});
    addToolbarItem (stepSequencerToolbarShiftLeft, "<", "Shift all steps left by one slot", false, 34,
                    [this] { shiftStepSequencerPage (-1); },
                    hasMidiClip,
                    {});
    addToolbarItem (stepSequencerToolbarShiftRight, ">", "Shift all steps right by one slot", false, 34,
                    [this] { shiftStepSequencerPage (1); },
                    hasMidiClip,
                    {});
    addToolbarItem (stepSequencerToolbarVaryVelocity, "Vel", "Randomly vary step velocities", false, 44,
                    [this] { varyStepSequencerPageVelocities (14); },
                    hasMidiClip,
                    {});
    addToolbarItem (stepSequencerToolbarQuantize, "Qtz", "Quantize selected MIDI clip to current grid", false, 48,
                    [this] { quantizeSelectedMidiClip(); },
                    hasMidiClip,
                    {});
    addToolbarItem (stepSequencerToolbarGenerateDrums, "GenD", "Generate a full drum pattern in selected MIDI clip", false, 54,
                    [this] { generateMidiDrumPattern(); },
                    hasEdit,
                    {});

    factory->setDefaultItems ({
        stepSequencerToolbarCreateMidi, stepSequencerToolbarClearPage, stepSequencerToolbarPatternFourOnFloor, stepSequencerToolbarRandomizePage,
        juce::ToolbarItemFactory::separatorBarId,
        stepSequencerToolbarShiftLeft, stepSequencerToolbarShiftRight, stepSequencerToolbarVaryVelocity, stepSequencerToolbarQuantize,
        juce::ToolbarItemFactory::separatorBarId, juce::ToolbarItemFactory::flexibleSpacerId,
        stepSequencerToolbarGenerateDrums
    });

    stepSequencerToolbarFactory = std::move (factory);
    stepSequencerToolbar.addDefaultItems (*stepSequencerToolbarFactory);
    refreshStepSequencerToolbarState();
}

void BeatMakerNoRecord::refreshStepSequencerToolbarState()
{
    for (int i = 0; i < stepSequencerToolbar.getNumItems(); ++i)
        if (auto* item = dynamic_cast<PianoRollToolbarTextItem*> (stepSequencerToolbar.getItemComponent (i)))
            item->refreshStateFromOwner();
}

void BeatMakerNoRecord::refreshAllToolbarStates()
{
    refreshCommandToolbarState();
    refreshTrackAreaToolbarState();
    refreshMixerToolsToolbarState();
    refreshPianoRollToolbarState();
    refreshStepSequencerToolbarState();
}

void BeatMakerNoRecord::toggleSectionFloating (FloatSection section)
{
    setSectionFloating (section, ! isSectionFloating (section));
}

void BeatMakerNoRecord::setSectionFloating (FloatSection section, bool shouldFloat, bool)
{
    bool* state = nullptr;
    std::unique_ptr<FloatingSectionWindow>* window = nullptr;
    SectionContainer* container = nullptr;

    switch (section)
    {
        case FloatSection::workspace:
            state = &workspaceSectionFloating;
            window = &workspaceFloatingWindow;
            container = &workspaceSection;
            break;
        case FloatSection::mixer:
            state = &mixerSectionFloating;
            window = &mixerFloatingWindow;
            container = &mixerSection;
            break;
        case FloatSection::piano:
            state = &pianoSectionFloating;
            window = &pianoFloatingWindow;
            container = &pianoSection;
            break;
    }

    if (state == nullptr || window == nullptr || container == nullptr || *state == shouldFloat)
        return;

    if (shouldFloat)
    {
        if (container->getParentComponent() == this)
            removeChildComponent (container);

        auto floatWindow = std::make_unique<FloatingSectionWindow> (getSectionFloatingTitle (section),
                                                                     [this, section] { setSectionFloating (section, false, true); });

        floatWindow->setContentNonOwned (container, false);

        int defaultWidth = 540;
        int defaultHeight = 360;

        if (section == FloatSection::workspace)
        {
            defaultWidth = 980;
            defaultHeight = 620;
        }
        else if (section == FloatSection::piano)
        {
            defaultWidth = 980;
            defaultHeight = 640;
        }

        bool usedSavedBounds = false;
        if (section == FloatSection::piano)
        {
            auto& properties = engine.getPropertyStorage().getPropertiesFile();
            const auto savedX = properties.getIntValue ("pianoFloatX", std::numeric_limits<int>::min());
            const auto savedY = properties.getIntValue ("pianoFloatY", std::numeric_limits<int>::min());
            const auto savedW = properties.getIntValue ("pianoFloatW", 0);
            const auto savedH = properties.getIntValue ("pianoFloatH", 0);
            pianoFloatingAlwaysOnTop = properties.getBoolValue ("pianoFloatAlwaysOnTop", true);

            if (savedW >= 420 && savedH >= 280
                && savedX > std::numeric_limits<int>::min() / 2
                && savedY > std::numeric_limits<int>::min() / 2)
            {
                floatWindow->setBounds (savedX, savedY, savedW, savedH);
                usedSavedBounds = true;
            }

            floatWindow->setAlwaysOnTop (pianoFloatingAlwaysOnTop);
        }

        if (! usedSavedBounds)
        {
            const int width = juce::jmax (defaultWidth, container->getWidth() > 0 ? container->getWidth() : defaultWidth);
            const int height = juce::jmax (defaultHeight, container->getHeight() > 0 ? container->getHeight() : defaultHeight);
            floatWindow->centreWithSize (width, height);
        }

        floatWindow->setVisible (true);

        *window = std::move (floatWindow);
        *state = true;
    }
    else
    {
        if (*window != nullptr)
        {
            if (section == FloatSection::piano)
            {
                auto bounds = (*window)->getBounds();
                auto& properties = engine.getPropertyStorage().getPropertiesFile();
                properties.setValue ("pianoFloatX", bounds.getX());
                properties.setValue ("pianoFloatY", bounds.getY());
                properties.setValue ("pianoFloatW", bounds.getWidth());
                properties.setValue ("pianoFloatH", bounds.getHeight());
                properties.setValue ("pianoFloatAlwaysOnTop", pianoFloatingAlwaysOnTop);
            }

            (*window)->clearContentComponent();
            (*window)->setVisible (false);
            window->reset();
        }

        if (container->getParentComponent() != this)
            addAndMakeVisible (*container);

        *state = false;
    }

    resized();
    updateButtonsFromState();
    if (section == FloatSection::piano)
    {
        stepSequencer.repaint();
        midiPianoRoll.repaint();
    }
    repaint();
}

void BeatMakerNoRecord::closeFloatingWindows()
{
    if (workspaceSectionFloating)
        setSectionFloating (FloatSection::workspace, false);

    if (mixerSectionFloating)
        setSectionFloating (FloatSection::mixer, false);

    if (pianoSectionFloating)
        setSectionFloating (FloatSection::piano, false);
}

bool BeatMakerNoRecord::keyPressed (const juce::KeyPress& key)
{
    if (auto* keyMappings = commandManager.getKeyMappings())
        if (keyMappings->keyPressed (key, this))
            return true;

    const auto mods = key.getModifiers();
    const juce_wchar keyChar = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if ((mods.isCommandDown() || mods.isCtrlDown()) && ! mods.isShiftDown())
    {
        if (keyChar == '1')
        {
            setUiDensityMode (UiDensityMode::compact, true, true);
            return true;
        }

        if (keyChar == '2')
        {
            setUiDensityMode (UiDensityMode::comfortable, true, true);
            return true;
        }

        if (keyChar == '3')
        {
            setUiDensityMode (UiDensityMode::accessible, true, true);
            return true;
        }

        if (keyChar == '4')
        {
            setLeftDockPanelMode (LeftDockPanelMode::all, true, true);
            return true;
        }

        if (keyChar == '5')
        {
            setLeftDockPanelMode (LeftDockPanelMode::project, true, true);
            return true;
        }

        if (keyChar == '6')
        {
            setLeftDockPanelMode (LeftDockPanelMode::editing, true, true);
            return true;
        }

        if (keyChar == '7')
        {
            setLeftDockPanelMode (LeftDockPanelMode::sound, true, true);
            return true;
        }
    }

    if (mods.isAltDown() && ! mods.isCommandDown() && ! mods.isCtrlDown() && ! mods.isShiftDown())
    {
        if (keyChar == '1')
        {
            setPianoEditorLayoutMode (PianoEditorLayoutMode::split, true, true);
            return true;
        }

        if (keyChar == '2')
        {
            setPianoEditorLayoutMode (PianoEditorLayoutMode::pianoRoll, true, true);
            return true;
        }

        if (keyChar == '3')
        {
            setPianoEditorLayoutMode (PianoEditorLayoutMode::stepSequencer, true, true);
            return true;
        }
    }

    if (mods.isCommandDown() && keyChar == 'c')
    {
        copyButton.triggerClick();
        return true;
    }

    if (mods.isCommandDown() && keyChar == 'x')
    {
        cutButton.triggerClick();
        return true;
    }

    if (mods.isCommandDown() && keyChar == 'v')
    {
        pasteButton.triggerClick();
        return true;
    }

    if (mods.isCommandDown() && keyChar == 'd' && ! mods.isShiftDown())
    {
        duplicateButton.triggerClick();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        deleteButton.triggerClick();
        return true;
    }

    if (key.getTextCharacter() == '?' || key.getKeyCode() == juce::KeyPress::F1Key)
    {
        showShortcutOverlay();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::rightKey)
    {
        if (edit == nullptr)
            return true;

        const bool forward = (key.getKeyCode() == juce::KeyPress::rightKey);

        if (mods.isCommandDown() || mods.isCtrlDown())
        {
            if (editComponent != nullptr)
            {
                auto& viewState = editComponent->getEditViewState();
                const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
                moveTimelineViewportBySeconds ((forward ? 1.0 : -1.0) * visibleSeconds * 0.12);
            }
            return true;
        }

        if (mods.isShiftDown())
        {
            jumpByBar (forward);
            updateTransportInfoLabel();
            return true;
        }

        const auto now = edit->getTransport().getPosition();
        auto step = getGridDurationAt (now);
        if (mods.isAltDown())
            step = te::TimeDuration::fromSeconds (juce::jmax (0.005, step.inSeconds() * 0.25));

        const auto destination = forward ? now + step : now - step;
        edit->getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, destination.inSeconds())));
        updateTransportInfoLabel();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey)
    {
        const bool down = (key.getKeyCode() == juce::KeyPress::downKey);

        if (mods.isShiftDown())
        {
            moveSelectedTrackVertically (down);
            return true;
        }

        const double step = mods.isAltDown() ? 0.035 : 0.08;
        const double next = juce::jlimit (0.0, 1.0, verticalScrollSlider.getValue() + (down ? step : -step));
        verticalScrollSlider.setValue (next);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::pageUpKey || key.getKeyCode() == juce::KeyPress::pageDownKey)
    {
        if (editComponent == nullptr)
            return true;

        auto& viewState = editComponent->getEditViewState();
        const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
        moveTimelineViewportBySeconds ((key.getKeyCode() == juce::KeyPress::pageDownKey ? 1.0 : -1.0) * visibleSeconds * 0.7);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::homeKey)
    {
        if (edit != nullptr)
        {
            edit->getTransport().setPosition (te::TimePosition::fromSeconds (0.0));
            if (mods.isShiftDown())
                centerPlayheadInView();
            else
                updateTransportInfoLabel();
        }
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::endKey)
    {
        if (edit != nullptr)
        {
            const auto end = te::TimePosition::fromSeconds (juce::jmax (0.0, getTimelineTotalLengthSeconds()));
            edit->getTransport().setPosition (end);
            if (mods.isShiftDown())
                centerPlayheadInView();
            else
                updateTransportInfoLabel();
        }
        return true;
    }

    if (! mods.isAnyModifierKeyDown() && (keyChar == '[' || keyChar == ']'))
    {
        if (editComponent == nullptr)
            return true;

        auto& viewState = editComponent->getEditViewState();
        const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
        moveTimelineViewportBySeconds ((keyChar == ']') ? visibleSeconds * 0.22 : -visibleSeconds * 0.22);
        return true;
    }

    return false;
}

void BeatMakerNoRecord::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! leftDockScrollSlider.isVisible() || ! leftDockScrollSlider.isEnabled())
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    const auto localPos = e.getEventRelativeTo (this).position.toInt();
    if (! leftDockViewportBounds.contains (localPos) || std::abs (wheel.deltaY) < 1.0e-5f)
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    const double direction = wheel.isReversed ? -1.0 : 1.0;
    const double deltaPixels = direction * (-wheel.deltaY) * 84.0;
    const double next = juce::jlimit (leftDockScrollSlider.getMinimum(),
                                      leftDockScrollSlider.getMaximum(),
                                      leftDockScrollSlider.getValue() + deltaPixels);
    leftDockScrollSlider.setValue (next);
}

bool BeatMakerNoRecord::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const juce::File file (path);
        if (file.existsAsFile() && isSupportedDroppedFileExtension (file.getFileExtension()))
            return true;
    }

    return false;
}

void BeatMakerNoRecord::fileDragEnter (const juce::StringArray& files, int, int)
{
    if (! isInterestedInFileDrag (files))
        return;

    if (! fileDragOverlayActive)
    {
        fileDragOverlayActive = true;
        repaint();
    }
}

void BeatMakerNoRecord::fileDragExit (const juce::StringArray&)
{
    if (fileDragOverlayActive)
    {
        fileDragOverlayActive = false;
        repaint();
    }
}

void BeatMakerNoRecord::filesDropped (const juce::StringArray& files, int, int)
{
    if (fileDragOverlayActive)
    {
        fileDragOverlayActive = false;
        repaint();
    }

    if (edit == nullptr || files.isEmpty())
        return;

    auto* selectedTrack = getSelectedTrackOrFirst();
    auto ensureTargetTrack = [this, &selectedTrack] (bool midiRole) -> te::AudioTrack*
    {
        if (selectedTrack == nullptr)
            selectedTrack = appendTrackWithRole (midiRole, false);
        return selectedTrack;
    };

    auto importAudioFromFile = [this, &ensureTargetTrack] (const juce::File& file) -> bool
    {
        auto* targetTrack = ensureTargetTrack (false);
        if (targetTrack == nullptr || ! file.existsAsFile())
            return false;

        te::AudioFile audioFile (engine, file);
        if (! audioFile.isValid())
            return false;

        auto clipStart = edit->getTransport().getPosition();
        if (auto* selectedClip = getSelectedClip())
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

        const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
        const double startBeat = edit->tempoSequence.toBeats (clipStart).inBeats();
        const double snappedBeat = std::ceil ((startBeat - 1.0e-9) / beatsPerBar) * beatsPerBar;
        clipStart = edit->tempoSequence.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, snappedBeat)));

        const te::ClipPosition position { { clipStart, te::TimeDuration::fromSeconds (audioFile.getLength()) }, {} };
        if (auto clip = targetTrack->insertWaveClip (file.getFileNameWithoutExtension(), file, position, false))
        {
            applyHighQualitySettingsToAudioClip (*clip);
            selectionManager.selectOnly (clip.get());
            return true;
        }

        return false;
    };

    auto importMidiFromFile = [this, &ensureTargetTrack] (const juce::File& file) -> bool
    {
        auto* targetTrack = ensureTargetTrack (true);
        if (targetTrack == nullptr || ! file.existsAsFile())
            return false;

        if (auto clip = te::createClipFromFile (file, *targetTrack, false))
        {
            auto clipStart = edit->getTransport().getPosition();
            if (auto* selectedClip = getSelectedClip())
                if (selectedClip->getTrack() == targetTrack && selectedClip->getPosition().getEnd() > clipStart)
                    clipStart = selectedClip->getPosition().getEnd();

            const auto previousPosition = clip->getPosition();
            clip->setPosition ({ { clipStart, previousPosition.getLength() }, previousPosition.getOffset() });
            ensureTrackHasInstrumentForMidiPlayback (*targetTrack);
            selectionManager.selectOnly (clip.get());
            return true;
        }

        return false;
    };

    int importedAudioCount = 0;
    int importedMidiCount = 0;
    int failedCount = 0;

    for (const auto& path : files)
    {
        const juce::File file (path);
        if (! file.existsAsFile())
        {
            ++failedCount;
            continue;
        }

        const auto extension = file.getFileExtension().toLowerCase();

        if (isSupportedDroppedMidiExtension (extension))
        {
            if (importMidiFromFile (file))
                ++importedMidiCount;
            else
                ++failedCount;
        }
        else if (isSupportedDroppedAudioExtension (extension))
        {
            if (importAudioFromFile (file))
                ++importedAudioCount;
            else
                ++failedCount;
        }
        else
        {
            ++failedCount;
        }
    }

    if (importedAudioCount > 0 || importedMidiCount > 0)
    {
        markPlaybackRoutingNeedsPreparation();
        updateButtonsFromState();

        juce::String status;
        status << "Dropped import complete: "
               << juce::String (importedAudioCount) << " audio, "
               << juce::String (importedMidiCount) << " MIDI";

        if (failedCount > 0)
            status << " (" << juce::String (failedCount) << " skipped/failed)";

        status << ".";
        setStatus (status);
    }
    else if (failedCount > 0)
    {
        setStatus ("Dropped files were unsupported or failed to import.");
    }
}

juce::ApplicationCommandTarget* BeatMakerNoRecord::getNextCommandTarget()
{
    return nullptr;
}

void BeatMakerNoRecord::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    static const juce::CommandID commandIds[] =
    {
        appCommandSaveProject,
        appCommandSaveProjectAs,
        appCommandUndo,
        appCommandRedo,
        appCommandPlayPause,
        appCommandStop,
        appCommandReturnToStart,
        appCommandToggleLoop,
        appCommandCreateMidiClip,
        appCommandQuantizeMidi,
        appCommandFocusSelection,
        appCommandCenterPlayhead,
        appCommandFitProject,
        appCommandToolSelect,
        appCommandToolPencil,
        appCommandToolScissors,
        appCommandToolResize,
        appCommandSplitSelection,
        appCommandTransposeDown,
        appCommandTransposeUp,
        appCommandVelocityDown,
        appCommandVelocityUp,
        appCommandBounceMidiToAudio,
        appCommandToggleFloatWorkspace,
        appCommandToggleFloatMixer,
        appCommandToggleFloatPiano,
        appCommandDockAllPanels,
        appCommandStepRandomize,
        appCommandStepFourOnFloor,
        appCommandStepClear,
        appCommandStepShiftLeft,
        appCommandStepShiftRight,
        appCommandStepVaryVelocity
    };

    commands.addArray (commandIds, (int) juce::numElementsInArray (commandIds));
}

void BeatMakerNoRecord::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    constexpr auto cmd = juce::ModifierKeys::commandModifier;
    constexpr auto shift = juce::ModifierKeys::shiftModifier;
    constexpr auto alt = juce::ModifierKeys::altModifier;

    switch (commandID)
    {
        case appCommandSaveProject:
            result.setInfo ("Save Project", "Save current project", "File", 0);
            result.addDefaultKeypress ('s', cmd);
            result.setActive (edit != nullptr);
            break;
        case appCommandSaveProjectAs:
            result.setInfo ("Save Project As", "Save current project as a new file", "File", 0);
            result.addDefaultKeypress ('s', cmd | shift);
            result.setActive (edit != nullptr);
            break;
        case appCommandUndo:
            result.setInfo ("Undo", "Undo last action", "Edit", 0);
            result.addDefaultKeypress ('z', cmd);
            result.setActive (edit != nullptr);
            break;
        case appCommandRedo:
            result.setInfo ("Redo", "Redo last action", "Edit", 0);
            result.addDefaultKeypress ('z', cmd | shift);
            result.setActive (edit != nullptr);
            break;
        case appCommandPlayPause:
            result.setInfo ("Play/Pause", "Toggle transport playback", "Transport", 0);
            result.addDefaultKeypress (juce::KeyPress::spaceKey, 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandStop:
            result.setInfo ("Stop", "Stop transport", "Transport", 0);
            result.addDefaultKeypress (juce::KeyPress::spaceKey, shift);
            result.setActive (edit != nullptr);
            break;
        case appCommandReturnToStart:
            result.setInfo ("Return To Start", "Move playhead to project start", "Transport", 0);
            result.addDefaultKeypress (juce::KeyPress::homeKey, 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandToggleLoop:
            result.setInfo ("Toggle Loop", "Toggle transport loop", "Transport", 0);
            result.addDefaultKeypress ('l', 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandCreateMidiClip:
            result.setInfo ("Create MIDI Clip", "Create one-bar MIDI clip at playhead", "MIDI", 0);
            result.addDefaultKeypress ('n', 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandQuantizeMidi:
            result.setInfo ("Quantize MIDI", "Quantize selected MIDI notes", "MIDI", 0);
            result.addDefaultKeypress ('q', 0);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandFocusSelection:
            result.setInfo ("Focus Selection", "Focus timeline on selected clip", "View", 0);
            result.addDefaultKeypress ('f', 0);
            result.setActive (editComponent != nullptr);
            break;
        case appCommandCenterPlayhead:
            result.setInfo ("Center Playhead", "Center timeline around playhead", "View", 0);
            result.addDefaultKeypress ('c', 0);
            result.setActive (editComponent != nullptr);
            break;
        case appCommandFitProject:
            result.setInfo ("Fit Project", "Fit timeline to full project length", "View", 0);
            result.addDefaultKeypress ('a', 0);
            result.setActive (editComponent != nullptr);
            break;
        case appCommandToolSelect:
            result.setInfo ("Tool Select", "Switch to Select tool", "Tools", 0);
            result.addDefaultKeypress ('1', 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandToolPencil:
            result.setInfo ("Tool Pencil", "Switch to Pencil tool", "Tools", 0);
            result.addDefaultKeypress ('2', 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandToolScissors:
            result.setInfo ("Tool Scissors", "Switch to Scissors tool", "Tools", 0);
            result.addDefaultKeypress ('3', 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandToolResize:
            result.setInfo ("Tool Resize", "Switch to Resize tool", "Tools", 0);
            result.addDefaultKeypress ('4', 0);
            result.setActive (edit != nullptr);
            break;
        case appCommandSplitSelection:
            result.setInfo ("Split Selection", "Split selected clip at playhead", "Edit", 0);
            result.addDefaultKeypress ('s', 0);
            result.setActive (getSelectedClip() != nullptr);
            break;
        case appCommandTransposeDown:
            result.setInfo ("Transpose Down", "Transpose selected MIDI notes down one semitone", "MIDI", 0);
            result.addDefaultKeypress (',', 0);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandTransposeUp:
            result.setInfo ("Transpose Up", "Transpose selected MIDI notes up one semitone", "MIDI", 0);
            result.addDefaultKeypress ('.', 0);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandVelocityDown:
            result.setInfo ("Velocity Down", "Lower selected MIDI note velocities", "MIDI", 0);
            result.addDefaultKeypress ('-', 0);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandVelocityUp:
            result.setInfo ("Velocity Up", "Raise selected MIDI note velocities", "MIDI", 0);
            result.addDefaultKeypress ('=', 0);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandBounceMidiToAudio:
            result.setInfo ("Bounce MIDI To Audio", "Render selected MIDI clip to audio", "MIDI", 0);
            result.addDefaultKeypress ('b', 0);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandToggleFloatWorkspace:
            result.setInfo ("Toggle Float Workspace", "Float/dock timeline workspace", "View", 0);
            result.addDefaultKeypress ('w', 0);
            result.addDefaultKeypress ('w', cmd | shift);
            result.setActive (editComponent != nullptr);
            break;
        case appCommandToggleFloatMixer:
            result.setInfo ("Toggle Float Mixer", "Float/dock mixer section", "View", 0);
            result.addDefaultKeypress ('m', 0);
            result.addDefaultKeypress ('m', cmd | shift);
            result.setActive (editComponent != nullptr);
            break;
        case appCommandToggleFloatPiano:
            result.setInfo ("Toggle Float Piano", "Float/dock piano editor section", "View", 0);
            result.addDefaultKeypress ('p', 0);
            result.addDefaultKeypress ('p', cmd | shift);
            result.setActive (editComponent != nullptr);
            break;
        case appCommandDockAllPanels:
            result.setInfo ("Dock All Panels", "Dock all floating panels", "View", 0);
            result.addDefaultKeypress ('d', cmd | shift);
            result.setActive (editComponent != nullptr);
            break;
        case appCommandStepRandomize:
            result.setInfo ("Step Randomize", "Randomize current step sequencer page", "Step Sequencer", 0);
            result.addDefaultKeypress ('r', cmd | alt);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandStepFourOnFloor:
            result.setInfo ("Step Four-On-Floor", "Apply four-on-the-floor pattern", "Step Sequencer", 0);
            result.addDefaultKeypress ('f', cmd | alt);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandStepClear:
            result.setInfo ("Step Clear Page", "Clear current step sequencer page", "Step Sequencer", 0);
            result.addDefaultKeypress ('k', cmd | alt);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandStepShiftLeft:
            result.setInfo ("Step Shift Left", "Shift step page left by one step", "Step Sequencer", 0);
            result.addDefaultKeypress (juce::KeyPress::leftKey, cmd | alt);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandStepShiftRight:
            result.setInfo ("Step Shift Right", "Shift step page right by one step", "Step Sequencer", 0);
            result.addDefaultKeypress (juce::KeyPress::rightKey, cmd | alt);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        case appCommandStepVaryVelocity:
            result.setInfo ("Step Vary Velocity", "Apply velocity variation to active steps", "Step Sequencer", 0);
            result.addDefaultKeypress ('v', cmd | alt);
            result.setActive (getSelectedMidiClip() != nullptr);
            break;
        default:
            break;
    }
}

bool BeatMakerNoRecord::perform (const InvocationInfo& info)
{
    auto triggerIfEnabled = [] (juce::Button& button)
    {
        if (! button.isEnabled())
            return false;
        button.triggerClick();
        return true;
    };

    switch (info.commandID)
    {
        case appCommandSaveProject:       return triggerIfEnabled (saveButton);
        case appCommandSaveProjectAs:     return triggerIfEnabled (saveAsButton);
        case appCommandUndo:              return triggerIfEnabled (undoButton);
        case appCommandRedo:              return triggerIfEnabled (redoButton);
        case appCommandPlayPause:         return triggerIfEnabled (playPauseButton);
        case appCommandStop:              return triggerIfEnabled (stopButton);
        case appCommandReturnToStart:     return triggerIfEnabled (returnToStartButton);
        case appCommandToggleLoop:        return triggerIfEnabled (transportLoopButton);
        case appCommandCreateMidiClip:    return triggerIfEnabled (createMidiClipButton);
        case appCommandQuantizeMidi:      return triggerIfEnabled (quantizeButton);
        case appCommandFocusSelection:    return triggerIfEnabled (focusSelectionButton);
        case appCommandCenterPlayhead:    return triggerIfEnabled (centerPlayheadButton);
        case appCommandFitProject:        return triggerIfEnabled (fitProjectButton);

        case appCommandToolSelect:        setTimelineEditToolFromUi (TimelineEditTool::select); return true;
        case appCommandToolPencil:        setTimelineEditToolFromUi (TimelineEditTool::pencil); return true;
        case appCommandToolScissors:      setTimelineEditToolFromUi (TimelineEditTool::scissors); return true;
        case appCommandToolResize:        setTimelineEditToolFromUi (TimelineEditTool::resize); return true;
        case appCommandSplitSelection:    return triggerIfEnabled (splitButton);
        case appCommandTransposeDown:     return triggerIfEnabled (midiTransposeDownButton);
        case appCommandTransposeUp:       return triggerIfEnabled (midiTransposeUpButton);
        case appCommandVelocityDown:      return triggerIfEnabled (midiVelocityDownButton);
        case appCommandVelocityUp:        return triggerIfEnabled (midiVelocityUpButton);
        case appCommandBounceMidiToAudio: return triggerIfEnabled (midiBounceToAudioButton);
        case appCommandToggleFloatWorkspace:
            toggleSectionFloating (FloatSection::workspace);
            return true;
        case appCommandToggleFloatMixer:
            toggleSectionFloating (FloatSection::mixer);
            return true;
        case appCommandToggleFloatPiano:
            toggleSectionFloating (FloatSection::piano);
            refreshPianoFloatingWindowUi();
            return true;
        case appCommandDockAllPanels:
            if (isSectionFloating (FloatSection::workspace))
                setSectionFloating (FloatSection::workspace, false);
            if (isSectionFloating (FloatSection::mixer))
                setSectionFloating (FloatSection::mixer, false);
            if (isSectionFloating (FloatSection::piano))
                setSectionFloating (FloatSection::piano, false);
            refreshPianoFloatingWindowUi();
            return true;

        case appCommandStepRandomize:
            if (getSelectedMidiClip() == nullptr) return false;
            randomizeStepSequencerPage();
            return true;
        case appCommandStepFourOnFloor:
            if (getSelectedMidiClip() == nullptr) return false;
            applyStepSequencerFourOnFloorPattern();
            return true;
        case appCommandStepClear:
            if (getSelectedMidiClip() == nullptr) return false;
            clearStepSequencerPage();
            return true;
        case appCommandStepShiftLeft:
            if (getSelectedMidiClip() == nullptr) return false;
            shiftStepSequencerPage (-1);
            return true;
        case appCommandStepShiftRight:
            if (getSelectedMidiClip() == nullptr) return false;
            shiftStepSequencerPage (1);
            return true;
        case appCommandStepVaryVelocity:
            if (getSelectedMidiClip() == nullptr) return false;
            varyStepSequencerPageVelocities (14);
            return true;
        default:
            break;
    }

    return false;
}

juce::StringArray BeatMakerNoRecord::getMenuBarNames()
{
    return { "File", "Edit", "Transport", "Track", "Plugins", "View", "Help" };
}

juce::PopupMenu BeatMakerNoRecord::getMenuForIndex (int topLevelMenuIndex, const juce::String&)
{
    juce::PopupMenu m;
    auto addButtonItem = [&m] (int id, const juce::String& label, juce::Button& button, bool isTicked = false)
    {
        m.addItem (id, label, button.isEnabled(), isTicked);
    };

    switch (topLevelMenuIndex)
    {
        case 0:
            addButtonItem (menuFileNew, "New", newEditButton);
            addButtonItem (menuFileOpen, "Open", openEditButton);
            m.addSeparator();
            addButtonItem (menuFileSave, "Save", saveButton);
            addButtonItem (menuFileSaveAs, "Save As", saveAsButton);
            break;

        case 1:
            addButtonItem (menuEditUndo, "Undo", undoButton);
            addButtonItem (menuEditRedo, "Redo", redoButton);
            m.addSeparator();
            addButtonItem (menuEditCopy, "Copy", copyButton);
            addButtonItem (menuEditCut, "Cut", cutButton);
            addButtonItem (menuEditPaste, "Paste", pasteButton);
            m.addSeparator();
            addButtonItem (menuEditDuplicate, "Duplicate", duplicateButton);
            addButtonItem (menuEditSplit, "Split", splitButton);
            addButtonItem (menuEditDelete, "Delete", deleteButton);
            m.addSeparator();
            addButtonItem (menuEditSelectAll, "Select All", selectAllButton);
            addButtonItem (menuEditDeselectAll, "Deselect", deselectAllButton);
            m.addSeparator();
            m.addItem (menuEditToolSelect, "Tool: Select", true, getTimelineEditTool() == TimelineEditTool::select);
            m.addItem (menuEditToolPencil, "Tool: Pencil", true, getTimelineEditTool() == TimelineEditTool::pencil);
            m.addItem (menuEditToolScissors, "Tool: Scissors", true, getTimelineEditTool() == TimelineEditTool::scissors);
            m.addItem (menuEditToolResize, "Tool: Resize", true, getTimelineEditTool() == TimelineEditTool::resize);
            m.addSeparator();
            m.addItem (menuEditStepRandomize, "Step Sequencer: Randomize (Cmd/Ctrl+Alt+R)",
                       getSelectedMidiClip() != nullptr, false);
            m.addItem (menuEditStepFourOnFloor, "Step Sequencer: 4-on-Floor (Cmd/Ctrl+Alt+F)",
                       getSelectedMidiClip() != nullptr, false);
            m.addItem (menuEditStepClearPage, "Step Sequencer: Clear Page (Cmd/Ctrl+Alt+K)",
                       getSelectedMidiClip() != nullptr, false);
            m.addItem (menuEditStepShiftLeft, "Step Sequencer: Shift Left (Cmd/Ctrl+Alt+Left)",
                       getSelectedMidiClip() != nullptr, false);
            m.addItem (menuEditStepShiftRight, "Step Sequencer: Shift Right (Cmd/Ctrl+Alt+Right)",
                       getSelectedMidiClip() != nullptr, false);
            m.addItem (menuEditStepVaryVelocity, "Step Sequencer: Vary Velocity (Cmd/Ctrl+Alt+V)",
                       getSelectedMidiClip() != nullptr, false);
            m.addSeparator();
            addButtonItem (menuEditAlignClipToBar, "Align Selected Clip To Bar", audioAlignToBarButton);
            addButtonItem (menuEditLoopClip2Bars, "Make 2-Bar Loop From Selected Clip", audioMake2BarLoopButton);
            addButtonItem (menuEditLoopClip4Bars, "Make 4-Bar Loop From Selected Clip", audioMake4BarLoopButton);
            addButtonItem (menuEditFillTransportLoop, "Fill Transport Loop With Selected Clip", audioFillTransportLoopButton);
            break;

        case 2:
            addButtonItem (menuTransportPlayPause, "Play / Pause", playPauseButton);
            addButtonItem (menuTransportStop, "Stop", stopButton);
            addButtonItem (menuTransportReturnStart, "Return To Start", returnToStartButton);
            addButtonItem (menuTransportLoopToggle, "Toggle Loop", transportLoopButton, edit != nullptr && edit->getTransport().looping);
            addButtonItem (menuTransportLoopSelection, "Loop To Selection", setLoopToSelectionButton);
            m.addSeparator();
            addButtonItem (menuTransportPrevBar, "Jump Previous Bar", jumpPrevBarButton);
            addButtonItem (menuTransportNextBar, "Jump Next Bar", jumpNextBarButton);
            m.addSeparator();
            m.addItem (menuTransportAudioQuality, "Apply High-Quality Audio Mode", true, false);
            break;

        case 3:
            addButtonItem (menuTrackAdd, "Add Audio Track", addTrackButton);
            addButtonItem (menuTrackAddMidi, "Add MIDI Track", addMidiTrackButton);
            addButtonItem (menuTrackAddFloatingSynth, "Add Floating Synth Track", addFloatingSynthTrackButton);
            addButtonItem (menuTrackDuplicate, "Duplicate Track", duplicateTrackButton);
            addButtonItem (menuTrackRename, "Rename Track", renameTrackButton);
            m.addSeparator();
            addButtonItem (menuTrackImportAudio, "Import Audio Clip", importAudioButton);
            addButtonItem (menuTrackImportMidi, "Import MIDI Clip", importMidiButton);
            addButtonItem (menuTrackCreateMidi, "Create MIDI Clip", createMidiClipButton);
            addButtonItem (menuTrackBounceMidi, "Bounce Selected MIDI To Audio", midiBounceToAudioButton);
            m.addSeparator();
            addButtonItem (menuTrackGenChords, "Generate Chord Progression", midiGenerateChordsButton);
            addButtonItem (menuTrackGenArp, "Generate Arpeggio", midiGenerateArpButton);
            addButtonItem (menuTrackGenBass, "Generate Bassline", midiGenerateBassButton);
            addButtonItem (menuTrackGenDrums, "Generate Drum Pattern", midiGenerateDrumsButton);
            m.addSeparator();
            addButtonItem (menuTrackPreviewDirectory, "Chord/Scale Directory: Preview", chordDirectoryPreviewButton);
            addButtonItem (menuTrackApplyDirectory, "Chord/Scale Directory: Apply To Clip", chordDirectoryApplyButton);
            addButtonItem (menuTrackExportDirectoryMidi, "Chord/Scale Directory: Export MIDI", chordDirectoryExportMidiButton);
            addButtonItem (menuTrackExportDirectoryWav, "Chord/Scale Directory: Export WAV", chordDirectoryExportWavButton);
            break;

        case 4:
            addButtonItem (menuPluginsScan, "Scan Plugins", fxScanButton);
            addButtonItem (menuPluginsScanSkipped, "Scan Skipped Plugins", fxScanSkippedButton);
            addButtonItem (menuPluginsPrepPlayback, "Prepare MIDI/FX Playback", fxPrepPlaybackButton);
            addButtonItem (menuPluginsOpenUi, "Open Selected Plugin UI", fxOpenEditorButton);
            m.addSeparator();
            m.addItem (menuPluginsAddBundledNova, "Add Bundled Sampledex Nova Synth",
                       fxAddExternalInstrumentButton.isEnabled(), false);
            addButtonItem (menuPluginsAddInstrument, "Add AU/VST3 Instrument", fxAddExternalInstrumentButton);
            addButtonItem (menuPluginsAddFx, "Add AU/VST3 Effect", fxAddExternalButton);
            m.addSeparator();
            addButtonItem (menuPluginsAddSampler, "Add Sampler", fxAddSamplerButton);
            addButtonItem (menuPluginsAdd4Osc, "Add 4OSC", fxAddSynthButton);
            addButtonItem (menuPluginsAddEq, "Add EQ", fxAddEQButton);
            addButtonItem (menuPluginsAddComp, "Add Compressor", fxAddCompButton);
            addButtonItem (menuPluginsAddReverb, "Add Reverb", fxAddReverbButton);
            addButtonItem (menuPluginsAddDelay, "Add Delay", fxAddDelayButton);
            m.addSeparator();
            addButtonItem (menuPluginsSynthInit, "4OSC Preset: Init", builtInSynthPresetInitButton);
            addButtonItem (menuPluginsSynthWarmPad, "4OSC Preset: Warm Pad", builtInSynthPresetWarmPadButton);
            addButtonItem (menuPluginsSynthPunchBass, "4OSC Preset: Punch Bass", builtInSynthPresetPunchBassButton);
            addButtonItem (menuPluginsSynthBrightPluck, "4OSC Preset: Bright Pluck", builtInSynthPresetBrightPluckButton);
            break;

        case 5:
            addButtonItem (menuViewMarkers, "Show/Hide Marker Track", showMarkerTrackButton,
                           showMarkerTrackButton.getButtonText().containsIgnoreCase ("On"));
            addButtonItem (menuViewArranger, "Show/Hide Arranger Track", showArrangerTrackButton,
                           showArrangerTrackButton.getButtonText().containsIgnoreCase ("On"));
            m.addSeparator();
            addButtonItem (menuViewZoomIn, "Zoom In", zoomInButton);
            addButtonItem (menuViewZoomOut, "Zoom Out", zoomOutButton);
            addButtonItem (menuViewZoomReset, "Reset Zoom", zoomResetButton);
            m.addSeparator();
            addButtonItem (menuViewFocusSelection, "Focus Selection", focusSelectionButton);
            addButtonItem (menuViewCenterPlayhead, "Center Playhead", centerPlayheadButton);
            addButtonItem (menuViewFitProject, "Fit Project", fitProjectButton);
            m.addSeparator();
            m.addItem (menuViewFloatWorkspace, "Float Timeline/Tracks Window (W / Cmd+Shift+W)", true, isSectionFloating (FloatSection::workspace));
            m.addItem (menuViewFloatMixer, "Float Mixer Window (M / Cmd+Shift+M)", true, isSectionFloating (FloatSection::mixer));
            m.addItem (menuViewFloatPiano, "Float Piano Roll Window (P / Cmd+Shift+P)", true, isSectionFloating (FloatSection::piano));
            m.addItem (menuViewDockAllPanels, "Dock All Floating Panels (Cmd+Shift+D)", true,
                       ! isSectionFloating (FloatSection::workspace)
                       && ! isSectionFloating (FloatSection::mixer)
                       && ! isSectionFloating (FloatSection::piano));
            m.addSeparator();
            m.addItem (menuViewPianoModeSplit, "Piano Editor: Split (Alt+1)", true,
                       getPianoEditorLayoutModeSelection() == PianoEditorLayoutMode::split);
            m.addItem (menuViewPianoModePiano, "Piano Editor: Piano (Alt+2)", true,
                       getPianoEditorLayoutModeSelection() == PianoEditorLayoutMode::pianoRoll);
            m.addItem (menuViewPianoModeSteps, "Piano Editor: Steps (Alt+3)", true,
                       getPianoEditorLayoutModeSelection() == PianoEditorLayoutMode::stepSequencer);
            m.addSeparator();
            m.addItem (menuViewLayoutMidiFocus, "Workspace Preset: MIDI Producer Focus", true, false);
            m.addItem (menuViewLayoutAudioFocus, "Workspace Preset: Audio Producer Focus", true, false);
            m.addItem (menuViewLayoutHybridFocus, "Workspace Preset: Hybrid Producer Focus", true, false);
            m.addSeparator();
            m.addItem (menuViewUiCompact, "UI Density: Compact (Cmd/Ctrl+1)", true, uiDensityMode == UiDensityMode::compact);
            m.addItem (menuViewUiComfortable, "UI Density: Comfortable (Cmd/Ctrl+2)", true, uiDensityMode == UiDensityMode::comfortable);
            m.addItem (menuViewUiAccessible, "UI Density: Accessible (Cmd/Ctrl+3)", true, uiDensityMode == UiDensityMode::accessible);
            m.addSeparator();
            m.addItem (menuViewPanelsAll, "Panels: All", true, getLeftDockPanelModeSelection() == LeftDockPanelMode::all);
            m.addItem (menuViewPanelsProject, "Panels: Project", true, getLeftDockPanelModeSelection() == LeftDockPanelMode::project);
            m.addItem (menuViewPanelsEditing, "Panels: Editing", true, getLeftDockPanelModeSelection() == LeftDockPanelMode::editing);
            m.addItem (menuViewPanelsSound, "Panels: Sound", true, getLeftDockPanelModeSelection() == LeftDockPanelMode::sound);
            break;

        case 6:
            m.addItem (menuHelpShortcuts, "Shortcuts & Gestures", true, false);
            break;

        default:
            break;
    }

    return m;
}

void BeatMakerNoRecord::menuItemSelected (int menuItemID, int)
{
    switch (menuItemID)
    {
        case menuFileNew: newEditButton.triggerClick(); break;
        case menuFileOpen: openEditButton.triggerClick(); break;
        case menuFileSave: saveButton.triggerClick(); break;
        case menuFileSaveAs: saveAsButton.triggerClick(); break;

        case menuEditUndo: undoButton.triggerClick(); break;
        case menuEditRedo: redoButton.triggerClick(); break;
        case menuEditCopy: copyButton.triggerClick(); break;
        case menuEditCut: cutButton.triggerClick(); break;
        case menuEditPaste: pasteButton.triggerClick(); break;
        case menuEditDuplicate: duplicateButton.triggerClick(); break;
        case menuEditSplit: commandManager.invokeDirectly (appCommandSplitSelection, true); break;
        case menuEditDelete: deleteButton.triggerClick(); break;
        case menuEditSelectAll: selectAllButton.triggerClick(); break;
        case menuEditDeselectAll: deselectAllButton.triggerClick(); break;
        case menuEditToolSelect: setTimelineEditToolFromUi (TimelineEditTool::select); break;
        case menuEditToolPencil: setTimelineEditToolFromUi (TimelineEditTool::pencil); break;
        case menuEditToolScissors: setTimelineEditToolFromUi (TimelineEditTool::scissors); break;
        case menuEditToolResize: setTimelineEditToolFromUi (TimelineEditTool::resize); break;
        case menuEditStepRandomize: commandManager.invokeDirectly (appCommandStepRandomize, true); break;
        case menuEditStepFourOnFloor: commandManager.invokeDirectly (appCommandStepFourOnFloor, true); break;
        case menuEditStepClearPage: commandManager.invokeDirectly (appCommandStepClear, true); break;
        case menuEditStepShiftLeft: commandManager.invokeDirectly (appCommandStepShiftLeft, true); break;
        case menuEditStepShiftRight: commandManager.invokeDirectly (appCommandStepShiftRight, true); break;
        case menuEditStepVaryVelocity: commandManager.invokeDirectly (appCommandStepVaryVelocity, true); break;
        case menuEditAlignClipToBar: audioAlignToBarButton.triggerClick(); break;
        case menuEditLoopClip2Bars: audioMake2BarLoopButton.triggerClick(); break;
        case menuEditLoopClip4Bars: audioMake4BarLoopButton.triggerClick(); break;
        case menuEditFillTransportLoop: audioFillTransportLoopButton.triggerClick(); break;

        case menuTransportPlayPause: playPauseButton.triggerClick(); break;
        case menuTransportStop: stopButton.triggerClick(); break;
        case menuTransportReturnStart: returnToStartButton.triggerClick(); break;
        case menuTransportLoopToggle: transportLoopButton.triggerClick(); break;
        case menuTransportLoopSelection: setLoopToSelectionButton.triggerClick(); break;
        case menuTransportPrevBar: jumpPrevBarButton.triggerClick(); break;
        case menuTransportNextBar: jumpNextBarButton.triggerClick(); break;
        case menuTransportAudioQuality: applyHighQualityAudioMode(); break;

        case menuTrackAdd: addTrackButton.triggerClick(); break;
        case menuTrackAddMidi: addMidiTrackButton.triggerClick(); break;
        case menuTrackAddFloatingSynth: addFloatingSynthTrackButton.triggerClick(); break;
        case menuTrackDuplicate: duplicateTrackButton.triggerClick(); break;
        case menuTrackRename: renameTrackButton.triggerClick(); break;
        case menuTrackImportAudio: importAudioButton.triggerClick(); break;
        case menuTrackImportMidi: importMidiButton.triggerClick(); break;
        case menuTrackCreateMidi: createMidiClipButton.triggerClick(); break;
        case menuTrackBounceMidi: commandManager.invokeDirectly (appCommandBounceMidiToAudio, true); break;
        case menuTrackGenChords: midiGenerateChordsButton.triggerClick(); break;
        case menuTrackGenArp: midiGenerateArpButton.triggerClick(); break;
        case menuTrackGenBass: midiGenerateBassButton.triggerClick(); break;
        case menuTrackGenDrums: midiGenerateDrumsButton.triggerClick(); break;
        case menuTrackPreviewDirectory: chordDirectoryPreviewButton.triggerClick(); break;
        case menuTrackApplyDirectory: chordDirectoryApplyButton.triggerClick(); break;
        case menuTrackExportDirectoryMidi: chordDirectoryExportMidiButton.triggerClick(); break;
        case menuTrackExportDirectoryWav: chordDirectoryExportWavButton.triggerClick(); break;

        case menuPluginsScan: fxScanButton.triggerClick(); break;
        case menuPluginsScanSkipped: fxScanSkippedButton.triggerClick(); break;
        case menuPluginsPrepPlayback: fxPrepPlaybackButton.triggerClick(); break;
        case menuPluginsOpenUi: fxOpenEditorButton.triggerClick(); break;
        case menuPluginsAddBundledNova: addBundledNovaSynthToSelectedTrack(); break;
        case menuPluginsAddInstrument: fxAddExternalInstrumentButton.triggerClick(); break;
        case menuPluginsAddFx: fxAddExternalButton.triggerClick(); break;
        case menuPluginsAddSampler: fxAddSamplerButton.triggerClick(); break;
        case menuPluginsAdd4Osc: fxAddSynthButton.triggerClick(); break;
        case menuPluginsAddEq: fxAddEQButton.triggerClick(); break;
        case menuPluginsAddComp: fxAddCompButton.triggerClick(); break;
        case menuPluginsAddReverb: fxAddReverbButton.triggerClick(); break;
        case menuPluginsAddDelay: fxAddDelayButton.triggerClick(); break;
        case menuPluginsSynthInit: builtInSynthPresetInitButton.triggerClick(); break;
        case menuPluginsSynthWarmPad: builtInSynthPresetWarmPadButton.triggerClick(); break;
        case menuPluginsSynthPunchBass: builtInSynthPresetPunchBassButton.triggerClick(); break;
        case menuPluginsSynthBrightPluck: builtInSynthPresetBrightPluckButton.triggerClick(); break;

        case menuViewMarkers: showMarkerTrackButton.triggerClick(); break;
        case menuViewArranger: showArrangerTrackButton.triggerClick(); break;
        case menuViewZoomIn: zoomInButton.triggerClick(); break;
        case menuViewZoomOut: zoomOutButton.triggerClick(); break;
        case menuViewZoomReset: zoomResetButton.triggerClick(); break;
        case menuViewFocusSelection: focusSelectionButton.triggerClick(); break;
        case menuViewCenterPlayhead: centerPlayheadButton.triggerClick(); break;
        case menuViewFitProject: fitProjectButton.triggerClick(); break;
        case menuViewFloatWorkspace: commandManager.invokeDirectly (appCommandToggleFloatWorkspace, true); break;
        case menuViewFloatMixer: commandManager.invokeDirectly (appCommandToggleFloatMixer, true); break;
        case menuViewFloatPiano: commandManager.invokeDirectly (appCommandToggleFloatPiano, true); break;
        case menuViewDockAllPanels: commandManager.invokeDirectly (appCommandDockAllPanels, true); break;
        case menuViewPianoModeSplit: setPianoEditorLayoutMode (PianoEditorLayoutMode::split, true, true); break;
        case menuViewPianoModePiano: setPianoEditorLayoutMode (PianoEditorLayoutMode::pianoRoll, true, true); break;
        case menuViewPianoModeSteps: setPianoEditorLayoutMode (PianoEditorLayoutMode::stepSequencer, true, true); break;
        case menuViewLayoutMidiFocus:
            setLeftDockPanelMode (LeftDockPanelMode::editing, true, true);
            setPianoEditorLayoutMode (PianoEditorLayoutMode::split, true, true);
            if (! isSectionFloating (FloatSection::piano))
                setSectionFloating (FloatSection::piano, true);
            break;
        case menuViewLayoutAudioFocus:
            setLeftDockPanelMode (LeftDockPanelMode::project, true, true);
            if (isSectionFloating (FloatSection::piano))
                setSectionFloating (FloatSection::piano, false);
            if (! isSectionFloating (FloatSection::workspace))
                setSectionFloating (FloatSection::workspace, true);
            break;
        case menuViewLayoutHybridFocus:
            setLeftDockPanelMode (LeftDockPanelMode::all, true, true);
            setPianoEditorLayoutMode (PianoEditorLayoutMode::split, true, true);
            break;
        case menuViewUiCompact: setUiDensityMode (UiDensityMode::compact, true, true); break;
        case menuViewUiComfortable: setUiDensityMode (UiDensityMode::comfortable, true, true); break;
        case menuViewUiAccessible: setUiDensityMode (UiDensityMode::accessible, true, true); break;
        case menuViewPanelsAll: setLeftDockPanelMode (LeftDockPanelMode::all, true, true); break;
        case menuViewPanelsProject: setLeftDockPanelMode (LeftDockPanelMode::project, true, true); break;
        case menuViewPanelsEditing: setLeftDockPanelMode (LeftDockPanelMode::editing, true, true); break;
        case menuViewPanelsSound: setLeftDockPanelMode (LeftDockPanelMode::sound, true, true); break;

        case menuHelpShortcuts: showShortcutOverlay(); break;
        default: break;
    }
}

void BeatMakerNoRecord::setupSliders()
{
    trackVolumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    trackVolumeSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 62, 24);
    trackVolumeSlider.setRange (-60.0, 12.0, 0.1);
    trackVolumeSlider.setDoubleClickReturnValue (true, 0.0);

    trackPanSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    trackPanSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 62, 24);
    trackPanSlider.setRange (-1.0, 1.0, 0.01);
    trackPanSlider.setDoubleClickReturnValue (true, 0.0);

    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 62, 24);
    tempoSlider.setRange (60.0, 200.0, 0.1);
    tempoSlider.setDoubleClickReturnValue (true, 120.0);

    trackHeightSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    trackHeightSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 24);
    trackHeightSlider.setRange (28.0, 140.0, 1.0);
    trackHeightSlider.setDoubleClickReturnValue (true, 58.0);
    trackHeightSlider.setValue (58.0, juce::dontSendNotification);

    leftDockScrollSlider.setSliderStyle (juce::Slider::LinearVertical);
    leftDockScrollSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    leftDockScrollSlider.setRange (0.0, 0.0, 1.0);
    leftDockScrollSlider.setValue (0.0, juce::dontSendNotification);
    leftDockScrollSlider.setScrollWheelEnabled (false);

    horizontalScrollSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    horizontalScrollSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    horizontalScrollSlider.setRange (0.0, 1.0, 0.0001);
    horizontalScrollSlider.setValue (0.0, juce::dontSendNotification);

    verticalScrollSlider.setSliderStyle (juce::Slider::LinearVertical);
    verticalScrollSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    verticalScrollSlider.setRange (0.0, 1.0, 0.0001);
    verticalScrollSlider.setValue (0.0, juce::dontSendNotification);

    auto setupBuiltInSynthSlider = [] (juce::Slider& slider, double defaultValue)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 24);
        slider.setRange (0.0, 1.0, 0.0001);
        slider.setDoubleClickReturnValue (true, defaultValue);
        slider.setValue (defaultValue, juce::dontSendNotification);
    };

    setupBuiltInSynthSlider (builtInSynthCutoffSlider, 0.82);
    setupBuiltInSynthSlider (builtInSynthResonanceSlider, 0.12);
    setupBuiltInSynthSlider (builtInSynthEnvSlider, 0.58);
    setupBuiltInSynthSlider (builtInSynthDriveSlider, 0.04);
    setupBuiltInSynthSlider (builtInSynthAttackSlider, 0.02);
    setupBuiltInSynthSlider (builtInSynthReleaseSlider, 0.14);
    setupBuiltInSynthSlider (builtInSynthReverbSlider, 0.08);
    setupBuiltInSynthSlider (builtInSynthDelaySlider, 0.00);
    setupBuiltInSynthSlider (builtInSynthOsc1Slider, 0.88);
    setupBuiltInSynthSlider (builtInSynthOsc2Slider, 0.68);
    setupBuiltInSynthSlider (builtInSynthOsc3Slider, 0.54);
    setupBuiltInSynthSlider (builtInSynthOsc4Slider, 0.36);
    setupBuiltInSynthSlider (builtInSynthUnisonSlider, 0.28);
    setupBuiltInSynthSlider (builtInSynthWidthSlider, 0.66);
    setupBuiltInSynthSlider (builtInSynthChorusSlider, 0.18);
    setupBuiltInSynthSlider (builtInSynthMasterSlider, 0.74);

    {
        int id = 1;
        for (const auto& q : te::QuantisationType::getAvailableQuantiseTypes (false))
            quantizeTypeBox.addItem (q, id++);

        if (quantizeTypeBox.getNumItems() > 0)
            quantizeTypeBox.setSelectedId (1, juce::dontSendNotification);
    }

    gridBox.addItem ("1 Bar", 1);
    gridBox.addItem ("1/2", 2);
    gridBox.addItem ("1/4", 3);
    gridBox.addItem ("1/8", 4);
    gridBox.addItem ("1/16", 5);
    gridBox.addItem ("1/32", 6);
    gridBox.setSelectedId (5, juce::dontSendNotification);

    chordDirectoryRootBox.addItem ("C", 1);
    chordDirectoryRootBox.addItem ("C#", 2);
    chordDirectoryRootBox.addItem ("D", 3);
    chordDirectoryRootBox.addItem ("D#", 4);
    chordDirectoryRootBox.addItem ("E", 5);
    chordDirectoryRootBox.addItem ("F", 6);
    chordDirectoryRootBox.addItem ("F#", 7);
    chordDirectoryRootBox.addItem ("G", 8);
    chordDirectoryRootBox.addItem ("G#", 9);
    chordDirectoryRootBox.addItem ("A", 10);
    chordDirectoryRootBox.addItem ("A#", 11);
    chordDirectoryRootBox.addItem ("B", 12);
    chordDirectoryRootBox.setSelectedId (1, juce::dontSendNotification);

    chordDirectoryScaleBox.addItem ("Major (Ionian)", 1);
    chordDirectoryScaleBox.addItem ("Natural Minor (Aeolian)", 2);
    chordDirectoryScaleBox.addItem ("Dorian", 3);
    chordDirectoryScaleBox.addItem ("Phrygian", 4);
    chordDirectoryScaleBox.addItem ("Lydian", 5);
    chordDirectoryScaleBox.addItem ("Mixolydian", 6);
    chordDirectoryScaleBox.addItem ("Locrian", 7);
    chordDirectoryScaleBox.addItem ("Harmonic Minor", 8);
    chordDirectoryScaleBox.addItem ("Melodic Minor", 9);
    chordDirectoryScaleBox.addItem ("Major Pentatonic", 10);
    chordDirectoryScaleBox.addItem ("Minor Pentatonic", 11);
    chordDirectoryScaleBox.addItem ("Blues Minor", 12);
    chordDirectoryScaleBox.setSelectedId (1, juce::dontSendNotification);

    chordDirectoryProgressionBox.addItem ("Pop: I-V-vi-IV", 1);
    chordDirectoryProgressionBox.addItem ("Anthem: vi-IV-I-V", 2);
    chordDirectoryProgressionBox.addItem ("Minor: i-VI-III-VII", 3);
    chordDirectoryProgressionBox.addItem ("House: I-vi-IV-V", 4);
    chordDirectoryProgressionBox.addItem ("Soul: ii-V-I-vi", 5);
    chordDirectoryProgressionBox.addItem ("Trap: i-VII-VI-VII", 6);
    chordDirectoryProgressionBox.addItem ("Cinematic: i-iv-VI-V", 7);
    chordDirectoryProgressionBox.addItem ("Circle 8: I-IV-vii-iii-vi-ii-V-I", 8);
    chordDirectoryProgressionBox.addItem ("Scale Walk: I-ii-iii-IV-V-vi-vii-I", 9);
    chordDirectoryProgressionBox.addItem ("Two-Chord Pump: I-V", 10);
    chordDirectoryProgressionBox.setSelectedId (1, juce::dontSendNotification);

    chordDirectoryBarsBox.addItem ("8", 1);
    chordDirectoryBarsBox.addItem ("12", 2);
    chordDirectoryBarsBox.addItem ("16", 3);
    chordDirectoryBarsBox.addItem ("24", 4);
    chordDirectoryBarsBox.addItem ("32", 5);
    chordDirectoryBarsBox.addItem ("48", 6);
    chordDirectoryBarsBox.addItem ("64", 7);
    chordDirectoryBarsBox.setSelectedId (3, juce::dontSendNotification);

    chordDirectoryTimeSignatureBox.addItem ("4/4", 1);
    chordDirectoryTimeSignatureBox.addItem ("3/4", 2);
    chordDirectoryTimeSignatureBox.addItem ("5/4", 3);
    chordDirectoryTimeSignatureBox.addItem ("6/8", 4);
    chordDirectoryTimeSignatureBox.addItem ("7/8", 5);
    chordDirectoryTimeSignatureBox.addItem ("12/8", 6);
    chordDirectoryTimeSignatureBox.setSelectedId (1, juce::dontSendNotification);

    for (int octave = 1; octave <= 7; ++octave)
        chordDirectoryOctaveBox.addItem (juce::String (octave), octave);
    chordDirectoryOctaveBox.setSelectedId (3, juce::dontSendNotification);

    chordDirectoryVoicingBox.addItem ("Triads", 1);
    chordDirectoryVoicingBox.addItem ("Seventh Chords", 2);
    chordDirectoryVoicingBox.addItem ("Wide Triads", 3);
    chordDirectoryVoicingBox.addItem ("Arp Pulse", 4);
    chordDirectoryVoicingBox.setSelectedId (1, juce::dontSendNotification);

    chordDirectoryDensityBox.addItem ("1 Chord/Bar", 1);
    chordDirectoryDensityBox.addItem ("2 Chords/Bar", 2);
    chordDirectoryDensityBox.addItem ("4 Chords/Bar", 3);
    chordDirectoryDensityBox.addItem ("8th Arp Flow", 4);
    chordDirectoryDensityBox.setSelectedId (1, juce::dontSendNotification);

    chordDirectoryPreviewPresetBox.addItem ("Keep Current Patch", 1);
    chordDirectoryPreviewPresetBox.addItem ("Init", 2);
    chordDirectoryPreviewPresetBox.addItem ("Warm Pad", 3);
    chordDirectoryPreviewPresetBox.addItem ("Punch Bass", 4);
    chordDirectoryPreviewPresetBox.addItem ("Bright Pluck", 5);
    chordDirectoryPreviewPresetBox.setSelectedId (3, juce::dontSendNotification);

    chordDirectoryVelocitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    chordDirectoryVelocitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 24);
    chordDirectoryVelocitySlider.setRange (24.0, 127.0, 1.0);
    chordDirectoryVelocitySlider.setDoubleClickReturnValue (true, 98.0);
    chordDirectoryVelocitySlider.setValue (98.0, juce::dontSendNotification);
    chordDirectoryVelocitySlider.setNumDecimalPlacesToDisplay (0);

    chordDirectorySwingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    chordDirectorySwingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 24);
    chordDirectorySwingSlider.setRange (0.0, 0.45, 0.001);
    chordDirectorySwingSlider.setDoubleClickReturnValue (true, 0.0);
    chordDirectorySwingSlider.setValue (0.0, juce::dontSendNotification);
    chordDirectorySwingSlider.setTextValueSuffix ("%");
    chordDirectorySwingSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value * 100.0)) + "%";
    };

    if (leftDockPanelTabs.getNumTabs() == 0)
    {
        leftDockPanelTabs.addTab ("All", juce::Colour::fromRGB (49, 76, 108), -1);
        leftDockPanelTabs.addTab ("Project", juce::Colour::fromRGB (52, 88, 109), -1);
        leftDockPanelTabs.addTab ("Editing", juce::Colour::fromRGB (62, 90, 120), -1);
        leftDockPanelTabs.addTab ("Sound", juce::Colour::fromRGB (57, 96, 116), -1);
    }

    if (midiToolsTabs.getNumTabs() == 0)
    {
        midiToolsTabs.addTab ("Editor", juce::Colour::fromRGB (52, 86, 119), -1);
        midiToolsTabs.addTab ("Chord Dir", juce::Colour::fromRGB (55, 95, 118), -1);
    }
    midiToolsTabs.setCurrentTabIndex (0, false);

    if (pianoEditorModeTabs.getNumTabs() == 0)
    {
        pianoEditorModeTabs.addTab ("Split", juce::Colour::fromRGB (51, 86, 121), -1);
        pianoEditorModeTabs.addTab ("Piano", juce::Colour::fromRGB (49, 83, 116), -1);
        pianoEditorModeTabs.addTab ("Steps", juce::Colour::fromRGB (54, 87, 115), -1);
    }
    pianoEditorModeTabs.setCurrentTabIndex (getTabIndexForPianoEditorLayoutMode (PianoEditorLayoutMode::split), false);

    leftDockPanelModeBox.addItem (getLeftDockPanelModeDisplayName (LeftDockPanelMode::all),
                                  getComboIdForLeftDockPanelMode (LeftDockPanelMode::all));
    leftDockPanelModeBox.addItem (getLeftDockPanelModeDisplayName (LeftDockPanelMode::project),
                                  getComboIdForLeftDockPanelMode (LeftDockPanelMode::project));
    leftDockPanelModeBox.addItem (getLeftDockPanelModeDisplayName (LeftDockPanelMode::editing),
                                  getComboIdForLeftDockPanelMode (LeftDockPanelMode::editing));
    leftDockPanelModeBox.addItem (getLeftDockPanelModeDisplayName (LeftDockPanelMode::sound),
                                  getComboIdForLeftDockPanelMode (LeftDockPanelMode::sound));
    leftDockPanelModeBox.setSelectedId (getComboIdForLeftDockPanelMode (LeftDockPanelMode::all),
                                        juce::dontSendNotification);

    defaultSynthModeBox.addItem (getDefaultSynthModeDisplayName (DefaultSynthMode::autoPreferExternal),
                                 getComboIdForDefaultSynthMode (DefaultSynthMode::autoPreferExternal));
    defaultSynthModeBox.addItem (getDefaultSynthModeDisplayName (DefaultSynthMode::force4Osc),
                                 getComboIdForDefaultSynthMode (DefaultSynthMode::force4Osc));
    defaultSynthModeBox.addItem (getDefaultSynthModeDisplayName (DefaultSynthMode::forceExternalVst3),
                                 getComboIdForDefaultSynthMode (DefaultSynthMode::forceExternalVst3));
    defaultSynthModeBox.setSelectedId (getComboIdForDefaultSynthMode (DefaultSynthMode::autoPreferExternal),
                                       juce::dontSendNotification);

    {
        const auto storedMode = engine.getPropertyStorage().getPropertiesFile().getValue (
            "defaultSynthMode",
            getDefaultSynthModeStorageValue (DefaultSynthMode::autoPreferExternal));
        const auto mode = getDefaultSynthModeForStorageValue (storedMode);
        defaultSynthModeBox.setSelectedId (getComboIdForDefaultSynthMode (mode), juce::dontSendNotification);
    }

    {
        const auto storedPanelMode = engine.getPropertyStorage().getPropertiesFile().getValue (
            "leftDockPanelMode",
            getLeftDockPanelModeStorageValue (LeftDockPanelMode::all));
        const auto panelMode = getLeftDockPanelModeForStorageValue (storedPanelMode);
        leftDockPanelModeBox.setSelectedId (getComboIdForLeftDockPanelMode (panelMode), juce::dontSendNotification);
        leftDockPanelTabs.setCurrentTabIndex (getComboIdForLeftDockPanelMode (panelMode) - 1, false);
    }

    {
        const auto storedEditorMode = engine.getPropertyStorage().getPropertiesFile().getValue (
            "pianoEditorLayoutMode",
            getPianoEditorLayoutModeStorageValue (PianoEditorLayoutMode::split));
        const auto editorMode = getPianoEditorLayoutModeForStorageValue (storedEditorMode);
        pianoEditorModeTabs.setCurrentTabIndex (getTabIndexForPianoEditorLayoutMode (editorMode), false);
    }

    {
        auto& properties = engine.getPropertyStorage().getPropertiesFile();
        leftDockWidthRatio = (float) juce::jlimit (0.20, 0.52, properties.getDoubleValue ("layoutLeftDockRatio", leftDockWidthRatio));
        workspaceMixerWidthRatio = (float) juce::jlimit (0.40, 0.86, properties.getDoubleValue ("layoutWorkspaceMixerRatio", workspaceMixerWidthRatio));
        workspaceBottomHeightRatio = (float) juce::jlimit (0.34, 0.78, properties.getDoubleValue ("layoutWorkspaceBottomRatio", workspaceBottomHeightRatio));
        mixerPianoHeightRatio = (float) juce::jlimit (0.24, 0.72, properties.getDoubleValue ("layoutMixerPianoRatio", mixerPianoHeightRatio));
        pianoStepHeightRatio = (float) juce::jlimit (0.30, 0.72, properties.getDoubleValue ("layoutPianoStepRatio", pianoStepHeightRatio));
        uiDensityMode = getUiDensityModeForStorageValue (properties.getValue ("uiDensityMode",
                                                                                getUiDensityStorageValue (UiDensityMode::comfortable)));
    }

    pianoFloatingAlwaysOnTop = engine.getPropertyStorage().getPropertiesFile().getBoolValue ("pianoFloatAlwaysOnTop", true);
    pianoAlwaysOnTopButton.setToggleState (pianoFloatingAlwaysOnTop, juce::dontSendNotification);

    editNameLabel.setJustificationType (juce::Justification::centredLeft);
    transportInfoLabel.setJustificationType (juce::Justification::centredRight);
    workflowStateLabel.setJustificationType (juce::Justification::centredLeft);
    selectedTrackLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setComponentID ("status-label");
    contextHintLabel.setJustificationType (juce::Justification::centredRight);
    gridLabel.setJustificationType (juce::Justification::centredLeft);
    editToolLabel.setJustificationType (juce::Justification::centredLeft);
    leftDockPanelModeLabel.setJustificationType (juce::Justification::centredLeft);
    defaultSynthModeLabel.setJustificationType (juce::Justification::centredLeft);
    fxChainLabel.setJustificationType (juce::Justification::centredLeft);
    trackHeightLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthTargetLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthCutoffLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthResonanceLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthEnvLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthDriveLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthAttackLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthReleaseLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthReverbLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthDelayLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthOsc1Label.setJustificationType (juce::Justification::centredLeft);
    builtInSynthOsc2Label.setJustificationType (juce::Justification::centredLeft);
    builtInSynthOsc3Label.setJustificationType (juce::Justification::centredLeft);
    builtInSynthOsc4Label.setJustificationType (juce::Justification::centredLeft);
    builtInSynthUnisonLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthWidthLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthChorusLabel.setJustificationType (juce::Justification::centredLeft);
    builtInSynthMasterLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryRootLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryScaleLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryProgressionLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryBarsLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryTimeSignatureLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryOctaveLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryVoicingLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryDensityLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryPreviewPresetLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectoryVelocityLabel.setJustificationType (juce::Justification::centredLeft);
    chordDirectorySwingLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    editNameLabel.setMinimumHorizontalScale (0.76f);
    transportInfoLabel.setMinimumHorizontalScale (0.72f);
    workflowStateLabel.setMinimumHorizontalScale (0.68f);
    selectedTrackLabel.setMinimumHorizontalScale (0.78f);
    statusLabel.setMinimumHorizontalScale (0.75f);
    contextHintLabel.setMinimumHorizontalScale (0.72f);
    editNameLabel.setBorderSize ({ 1, 8, 1, 8 });
    transportInfoLabel.setBorderSize ({ 1, 8, 1, 8 });
    workflowStateLabel.setBorderSize ({ 1, 8, 1, 8 });
    selectedTrackLabel.setBorderSize ({ 1, 8, 1, 8 });
    statusLabel.setBorderSize ({ 1, 8, 1, 8 });
    contextHintLabel.setBorderSize ({ 1, 8, 1, 8 });

    topMenuBar.setColour (juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB (12, 18, 28).withAlpha (0.98f));
    topMenuBar.setColour (juce::PopupMenu::textColourId, juce::Colours::white.withAlpha (0.96f));
    topMenuBar.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour::fromRGB (57, 140, 214).withAlpha (0.90f));
    topMenuBar.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white.withAlpha (0.99f));

    sessionGroup.setText ("Project + Transport");
    arrangementGroup.setText ("Arrangement Navigation");
    trackGroup.setText ("Track Management");
    clipEditGroup.setText ("Clip Tools");
    midiEditGroup.setText ("MIDI Tools");
    audioEditGroup.setText ("Audio Tools");
    fxGroup.setText ("Instrument + FX Rack");
    mixerGroup.setText ("Track Inspector");
    workspaceGroup.setText ("Timeline");
    mixerAreaGroup.setText ("Mixer");
    channelRackGroup.setText ("Channel Rack");
    inspectorGroup.setText ("Mixer Inspector");
    builtInSynthGroup.setText ("Built-in Synth (4OSC)");
    // The editor surfaces render their own chrome and headers.
    stepSequencerGroup.setText ({});
    pianoRollGroup.setText ({});

    auto styleGroup = [] (juce::GroupComponent& group, juce::Colour outline, juce::Colour text)
    {
        group.setColour (juce::GroupComponent::outlineColourId, outline);
        group.setColour (juce::GroupComponent::textColourId, text);
    };

    styleGroup (sessionGroup, juce::Colour::fromRGB (90, 126, 172).withAlpha (0.60f), juce::Colours::white.withAlpha (0.95f));
    styleGroup (arrangementGroup, juce::Colour::fromRGB (109, 143, 188).withAlpha (0.56f), juce::Colours::white.withAlpha (0.94f));
    styleGroup (trackGroup, juce::Colour::fromRGB (101, 170, 174).withAlpha (0.52f), juce::Colours::white.withAlpha (0.93f));
    styleGroup (clipEditGroup, juce::Colour::fromRGB (164, 133, 201).withAlpha (0.53f), juce::Colours::white.withAlpha (0.93f));
    styleGroup (midiEditGroup, juce::Colour::fromRGB (97, 152, 209).withAlpha (0.56f), juce::Colours::white.withAlpha (0.94f));
    styleGroup (audioEditGroup, juce::Colour::fromRGB (95, 154, 176).withAlpha (0.54f), juce::Colours::white.withAlpha (0.93f));
    styleGroup (fxGroup, juce::Colour::fromRGB (241, 161, 95).withAlpha (0.52f), juce::Colours::white.withAlpha (0.94f));
    styleGroup (mixerGroup, juce::Colour::fromRGB (108, 153, 215).withAlpha (0.55f), juce::Colours::white.withAlpha (0.94f));
    styleGroup (workspaceGroup, juce::Colour::fromRGB (88, 153, 216).withAlpha (0.64f), juce::Colours::white.withAlpha (0.96f));
    styleGroup (mixerAreaGroup, juce::Colour::fromRGB (95, 173, 203).withAlpha (0.61f), juce::Colours::white.withAlpha (0.95f));
    styleGroup (channelRackGroup, juce::Colour::fromRGB (97, 163, 214).withAlpha (0.61f), juce::Colours::white.withAlpha (0.95f));
    styleGroup (inspectorGroup, juce::Colour::fromRGB (102, 148, 210).withAlpha (0.60f), juce::Colours::white.withAlpha (0.94f));
    styleGroup (builtInSynthGroup, juce::Colour::fromRGB (244, 176, 102).withAlpha (0.55f), juce::Colours::white.withAlpha (0.95f));
    styleGroup (stepSequencerGroup, juce::Colours::transparentBlack, juce::Colours::transparentWhite);
    styleGroup (pianoRollGroup, juce::Colours::transparentBlack, juce::Colours::transparentWhite);

    auto styleButtonSet = [] (std::initializer_list<juce::Button*> buttons,
                              juce::Colour bg,
                              juce::Colour bgOn,
                              juce::Colour text)
    {
        for (auto* b : buttons)
        {
            b->setColour (juce::TextButton::buttonColourId, bg);
            b->setColour (juce::TextButton::buttonOnColourId, bgOn);
            b->setColour (juce::TextButton::textColourOffId, text);
            b->setColour (juce::TextButton::textColourOnId, text);
        }
    };

    const auto buttonBg = juce::Colour::fromRGB (34, 48, 69);
    const auto buttonBgOn = juce::Colour::fromRGB (58, 152, 227);
    const auto buttonText = juce::Colours::white.withAlpha (0.95f);
    styleButtonSet ({ &newEditButton, &openEditButton, &saveButton, &saveAsButton, &undoButton, &redoButton, &helpButton,
                      &focusSelectionButton, &centerPlayheadButton, &fitProjectButton,
                      &playPauseButton, &stopButton, &returnToStartButton, &transportLoopButton,
                      &setLoopToSelectionButton, &jumpPrevBarButton, &jumpNextBarButton, &zoomInButton, &zoomOutButton, &zoomResetButton,
                      &showMarkerTrackButton, &showArrangerTrackButton, &addMarkerButton, &prevMarkerButton, &nextMarkerButton,
                      &loopMarkersButton, &addSectionButton, &prevSectionButton, &nextSectionButton, &loopSectionButton,
                      &addTrackButton, &addMidiTrackButton, &addFloatingSynthTrackButton, &moveTrackUpButton, &moveTrackDownButton,
                      &duplicateTrackButton, &colorTrackButton, &renameTrackButton,
                      &importAudioButton, &importMidiButton, &createMidiClipButton,
                      &editToolSelectButton, &editToolPencilButton, &editToolScissorsButton, &editToolResizeButton,
                      &copyButton, &cutButton, &pasteButton, &deleteButton,
                      &duplicateButton, &splitButton, &trimStartButton, &trimEndButton, &moveStartToCursorButton, &moveEndToCursorButton,
                      &nudgeLeftButton, &nudgeRightButton, &slipLeftButton, &slipRightButton, &moveToPrevButton, &moveToNextButton,
                      &toggleClipLoopButton, &renameClipButton, &selectAllButton, &deselectAllButton, &splitAllTracksButton,
                      &insertBarButton, &deleteBarButton, &quantizeButton, &midiTransposeDownButton, &midiTransposeUpButton,
                      &midiOctaveDownButton, &midiOctaveUpButton, &midiVelocityDownButton, &midiVelocityUpButton,
                      &midiHumanizeTimingButton, &midiHumanizeVelocityButton, &midiLegatoButton, &midiBounceToAudioButton,
                      &midiGenerateChordsButton, &midiGenerateArpButton, &midiGenerateBassButton, &midiGenerateDrumsButton,
                      &chordDirectoryPreviewButton, &chordDirectoryApplyButton, &chordDirectoryExportMidiButton, &chordDirectoryExportWavButton,
                      &audioGainDownButton, &audioGainUpButton,
                      &audioFadeInButton, &audioFadeOutButton, &audioClearFadesButton, &audioReverseButton, &audioSpeedDownButton,
                      &audioSpeedUpButton, &audioPitchDownButton, &audioPitchUpButton, &audioAutoTempoButton, &audioWarpButton,
                      &audioAlignToBarButton, &audioMake2BarLoopButton, &audioMake4BarLoopButton, &audioFillTransportLoopButton,
                      &fxRefreshButton, &fxScanButton, &fxScanSkippedButton, &fxPrepPlaybackButton, &fxAddSamplerButton, &fxAddSynthButton, &fxAddEQButton, &fxAddCompButton,
                      &fxAddReverbButton, &fxAddDelayButton, &fxAddExternalInstrumentButton, &fxAddExternalButton,
                      &fxOpenEditorButton, &fxMoveUpButton, &fxMoveDownButton, &fxBypassButton, &fxDeleteButton,
                      &channelRackAddInstrumentButton, &channelRackAddFxButton, &channelRackOpenPluginButton,
                      &pianoFloatToggleButton, &pianoEnsureInstrumentButton, &pianoOpenInstrumentButton,
                      &builtInSynthPresetInitButton, &builtInSynthPresetWarmPadButton,
                      &builtInSynthPresetPunchBassButton, &builtInSynthPresetBrightPluckButton },
                    buttonBg, buttonBgOn, buttonText);

    const auto utilityBg = juce::Colour::fromRGB (48, 60, 81);
    const auto utilityBgOn = juce::Colour::fromRGB (97, 136, 186);
    styleButtonSet ({ &newEditButton, &openEditButton, &saveButton, &saveAsButton, &undoButton, &redoButton },
                    utilityBg, utilityBgOn, buttonText);

    const auto transportBg = juce::Colour::fromRGB (31, 76, 111);
    const auto transportBgOn = juce::Colour::fromRGB (64, 173, 238);
    styleButtonSet ({ &playPauseButton, &stopButton, &returnToStartButton, &transportLoopButton,
                      &setLoopToSelectionButton, &zoomInButton, &zoomOutButton, &zoomResetButton },
                    transportBg, transportBgOn, buttonText);

    const auto createBg = juce::Colour::fromRGB (57, 83, 87);
    const auto createBgOn = juce::Colour::fromRGB (89, 176, 181);
    styleButtonSet ({ &addTrackButton, &addMidiTrackButton, &addFloatingSynthTrackButton,
                      &importAudioButton, &importMidiButton, &createMidiClipButton,
                      &addMarkerButton, &addSectionButton, &fxAddSamplerButton, &fxAddSynthButton,
                      &fxAddEQButton, &fxAddCompButton, &fxAddReverbButton, &fxAddDelayButton,
                      &fxAddExternalInstrumentButton, &fxAddExternalButton,
                      &pianoEnsureInstrumentButton, &pianoOpenInstrumentButton,
                      &audioMake2BarLoopButton, &audioMake4BarLoopButton, &audioFillTransportLoopButton,
                      &midiGenerateChordsButton, &midiGenerateArpButton, &midiGenerateBassButton, &midiGenerateDrumsButton,
                      &chordDirectoryPreviewButton, &chordDirectoryApplyButton, &chordDirectoryExportMidiButton, &chordDirectoryExportWavButton },
                    createBg, createBgOn, buttonText);

    const auto toolBg = juce::Colour::fromRGB (49, 72, 97);
    const auto toolBgOn = juce::Colour::fromRGB (94, 182, 242);
    styleButtonSet ({ &editToolSelectButton, &editToolPencilButton, &editToolScissorsButton, &editToolResizeButton },
                    toolBg, toolBgOn, buttonText);

    styleButtonSet ({ &pianoFloatToggleButton }, toolBg, toolBgOn, buttonText);

    const auto scanBg = juce::Colour::fromRGB (75, 66, 52);
    const auto scanBgOn = juce::Colour::fromRGB (172, 137, 80);
    styleButtonSet ({ &fxRefreshButton, &fxScanButton, &fxScanSkippedButton, &fxPrepPlaybackButton, &fxOpenEditorButton },
                    scanBg, scanBgOn, buttonText);

    const auto synthBg = juce::Colour::fromRGB (70, 70, 104);
    const auto synthBgOn = juce::Colour::fromRGB (131, 131, 213);
    styleButtonSet ({ &builtInSynthPresetInitButton, &builtInSynthPresetWarmPadButton,
                      &builtInSynthPresetPunchBassButton, &builtInSynthPresetBrightPluckButton },
                    synthBg, synthBgOn, buttonText);

    const auto helpBg = juce::Colour::fromRGB (69, 74, 112);
    const auto helpBgOn = juce::Colour::fromRGB (129, 153, 232);
    styleButtonSet ({ &helpButton }, helpBg, helpBgOn, buttonText);

    const auto quickNavBg = juce::Colour::fromRGB (45, 71, 101);
    const auto quickNavBgOn = juce::Colour::fromRGB (84, 169, 237);
    styleButtonSet ({ &focusSelectionButton, &centerPlayheadButton, &fitProjectButton },
                    quickNavBg, quickNavBgOn, buttonText);

    const auto dangerBg = juce::Colour::fromRGB (96, 48, 56);
    const auto dangerBgOn = juce::Colour::fromRGB (184, 88, 102);
    styleButtonSet ({ &deleteButton, &deleteBarButton, &fxDeleteButton }, dangerBg, dangerBgOn, buttonText);

    trackMuteButton.setColour (juce::ToggleButton::tickColourId, juce::Colour::fromRGB (90, 209, 142));
    trackMuteButton.setColour (juce::ToggleButton::textColourId, buttonText);
    trackSoloButton.setColour (juce::ToggleButton::tickColourId, juce::Colour::fromRGB (247, 197, 82));
    trackSoloButton.setColour (juce::ToggleButton::textColourId, buttonText);
    pianoAlwaysOnTopButton.setColour (juce::ToggleButton::tickColourId, juce::Colour::fromRGB (125, 188, 244));
    pianoAlwaysOnTopButton.setColour (juce::ToggleButton::textColourId, buttonText);

    const auto comboBg = juce::Colour::fromRGB (21, 30, 44);
    const auto comboOutline = juce::Colour::fromRGB (88, 124, 166);
    for (auto* combo : { &quantizeTypeBox, &gridBox, &leftDockPanelModeBox, &defaultSynthModeBox, &fxChainBox, &channelRackTrackBox, &channelRackPluginBox,
                         &chordDirectoryRootBox, &chordDirectoryScaleBox, &chordDirectoryProgressionBox, &chordDirectoryBarsBox,
                         &chordDirectoryTimeSignatureBox, &chordDirectoryOctaveBox, &chordDirectoryVoicingBox,
                         &chordDirectoryDensityBox, &chordDirectoryPreviewPresetBox })
    {
        combo->setColour (juce::ComboBox::backgroundColourId, comboBg);
        combo->setColour (juce::ComboBox::outlineColourId, comboOutline);
        combo->setColour (juce::ComboBox::textColourId, buttonText);
        combo->setColour (juce::ComboBox::arrowColourId, juce::Colour::fromRGB (245, 178, 102));
    }

    for (auto* tabs : { &leftDockPanelTabs, &midiToolsTabs, &pianoEditorModeTabs })
    {
        tabs->setColour (juce::TabbedButtonBar::tabTextColourId, juce::Colours::white.withAlpha (0.84f));
        tabs->setColour (juce::TabbedButtonBar::frontTextColourId, juce::Colours::white.withAlpha (0.98f));
        tabs->setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colour::fromRGB (83, 117, 160).withAlpha (0.86f));
        tabs->setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colour::fromRGB (246, 181, 104).withAlpha (0.96f));
    }

    for (auto* slider : { &trackVolumeSlider, &trackPanSlider, &tempoSlider, &trackHeightSlider, &leftDockScrollSlider, &horizontalScrollSlider, &verticalScrollSlider,
                          &builtInSynthCutoffSlider, &builtInSynthResonanceSlider, &builtInSynthEnvSlider, &builtInSynthDriveSlider,
                          &builtInSynthAttackSlider, &builtInSynthReleaseSlider, &builtInSynthReverbSlider, &builtInSynthDelaySlider,
                          &builtInSynthOsc1Slider, &builtInSynthOsc2Slider, &builtInSynthOsc3Slider, &builtInSynthOsc4Slider,
                          &builtInSynthUnisonSlider, &builtInSynthWidthSlider, &builtInSynthChorusSlider, &builtInSynthMasterSlider,
                          &chordDirectoryVelocitySlider, &chordDirectorySwingSlider })
    {
        slider->setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (70, 100, 139));
        slider->setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (242, 179, 101));
        slider->setColour (juce::Slider::textBoxTextColourId, buttonText);
        slider->setColour (juce::Slider::textBoxOutlineColourId, comboOutline);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, comboBg);
    }

    for (auto* label : { &editNameLabel, &transportInfoLabel, &workflowStateLabel, &selectedTrackLabel, &statusLabel, &contextHintLabel, &gridLabel,
                         &editToolLabel, &leftDockPanelModeLabel,
                         &defaultSynthModeLabel,
                         &fxChainLabel, &trackHeightLabel, &trackVolumeLabel, &trackPanLabel,
                         &tempoLabel, &channelRackTrackLabel, &channelRackPluginLabel,
                         &inspectorTrackNameLabel, &inspectorRouteLabel, &inspectorPluginLabel, &inspectorMeterLabel,
                         &builtInSynthTargetLabel,
                         &builtInSynthCutoffLabel, &builtInSynthResonanceLabel,
                         &builtInSynthEnvLabel, &builtInSynthDriveLabel,
                         &builtInSynthAttackLabel, &builtInSynthReleaseLabel,
                         &builtInSynthReverbLabel, &builtInSynthDelayLabel,
                         &builtInSynthOsc1Label, &builtInSynthOsc2Label, &builtInSynthOsc3Label, &builtInSynthOsc4Label,
                         &builtInSynthUnisonLabel, &builtInSynthWidthLabel, &builtInSynthChorusLabel, &builtInSynthMasterLabel,
                         &chordDirectoryRootLabel, &chordDirectoryScaleLabel, &chordDirectoryProgressionLabel,
                         &chordDirectoryBarsLabel, &chordDirectoryTimeSignatureLabel, &chordDirectoryOctaveLabel,
                         &chordDirectoryVoicingLabel, &chordDirectoryDensityLabel, &chordDirectoryPreviewPresetLabel,
                         &chordDirectoryVelocityLabel, &chordDirectorySwingLabel })
        label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));

    editNameLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.97f));
    transportInfoLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (141, 212, 255).withAlpha (0.95f));
    workflowStateLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (186, 228, 255).withAlpha (0.94f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (255, 196, 120));
    contextHintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.70f));

    editNameLabel.setTooltip ("Current project file name.");
    transportInfoLabel.setTooltip ("Live transport HUD: time, bar/beat, tempo, zoom and recording policy state.");
    workflowStateLabel.setTooltip ("Live workflow summary: selected track, active clip context, and track instrument/FX state.");
    statusLabel.setTooltip ("Latest action feedback and warnings.");
    leftDockScrollSlider.setTooltip ("Scroll left control dock when all sections are not visible.");
    horizontalScrollSlider.setTooltip ("Timeline horizontal scroll.");
    verticalScrollSlider.setTooltip ("Track area vertical scroll.");

    newEditButton.setTooltip ("Create a new project with default startup state (MIDI instrument-ready).");
    openEditButton.setTooltip ("Open an existing .tracktionedit project.");
    saveButton.setTooltip ("Save the current project (Cmd+S).");
    saveAsButton.setTooltip ("Save project to a new location (Shift+Cmd+S).");
    undoButton.setTooltip ("Undo last action (Cmd+Z).");
    redoButton.setTooltip ("Redo last undone action (Shift+Cmd+Z).");
    focusSelectionButton.setTooltip ("Zoom timeline to selected clip. If no clip is selected, center around playhead. Shortcut: F");
    centerPlayheadButton.setTooltip ("Center timeline viewport around the current playhead. Shortcut: C");
    fitProjectButton.setTooltip ("Fit timeline viewport to the current project length. Shortcut: A");
    editToolSelectButton.setTooltip ("Select/Move tool. Shortcut: 1");
    editToolPencilButton.setTooltip ("Pencil tool for drawing MIDI clips/notes. Shortcut: 2");
    editToolScissorsButton.setTooltip ("Scissors tool for splitting clips/notes. Shortcut: 3");
    editToolResizeButton.setTooltip ("Resize tool for fast clip/note length edits. Shortcut: 4");

    playPauseButton.setTooltip ("Toggle playback (Space).");
    stopButton.setTooltip ("Stop playback and keep playhead position.");
    returnToStartButton.setTooltip ("Move playhead to 0:00.");
    transportLoopButton.setTooltip ("Toggle transport looping.");
    setLoopToSelectionButton.setTooltip ("Set loop range to selected clip.");
    zoomInButton.setTooltip ("Zoom timeline in.");
    zoomOutButton.setTooltip ("Zoom timeline out.");
    zoomResetButton.setTooltip ("Reset timeline zoom to default.");

    addTrackButton.setTooltip ("Add a new audio track.");
    addMidiTrackButton.setTooltip ("Add a new MIDI track with default instrument routing.");
    addFloatingSynthTrackButton.setTooltip ("Create a MIDI track with a floating synth workflow and open it in piano roll.");
    pianoFloatToggleButton.setTooltip ("Float or dock the piano roll + step sequencer section.");
    pianoEnsureInstrumentButton.setTooltip ("Ensure current MIDI context has a playable instrument.");
    pianoOpenInstrumentButton.setTooltip ("Open instrument UI for selected MIDI clip/track.");
    pianoAlwaysOnTopButton.setTooltip ("Keep floating piano roll window above other windows.");
    leftDockPanelModeBox.setTooltip ("Choose major left-dock panel set: All, Project, Editing, or Sound.");
    defaultSynthModeBox.setTooltip ("Default instrument policy for MIDI tracks and auto instrument insertion.");
    importAudioButton.setTooltip ("Import audio to selected track at bar-aligned position; loop-shaped files auto-arm transport loop.");
    importMidiButton.setTooltip ("Import MIDI file onto selected/first track.");
    createMidiClipButton.setTooltip ("Create a one-bar MIDI clip at playhead.");

    splitButton.setTooltip ("Split selected clip at playhead.");
    duplicateButton.setTooltip ("Duplicate selected clip.");
    deleteButton.setTooltip ("Delete current selection (Delete/Backspace).");
    quantizeButton.setTooltip ("Quantize selected MIDI clip using chosen grid.");
    midiBounceToAudioButton.setTooltip ("Render selected MIDI clip to audio on the same track.");
    midiGenerateChordsButton.setTooltip ("Generate a musical chord progression into the selected MIDI clip.");
    midiGenerateArpButton.setTooltip ("Generate an arpeggiated pattern into the selected MIDI clip.");
    midiGenerateBassButton.setTooltip ("Generate a bassline pattern into the selected MIDI clip.");
    midiGenerateDrumsButton.setTooltip ("Generate a drum groove (kick/snare/hats) into the selected MIDI clip.");
    chordDirectoryRootBox.setTooltip ("Chord/scale directory key root. Includes all 12 keys.");
    chordDirectoryScaleBox.setTooltip ("Choose scale/mode template for the directory generator.");
    chordDirectoryProgressionBox.setTooltip ("Choose progression family used for generated chord movement.");
    chordDirectoryBarsBox.setTooltip ("Directory pattern length in bars (8 to 64).");
    chordDirectoryTimeSignatureBox.setTooltip ("Target time signature for generated pattern and exports.");
    chordDirectoryOctaveBox.setTooltip ("Base register for generated chords and arps.");
    chordDirectoryVoicingBox.setTooltip ("Chord voicing style used when generating notes.");
    chordDirectoryDensityBox.setTooltip ("How often chord movement happens within each bar.");
    chordDirectoryPreviewPresetBox.setTooltip ("Internal synth preview patch for directory previews.");
    chordDirectoryVelocitySlider.setTooltip ("Base MIDI velocity used by chord directory generation.");
    chordDirectorySwingSlider.setTooltip ("Swing amount applied to alternating directory events.");
    chordDirectoryPreviewButton.setTooltip ("Generate + audition the directory pattern through an internal synth.");
    chordDirectoryApplyButton.setTooltip ("Write the directory pattern into the selected MIDI clip.");
    chordDirectoryExportMidiButton.setTooltip ("Export current directory selection as a MIDI file.");
    chordDirectoryExportWavButton.setTooltip ("Render current directory selection to WAV in Exports.");
    gridBox.setTooltip ("Editing grid used by fades and timeline actions.");
    audioAlignToBarButton.setTooltip ("Snap selected clip start to nearest bar.");
    audioMake2BarLoopButton.setTooltip ("Build a 2-bar beat loop from selected clip and arm transport loop.");
    audioMake4BarLoopButton.setTooltip ("Build a 4-bar beat loop from selected clip and arm transport loop.");
    audioFillTransportLoopButton.setTooltip ("Repeat selected clip to fill current transport loop range.");

    fxScanButton.setTooltip ("Open plugin scanner for AU/VST3 discovery.");
    fxScanSkippedButton.setTooltip ("Rescan only plugins previously skipped because they timed out.");
    fxPrepPlaybackButton.setTooltip ("Auto-fix MIDI playback chains and plugin ordering.");
    fxOpenEditorButton.setTooltip ("Open selected plugin editor window.");
    fxAddExternalInstrumentButton.setTooltip ("Insert scanned AU/VST3 instrument.");
    fxAddExternalButton.setTooltip ("Insert scanned AU/VST3 effect plugin.");
    channelRackTrackBox.setTooltip ("Channel rack track selector linked with timeline and mixer selection.");
    channelRackPluginBox.setTooltip ("Per-track plugin lane linked with FX chain selection.");
    channelRackAddInstrumentButton.setTooltip ("Insert AU/VST3 instrument on selected rack track.");
    channelRackAddFxButton.setTooltip ("Insert AU/VST3 effect on selected rack track.");
    channelRackOpenPluginButton.setTooltip ("Open plugin UI for selected rack plugin.");
    builtInSynthTargetLabel.setTooltip ("Selected built-in 4OSC target on the current track.");
    builtInSynthPresetInitButton.setTooltip ("Apply a neutral 4OSC baseline patch.");
    builtInSynthPresetWarmPadButton.setTooltip ("Apply a wide, slow-attack warm pad.");
    builtInSynthPresetPunchBassButton.setTooltip ("Apply a tight punch bass patch.");
    builtInSynthPresetBrightPluckButton.setTooltip ("Apply a bright pluck patch.");
    builtInSynthCutoffSlider.setTooltip ("4OSC filter cutoff.");
    builtInSynthResonanceSlider.setTooltip ("4OSC filter resonance.");
    builtInSynthEnvSlider.setTooltip ("4OSC filter envelope amount.");
    builtInSynthDriveSlider.setTooltip ("4OSC distortion amount.");
    builtInSynthAttackSlider.setTooltip ("4OSC amp envelope attack.");
    builtInSynthReleaseSlider.setTooltip ("4OSC amp envelope release.");
    builtInSynthReverbSlider.setTooltip ("4OSC reverb mix.");
    builtInSynthDelaySlider.setTooltip ("4OSC delay mix.");
    builtInSynthOsc1Slider.setTooltip ("4OSC oscillator 1 level.");
    builtInSynthOsc2Slider.setTooltip ("4OSC oscillator 2 level.");
    builtInSynthOsc3Slider.setTooltip ("4OSC oscillator 3 level.");
    builtInSynthOsc4Slider.setTooltip ("4OSC oscillator 4 level.");
    builtInSynthUnisonSlider.setTooltip ("4OSC unison/detune spread macro.");
    builtInSynthWidthSlider.setTooltip ("4OSC stereo width macro.");
    builtInSynthChorusSlider.setTooltip ("4OSC chorus mix.");
    builtInSynthMasterSlider.setTooltip ("4OSC master level.");

    applyUiDensityToControlSizing();

}

juce::String BeatMakerNoRecord::getLeftDockPanelModeStorageValue (LeftDockPanelMode mode)
{
    switch (mode)
    {
        case LeftDockPanelMode::project: return "project";
        case LeftDockPanelMode::editing: return "editing";
        case LeftDockPanelMode::sound:   return "sound";
        case LeftDockPanelMode::all:
        default:                         return "all";
    }
}

juce::String BeatMakerNoRecord::getLeftDockPanelModeDisplayName (LeftDockPanelMode mode)
{
    switch (mode)
    {
        case LeftDockPanelMode::project: return "Project Panels";
        case LeftDockPanelMode::editing: return "Editing Panels";
        case LeftDockPanelMode::sound:   return "Sound Panels";
        case LeftDockPanelMode::all:
        default:                         return "All Panels";
    }
}

int BeatMakerNoRecord::getComboIdForLeftDockPanelMode (LeftDockPanelMode mode)
{
    switch (mode)
    {
        case LeftDockPanelMode::all:     return 1;
        case LeftDockPanelMode::project: return 2;
        case LeftDockPanelMode::editing: return 3;
        case LeftDockPanelMode::sound:   return 4;
    }

    return 1;
}

auto BeatMakerNoRecord::getLeftDockPanelModeForStorageValue (const juce::String& value) -> LeftDockPanelMode
{
    const auto normalised = value.trim().toLowerCase();
    if (normalised == "project")
        return LeftDockPanelMode::project;
    if (normalised == "editing")
        return LeftDockPanelMode::editing;
    if (normalised == "sound")
        return LeftDockPanelMode::sound;
    return LeftDockPanelMode::all;
}

auto BeatMakerNoRecord::getLeftDockPanelModeForComboId (int comboId) -> LeftDockPanelMode
{
    switch (comboId)
    {
        case 2:  return LeftDockPanelMode::project;
        case 3:  return LeftDockPanelMode::editing;
        case 4:  return LeftDockPanelMode::sound;
        case 1:
        default: return LeftDockPanelMode::all;
    }
}

auto BeatMakerNoRecord::getLeftDockPanelModeSelection() const -> LeftDockPanelMode
{
    return getLeftDockPanelModeForComboId (leftDockPanelModeBox.getSelectedId());
}

void BeatMakerNoRecord::setLeftDockPanelMode (LeftDockPanelMode mode, bool persist, bool announceStatus)
{
    const auto comboId = getComboIdForLeftDockPanelMode (mode);
    if (leftDockPanelModeBox.getSelectedId() != comboId)
        leftDockPanelModeBox.setSelectedId (comboId, juce::dontSendNotification);

    const int tabIndex = comboId - 1;
    if (leftDockPanelTabs.getCurrentTabIndex() != tabIndex)
        leftDockPanelTabs.setCurrentTabIndex (tabIndex, false);

    if (persist)
    {
        auto& properties = engine.getPropertyStorage().getPropertiesFile();
        properties.setValue ("leftDockPanelMode", getLeftDockPanelModeStorageValue (mode));
    }

    if (announceStatus)
        setStatus ("Left dock panel set: " + getLeftDockPanelModeDisplayName (mode) + ".");

    resized();
}

juce::String BeatMakerNoRecord::getPianoEditorLayoutModeStorageValue (PianoEditorLayoutMode mode)
{
    switch (mode)
    {
        case PianoEditorLayoutMode::pianoRoll:     return "piano";
        case PianoEditorLayoutMode::stepSequencer: return "steps";
        case PianoEditorLayoutMode::split:
        default:                                   return "split";
    }
}

int BeatMakerNoRecord::getTabIndexForPianoEditorLayoutMode (PianoEditorLayoutMode mode)
{
    switch (mode)
    {
        case PianoEditorLayoutMode::split:         return 0;
        case PianoEditorLayoutMode::pianoRoll:     return 1;
        case PianoEditorLayoutMode::stepSequencer: return 2;
    }

    return 0;
}

auto BeatMakerNoRecord::getPianoEditorLayoutModeForStorageValue (const juce::String& value) -> PianoEditorLayoutMode
{
    const auto normalised = value.trim().toLowerCase();
    if (normalised == "piano" || normalised == "pianoroll")
        return PianoEditorLayoutMode::pianoRoll;
    if (normalised == "steps" || normalised == "step")
        return PianoEditorLayoutMode::stepSequencer;
    return PianoEditorLayoutMode::split;
}

auto BeatMakerNoRecord::getPianoEditorLayoutModeForTabIndex (int tabIndex) -> PianoEditorLayoutMode
{
    switch (tabIndex)
    {
        case 1:  return PianoEditorLayoutMode::pianoRoll;
        case 2:  return PianoEditorLayoutMode::stepSequencer;
        case 0:
        default: return PianoEditorLayoutMode::split;
    }
}

auto BeatMakerNoRecord::getPianoEditorLayoutModeSelection() const -> PianoEditorLayoutMode
{
    return getPianoEditorLayoutModeForTabIndex (pianoEditorModeTabs.getCurrentTabIndex());
}

void BeatMakerNoRecord::setPianoEditorLayoutMode (PianoEditorLayoutMode mode, bool persist, bool announceStatus)
{
    const auto previousMode = getPianoEditorLayoutModeSelection();

    const int tabIndex = getTabIndexForPianoEditorLayoutMode (mode);
    if (pianoEditorModeTabs.getCurrentTabIndex() != tabIndex)
        pianoEditorModeTabs.setCurrentTabIndex (tabIndex, false);

    if (persist)
    {
        auto& properties = engine.getPropertyStorage().getPropertiesFile();
        properties.setValue ("pianoEditorLayoutMode", getPianoEditorLayoutModeStorageValue (mode));
    }

    if (announceStatus)
    {
        juce::String label ("Split");
        if (mode == PianoEditorLayoutMode::pianoRoll)
            label = "Piano";
        else if (mode == PianoEditorLayoutMode::stepSequencer)
            label = "Steps";

        setStatus ("Piano editor view: " + label + ".");
    }

    if (previousMode != mode)
    {
        if (mode == PianoEditorLayoutMode::pianoRoll || mode == PianoEditorLayoutMode::split)
        {
            resetStepSequencerDragState();
        }

        if (mode == PianoEditorLayoutMode::stepSequencer || mode == PianoEditorLayoutMode::split)
        {
            resetPianoRollNoteDragState();
            clearPianoRollNavigationInteraction();
        }
    }

    resized();
    updateButtonsFromState();
    stepSequencer.repaint();
    midiPianoRoll.repaint();
}

juce::String BeatMakerNoRecord::getUiDensityStorageValue (UiDensityMode mode)
{
    switch (mode)
    {
        case UiDensityMode::compact: return "compact";
        case UiDensityMode::accessible: return "accessible";
        case UiDensityMode::comfortable:
        default: return "comfortable";
    }
}

juce::String BeatMakerNoRecord::getUiDensityDisplayName (UiDensityMode mode)
{
    switch (mode)
    {
        case UiDensityMode::compact: return "Compact";
        case UiDensityMode::accessible: return "Accessible";
        case UiDensityMode::comfortable:
        default: return "Comfortable";
    }
}

auto BeatMakerNoRecord::getUiDensityModeForStorageValue (const juce::String& value) -> UiDensityMode
{
    const auto normalised = value.trim().toLowerCase();
    if (normalised == "compact")
        return UiDensityMode::compact;
    if (normalised == "accessible")
        return UiDensityMode::accessible;
    return UiDensityMode::comfortable;
}

float BeatMakerNoRecord::getUiDensityScale() const
{
    switch (uiDensityMode)
    {
        case UiDensityMode::compact: return 0.90f;
        case UiDensityMode::accessible: return 1.16f;
        case UiDensityMode::comfortable:
        default: return 1.00f;
    }
}

void BeatMakerNoRecord::applyUiDensityToControlSizing()
{
    const bool compact = uiDensityMode == UiDensityMode::compact;
    const bool accessible = uiDensityMode == UiDensityMode::accessible;

    const int mainTextHeight = accessible ? 28 : (compact ? 22 : 24);
    const int mainTextWidth = accessible ? 72 : (compact ? 56 : 62);
    const int trackHeightTextWidth = accessible ? 70 : (compact ? 54 : 60);
    const int synthTextHeight = accessible ? 28 : (compact ? 22 : 24);
    const int synthTextWidth = accessible ? 62 : (compact ? 48 : 52);

    trackVolumeSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, mainTextWidth, mainTextHeight);
    trackPanSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, mainTextWidth, mainTextHeight);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, mainTextWidth, mainTextHeight);
    trackHeightSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, trackHeightTextWidth, mainTextHeight);

    for (auto* slider : { &builtInSynthCutoffSlider, &builtInSynthResonanceSlider, &builtInSynthEnvSlider, &builtInSynthDriveSlider,
                          &builtInSynthAttackSlider, &builtInSynthReleaseSlider, &builtInSynthReverbSlider, &builtInSynthDelaySlider,
                          &builtInSynthOsc1Slider, &builtInSynthOsc2Slider, &builtInSynthOsc3Slider, &builtInSynthOsc4Slider,
                          &builtInSynthUnisonSlider, &builtInSynthWidthSlider, &builtInSynthChorusSlider, &builtInSynthMasterSlider })
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, synthTextWidth, synthTextHeight);

    chordDirectoryVelocitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, synthTextWidth, synthTextHeight);
    chordDirectorySwingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, synthTextWidth, synthTextHeight);

    const float compactScale = compact ? 0.66f : (accessible ? 0.84f : 0.72f);
    editNameLabel.setMinimumHorizontalScale (compact ? 0.70f : (accessible ? 0.86f : 0.76f));
    transportInfoLabel.setMinimumHorizontalScale (compactScale);
    workflowStateLabel.setMinimumHorizontalScale (compactScale);
    selectedTrackLabel.setMinimumHorizontalScale (compact ? 0.72f : (accessible ? 0.88f : 0.78f));
    statusLabel.setMinimumHorizontalScale (compact ? 0.70f : (accessible ? 0.86f : 0.75f));
    contextHintLabel.setMinimumHorizontalScale (compactScale);

    contextHintLabel.setColour (juce::Label::textColourId,
                                accessible ? juce::Colours::white.withAlpha (0.84f)
                                           : juce::Colours::white.withAlpha (0.70f));
    statusLabel.setColour (juce::Label::textColourId,
                           accessible ? juce::Colour::fromRGB (176, 240, 189)
                                      : juce::Colour::fromRGB (157, 229, 173));

    tooltipWindow.setMillisecondsBeforeTipAppears (accessible ? 460 : (compact ? 720 : 650));
}

void BeatMakerNoRecord::setUiDensityMode (UiDensityMode mode, bool persist, bool announceStatus)
{
    const bool changed = (uiDensityMode != mode);
    uiDensityMode = mode;

    if (persist)
    {
        auto& properties = engine.getPropertyStorage().getPropertiesFile();
        properties.setValue ("uiDensityMode", getUiDensityStorageValue (uiDensityMode));
    }

    applyUiDensityToControlSizing();

    if (auto* window = findParentComponentOfClass<juce::ResizableWindow>())
    {
        const int minWidth = mode == UiDensityMode::accessible ? 1160 : (mode == UiDensityMode::compact ? 960 : 1040);
        const int minHeight = mode == UiDensityMode::accessible ? 740 : (mode == UiDensityMode::compact ? 620 : 680);
        window->setResizeLimits (minWidth, minHeight, 4096, 4096);
    }

    resized();
    repaint();

    if (changed && announceStatus)
        setStatus ("UI density set to " + getUiDensityDisplayName (mode) + ".");
}

auto BeatMakerNoRecord::getDefaultSynthModeSelection() const -> DefaultSynthMode
{
    return getDefaultSynthModeForComboId (defaultSynthModeBox.getSelectedId());
}

juce::String BeatMakerNoRecord::getDefaultSynthModeStorageValue (DefaultSynthMode mode)
{
    switch (mode)
    {
        case DefaultSynthMode::force4Osc: return "force-4osc";
        case DefaultSynthMode::forceExternalVst3: return "force-vst3";
        case DefaultSynthMode::autoPreferExternal:
        default: return "auto";
    }
}

juce::String BeatMakerNoRecord::getDefaultSynthModeDisplayName (DefaultSynthMode mode)
{
    switch (mode)
    {
        case DefaultSynthMode::force4Osc: return "Force 4OSC";
        case DefaultSynthMode::forceExternalVst3: return "Force external VST3";
        case DefaultSynthMode::autoPreferExternal:
        default: return "Auto (VST3 > AU > 4OSC)";
    }
}

int BeatMakerNoRecord::getComboIdForDefaultSynthMode (DefaultSynthMode mode)
{
    switch (mode)
    {
        case DefaultSynthMode::force4Osc: return 2;
        case DefaultSynthMode::forceExternalVst3: return 3;
        case DefaultSynthMode::autoPreferExternal:
        default: return 1;
    }
}

auto BeatMakerNoRecord::getDefaultSynthModeForStorageValue (const juce::String& value) -> DefaultSynthMode
{
    if (value.equalsIgnoreCase ("force-4osc"))
        return DefaultSynthMode::force4Osc;

    if (value.equalsIgnoreCase ("force-vst3"))
        return DefaultSynthMode::forceExternalVst3;

    return DefaultSynthMode::autoPreferExternal;
}

auto BeatMakerNoRecord::getDefaultSynthModeForComboId (int comboId) -> DefaultSynthMode
{
    switch (comboId)
    {
        case 2: return DefaultSynthMode::force4Osc;
        case 3: return DefaultSynthMode::forceExternalVst3;
        case 1:
        default: return DefaultSynthMode::autoPreferExternal;
    }
}

double BeatMakerNoRecord::getBeatsPerBarAt (te::TimePosition time) const
{
    if (edit == nullptr)
        return 4.0;

    const auto& sig = edit->tempoSequence.getTimeSigAt (time);
    const auto numerator = juce::jmax (1, sig.numerator.get());
    const auto denominator = juce::jmax (1, sig.denominator.get());
    return (double) numerator * 4.0 / (double) denominator;
}

te::TimeDuration BeatMakerNoRecord::getGridDurationAt (te::TimePosition time) const
{
    if (edit == nullptr)
        return te::TimeDuration::fromSeconds (0.25);

    const auto bps = juce::jmax (1.0e-4, edit->tempoSequence.getBeatsPerSecondAt (time, true));
    const auto beatsPerBar = getBeatsPerBarAt (time);

    double gridBeats = 0.25;
    switch (gridBox.getSelectedId())
    {
        case 1: gridBeats = beatsPerBar; break;
        case 2: gridBeats = 2.0; break;
        case 3: gridBeats = 1.0; break;
        case 4: gridBeats = 0.5; break;
        case 5: gridBeats = 0.25; break;
        case 6: gridBeats = 0.125; break;
        default: break;
    }

    return te::TimeDuration::fromSeconds (gridBeats / bps);
}

te::TimeDuration BeatMakerNoRecord::getBarDurationAt (te::TimePosition time) const
{
    if (edit == nullptr)
        return te::TimeDuration::fromSeconds (2.0);

    const auto bps = juce::jmax (1.0e-4, edit->tempoSequence.getBeatsPerSecondAt (time, true));
    return te::TimeDuration::fromSeconds (getBeatsPerBarAt (time) / bps);
}

void BeatMakerNoRecord::jumpByBar (bool forward)
{
    if (edit == nullptr)
        return;

    const auto now = edit->getTransport().getPosition();
    const auto delta = getBarDurationAt (now);
    const auto destination = forward ? now + delta : now - delta;
    const auto clamped = te::TimePosition::fromSeconds (juce::jmax (0.0, destination.inSeconds()));

    edit->getTransport().setPosition (clamped);
}

int BeatMakerNoRecord::getHeaderWidth() const
{
    if (editComponent == nullptr)
        return 0;

    return editComponent->getEditViewState().showHeaders.get() ? 150 : 0;
}

int BeatMakerNoRecord::getFooterWidth() const
{
    if (editComponent == nullptr)
        return 0;

    return editComponent->getEditViewState().showFooters.get() ? 150 : 0;
}

int BeatMakerNoRecord::getVisibleTrackCount() const
{
    if (edit == nullptr)
        return 0;

    int count = te::getAudioTracks (*edit).size();
    if (editComponent != nullptr)
    {
        const auto& view = editComponent->getEditViewState();
        if (view.showMasterTrack.get())  ++count;
        if (view.showGlobalTrack.get())  ++count;
        if (view.showMarkerTrack.get())  ++count;
        if (view.showChordTrack.get())   ++count;
        if (view.showArrangerTrack.get()) ++count;
    }

    return juce::jmax (count, 0);
}

double BeatMakerNoRecord::getTimelineTotalLengthSeconds() const
{
    double endTime = 16.0;

    if (edit != nullptr)
    {
        for (auto* track : te::getAudioTracks (*edit))
            for (auto* clip : track->getClips())
                endTime = juce::jmax (endTime, clip->getEditTimeRange().getEnd().inSeconds());

        endTime = juce::jmax (endTime, edit->getTransport().getLoopRange().getEnd().inSeconds());
        endTime = juce::jmax (endTime, edit->getTransport().getPosition().inSeconds());
    }

    return endTime + 8.0;
}

void BeatMakerNoRecord::syncPianoRollViewportToSelection (bool resetForNewClip)
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return;

    const bool clipChanged = (pianoRollViewportClipID != midiClip->itemID);
    if (resetForNewClip && clipChanged)
        focusPianoRollViewportOnClip (*midiClip, true);

    const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClip));
    const int maxVisibleNotes = pianoRollMaxNote - pianoRollMinNote + 1;
    const double minViewBeats = juce::jmin (clipLengthBeats,
                                            juce::jmax (1.0 / 8.0, getPianoRollGridBeats().inBeats() * 2.0));

    pianoRollViewLengthBeats = juce::jlimit (minViewBeats, clipLengthBeats, pianoRollViewLengthBeats);
    const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - pianoRollViewLengthBeats);
    pianoRollViewStartBeat = juce::jlimit (0.0, maxStartBeat, pianoRollViewStartBeat);

    pianoRollViewNoteCount = juce::jlimit (pianoRollMinVisibleNotes, maxVisibleNotes, pianoRollViewNoteCount);
    const int maxLowestNote = pianoRollMaxNote - pianoRollViewNoteCount + 1;
    pianoRollViewLowestNote = juce::jlimit (pianoRollMinNote, maxLowestNote, pianoRollViewLowestNote);
    pianoRollViewportClipID = midiClip->itemID;
    updatePianoRollScrollbarsFromViewport();
}

void BeatMakerNoRecord::focusPianoRollViewportOnClip (const te::MidiClip& midiClip, bool centerOnContent)
{
    const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (midiClip));
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (midiClip.getPosition().getStart()));
    const int maxVisibleNotes = pianoRollMaxNote - pianoRollMinNote + 1;

    double targetLengthBeats = juce::jlimit (1.0, clipLengthBeats, juce::jmax (4.0, beatsPerBar * 2.0));
    double targetStartBeat = 0.0;
    int targetNoteCount = pianoRollDefaultVisibleNotes;
    int targetLowestNote = pianoRollDefaultLowestNote;

    if (centerOnContent)
    {
        double firstBeat = std::numeric_limits<double>::infinity();
        double lastBeat = 0.0;
        int lowestNoteSeen = std::numeric_limits<int>::max();
        int highestNoteSeen = std::numeric_limits<int>::min();

        for (auto* note : midiClip.getSequence().getNotes())
        {
            if (note == nullptr)
                continue;

            const double startBeat = note->getStartBeat().inBeats();
            const double endBeat = startBeat + juce::jmax (1.0 / 128.0, note->getLengthBeats().inBeats());
            firstBeat = juce::jmin (firstBeat, startBeat);
            lastBeat = juce::jmax (lastBeat, endBeat);
            lowestNoteSeen = juce::jmin (lowestNoteSeen, note->getNoteNumber());
            highestNoteSeen = juce::jmax (highestNoteSeen, note->getNoteNumber());
        }

        if (std::isfinite (firstBeat))
        {
            const double beatPadding = juce::jmax (0.25, getPianoRollGridBeats().inBeats() * 2.0);
            const double contentStart = juce::jmax (0.0, firstBeat - beatPadding);
            const double contentEnd = juce::jmin (clipLengthBeats, lastBeat + beatPadding);
            const double contentLength = juce::jmax (beatPadding * 2.0, contentEnd - contentStart);

            targetLengthBeats = juce::jlimit (juce::jmin (clipLengthBeats, 1.0 / 8.0),
                                              clipLengthBeats,
                                              juce::jmax (beatsPerBar * 0.75, contentLength));

            const double centeredStart = (contentStart + contentEnd - targetLengthBeats) * 0.5;
            targetStartBeat = juce::jlimit (0.0,
                                            juce::jmax (0.0, clipLengthBeats - targetLengthBeats),
                                            centeredStart);

            if (lowestNoteSeen <= highestNoteSeen)
            {
                const int noteSpan = juce::jmax (1, highestNoteSeen - lowestNoteSeen + 1);
                targetNoteCount = juce::jlimit (pianoRollMinVisibleNotes, maxVisibleNotes,
                                                juce::jmax (pianoRollDefaultVisibleNotes / 2, noteSpan + 10));
                const int maxLowestNote = pianoRollMaxNote - targetNoteCount + 1;
                targetLowestNote = juce::jlimit (pianoRollMinNote, maxLowestNote, lowestNoteSeen - 5);
            }
        }
    }

    pianoRollViewLengthBeats = juce::jlimit (1.0 / 8.0, clipLengthBeats, targetLengthBeats);
    pianoRollViewStartBeat = juce::jlimit (0.0,
                                           juce::jmax (0.0, clipLengthBeats - pianoRollViewLengthBeats),
                                           targetStartBeat);
    pianoRollViewNoteCount = juce::jlimit (pianoRollMinVisibleNotes, maxVisibleNotes, targetNoteCount);
    const int maxLowestNote = pianoRollMaxNote - pianoRollViewNoteCount + 1;
    pianoRollViewLowestNote = juce::jlimit (pianoRollMinNote, maxLowestNote, targetLowestNote);
    pianoRollViewportClipID = midiClip.itemID;
    updatePianoRollScrollbarsFromViewport();
}

void BeatMakerNoRecord::panPianoRollViewport (double beatDelta, int noteDelta)
{
    syncPianoRollViewportToSelection (false);
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return;

    const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClip));
    const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - pianoRollViewLengthBeats);
    pianoRollViewStartBeat = juce::jlimit (0.0, maxStartBeat, pianoRollViewStartBeat + beatDelta);

    const int maxLowestNote = pianoRollMaxNote - pianoRollViewNoteCount + 1;
    pianoRollViewLowestNote = juce::jlimit (pianoRollMinNote, maxLowestNote, pianoRollViewLowestNote + noteDelta);
    updatePianoRollScrollbarsFromViewport();
    midiPianoRoll.repaint();
}

void BeatMakerNoRecord::zoomPianoRollViewportTime (double factor, double anchorBeat)
{
    syncPianoRollViewportToSelection (false);
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return;

    const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClip));
    const double minLengthBeats = juce::jmin (clipLengthBeats,
                                              juce::jmax (1.0 / 8.0, getPianoRollGridBeats().inBeats() * 2.0));
    const double oldLengthBeats = juce::jmax (minLengthBeats, pianoRollViewLengthBeats);
    const double newLengthBeats = juce::jlimit (minLengthBeats,
                                                clipLengthBeats,
                                                oldLengthBeats * juce::jlimit (0.08, 12.0, factor));
    const double ratio = (anchorBeat - pianoRollViewStartBeat) / juce::jmax (1.0e-6, oldLengthBeats);
    const double desiredStart = anchorBeat - ratio * newLengthBeats;
    const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - newLengthBeats);

    pianoRollViewLengthBeats = newLengthBeats;
    pianoRollViewStartBeat = juce::jlimit (0.0, maxStartBeat, desiredStart);
    updatePianoRollScrollbarsFromViewport();
    midiPianoRoll.repaint();
}

void BeatMakerNoRecord::zoomPianoRollViewportPitch (double factor, int anchorNote)
{
    syncPianoRollViewportToSelection (false);

    const int maxVisibleNotes = pianoRollMaxNote - pianoRollMinNote + 1;
    const int oldNoteCount = juce::jlimit (pianoRollMinVisibleNotes, maxVisibleNotes, pianoRollViewNoteCount);
    const int newNoteCount = juce::jlimit (pianoRollMinVisibleNotes,
                                           maxVisibleNotes,
                                           juce::roundToInt ((double) oldNoteCount * juce::jlimit (0.08, 12.0, factor)));

    const int clampedAnchorNote = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, anchorNote);
    const double ratio = (double) (clampedAnchorNote - pianoRollViewLowestNote) / (double) juce::jmax (1, oldNoteCount - 1);
    const int desiredLowest = juce::roundToInt ((double) clampedAnchorNote - ratio * (double) juce::jmax (1, newNoteCount - 1));
    const int maxLowestNote = pianoRollMaxNote - newNoteCount + 1;

    pianoRollViewNoteCount = newNoteCount;
    pianoRollViewLowestNote = juce::jlimit (pianoRollMinNote, maxLowestNote, desiredLowest);
    updatePianoRollScrollbarsFromViewport();
    midiPianoRoll.repaint();
}

void BeatMakerNoRecord::updatePianoRollScrollbarsFromViewport()
{
    juce::ScopedValueSetter<bool> guard (updatingEditorScrollBars, true);

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        pianoRollHorizontalScrollBar.setEnabled (false);
        pianoRollVerticalScrollBar.setEnabled (false);
        pianoRollHorizontalScrollBar.setRangeLimits ({ 0.0, 1.0 });
        pianoRollHorizontalScrollBar.setCurrentRange (0.0, 1.0);
        pianoRollVerticalScrollBar.setRangeLimits ({ (double) pianoRollMinNote, (double) (pianoRollMinNote + 1) });
        pianoRollVerticalScrollBar.setCurrentRange ((double) pianoRollMinNote, 1.0);
        return;
    }

    const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClip));
    const double minViewBeats = juce::jmin (clipLengthBeats,
                                            juce::jmax (1.0 / 8.0, getPianoRollGridBeats().inBeats() * 2.0));
    const double viewLengthBeats = juce::jlimit (minViewBeats, clipLengthBeats, pianoRollViewLengthBeats);
    const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - viewLengthBeats);
    const double viewStartBeat = juce::jlimit (0.0, maxStartBeat, pianoRollViewStartBeat);

    const int maxVisibleNotes = pianoRollMaxNote - pianoRollMinNote + 1;
    const int noteCount = juce::jlimit (pianoRollMinVisibleNotes, maxVisibleNotes, pianoRollViewNoteCount);
    const int maxLowestNote = pianoRollMaxNote - noteCount + 1;
    const int lowestNote = juce::jlimit (pianoRollMinNote, maxLowestNote, pianoRollViewLowestNote);

    const bool allowScrolling = pianoRollHorizontalScrollBar.isVisible() || pianoRollVerticalScrollBar.isVisible();
    pianoRollHorizontalScrollBar.setEnabled (allowScrolling);
    pianoRollHorizontalScrollBar.setRangeLimits ({ 0.0, clipLengthBeats });
    pianoRollHorizontalScrollBar.setCurrentRange (viewStartBeat, viewLengthBeats);
    pianoRollHorizontalScrollBar.setSingleStepSize (juce::jmax (1.0 / 64.0, getPianoRollGridBeats().inBeats()));

    pianoRollVerticalScrollBar.setEnabled (allowScrolling);
    pianoRollVerticalScrollBar.setRangeLimits ({ (double) pianoRollMinNote, (double) (pianoRollMaxNote + 1) });
    pianoRollVerticalScrollBar.setCurrentRange ((double) lowestNote, (double) noteCount);
    pianoRollVerticalScrollBar.setSingleStepSize (1.0);
}

void BeatMakerNoRecord::setStepSequencerViewportStartBeat (double startBeat, bool markManualOverride)
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return;

    const double clipLengthBeats = juce::jmax (1.0 / 64.0, getMidiClipLengthBeats (*midiClip));
    const double stepLengthBeats = juce::jmax (1.0 / 64.0, getStepSequencerStepLengthBeats (*midiClip).inBeats());
    const double pageSpanBeats = stepLengthBeats * (double) stepSequencerStepsPerBar;
    const double maxPageStart = juce::jmax (0.0, clipLengthBeats - pageSpanBeats);

    stepSequencerViewportClipID = midiClip->itemID;
    stepSequencerViewportStartBeat = juce::jlimit (0.0, maxPageStart, startBeat);
    if (markManualOverride)
        stepSequencerManualPageOverrideActive = true;
}

void BeatMakerNoRecord::clearStepSequencerViewportOverride()
{
    stepSequencerViewportClipID = {};
    stepSequencerViewportStartBeat = 0.0;
    stepSequencerManualPageOverrideActive = false;
}

void BeatMakerNoRecord::updateStepSequencerScrollbarFromPageContext()
{
    juce::ScopedValueSetter<bool> guard (updatingEditorScrollBars, true);

    StepSequencerPageContext context;
    if (! buildStepSequencerPageContext (context) || context.midiClip == nullptr)
    {
        stepSequencerHorizontalScrollBar.setEnabled (false);
        stepSequencerHorizontalScrollBar.setRangeLimits ({ 0.0, 1.0 });
        stepSequencerHorizontalScrollBar.setCurrentRange (0.0, 1.0);
        return;
    }

    const double pageSpanBeats = juce::jmax (1.0 / 64.0, context.stepLengthBeats * (double) stepSequencerStepsPerBar);
    const double clipLengthBeats = juce::jmax (pageSpanBeats, context.clipLengthBeats);
    const double maxPageStart = juce::jmax (0.0, clipLengthBeats - pageSpanBeats);
    const double pageStartBeat = juce::jlimit (0.0, maxPageStart, context.pageStartBeat);

    stepSequencerHorizontalScrollBar.setEnabled (stepSequencerHorizontalScrollBar.isVisible());
    stepSequencerHorizontalScrollBar.setRangeLimits ({ 0.0, clipLengthBeats });
    stepSequencerHorizontalScrollBar.setCurrentRange (pageStartBeat, pageSpanBeats);
    stepSequencerHorizontalScrollBar.setSingleStepSize (juce::jmax (1.0 / 64.0, context.stepLengthBeats));
}

int BeatMakerNoRecord::pianoRollYToNoteNumber (int y, int height) const
{
    const int noteCount = juce::jmax (1, pianoRollViewNoteCount);
    const int lowestNote = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, pianoRollViewLowestNote);
    const int highestNote = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, lowestNote + noteCount - 1);
    const int clampedY = juce::jlimit (0, juce::jmax (0, height - 1), y);
    const int rowHeight = juce::jmax (1, height / noteCount);
    const int rowFromTop = juce::jlimit (0, noteCount - 1, clampedY / rowHeight);
    return juce::jlimit (pianoRollMinNote, pianoRollMaxNote, highestNote - rowFromTop);
}

double BeatMakerNoRecord::pianoRollXToBeat (int x, int width) const
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return 0.0;

    const double maxBeat = getMidiClipLengthBeats (*midiClip);
    const double viewStartBeat = juce::jlimit (0.0, maxBeat, pianoRollViewStartBeat);
    const double viewLengthBeats = juce::jlimit (1.0 / 16.0, maxBeat, pianoRollViewLengthBeats);
    const double unclamped = viewStartBeat
                             + (double) juce::jlimit (0, juce::jmax (1, width), x)
                               / juce::jmax (1, width)
                               * viewLengthBeats;
    const double grid = juce::jmax (1.0 / 32.0, getPianoRollGridBeats().inBeats());
    return juce::jlimit (0.0, maxBeat, std::round (unclamped / grid) * grid);
}

te::BeatDuration BeatMakerNoRecord::getPianoRollGridBeats() const
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr || edit == nullptr)
        return te::BeatDuration::fromBeats (0.25);

    const auto clipStart = midiClip->getPosition().getStart();
    const auto bps = juce::jmax (1.0e-4, edit->tempoSequence.getBeatsPerSecondAt (clipStart, true));
    const double gridBeats = getGridDurationAt (clipStart).inSeconds() * bps;
    return te::BeatDuration::fromBeats (juce::jmax (1.0 / 32.0, gridBeats));
}

juce::Rectangle<int> BeatMakerNoRecord::getPianoRollNoteBounds (const te::MidiNote& note, int width, int height) const
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return {};

    const int noteCount = juce::jmax (1, pianoRollViewNoteCount);
    const int lowestNote = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, pianoRollViewLowestNote);
    const int highestNote = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, lowestNote + noteCount - 1);
    if (note.getNoteNumber() < lowestNote || note.getNoteNumber() > highestNote)
        return {};

    const int rowHeight = juce::jmax (1, height / noteCount);
    const double maxBeat = getMidiClipLengthBeats (*midiClip);
    const double viewStartBeat = juce::jlimit (0.0, maxBeat, pianoRollViewStartBeat);
    const double viewLengthBeats = juce::jlimit (1.0 / 16.0, maxBeat, pianoRollViewLengthBeats);
    const double viewEndBeat = viewStartBeat + viewLengthBeats;

    const double startBeat = note.getStartBeat().inBeats();
    const double endBeat = startBeat + juce::jmax (1.0 / 32.0, note.getLengthBeats().inBeats());
    if (endBeat < viewStartBeat || startBeat > viewEndBeat)
        return {};

    const double visibleStart = juce::jmax (viewStartBeat, startBeat);
    const double visibleEnd = juce::jmin (viewEndBeat, endBeat);
    const int x1 = (int) std::floor (((visibleStart - viewStartBeat) / viewLengthBeats) * width);
    const int x2 = juce::jmax (x1 + 2, (int) std::ceil (((visibleEnd - viewStartBeat) / viewLengthBeats) * width));

    const int noteIndex = juce::jlimit (0, noteCount - 1, note.getNoteNumber() - lowestNote);
    const int rowFromTop = noteCount - 1 - noteIndex;
    const int y = rowFromTop * rowHeight;

    return { x1, y + 1, juce::jmax (2, x2 - x1), juce::jmax (1, rowHeight - 2) };
}

te::MidiNote* BeatMakerNoRecord::getPianoRollNoteAt (int x, int y, int width, int height) const
{
    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return nullptr;

    const auto& notes = midiClip->getSequence().getNotes();

    for (auto* note : notes)
    {
        if (getPianoRollNoteBounds (*note, width, height).contains (x, y))
            return note;
    }

    return nullptr;
}

double BeatMakerNoRecord::getStepSequencerPageStartBeat (const te::MidiClip& midiClip)
{
    const auto clipStart = midiClip.getPosition().getStart();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
    const double clipLengthBeats = juce::jmax (1.0 / 64.0, getMidiClipLengthBeats (midiClip));
    const double stepLengthBeats = juce::jmax (1.0 / 64.0, getStepSequencerStepLengthBeats (midiClip).inBeats());
    const double pageSpanBeats = stepLengthBeats * (double) stepSequencerStepsPerBar;
    const auto maxAnchorBeat = juce::jmax (0.0, clipLengthBeats - (1.0 / 64.0));
    const double maxPageStart = juce::jmax (0.0, clipLengthBeats - pageSpanBeats);

    if (stepSequencerViewportClipID != midiClip.itemID)
    {
        stepSequencerViewportClipID = midiClip.itemID;
        stepSequencerViewportStartBeat = 0.0;
        stepSequencerManualPageOverrideActive = false;
    }

    if (stepSequencerManualPageOverrideActive)
    {
        stepSequencerViewportStartBeat = juce::jlimit (0.0, maxPageStart, stepSequencerViewportStartBeat);
        return stepSequencerViewportStartBeat;
    }

    auto findFirstNoteBeat = [&midiClip, maxAnchorBeat]
    {
        double firstBeat = std::numeric_limits<double>::infinity();
        for (auto* note : midiClip.getSequence().getNotes())
        {
            if (note == nullptr)
                continue;

            firstBeat = juce::jmin (firstBeat, note->getStartBeat().inBeats());
        }

        if (! std::isfinite (firstBeat))
            return 0.0;

        return juce::jlimit (0.0, maxAnchorBeat, firstBeat);
    };

    // Keep the sequencer page tied to clip content. While playing (or when the playhead is inside
    // the clip), follow transport; otherwise anchor to the first note bar.
    double anchorBeat = findFirstNoteBeat();
    if (edit != nullptr)
    {
        const double playBeat = midiClip.getContentBeatAtTime (edit->getTransport().getPosition()).inBeats();
        const bool playheadInsideClip = playBeat >= -(1.0 / 64.0) && playBeat <= clipLengthBeats + (1.0 / 64.0);
        if (edit->getTransport().isPlaying() || playheadInsideClip)
            anchorBeat = juce::jlimit (0.0, maxAnchorBeat, playBeat);
    }

    const double pageStart = std::floor (anchorBeat / beatsPerBar) * beatsPerBar;
    stepSequencerViewportStartBeat = juce::jlimit (0.0, maxPageStart, pageStart);
    return stepSequencerViewportStartBeat;
}

te::BeatDuration BeatMakerNoRecord::getStepSequencerStepLengthBeats (const te::MidiClip& midiClip) const
{
    const auto clipStart = midiClip.getPosition().getStart();
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (clipStart));
    return te::BeatDuration::fromBeats (juce::jmax (1.0 / 64.0, beatsPerBar / (double) stepSequencerStepsPerBar));
}

te::MidiNote* BeatMakerNoRecord::getStepSequencerNoteAt (te::MidiClip& midiClip,
                                                         int laneIndex,
                                                         int stepIndex,
                                                         double pageStartBeat,
                                                         te::BeatDuration stepLength) const
{
    const auto& lanes = getStepSequencerLanes();
    if (laneIndex < 0 || laneIndex >= (int) lanes.size()
        || stepIndex < 0 || stepIndex >= stepSequencerStepsPerBar)
        return nullptr;

    const int noteNumber = lanes[(size_t) laneIndex].noteNumber;
    const double targetBeat = pageStartBeat + (double) stepIndex * stepLength.inBeats();
    const double tolerance = juce::jmax (1.0 / 128.0, stepLength.inBeats() * 0.42);
    return findStepSequencerNoteByPitchAndBeat (midiClip, noteNumber, targetBeat, tolerance);
}

bool BeatMakerNoRecord::setStepSequencerCellEnabled (te::MidiClip& midiClip,
                                                     int laneIndex,
                                                     int stepIndex,
                                                     double pageStartBeat,
                                                     te::BeatDuration stepLength,
                                                     bool shouldEnable)
{
    if (edit == nullptr)
        return false;

    const auto& lanes = getStepSequencerLanes();
    if (laneIndex < 0 || laneIndex >= (int) lanes.size()
        || stepIndex < 0 || stepIndex >= stepSequencerStepsPerBar)
        return false;

    auto& sequence = midiClip.getSequence();
    auto& undoManager = edit->getUndoManager();
    auto* noteAtCell = getStepSequencerNoteAt (midiClip, laneIndex, stepIndex, pageStartBeat, stepLength);
    const double clipLengthBeats = getMidiClipLengthBeats (midiClip);

    if (shouldEnable)
    {
        if (noteAtCell != nullptr)
            return false;

        const int noteNumber = lanes[(size_t) laneIndex].noteNumber;
        return insertStepSequencerNoteAtCell (midiClip,
                                              noteNumber,
                                              stepIndex,
                                              pageStartBeat,
                                              stepLength.inBeats(),
                                              clipLengthBeats,
                                              112,
                                              undoManager);
    }

    if (noteAtCell == nullptr)
        return false;

    sequence.removeNote (*noteAtCell, &undoManager);
    return true;
}

bool BeatMakerNoRecord::buildStepSequencerPageContext (StepSequencerPageContext& context)
{
    context = {};

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return false;

    context.midiClip = midiClip;
    context.stepLength = getStepSequencerStepLengthBeats (*midiClip);
    if (context.stepLength.inBeats() <= 0.0)
        context.stepLength = te::BeatDuration::fromBeats (0.25);

    context.stepLengthBeats = juce::jmax (1.0 / 64.0, context.stepLength.inBeats());
    context.clipLengthBeats = juce::jmax (1.0 / 64.0, getMidiClipLengthBeats (*midiClip));
    context.pageStartBeat = getStepSequencerPageStartBeat (*midiClip);

    const double pageSpanBeats = context.stepLengthBeats * (double) stepSequencerStepsPerBar;
    const double maxPageStart = juce::jmax (0.0, context.clipLengthBeats - pageSpanBeats);
    context.pageStartBeat = juce::jlimit (0.0, maxPageStart, context.pageStartBeat);
    return true;
}

void BeatMakerNoRecord::refreshStepSequencerEditSurfaces()
{
    updateStepSequencerScrollbarFromPageContext();
    stepSequencer.repaint();
    midiPianoRoll.repaint();
    updateButtonsFromState();
}

void BeatMakerNoRecord::resetStepSequencerDragState()
{
    stepSequencerDragMode = StepSequencerDragMode::none;
    stepSequencerDragLastLane = -1;
    stepSequencerDragLastStep = -1;
    stepSequencerDragChangedAnyCell = false;
    stepSequencerDragPageStartBeat = 0.0;
    stepSequencerDragStepLength = te::BeatDuration::fromBeats (0.25);
}

void BeatMakerNoRecord::resetPianoRollNoteDragState()
{
    pianoRollDraggedNote = nullptr;
    pianoRollDragMode = PianoRollDragMode::none;
    pianoRollAddMode = false;
    pianoRollDragStart = {};
}

void BeatMakerNoRecord::clearPianoRollNavigationInteraction()
{
    pianoRollRulerScrubActive = false;
    pianoRollPanDragActive = false;
    pianoRollPanDragStart = {};
    pianoRollPanStartBeat = 0.0;
    pianoRollPanStartLowestNote = pianoRollDefaultLowestNote;
    midiPianoRoll.setMouseCursor (juce::MouseCursor::NormalCursor);
}

void BeatMakerNoRecord::clearStepSequencerPage()
{
    if (edit == nullptr)
        return;

    StepSequencerPageContext context;
    if (! buildStepSequencerPageContext (context))
        return;

    const auto& lanes = getStepSequencerLanes();
    auto& undoManager = edit->getUndoManager();

    undoManager.beginNewTransaction ("Step Sequencer Clear Page");
    const bool changed = clearStepSequencerPageLaneNotes (*context.midiClip,
                                                          lanes,
                                                          context.pageStartBeat,
                                                          context.stepLength,
                                                          undoManager);

    if (changed)
    {
        refreshStepSequencerEditSurfaces();
        setStatus ("Cleared current step sequencer page.");
    }
    else
    {
        setStatus ("Step sequencer page already empty.");
    }
}

void BeatMakerNoRecord::applyStepSequencerFourOnFloorPattern()
{
    if (edit == nullptr)
        return;

    StepSequencerPageContext context;
    if (! buildStepSequencerPageContext (context))
        return;

    const auto& lanes = getStepSequencerLanes();

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Step Sequencer Four On Floor");

    clearStepSequencerPageLaneNotes (*context.midiClip,
                                     lanes,
                                     context.pageStartBeat,
                                     context.stepLength,
                                     undoManager);

    auto addStep = [&] (int lane, int step, int velocity)
    {
        if (lane < 0 || lane >= (int) lanes.size() || step < 0 || step >= stepSequencerStepsPerBar)
            return;

        insertStepSequencerNoteAtCell (*context.midiClip,
                                       lanes[(size_t) lane].noteNumber,
                                       step,
                                       context.pageStartBeat,
                                       context.stepLengthBeats,
                                       context.clipLengthBeats,
                                       velocity,
                                       undoManager);
    };

    for (int step = 0; step < stepSequencerStepsPerBar; step += 4)
        addStep (0, step, step == 0 ? 124 : 116); // Kick

    addStep (1, 4, 114);   // Snare
    addStep (1, 12, 114);  // Snare

    for (int step = 0; step < stepSequencerStepsPerBar; step += 2)
        addStep (2, step, ((step % 4) == 0) ? 98 : 84); // Closed HH

    addStep (3, 7, 92);    // Open HH
    addStep (3, 15, 96);   // Open HH

    if ((int) lanes.size() > 6)
        addStep (6, 0, 104); // Crash

    refreshStepSequencerEditSurfaces();
    setStatus ("Applied four-on-the-floor pattern to current page.");
}

void BeatMakerNoRecord::randomizeStepSequencerPage()
{
    if (edit == nullptr)
        return;

    StepSequencerPageContext context;
    if (! buildStepSequencerPageContext (context))
        return;

    const auto& lanes = getStepSequencerLanes();

    static const std::array<float, 8> laneDensity
    {{
        0.90f, // Kick
        0.26f, // Snare
        0.72f, // Closed HH
        0.17f, // Open HH
        0.14f, // Low Tom
        0.12f, // High Tom
        0.09f, // Crash
        0.09f  // Ride
    }};

    auto& undoManager = edit->getUndoManager();
    auto& rng = juce::Random::getSystemRandom();
    undoManager.beginNewTransaction ("Step Sequencer Randomize Page");

    clearStepSequencerPageLaneNotes (*context.midiClip,
                                     lanes,
                                     context.pageStartBeat,
                                     context.stepLength,
                                     undoManager);

    auto addStep = [&] (int lane, int step, int velocity)
    {
        if (lane < 0 || lane >= (int) lanes.size() || step < 0 || step >= stepSequencerStepsPerBar)
            return;

        insertStepSequencerNoteAtCell (*context.midiClip,
                                       lanes[(size_t) lane].noteNumber,
                                       step,
                                       context.pageStartBeat,
                                       context.stepLengthBeats,
                                       context.clipLengthBeats,
                                       velocity,
                                       undoManager);
    };

    bool addedAny = false;

    for (int lane = 0; lane < (int) lanes.size(); ++lane)
    {
        const float baseDensity = laneDensity[(size_t) juce::jlimit (0, (int) laneDensity.size() - 1, lane)];
        for (int step = 0; step < stepSequencerStepsPerBar; ++step)
        {
            float chance = baseDensity;
            if (lane == 0 && (step % 4) == 0)
                chance = juce::jmax (chance, 0.94f);
            if (lane == 1 && (step == 4 || step == 12))
                chance = juce::jmax (chance, 0.90f);
            if (lane == 2 && (step % 2) == 0)
                chance = juce::jmax (chance, 0.85f);
            if (lane >= 4 && (step % 4) == 3)
                chance *= 0.7f;

            if (rng.nextFloat() <= chance)
            {
                const int baseVelocity = 74 + rng.nextInt (43); // 74..116
                const int accentBoost = (step % 4) == 0 ? 10 : 0;
                addStep (lane, step, baseVelocity + accentBoost);
                addedAny = true;
            }
        }
    }

    if (! addedAny)
        addStep (0, 0, 120);

    refreshStepSequencerEditSurfaces();
    setStatus ("Randomized current step sequencer page.");
}

void BeatMakerNoRecord::shiftStepSequencerPage (int stepDelta)
{
    if (stepDelta == 0)
        return;

    if (edit == nullptr)
        return;

    StepSequencerPageContext context;
    if (! buildStepSequencerPageContext (context))
        return;

    const auto& lanes = getStepSequencerLanes();

    std::vector<std::vector<int>> cellVelocity ((size_t) lanes.size(),
                                                std::vector<int> ((size_t) stepSequencerStepsPerBar, 0));

    bool hadAnyCells = false;
    for (int lane = 0; lane < (int) lanes.size(); ++lane)
    {
        for (int step = 0; step < stepSequencerStepsPerBar; ++step)
        {
            if (auto* note = getStepSequencerNoteAt (*context.midiClip,
                                                     lane,
                                                     step,
                                                     context.pageStartBeat,
                                                     context.stepLength))
            {
                cellVelocity[(size_t) lane][(size_t) step] = juce::jlimit (1, 127, note->getVelocity());
                hadAnyCells = true;
            }
        }
    }

    if (! hadAnyCells)
    {
        setStatus ("No steps to shift on current page.");
        return;
    }

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Step Sequencer Shift Page");

    clearStepSequencerPageLaneNotes (*context.midiClip,
                                     lanes,
                                     context.pageStartBeat,
                                     context.stepLength,
                                     undoManager);

    for (int lane = 0; lane < (int) lanes.size(); ++lane)
    {
        for (int step = 0; step < stepSequencerStepsPerBar; ++step)
        {
            const int velocity = cellVelocity[(size_t) lane][(size_t) step];
            if (velocity <= 0)
                continue;

            int shiftedStep = (step + stepDelta) % stepSequencerStepsPerBar;
            if (shiftedStep < 0)
                shiftedStep += stepSequencerStepsPerBar;

            insertStepSequencerNoteAtCell (*context.midiClip,
                                           lanes[(size_t) lane].noteNumber,
                                           shiftedStep,
                                           context.pageStartBeat,
                                           context.stepLengthBeats,
                                           context.clipLengthBeats,
                                           velocity,
                                           undoManager);
        }
    }

    refreshStepSequencerEditSurfaces();
    setStatus ("Shifted step sequencer page " + juce::String (stepDelta > 0 ? "right." : "left."));
}

void BeatMakerNoRecord::varyStepSequencerPageVelocities (int maxDelta)
{
    if (edit == nullptr)
        return;

    StepSequencerPageContext context;
    if (! buildStepSequencerPageContext (context))
        return;

    const auto& lanes = getStepSequencerLanes();
    const int clampedMaxDelta = juce::jlimit (1, 48, maxDelta);
    auto& undoManager = edit->getUndoManager();
    auto& rng = juce::Random::getSystemRandom();
    undoManager.beginNewTransaction ("Step Sequencer Vary Velocities");

    bool changed = false;
    for (int lane = 0; lane < (int) lanes.size(); ++lane)
    {
        for (int step = 0; step < stepSequencerStepsPerBar; ++step)
        {
            if (auto* note = getStepSequencerNoteAt (*context.midiClip,
                                                     lane,
                                                     step,
                                                     context.pageStartBeat,
                                                     context.stepLength))
            {
                const int randomDelta = rng.nextInt (clampedMaxDelta * 2 + 1) - clampedMaxDelta;
                const int accent = (step % 4) == 0 ? 6 : 0;
                const int velocity = juce::jlimit (1, 127, note->getVelocity() + randomDelta + accent);
                note->setVelocity (velocity, &undoManager);
                changed = true;
            }
        }
    }

    if (changed)
    {
        refreshStepSequencerEditSurfaces();
        setStatus ("Applied step velocity variation.");
    }
    else
    {
        setStatus ("No active steps found for velocity variation.");
    }
}

void BeatMakerNoRecord::handleTimelineRulerMouseDown (const juce::MouseEvent& e, int width)
{
    if (edit == nullptr || editComponent == nullptr || width <= 1)
        return;

    if (e.mods.isRightButtonDown())
    {
        timelinePanDragActive = false;
        timelineLoopDragActive = false;
        showTimelineRulerContextMenu (e, width);
        return;
    }

    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        timelineLoopDragActive = false;
        timelinePanDragActive = true;
        timelinePanDragLastX = e.x;
        return;
    }

    if (e.mods.isShiftDown())
    {
        timelinePanDragActive = false;
        timelineLoopDragActive = true;
        timelineLoopDragStartX = juce::jlimit (0, width, e.x);
        handleTimelineRulerMouseDrag (e, width);
        return;
    }

    timelinePanDragActive = false;
    timelineLoopDragActive = false;
    setPlayheadFromRulerX (e.x, width);

    if (e.getNumberOfClicks() >= 2)
    {
        centerPlayheadInView();
        setStatus ("Centered timeline around clicked ruler position.");
    }
}

void BeatMakerNoRecord::handleTimelineRulerMouseDrag (const juce::MouseEvent& e, int width)
{
    if (timelinePanDragActive)
    {
        if (editComponent != nullptr && width > 1)
        {
            auto& viewState = editComponent->getEditViewState();
            const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
            const int deltaPixels = e.x - timelinePanDragLastX;
            moveTimelineViewportBySeconds (-((double) deltaPixels / (double) width) * visibleSeconds);
            timelinePanDragLastX = e.x;
        }
        return;
    }

    if (! timelineLoopDragActive)
    {
        if (editComponent != nullptr && width > 1)
        {
            auto& viewState = editComponent->getEditViewState();
            const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
            const int edgeZone = juce::jmax (12, width / 22);

            if (e.x < edgeZone)
            {
                const double norm = juce::jlimit (0.0, 1.0, (double) (edgeZone - e.x) / (double) edgeZone);
                moveTimelineViewportBySeconds (-visibleSeconds * (0.015 + 0.045 * norm));
            }
            else if (e.x > (width - edgeZone))
            {
                const double norm = juce::jlimit (0.0, 1.0, (double) (e.x - (width - edgeZone)) / (double) edgeZone);
                moveTimelineViewportBySeconds (visibleSeconds * (0.015 + 0.045 * norm));
            }
        }

        setPlayheadFromRulerX (e.x, width);
        return;
    }

    if (edit == nullptr || editComponent == nullptr || width <= 1)
        return;

    const int loopEndX = juce::jlimit (0, width, e.x);
    const int x1 = juce::jmin (timelineLoopDragStartX, loopEndX);
    const int x2 = juce::jmax (timelineLoopDragStartX, loopEndX);

    auto& viewState = editComponent->getEditViewState();
    auto loopStart = viewState.xToTime (x1, width);
    auto loopEnd = viewState.xToTime (x2, width);

    const auto minLength = getGridDurationAt (loopStart);
    const double minLengthSeconds = juce::jmax (0.01, minLength.inSeconds());
    if ((loopEnd - loopStart).inSeconds() < minLengthSeconds * 0.25)
        loopEnd = loopStart + te::TimeDuration::fromSeconds (minLengthSeconds);

    edit->getTransport().setLoopRange ({ loopStart, loopEnd });
    edit->getTransport().looping = true;
    updateTransportLoopButton();
    timelineRuler.repaint();
}

void BeatMakerNoRecord::handleTimelineRulerMouseUp (const juce::MouseEvent&, int)
{
    if (timelinePanDragActive)
    {
        timelinePanDragActive = false;
        setStatus ("Panned timeline view.");
        return;
    }

    if (! timelineLoopDragActive)
        return;

    timelineLoopDragActive = false;
    setStatus ("Set loop range from ruler drag.");
}

void BeatMakerNoRecord::handleTimelineRulerMouseWheel (const juce::MouseEvent& e,
                                                       const juce::MouseWheelDetails& wheel,
                                                       int width)
{
    if (edit == nullptr || editComponent == nullptr || width <= 1)
        return;

    const double primaryDelta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
    if (std::abs (primaryDelta) <= 1.0e-6)
        return;

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        auto& viewState = editComponent->getEditViewState();
        const int x = juce::jlimit (0, width, e.x);
        const auto anchor = viewState.xToTime (x, width);
        const double strength = juce::jlimit (0.2, 2.0, std::abs (primaryDelta) * 2.5);
        const double factor = primaryDelta >= 0.0 ? std::pow (0.86, strength) : std::pow (1.16, strength);
        zoomTimelineAroundTime (factor, anchor);
        return;
    }

    if (e.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        auto& viewState = editComponent->getEditViewState();
        const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
        moveTimelineViewportBySeconds (-primaryDelta * visibleSeconds * 0.28);
        return;
    }

    auto step = getGridDurationAt (edit->getTransport().getPosition());
    if (e.mods.isAltDown())
        step = te::TimeDuration::fromSeconds (juce::jmax (0.005, step.inSeconds() * 0.25));

    const auto now = edit->getTransport().getPosition();
    const auto destination = now + te::TimeDuration::fromSeconds (primaryDelta * step.inSeconds() * 3.0);
    edit->getTransport().setPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, destination.inSeconds())));
    updateTransportInfoLabel();
}

void BeatMakerNoRecord::showTimelineRulerContextMenu (const juce::MouseEvent&, int)
{
    if (edit == nullptr)
        return;

    enum TimelineMenuIds
    {
        timelineMenuCenterPlayhead = 1,
        timelineMenuFocusSelection,
        timelineMenuFitProject,
        timelineMenuToggleLoop,
        timelineMenuLoopSelection,
        timelineMenuAlignSelectionToBar,
        timelineMenuLoop2Bars,
        timelineMenuLoop4Bars,
        timelineMenuFillTransportLoop,
        timelineMenuAddMarker,
        timelineMenuReturnStart
    };

    const bool hasSelectionClip = (getSelectedClip() != nullptr);
    juce::PopupMenu menu;
    menu.addSectionHeader ("Timeline");
    menu.addItem (timelineMenuCenterPlayhead, "Center Playhead");
    menu.addItem (timelineMenuFocusSelection, "Focus Selection");
    menu.addItem (timelineMenuFitProject, "Fit Project");
    menu.addSeparator();
    menu.addItem (timelineMenuToggleLoop, edit->getTransport().looping ? "Disable Loop" : "Enable Loop");
    menu.addItem (timelineMenuLoopSelection, "Loop Selected Clip", hasSelectionClip);
    menu.addSeparator();
    menu.addItem (timelineMenuAlignSelectionToBar, "Align Selected Clip To Bar", hasSelectionClip);
    menu.addItem (timelineMenuLoop2Bars, "Make 2-Bar Loop From Selected Clip", hasSelectionClip);
    menu.addItem (timelineMenuLoop4Bars, "Make 4-Bar Loop From Selected Clip", hasSelectionClip);
    menu.addItem (timelineMenuFillTransportLoop, "Fill Transport Loop With Selected Clip", hasSelectionClip);
    menu.addSeparator();
    menu.addItem (timelineMenuAddMarker, "Add Marker At Playhead");
    menu.addItem (timelineMenuReturnStart, "Return To Start");

    switch (menu.showMenu (juce::PopupMenu::Options().withTargetComponent (&timelineRuler)))
    {
        case timelineMenuCenterPlayhead:
            centerPlayheadInView();
            break;
        case timelineMenuFocusSelection:
            focusSelectedClipInView();
            break;
        case timelineMenuFitProject:
            fitProjectInView();
            break;
        case timelineMenuToggleLoop:
            toggleTransportLooping();
            break;
        case timelineMenuLoopSelection:
            setTransportLoopToSelectedClip();
            break;
        case timelineMenuAlignSelectionToBar:
            alignSelectedClipToBar();
            break;
        case timelineMenuLoop2Bars:
            makeSelectedClipLoop (2);
            break;
        case timelineMenuLoop4Bars:
            makeSelectedClipLoop (4);
            break;
        case timelineMenuFillTransportLoop:
            fillTransportLoopWithSelectedClip();
            break;
        case timelineMenuAddMarker:
            addMarkerAtPlayhead();
            break;
        case timelineMenuReturnStart:
            if (edit != nullptr)
            {
                edit->getTransport().setPosition (te::TimePosition());
                updateTransportInfoLabel();
                setStatus ("Moved playhead to start.");
            }
            break;
        default:
            break;
    }
}

void BeatMakerNoRecord::setPlayheadFromRulerX (int x, int width)
{
    if (edit == nullptr || editComponent == nullptr || width <= 1)
        return;

    const int clamped = juce::jlimit (0, width, x);
    auto& viewState = editComponent->getEditViewState();
    edit->getTransport().setPosition (viewState.xToTime (clamped, width));
    updateTransportInfoLabel();
}

void BeatMakerNoRecord::moveTimelineViewportBySeconds (double deltaSeconds)
{
    if (editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const double visibleSeconds = juce::jmax (0.25, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
    const double totalSeconds = juce::jmax (visibleSeconds, getTimelineTotalLengthSeconds() + 2.0);
    const double maxStart = juce::jmax (0.0, totalSeconds - visibleSeconds);
    const double currentStart = juce::jmax (0.0, viewState.viewX1.get().inSeconds());
    const double nextStart = juce::jlimit (0.0, maxStart, currentStart + deltaSeconds);

    viewState.viewX1 = te::TimePosition::fromSeconds (nextStart);
    viewState.viewX2 = te::TimePosition::fromSeconds (nextStart + visibleSeconds);
    syncViewControlsFromState();
}

void BeatMakerNoRecord::zoomTimelineAroundTime (double factor, te::TimePosition anchorTime)
{
    if (editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const double currentStart = viewState.viewX1.get().inSeconds();
    const double currentEnd = viewState.viewX2.get().inSeconds();
    const double currentVisible = juce::jmax (0.25, currentEnd - currentStart);
    const double anchorSeconds = juce::jmax (0.0, anchorTime.inSeconds());
    const double anchorRatio = juce::jlimit (0.0, 1.0, (anchorSeconds - currentStart) / currentVisible);

    const double newVisible = juce::jlimit (0.125, 3600.0, currentVisible * factor);
    const double totalSeconds = juce::jmax (newVisible, getTimelineTotalLengthSeconds() + 2.0);
    const double maxStart = juce::jmax (0.0, totalSeconds - newVisible);
    const double newStart = juce::jlimit (0.0, maxStart, anchorSeconds - newVisible * anchorRatio);

    viewState.viewX1 = te::TimePosition::fromSeconds (newStart);
    viewState.viewX2 = te::TimePosition::fromSeconds (newStart + newVisible);
    syncViewControlsFromState();
}

bool BeatMakerNoRecord::shouldAnimateTimelineRuler() const
{
    if (! isShowing() || ! timelineRuler.isShowing() || edit == nullptr || editComponent == nullptr)
        return false;

    return timelineLoopDragActive || edit->getTransport().isPlaying();
}

bool BeatMakerNoRecord::shouldAnimateMidiPianoRoll() const
{
    if (! isShowing() || ! midiPianoRoll.isShowing() || edit == nullptr || editComponent == nullptr)
        return false;

    if (pianoRollDraggedNote != nullptr)
        return true;

    return getSelectedMidiClip() != nullptr && edit->getTransport().isPlaying();
}

bool BeatMakerNoRecord::shouldAnimateStepSequencer() const
{
    if (! isShowing() || ! stepSequencer.isShowing() || edit == nullptr || editComponent == nullptr)
        return false;

    if (stepSequencerDragMode != StepSequencerDragMode::none)
        return true;

    return getSelectedMidiClip() != nullptr && edit->getTransport().isPlaying();
}

void BeatMakerNoRecord::paintTimelineRuler (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Opaque timeline component must be fully cleared each frame to avoid playhead/marker trails.
    g.fillAll (juce::Colour::fromRGB (12, 17, 24));

    if (edit == nullptr || editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const auto visibleStart = viewState.viewX1.get();
    const auto visibleEnd = viewState.viewX2.get();
    const auto visibleLenSeconds = (visibleEnd - visibleStart).inSeconds();

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.toFloat(), 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine (area.getY() + 1, (float) area.getX() + 4.0f, (float) area.getRight() - 4.0f);

    if (visibleLenSeconds <= 0.0 || area.getWidth() <= 1)
        return;

    if (edit->getTransport().looping)
    {
        const auto loopRange = edit->getTransport().getLoopRange();
        if (! loopRange.isEmpty())
        {
            const int loopX1 = area.getX() + viewState.timeToX (loopRange.getStart(), area.getWidth());
            const int loopX2 = area.getX() + viewState.timeToX (loopRange.getEnd(), area.getWidth());
            const int x1 = juce::jlimit (area.getX(), area.getRight(), juce::jmin (loopX1, loopX2));
            const int x2 = juce::jlimit (area.getX(), area.getRight(), juce::jmax (loopX1, loopX2));
            if (x2 > x1)
            {
                g.setColour (juce::Colours::limegreen.withAlpha (0.18f));
                g.fillRect (juce::Rectangle<int> (x1, area.getY() + 1, x2 - x1, area.getHeight() - 2));
            }
        }
    }

    if (auto* arrangerTrack = edit->getArrangerTrack())
    {
        for (auto* clip : arrangerTrack->getClips())
        {
            auto* section = dynamic_cast<te::ArrangerClip*> (clip);
            if (section == nullptr)
                continue;

            const auto sectionRange = section->getEditTimeRange();
            const int x1 = area.getX() + viewState.timeToX (sectionRange.getStart(), area.getWidth());
            const int x2 = area.getX() + viewState.timeToX (sectionRange.getEnd(), area.getWidth());
            const int clampedX1 = juce::jlimit (area.getX(), area.getRight(), juce::jmin (x1, x2));
            const int clampedX2 = juce::jlimit (area.getX(), area.getRight(), juce::jmax (x1, x2));

            if (clampedX2 > clampedX1)
            {
                g.setColour (juce::Colour::fromRGB (74, 157, 226).withAlpha (0.16f));
                g.fillRect (juce::Rectangle<int> (clampedX1, area.getY() + 1, clampedX2 - clampedX1, 7));
            }
        }
    }

    const auto beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (visibleStart));
    const int beatsPerBarInt = juce::jmax (1, roundToInt (beatsPerBar));

    const auto startBeat = (int) std::floor (edit->tempoSequence.toBeats (visibleStart).inBeats());
    const auto endBeat = (int) std::ceil (edit->tempoSequence.toBeats (visibleEnd).inBeats()) + 1;

    for (int beat = startBeat; beat <= endBeat; ++beat)
    {
        const auto time = edit->tempoSequence.toTime (te::BeatPosition::fromBeats ((double) beat));
        const int x = area.getX() + viewState.timeToX (time, area.getWidth());
        if (! area.contains (x, area.getCentreY()))
            continue;

        const bool isBar = (beat % beatsPerBarInt) == 0;
        g.setColour (isBar ? juce::Colours::white.withAlpha (0.7f) : juce::Colours::white.withAlpha (0.3f));
        g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());

        if (isBar)
        {
            const int barIndex = juce::jmax (1, (beat / beatsPerBarInt) + 1);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText (juce::String (barIndex), x + 3, area.getY(), 36, area.getHeight() - 2, juce::Justification::topLeft, false);
        }
    }

    const double gridStepSeconds = getGridDurationAt (visibleStart).inSeconds();
    if (gridStepSeconds > 0.0)
    {
        const auto firstVisibleSeconds = visibleStart.inSeconds();
        const auto lastVisibleSeconds = visibleEnd.inSeconds();
        const auto firstGrid = std::floor (firstVisibleSeconds / gridStepSeconds) * gridStepSeconds;

        for (double t = firstGrid; t <= lastVisibleSeconds; t += gridStepSeconds)
        {
            const int x = area.getX() + viewState.timeToX (te::TimePosition::fromSeconds (t), area.getWidth());
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawVerticalLine (x, (float) (area.getBottom() - 7), (float) area.getBottom());
        }
    }

    auto markers = edit->getMarkerManager().getMarkers();
    for (auto* marker : markers)
    {
        const auto markerStart = marker->getPosition().getStart();
        const int x = area.getX() + viewState.timeToX (markerStart, area.getWidth());
        if (! area.contains (x, area.getCentreY()))
            continue;

        g.setColour (juce::Colour::fromRGB (255, 178, 43).withAlpha (0.9f));
        g.drawLine ((float) x, (float) area.getY(), (float) x, (float) area.getBottom(), 1.6f);
        g.drawText (juce::String (marker->getMarkerID()), x + 2, area.getY() + 8, 24, 12, juce::Justification::left, false);
    }

    const int playheadX = area.getX() + viewState.timeToX (edit->getTransport().getPosition(), area.getWidth());
    g.setColour (juce::Colours::yellow);
    g.drawLine ((float) playheadX, (float) area.getY(), (float) playheadX, (float) area.getBottom(), 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 3.0f, 1.0f);
}

void BeatMakerNoRecord::paintStepSequencer (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.fillAll (juce::Colour::fromRGB (14, 18, 27));

    const auto layout = getStepSequencerGeometry (area);
    const auto header = layout.headerArea;
    const auto grid = layout.gridArea;
    const auto laneArea = layout.laneArea;
    const auto footer = layout.footerArea;

    g.setColour (juce::Colours::black.withAlpha (0.26f));
    g.fillRoundedRectangle (layout.frame.toFloat().translated (0.0f, 1.0f), 5.0f);

    juce::ColourGradient frameFill (juce::Colour::fromRGB (16, 21, 30).withAlpha (0.94f),
                                    (float) layout.frame.getX(), (float) layout.frame.getY(),
                                    juce::Colour::fromRGB (10, 14, 22).withAlpha (0.92f),
                                    (float) layout.frame.getX(), (float) layout.frame.getBottom(),
                                    false);
    g.setGradientFill (frameFill);
    g.fillRoundedRectangle (layout.frame.toFloat(), 4.0f);

    if (grid.isEmpty())
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.82f));
        g.drawText ("Select a MIDI clip to use the step sequencer",
                    layout.frame.reduced (8, 4),
                    juce::Justification::centred,
                    false);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawRoundedRectangle (layout.frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
        return;
    }

    const auto& lanes = getStepSequencerLanes();
    const int laneCount = (int) lanes.size();
    if (laneCount <= 0)
        return;

    const bool useDragLockedPage = stepSequencerDragMode != StepSequencerDragMode::none;
    auto stepLength = useDragLockedPage ? stepSequencerDragStepLength
                                        : getStepSequencerStepLengthBeats (*midiClip);
    if (stepLength.inBeats() <= 0.0)
        stepLength = getStepSequencerStepLengthBeats (*midiClip);

    const double pageStartBeat = useDragLockedPage ? stepSequencerDragPageStartBeat
                                                   : getStepSequencerPageStartBeat (*midiClip);
    const double stepLengthBeats = juce::jmax (1.0 / 64.0, stepLength.inBeats());
    const double pageEndBeat = pageStartBeat + stepLengthBeats * (double) stepSequencerStepsPerBar;
    const int laneHeight = juce::jmax (1, grid.getHeight() / laneCount);
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (midiClip->getPosition().getStart()));
    const int barIndex = juce::jmax (1, juce::roundToInt (std::floor (pageStartBeat / beatsPerBar)) + 1);

    auto headerLane = header;
    headerLane.setWidth (laneArea.getWidth());
    auto headerGrid = header.withTrimmedLeft (laneArea.getWidth() + 4);

    g.setColour (juce::Colour::fromRGB (26, 34, 50).withAlpha (0.92f));
    g.fillRoundedRectangle (header.toFloat(), 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.84f));
    g.setFont (juce::Font (juce::FontOptions (10.4f, juce::Font::bold)));
    g.drawText ("Step Sequencer", headerLane.reduced (7, 0), juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.56f));
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::plain)));
    g.drawText ("Bar " + juce::String (barIndex) + " | 1/16",
                headerGrid.reduced (2, 0),
                juce::Justification::centredRight,
                false);

    for (int step = 0; step < stepSequencerStepsPerBar; ++step)
    {
        const int x1 = grid.getX() + (step * grid.getWidth()) / stepSequencerStepsPerBar;
        const int x2 = grid.getX() + ((step + 1) * grid.getWidth()) / stepSequencerStepsPerBar;
        auto stepHeader = juce::Rectangle<int> (x1, headerGrid.getY(), juce::jmax (1, x2 - x1), headerGrid.getHeight());

        const bool barBoundary = (step % 4) == 0;
        g.setColour (barBoundary ? juce::Colour::fromRGB (67, 107, 165).withAlpha (0.34f)
                                 : juce::Colour::fromRGB (38, 53, 77).withAlpha (0.24f));
        g.fillRect (stepHeader.reduced (1, 1));

        g.setColour (juce::Colours::white.withAlpha (barBoundary ? 0.88f : 0.64f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::plain)));
        g.drawText (juce::String (step + 1), stepHeader, juce::Justification::centred, false);

        g.setColour (juce::Colours::white.withAlpha (barBoundary ? 0.24f : 0.11f));
        g.drawVerticalLine (x1, (float) headerGrid.getY(), (float) grid.getBottom());
    }

    std::array<std::array<float, stepSequencerStepsPerBar>, 8> cells {};
    for (auto* note : midiClip->getSequence().getNotes())
    {
        if (note == nullptr)
            continue;

        const double startBeat = note->getStartBeat().inBeats();
        if (startBeat < pageStartBeat - (stepLengthBeats * 0.45) || startBeat > pageEndBeat + (stepLengthBeats * 0.45))
            continue;

        int laneIndex = -1;
        for (size_t i = 0; i < lanes.size(); ++i)
            if (lanes[i].noteNumber == note->getNoteNumber())
            {
                laneIndex = (int) i;
                break;
            }

        if (laneIndex < 0 || laneIndex >= laneCount)
            continue;

        const int step = juce::roundToInt ((startBeat - pageStartBeat) / stepLengthBeats);
        if (step < 0 || step >= stepSequencerStepsPerBar)
            continue;

        const float velocity = (float) juce::jlimit (0.0, 1.0, note->getVelocity() / 127.0);
        cells[(size_t) laneIndex][(size_t) step] = juce::jmax (cells[(size_t) laneIndex][(size_t) step], velocity);
    }

    for (int lane = 0; lane < laneCount; ++lane)
    {
        const int rowY = grid.getY() + lane * laneHeight;
        const int rowBottom = (lane == laneCount - 1) ? grid.getBottom() : (rowY + laneHeight);
        const int rowHeight = juce::jmax (1, rowBottom - rowY);

        const auto laneLabelRect = juce::Rectangle<int> (laneArea.getX(), rowY, laneArea.getWidth(), rowHeight);
        const auto rowRect = juce::Rectangle<int> (grid.getX(), rowY, grid.getWidth(), rowHeight);

        g.setColour ((lane % 2 == 0) ? juce::Colour::fromRGB (28, 36, 51) : juce::Colour::fromRGB (24, 32, 46));
        g.fillRect (laneLabelRect);
        g.setColour ((lane % 2 == 0) ? juce::Colour::fromRGB (35, 47, 66) : juce::Colour::fromRGB (31, 42, 59));
        g.fillRect (rowRect);

        g.setColour (juce::Colours::white.withAlpha (0.84f));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (lanes[(size_t) lane].label, laneLabelRect.reduced (7, 0), juce::Justification::centredLeft, false);

        for (int step = 0; step < stepSequencerStepsPerBar; ++step)
        {
            const int x1 = grid.getX() + (step * grid.getWidth()) / stepSequencerStepsPerBar;
            const int x2 = grid.getX() + ((step + 1) * grid.getWidth()) / stepSequencerStepsPerBar;
            auto cell = juce::Rectangle<int> (x1, rowY, juce::jmax (1, x2 - x1), rowHeight).reduced (1, 1);

            const float velocity = cells[(size_t) lane][(size_t) step];
            if (velocity > 0.0f)
            {
                auto onColour = juce::Colour::fromRGB (96, 176, 255).interpolatedWith (juce::Colour::fromRGB (158, 224, 255), velocity);
                g.setColour (onColour);
                g.fillRoundedRectangle (cell.toFloat(), 2.6f);
                g.setColour (juce::Colours::white.withAlpha (0.24f));
                g.drawRoundedRectangle (cell.toFloat().reduced (0.6f), 2.4f, 1.0f);
            }
            else
            {
                g.setColour (juce::Colour::fromRGB (17, 23, 34).withAlpha (0.92f));
                g.fillRoundedRectangle (cell.toFloat(), 2.1f);
            }

            const bool quarterStep = (step % 4) == 0;
            g.setColour (quarterStep ? juce::Colours::white.withAlpha (0.22f)
                                     : juce::Colours::white.withAlpha (0.08f));
            g.drawVerticalLine (x1, (float) rowY, (float) rowBottom);
        }

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawHorizontalLine (rowBottom, (float) grid.getX(), (float) grid.getRight());
    }

    if (edit != nullptr)
    {
        const double playBeat = midiClip->getContentBeatAtTime (edit->getTransport().getPosition()).inBeats();
        if (playBeat >= pageStartBeat && playBeat <= pageEndBeat)
        {
            const double norm = (playBeat - pageStartBeat) / juce::jmax (1.0e-6, pageEndBeat - pageStartBeat);
            const int playheadX = grid.getX() + roundToInt (norm * grid.getWidth());
            g.setColour (juce::Colours::yellow.withAlpha (0.85f));
            g.drawLine ((float) playheadX, (float) grid.getY(), (float) playheadX, (float) grid.getBottom(), 1.8f);
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.70f));
    g.setFont (juce::Font (juce::FontOptions (9.8f, juce::Font::plain)));
    g.drawText ("Bar " + juce::String (barIndex) + " | Left click toggle | Drag paint | Right click erase",
                footer.reduced (2, 0),
                juce::Justification::centredLeft,
                false);

    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (layout.frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
}

void BeatMakerNoRecord::paintMidiPianoRoll (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Opaque piano-roll component must be fully cleared each frame to avoid playhead trails.
    g.fillAll (juce::Colour::fromRGB (17, 20, 27));

    const auto layout = getPianoRollGeometry (area);
    const auto frame = layout.frame;
    const auto ruler = layout.rulerArea;
    const auto keyboard = layout.keyboardArea;
    const auto grid = layout.gridArea;
    const auto velocityLane = layout.velocityArea;
    const auto footer = layout.footerArea;

    g.setColour (juce::Colours::black.withAlpha (0.24f));
    g.fillRoundedRectangle (frame.toFloat().translated (0.0f, 1.0f), 5.0f);

    juce::ColourGradient frameFill (juce::Colour::fromRGB (18, 23, 33).withAlpha (0.95f),
                                    (float) frame.getX(), (float) frame.getY(),
                                    juce::Colour::fromRGB (12, 16, 24).withAlpha (0.92f),
                                    (float) frame.getX(), (float) frame.getBottom(),
                                    false);
    g.setGradientFill (frameFill);
    g.fillRoundedRectangle (frame.toFloat(), 5.0f);

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawText ("Select a MIDI clip to edit notes", frame, juce::Justification::centred, false);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
        return;
    }

    syncPianoRollViewportToSelection (true);

    auto* clipTrack = getMidiClipOwnerTrack (midiClip);
    auto* instrumentPlugin = clipTrack != nullptr ? getFirstEnabledInstrumentPlugin (*clipTrack) : nullptr;
    const auto instrumentLabel = getInstrumentWorkflowLabel (instrumentPlugin);
    const double maxBeat = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClip));
    const double viewStartBeat = juce::jlimit (0.0, maxBeat, pianoRollViewStartBeat);
    const double viewLengthBeats = juce::jlimit (1.0 / 16.0, maxBeat, pianoRollViewLengthBeats);
    const double viewEndBeat = viewStartBeat + viewLengthBeats;
    const double beatsPerBar = juce::jmax (1.0, getBeatsPerBarAt (midiClip->getPosition().getStart()));
    const int beatsPerBarInt = juce::jmax (1, roundToInt (beatsPerBar));

    const auto headerTabs = getPianoRollHeaderTabs (layout);
    auto drawEditorTab = [&g] (juce::Rectangle<int> tabBounds, const juce::String& text, bool active)
    {
        g.setColour (active ? juce::Colour::fromRGB (62, 123, 198).withAlpha (0.92f)
                            : juce::Colour::fromRGB (36, 47, 63).withAlpha (0.90f));
        g.fillRoundedRectangle (tabBounds.toFloat(), 4.0f);
        g.setColour (juce::Colours::white.withAlpha (active ? 0.96f : 0.80f));
        g.setFont (juce::Font (juce::FontOptions (10.4f, juce::Font::bold)));
        g.drawText (text, tabBounds, juce::Justification::centred, false);
    };
    drawEditorTab (headerTabs.pianoTab, "Piano Roll", true);
    drawEditorTab (headerTabs.stepTab, "Step Sequencer", false);

    g.setColour (juce::Colours::white.withAlpha (0.82f));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("Track: " + (clipTrack != nullptr ? clipTrack->getName() : juce::String ("-"))
                + " | Instrument: " + instrumentLabel
                + " | View " + juce::String (viewStartBeat, 2) + "-" + juce::String (viewEndBeat, 2)
                + " beats",
                headerTabs.infoArea.reduced (4, 0),
                juce::Justification::centredRight,
                false);

    if (instrumentPlugin == nullptr)
    {
        g.setColour (juce::Colour::fromRGB (243, 188, 91).withAlpha (0.88f));
        g.setFont (juce::Font (juce::FontOptions (9.6f, juce::Font::plain)));
        g.drawText ("No instrument on this MIDI track. Right-click piano roll -> Ensure Instrument / Add AU/VST3 Instrument.",
                    footer.reduced (3, 0),
                    juce::Justification::centredRight,
                    false);
    }

    const int lowestNote = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, pianoRollViewLowestNote);
    const int noteCount = juce::jmax (1, pianoRollViewNoteCount);
    const int highestNote = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, lowestNote + noteCount - 1);
    const int rowHeight = juce::jmax (1, grid.getHeight() / noteCount);

    g.setColour (juce::Colour::fromRGB (28, 36, 50).withAlpha (0.92f));
    g.fillRect (ruler);
    g.setColour (juce::Colour::fromRGB (31, 39, 55).withAlpha (0.94f));
    g.fillRect (keyboard.withY (ruler.getY()).withHeight (ruler.getHeight()));

    const int fullRulerX = grid.getX();
    const int fullRulerWidth = grid.getWidth();
    const double firstRulerBeat = std::floor (viewStartBeat);
    const double lastRulerBeat = std::ceil (viewEndBeat);
    for (double beat = firstRulerBeat; beat <= lastRulerBeat + 0.0001; beat += 1.0)
    {
        const int x = fullRulerX + (int) std::round (((beat - viewStartBeat) / viewLengthBeats) * (double) fullRulerWidth);
        const bool isBar = (juce::roundToInt (beat) % beatsPerBarInt) == 0;
        g.setColour (isBar ? juce::Colours::white.withAlpha (0.35f) : juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine (x, (float) ruler.getY(), (float) grid.getBottom());

        if (isBar)
        {
            const int barNumber = juce::jmax (1, (int) std::floor (beat / beatsPerBar) + 1);
            g.setColour (juce::Colours::white.withAlpha (0.86f));
            g.setFont (juce::Font (juce::FontOptions (9.6f, juce::Font::bold)));
            g.drawText (juce::String (barNumber),
                        juce::Rectangle<int> (x + 3, ruler.getY(), 26, ruler.getHeight()),
                        juce::Justification::centredLeft,
                        false);
        }
    }

    for (int i = 0; i < noteCount; ++i)
    {
        const int noteNumber = highestNote - i;
        const bool blackKey = isBlackMidiKey (noteNumber);
        const int y = grid.getY() + i * rowHeight;
        const auto keyRow = juce::Rectangle<int> (keyboard.getX(), y, keyboard.getWidth(), rowHeight);
        const auto gridRow = juce::Rectangle<int> (grid.getX(), y, grid.getWidth(), rowHeight);

        g.setColour (blackKey ? juce::Colour::fromRGB (33, 36, 44) : juce::Colour::fromRGB (228, 231, 236));
        g.fillRect (keyRow);
        g.setColour (blackKey ? juce::Colour::fromRGB (29, 33, 43) : juce::Colour::fromRGB (39, 44, 55));
        g.fillRect (gridRow);

        if ((noteNumber % 12) == 0)
        {
            const int octave = (noteNumber / 12) - 1;
            g.setColour (blackKey ? juce::Colours::white.withAlpha (0.74f) : juce::Colour::fromRGB (16, 20, 28).withAlpha (0.90f));
            g.setFont (juce::Font (juce::FontOptions (9.4f, juce::Font::bold)));
            g.drawText ("C" + juce::String (octave), keyRow.reduced (4, 0), juce::Justification::centredLeft, false);
        }
    }

    const double baseGridBeats = juce::jmax (1.0 / 32.0, getPianoRollGridBeats().inBeats());
    const int maxGridLineCount = juce::jmax (96, grid.getWidth() / 2);
    auto resolveAdaptiveStep = [maxGridLineCount, viewLengthBeats] (double step)
    {
        const double safeStep = juce::jmax (1.0 / 64.0, step);
        const double targetLines = juce::jmax (1.0, viewLengthBeats / safeStep);
        const double multiple = std::ceil (targetLines / (double) juce::jmax (1, maxGridLineCount));
        return safeStep * juce::jmax (1.0, multiple);
    };

    const double majorStepBeats = resolveAdaptiveStep (1.0);
    const double firstMajorBeat = std::floor (viewStartBeat / majorStepBeats) * majorStepBeats;
    for (double beat = firstMajorBeat; beat <= viewEndBeat + 0.0001; beat += majorStepBeats)
    {
        const int x = grid.getX() + (int) std::round (((beat - viewStartBeat) / viewLengthBeats) * grid.getWidth());
        g.setColour (juce::Colours::white.withAlpha (majorStepBeats <= 1.0001 ? 0.22f : 0.18f));
        g.drawVerticalLine (x, (float) grid.getY(), (float) grid.getBottom());
    }

    const double minorStepBeats = resolveAdaptiveStep (baseGridBeats);
    if (minorStepBeats + 1.0e-6 < majorStepBeats)
    {
        const double firstMinorBeat = std::floor (viewStartBeat / minorStepBeats) * minorStepBeats;
        for (double beat = firstMinorBeat; beat <= viewEndBeat + 0.0001; beat += minorStepBeats)
        {
            const double majorRemainder = std::fmod (beat, majorStepBeats);
            if (majorRemainder <= 1.0e-6 || std::abs (majorRemainder - majorStepBeats) <= 1.0e-6)
                continue;

            const int x = grid.getX() + (int) std::round (((beat - viewStartBeat) / viewLengthBeats) * grid.getWidth());
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.drawVerticalLine (x, (float) grid.getY(), (float) grid.getBottom());
        }
    }

    const auto notes = midiClip->getSequence().getNotes();
    const bool denseNoteView = notes.size() > 4000 || viewLengthBeats > 96.0;
    for (auto* note : notes)
    {
        auto noteRect = getPianoRollNoteBounds (*note, grid.getWidth(), grid.getHeight())
                            .translated (grid.getX(), grid.getY());
        if (noteRect.isEmpty())
            continue;

        const float velocityNorm = (float) juce::jlimit (0.0, 1.0, note->getVelocity() / 127.0);
        auto noteColour = juce::Colour::fromRGB (110, 221, 112).interpolatedWith (juce::Colour::fromRGB (215, 246, 122), velocityNorm);

        if (note == pianoRollDraggedNote)
            noteColour = juce::Colour::fromRGB (245, 162, 49);

        g.setColour (noteColour);
        g.fillRoundedRectangle (noteRect.toFloat(), 2.0f);
        if (! denseNoteView)
        {
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawRoundedRectangle (noteRect.toFloat().reduced (0.5f), 2.0f, 1.0f);

            const int handleX = juce::jmax (noteRect.getX(), noteRect.getRight() - 3);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawVerticalLine (handleX, (float) noteRect.getY(), (float) noteRect.getBottom());
        }
    }

    if (edit != nullptr)
    {
        const double playBeat = juce::jmax (0.0, midiClip->getContentBeatAtTime (edit->getTransport().getPosition()).inBeats());
        if (playBeat >= viewStartBeat - 1.0e-6 && playBeat <= viewEndBeat + 1.0e-6)
        {
            const int playheadX = grid.getX() + (int) std::round (((playBeat - viewStartBeat) / viewLengthBeats) * grid.getWidth());
            g.setColour (juce::Colours::yellow.withAlpha (0.8f));
            g.drawLine ((float) playheadX, (float) ruler.getY(), (float) playheadX, (float) grid.getBottom(), 1.5f);
            g.drawLine ((float) playheadX, (float) velocityLane.getY(), (float) playheadX, (float) velocityLane.getBottom(), 1.2f);
        }
    }

    g.setColour (juce::Colour::fromRGB (24, 31, 43).withAlpha (0.92f));
    g.fillRect (velocityLane);
    for (auto* note : notes)
    {
        if (note == nullptr)
            continue;

        const double startBeat = juce::jmax (0.0, note->getStartBeat().inBeats());
        if (startBeat < viewStartBeat || startBeat > viewEndBeat)
            continue;

        const int x = grid.getX() + (int) std::round (((startBeat - viewStartBeat) / viewLengthBeats) * grid.getWidth());
        const float velNorm = (float) juce::jlimit (0.0, 1.0, note->getVelocity() / 127.0);
        const int barHeight = juce::jmax (2, roundToInt (velNorm * (float) (velocityLane.getHeight() - 4)));
        const auto velRect = juce::Rectangle<int> (x - 1,
                                                   velocityLane.getBottom() - barHeight - 1,
                                                   2,
                                                   barHeight);
        g.setColour (juce::Colour::fromRGB (117, 217, 126).withAlpha (0.82f));
        g.fillRect (velRect);
    }
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::plain)));
    g.drawText ("Velocity", velocityLane.reduced (4, 0), juce::Justification::topLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.72f));
    g.drawText ("L-drag move | edge-drag resize | Shift-drag velocity | Alt-drag duplicate | Ruler-drag scrub playhead | Wheel pitch scroll | Shift+wheel time scroll | Cmd/Ctrl+wheel zoom time | Alt+wheel zoom pitch | Middle-drag pan | Dbl-click create/delete | Right-click tools",
                footer.reduced (4, 0),
                juce::Justification::centredLeft,
                false);
}

void BeatMakerNoRecord::paintMixerArea (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.fillAll (juce::Colour::fromRGB (18, 24, 34));

    auto frame = area.reduced (2);
    g.setColour (juce::Colour::fromRGB (12, 16, 23).withAlpha (0.86f));
    g.fillRoundedRectangle (frame.toFloat(), 4.0f);

    if (edit == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText ("No edit loaded", frame, juce::Justification::centred, false);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
        return;
    }

    auto tracks = te::getAudioTracks (*edit);
    if (tracks.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText ("No tracks. Add a track to use the mixer.", frame, juce::Justification::centred, false);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
        return;
    }

    auto* selectedTrack = getSelectedTrackOrFirst();
    const auto routingLayout = getMixerRoutingLayout (frame);
    const auto stripsArea = routingLayout.stripsArea;
    auto busArea = routingLayout.busArea;
    const int trackCount = tracks.size();
    const int routeTop = busArea.getY() + 42;
    const int routeBottom = juce::jmax (routeTop + 4, busArea.getBottom() - 28);
    const float railX = (float) routingLayout.railX;

    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (busArea.toFloat().translated (0.0f, 1.0f), 7.0f);

    juce::ColourGradient busGradient (juce::Colour::fromRGB (30, 43, 61).withAlpha (0.88f), (float) busArea.getX(), (float) busArea.getY(),
                                      juce::Colour::fromRGB (19, 27, 38).withAlpha (0.92f), (float) busArea.getX(), (float) busArea.getBottom(), false);
    g.setGradientFill (busGradient);
    g.fillRoundedRectangle (busArea.toFloat(), 7.0f);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (busArea.toFloat().reduced (0.5f), 7.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.90f));
    g.setFont (juce::Font (juce::FontOptions (11.6f, juce::Font::bold)));
    g.drawFittedText ("MASTER BUS", busArea.removeFromTop (20).reduced (8, 0), juce::Justification::centredLeft, 1);
    g.setColour (juce::Colours::white.withAlpha (0.58f));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::plain)));
    g.drawFittedText ("Track outputs route through these wires.", busArea.removeFromTop (14).reduced (8, 0), juce::Justification::centredLeft, 1);

    const auto railRect = juce::Rectangle<float> (railX - 1.5f, (float) routeTop, 3.0f, (float) juce::jmax (2, routeBottom - routeTop));
    g.setColour (juce::Colour::fromRGB (74, 118, 178).withAlpha (0.58f));
    g.fillRoundedRectangle (railRect, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (railRect, 1.5f, 1.0f);

    for (int i = 0; i < trackCount; ++i)
    {
        auto* track = tracks.getUnchecked (i);
        if (track == nullptr)
            continue;

        const auto layout = getMixerStripLayout (stripsArea, i, trackCount);
        const bool selected = (track == selectedTrack);
        const bool muted = track->isMuted (false);
        const bool solo = track->isSolo (false);
        const auto stripRect = layout.strip;
        const auto trackState = getMixerTrackUiState (*track);

        const float sourceX = (float) (layout.strip.getRight() - 2);
        const float sourceY = (float) layout.panTrack.getCentreY();
        const float targetY = trackCount <= 1
                                  ? (float) (routeTop + routeBottom) * 0.5f
                                  : juce::jmap ((float) i, 0.0f, (float) juce::jmax (1, trackCount - 1), (float) routeTop, (float) routeBottom);
        const auto wireColour = getRoutingWireColour (selected, muted, solo);

        juce::Path routePath;
        routePath.startNewSubPath (sourceX, sourceY);
        routePath.cubicTo (sourceX + 22.0f, sourceY, railX - 24.0f, targetY, railX, targetY);

        g.setColour (wireColour.withAlpha (wireColour.getFloatAlpha() * 0.28f));
        g.strokePath (routePath, juce::PathStrokeType (4.8f));
        g.setColour (wireColour);
        g.strokePath (routePath, juce::PathStrokeType (2.2f));

        g.setColour (wireColour.withAlpha (0.95f));
        g.fillEllipse (sourceX - 2.8f, sourceY - 2.8f, 5.6f, 5.6f);

        juce::Path arrow;
        arrow.addTriangle (railX + 1.0f, targetY, railX - 7.0f, targetY - 4.0f, railX - 7.0f, targetY + 4.0f);
        g.fillPath (arrow);

        const auto trackAccent = track->getColour().withSaturation (0.48f).withAlpha (selected ? 0.84f : 0.52f);
        auto stripColour = juce::Colour::fromRGB (28, 38, 54);
        if (selected)
            stripColour = stripColour.brighter (0.10f);

        g.setColour (juce::Colours::black.withAlpha (0.24f));
        g.fillRoundedRectangle (stripRect.toFloat().translated (0.0f, 1.0f), 6.0f);

        juce::ColourGradient stripFill (stripColour.brighter (0.05f), (float) stripRect.getX(), (float) stripRect.getY(),
                                        stripColour.darker (0.18f), (float) stripRect.getX(), (float) stripRect.getBottom(), false);
        g.setGradientFill (stripFill);
        g.fillRoundedRectangle (stripRect.toFloat(), 6.0f);

        g.setColour (trackAccent);
        auto accentStrip = stripRect;
        g.fillRoundedRectangle (accentStrip.removeFromTop (3).toFloat(), 2.0f);

        g.setColour (juce::Colours::white.withAlpha (selected ? 0.94f : 0.78f));
        g.setFont (juce::Font (juce::FontOptions (11.8f, juce::Font::bold)));
        auto titleArea = layout.header;
        titleArea.reduce (1, 0);

        if (trackState.hasInstrument)
            titleArea.removeFromLeft (11);
        if (trackState.userFxCount > 0)
            titleArea.removeFromRight (18);

        g.drawFittedText (track->getName(), titleArea, juce::Justification::centred, 1);

        if (trackState.hasInstrument)
        {
            g.setColour (juce::Colour::fromRGB (118, 224, 167).withAlpha (0.90f));
            g.fillEllipse ((float) layout.header.getX() + 3.0f, (float) layout.header.getCentreY() - 3.0f, 6.0f, 6.0f);
        }

        if (trackState.userFxCount > 0)
        {
            auto fxBadge = juce::Rectangle<int> (layout.header.getRight() - 16, layout.header.getY() + 2, 14, 12);
            g.setColour (juce::Colour::fromRGB (59, 108, 170).withAlpha (0.95f));
            g.fillRoundedRectangle (fxBadge.toFloat(), 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.setFont (juce::Font (juce::FontOptions (9.2f, juce::Font::bold)));
            g.drawFittedText (juce::String (trackState.userFxCount), fxBadge, juce::Justification::centred, 1);
        }

        const auto muteRect = layout.muteButton;
        const auto soloRect = layout.soloButton;
        drawMixerModeButton (g, muteRect, "M", muted, juce::Colour::fromRGB (75, 156, 110));
        drawMixerModeButton (g, soloRect, "S", solo, juce::Colour::fromRGB (196, 152, 60));

        const auto faderTrack = layout.faderTrack;

        const auto level = getMixerVolumeNormalised (trackState.volumeDb);
        const int thumbY = roundToInt (juce::jmap (level, (double) faderTrack.getBottom(), (double) faderTrack.getY()));
        const auto thumb = juce::Rectangle<int> (faderTrack.getX() - 5, thumbY - 4, faderTrack.getWidth() + 10, 8);

        g.setColour (juce::Colour::fromRGB (30, 38, 50));
        g.fillRoundedRectangle (faderTrack.toFloat(), 3.0f);
        g.setColour (juce::Colour::fromRGB (82, 149, 223).withAlpha (0.74f));
        g.fillRoundedRectangle (juce::Rectangle<int> (faderTrack.getX(), thumbY, faderTrack.getWidth(), faderTrack.getBottom() - thumbY).toFloat(), 3.0f);
        g.setColour (juce::Colour::fromRGB (173, 214, 255).withAlpha (0.96f));
        g.fillRoundedRectangle (thumb.toFloat(), 3.0f);

        const auto panTrack = layout.panTrack;
        g.setColour (juce::Colour::fromRGB (52, 68, 89));
        g.fillRoundedRectangle (panTrack.toFloat(), 2.0f);
        const int centreX = panTrack.getCentreX();
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.drawVerticalLine (centreX, (float) panTrack.getY() - 2.0f, (float) panTrack.getBottom() + 2.0f);

        const double panNormalised = juce::jlimit (0.0, 1.0, ((double) trackState.pan + 1.0) * 0.5);
        const int panX = roundToInt (juce::jmap (panNormalised, (double) panTrack.getX(), (double) panTrack.getRight()));
        g.setColour (juce::Colour::fromRGB (146, 201, 255).withAlpha (0.95f));
        g.fillEllipse ((float) panX - 4.5f, (float) panTrack.getCentreY() - 4.5f, 9.0f, 9.0f);

        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.setFont (juce::Font (juce::FontOptions (9.8f, juce::Font::plain)));
        const auto panText = juce::String (trackState.pan, 2);
        g.drawFittedText (juce::String (trackState.volumeDb, 1) + " dB", juce::Rectangle<int> (stripRect.getX() + 6, stripRect.getBottom() - 26, stripRect.getWidth() - 12, 10), juce::Justification::centred, 1);
        g.drawFittedText ("Pan " + panText, juce::Rectangle<int> (stripRect.getX() + 6, stripRect.getBottom() - 14, stripRect.getWidth() - 12, 10), juce::Justification::centred, 1);

        g.setColour (juce::Colours::white.withAlpha (selected ? 0.24f : 0.12f));
        g.drawRoundedRectangle (stripRect.toFloat().reduced (0.5f), 6.0f, 1.0f);

        g.setColour (wireColour.withAlpha (0.82f));
        g.drawEllipse ((float) (stripRect.getRight() - 7), (float) layout.panTrack.getCentreY() - 3.0f, 6.0f, 6.0f, 1.4f);
    }

    const float liveLevel = outputLevelMeter != nullptr
                                ? (float) juce::jlimit (0.0, 1.0, outputLevelMeter->getCurrentLevel())
                                : 0.0f;
    const float ballistic = liveLevel > outputMeterSmoothed ? 0.42f : 0.11f;
    outputMeterSmoothed += (liveLevel - outputMeterSmoothed) * ballistic;
    const double masterLevel = juce::jlimit (0.0, 1.0, (double) outputMeterSmoothed);

    auto meterArea = routingLayout.meterArea;
    if (! meterArea.isEmpty())
    {
        g.setColour (juce::Colour::fromRGB (24, 34, 47));
        g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);

        const int fillHeight = juce::jlimit (0, meterArea.getHeight(), roundToInt (masterLevel * meterArea.getHeight()));
        auto filled = meterArea.removeFromBottom (fillHeight);
        juce::ColourGradient meterGrad (juce::Colour::fromRGB (102, 219, 136), (float) filled.getX(), (float) filled.getBottom(),
                                        juce::Colour::fromRGB (239, 202, 86), (float) filled.getX(), (float) filled.getY(), false);
        g.setGradientFill (meterGrad);
        g.fillRoundedRectangle (filled.toFloat(), 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (routingLayout.meterArea.toFloat().reduced (0.5f), 3.0f, 1.0f);

        auto meterLabel = routingLayout.meterArea.withY (routingLayout.meterArea.getBottom() + 4).withHeight (14);
        g.setColour (juce::Colours::white.withAlpha (0.68f));
        g.setFont (juce::Font (juce::FontOptions (9.8f, juce::Font::bold)));
        g.drawFittedText ("OUT", meterLabel, juce::Justification::centred, 1);
    }

    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (frame.toFloat().reduced (0.5f), 4.0f, 1.0f);

    auto hintArea = routingLayout.busArea.withTrimmedTop (routingLayout.busArea.getHeight() - 20).reduced (6, 0);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (9.6f, juce::Font::plain)));
    g.drawFittedText ("Bright wire = active route", hintArea, juce::Justification::centredLeft, 1);
}

void BeatMakerNoRecord::handleStepSequencerMouseDown (const juce::MouseEvent& e, int width, int height)
{
    if (edit == nullptr || width <= 0 || height <= 0)
        return;

    if (getSelectedMidiClip() == nullptr)
    {
        createMidiClip();
    }

    StepSequencerPageContext context;
    if (! buildStepSequencerPageContext (context) || context.midiClip == nullptr)
        return;

    activeMidiClipID = context.midiClip->itemID;

    const auto layout = getStepSequencerGeometry ({ 0, 0, width, height });
    const int laneIndex = getStepSequencerLaneIndexForY (e.y, layout.gridArea);
    const int stepIndex = getStepSequencerStepIndexForX (e.x, layout.gridArea);
    if (laneIndex < 0 || stepIndex < 0)
    {
        resetStepSequencerDragState();
        return;
    }

    auto& undoManager = edit->getUndoManager();
    undoManager.beginNewTransaction ("Step Sequencer Edit");

    stepSequencerDragStepLength = context.stepLength;
    stepSequencerDragPageStartBeat = context.pageStartBeat;
    const bool hasNote = (getStepSequencerNoteAt (*context.midiClip,
                                                  laneIndex,
                                                  stepIndex,
                                                  context.pageStartBeat,
                                                  context.stepLength) != nullptr);
    const bool shouldEnable = e.mods.isRightButtonDown() ? false : ! hasNote;

    stepSequencerDragMode = shouldEnable ? StepSequencerDragMode::add : StepSequencerDragMode::remove;
    stepSequencerDragLastLane = laneIndex;
    stepSequencerDragLastStep = stepIndex;
    stepSequencerDragChangedAnyCell = setStepSequencerCellEnabled (*context.midiClip,
                                                                   laneIndex,
                                                                   stepIndex,
                                                                   context.pageStartBeat,
                                                                   context.stepLength,
                                                                   shouldEnable);

    if (stepSequencerDragChangedAnyCell)
    {
        stepSequencer.repaint();
        midiPianoRoll.repaint();
    }
}

void BeatMakerNoRecord::handleStepSequencerMouseDrag (const juce::MouseEvent& e, int width, int height)
{
    if (stepSequencerDragMode == StepSequencerDragMode::none)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr || edit == nullptr || width <= 0 || height <= 0)
        return;

    const auto layout = getStepSequencerGeometry ({ 0, 0, width, height });
    const int laneIndex = getStepSequencerLaneIndexForY (e.y, layout.gridArea);
    const int stepIndex = getStepSequencerStepIndexForX (e.x, layout.gridArea);
    if (laneIndex < 0 || stepIndex < 0)
        return;

    if (laneIndex == stepSequencerDragLastLane && stepIndex == stepSequencerDragLastStep)
        return;

    stepSequencerDragLastLane = laneIndex;
    stepSequencerDragLastStep = stepIndex;

    const bool shouldEnable = (stepSequencerDragMode == StepSequencerDragMode::add);
    auto stepLength = stepSequencerDragStepLength;
    double pageStartBeat = stepSequencerDragPageStartBeat;
    if (stepLength.inBeats() <= 0.0)
    {
        StepSequencerPageContext context;
        if (buildStepSequencerPageContext (context) && context.midiClip != nullptr)
        {
            midiClip = context.midiClip;
            stepLength = context.stepLength;
            pageStartBeat = context.pageStartBeat;
            stepSequencerDragStepLength = stepLength;
            stepSequencerDragPageStartBeat = pageStartBeat;
        }
        else
        {
            stepLength = getStepSequencerStepLengthBeats (*midiClip);
            pageStartBeat = getStepSequencerPageStartBeat (*midiClip);
        }
    }

    if (setStepSequencerCellEnabled (*midiClip,
                                     laneIndex,
                                     stepIndex,
                                     pageStartBeat,
                                     stepLength,
                                     shouldEnable))
    {
        stepSequencerDragChangedAnyCell = true;
        stepSequencer.repaint();
        midiPianoRoll.repaint();
    }
}

void BeatMakerNoRecord::handleStepSequencerMouseUp (const juce::MouseEvent&)
{
    if (stepSequencerDragMode == StepSequencerDragMode::none)
        return;

    if (stepSequencerDragChangedAnyCell)
    {
        if (stepSequencerDragMode == StepSequencerDragMode::add)
            setStatus ("Step sequencer updated (added notes).");
        else
            setStatus ("Step sequencer updated (removed notes).");
    }

    resetStepSequencerDragState();
}

void BeatMakerNoRecord::handleMixerAreaMouseDown (const juce::MouseEvent& e, int width, int height)
{
    auto syncMixerUi = [this]
    {
        updateTrackControlsFromSelection();
        updateButtonsFromState();
        mixerArea.repaint();
    };

    if (edit == nullptr || width <= 0 || height <= 0)
        return;

    auto tracks = te::getAudioTracks (*edit);
    if (tracks.isEmpty())
        return;

    const auto routingLayout = getMixerRoutingLayout (juce::Rectangle<int> (0, 0, width, height));
    const auto stripsArea = routingLayout.stripsArea;

    if (routingLayout.busArea.contains (e.getPosition()))
    {
        enum BusMenuIds
        {
            busMenuPrepareRouting = 1001,
            busMenuScanPlugins,
            busMenuAddTrack,
            busMenuAddFxToSelected
        };

        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addSectionHeader ("Master Routing");
            menu.addItem (busMenuPrepareRouting, "Prepare Playback Routing");
            menu.addItem (busMenuScanPlugins, "Scan Plugins...");
            menu.addItem (busMenuAddTrack, "Add Track");
            menu.addItem (busMenuAddFxToSelected, "Add AU/VST3 FX To Selected Track", getSelectedTrackOrFirst() != nullptr);

            switch (menu.show())
            {
                case busMenuPrepareRouting: prepareEditForPluginPlayback (true); break;
                case busMenuScanPlugins: openPluginScanDialog(); break;
                case busMenuAddTrack: addTrack(); break;
                case busMenuAddFxToSelected: addExternalPluginToSelectedTrack(); break;
                default: break;
            }

            syncMixerUi();
        }
        else
        {
            setStatus ("Mixer routing: channel strips feed the master bus wires on the right.");
        }

        mixerDragMode = MixerDragMode::none;
        mixerDragTrackIndex = -1;
        return;
    }

    int clickedTrackIndex = -1;

    for (int i = 0; i < tracks.size(); ++i)
    {
        const auto layout = getMixerStripLayout (stripsArea, i, tracks.size());
        if (layout.strip.contains (e.getPosition()))
        {
            clickedTrackIndex = i;
            break;
        }
    }

    if (clickedTrackIndex < 0)
    {
        mixerDragMode = MixerDragMode::none;
        mixerDragTrackIndex = -1;
        return;
    }

    auto* track = tracks.getUnchecked (clickedTrackIndex);
    if (track == nullptr)
        return;

    selectionManager.selectOnly (track);
    syncMixerUi();

    if (e.mods.isRightButtonDown())
    {
        enum TrackMenuIds
        {
            trackMenuToggleMute = 1,
            trackMenuToggleSolo,
            trackMenuResetMixer,
            trackMenuRename,
            trackMenuAddInstrument,
            trackMenuAddFx
        };

        juce::PopupMenu menu;
        menu.addSectionHeader (track->getName());
        menu.addItem (trackMenuToggleMute, track->isMuted (false) ? "Unmute Track" : "Mute Track");
        menu.addItem (trackMenuToggleSolo, track->isSolo (false) ? "Unsolo Track" : "Solo Track");
        menu.addSeparator();
        menu.addItem (trackMenuResetMixer, "Reset Volume/Pan");
        menu.addItem (trackMenuRename, "Rename Track...");
        menu.addSeparator();
        menu.addItem (trackMenuAddInstrument, "Add AU/VST3 Instrument...");
        menu.addItem (trackMenuAddFx, "Add AU/VST3 Effect...");

        switch (menu.show())
        {
            case trackMenuToggleMute:
                track->setMute (! track->isMuted (false));
                setStatus (track->isMuted (false) ? "Track muted." : "Track unmuted.");
                break;
            case trackMenuToggleSolo:
                track->setSolo (! track->isSolo (false));
                setStatus (track->isSolo (false) ? "Track solo enabled." : "Track solo disabled.");
                break;
            case trackMenuResetMixer:
                if (auto* vp = track->getVolumePlugin())
                {
                    vp->setVolumeDb (0.0f);
                    vp->setPan (0.0f);
                    setStatus ("Reset track volume and pan.");
                }
                break;
            case trackMenuRename:
                renameSelectedTrack();
                break;
            case trackMenuAddInstrument:
                addExternalInstrumentPluginToSelectedTrack();
                break;
            case trackMenuAddFx:
                addExternalPluginToSelectedTrack();
                break;
            default:
                break;
        }

        syncMixerUi();
        return;
    }

    const auto layout = getMixerStripLayout (stripsArea, clickedTrackIndex, tracks.size());

    if (auto* volumePlugin = track->getVolumePlugin())
    {
        const bool shouldResetVolume = layout.faderTrack.expanded (12, 4).contains (e.getPosition())
                                    && (e.getNumberOfClicks() >= 2 || e.mods.isAltDown());
        const bool shouldResetPan = layout.panTrack.expanded (4, 8).contains (e.getPosition())
                                 && (e.getNumberOfClicks() >= 2 || e.mods.isAltDown());

        if (shouldResetVolume || shouldResetPan)
        {
            if (shouldResetVolume)
                volumePlugin->setVolumeDb (0.0f);

            if (shouldResetPan)
                volumePlugin->setPan (0.0f);

            mixerDragMode = MixerDragMode::none;
            mixerDragTrackIndex = -1;
            syncMixerUi();

            if (shouldResetVolume && shouldResetPan)
                setStatus ("Reset mixer volume and pan.");
            else if (shouldResetVolume)
                setStatus ("Reset mixer volume to 0.0 dB.");
            else
                setStatus ("Reset mixer pan to center.");

            return;
        }
    }

    if (layout.muteButton.contains (e.getPosition()))
    {
        track->setMute (! track->isMuted (false));
        mixerDragMode = MixerDragMode::none;
        mixerDragTrackIndex = -1;
        syncMixerUi();
        return;
    }

    if (layout.soloButton.contains (e.getPosition()))
    {
        track->setSolo (! track->isSolo (false));
        mixerDragMode = MixerDragMode::none;
        mixerDragTrackIndex = -1;
        syncMixerUi();
        return;
    }

    mixerDragTrackIndex = clickedTrackIndex;

    if (layout.faderTrack.expanded (12, 4).contains (e.getPosition()))
        mixerDragMode = MixerDragMode::volume;
    else if (layout.panTrack.expanded (4, 8).contains (e.getPosition()))
        mixerDragMode = MixerDragMode::pan;
    else
        mixerDragMode = MixerDragMode::none;

    handleMixerAreaMouseDrag (e, width, height);
}

void BeatMakerNoRecord::handleMixerAreaMouseDrag (const juce::MouseEvent& e, int width, int height)
{
    if (edit == nullptr || mixerDragMode == MixerDragMode::none || mixerDragTrackIndex < 0)
        return;

    auto tracks = te::getAudioTracks (*edit);
    if (mixerDragTrackIndex >= tracks.size())
        return;

    auto* track = tracks.getUnchecked (mixerDragTrackIndex);
    if (track == nullptr)
        return;

    auto* volumePlugin = track->getVolumePlugin();
    if (volumePlugin == nullptr)
        return;

    const auto stripsArea = getMixerRoutingLayout (juce::Rectangle<int> (0, 0, width, height)).stripsArea;
    const auto layout = getMixerStripLayout (stripsArea, mixerDragTrackIndex, tracks.size());

    if (mixerDragMode == MixerDragMode::volume)
        volumePlugin->setVolumeDb ((float) getMixerVolumeDbFromY (e.y, layout.faderTrack));
    else if (mixerDragMode == MixerDragMode::pan)
        volumePlugin->setPan ((float) getMixerPanFromX (e.x, layout.panTrack));

    updatingTrackControls = true;
    trackVolumeSlider.setValue (volumePlugin->getVolumeDb(), juce::dontSendNotification);
    trackPanSlider.setValue (volumePlugin->getPan(), juce::dontSendNotification);
    updatingTrackControls = false;

    mixerArea.repaint();
}

void BeatMakerNoRecord::handleMixerAreaMouseUp (const juce::MouseEvent&)
{
    if (mixerDragMode == MixerDragMode::volume)
        setStatus ("Adjusted mixer volume.");
    else if (mixerDragMode == MixerDragMode::pan)
        setStatus ("Adjusted mixer pan.");

    mixerDragMode = MixerDragMode::none;
    mixerDragTrackIndex = -1;
    updateTrackControlsFromSelection();
    mixerArea.repaint();
}

void BeatMakerNoRecord::handleMidiPianoRollMouseDown (const juce::MouseEvent& e, int width, int height)
{
    if (edit == nullptr)
        return;

    const auto layout = getPianoRollGeometry ({ 0, 0, width, height });
    if (e.mods.isLeftButtonDown() && ! e.mods.isRightButtonDown())
    {
        const auto headerTabs = getPianoRollHeaderTabs (layout);
        if (headerTabs.stepTab.contains (e.getPosition()))
        {
            setPianoEditorLayoutMode (PianoEditorLayoutMode::stepSequencer, true, false);
            return;
        }

        if (headerTabs.pianoTab.contains (e.getPosition()))
        {
            setPianoEditorLayoutMode (PianoEditorLayoutMode::pianoRoll, true, false);
            return;
        }
    }

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
    {
        createMidiClip();
        midiClip = getSelectedMidiClip();
    }

    if (midiClip == nullptr)
        return;

    activeMidiClipID = midiClip->itemID;
    syncPianoRollViewportToSelection (true);

    auto scrubPlayheadToRulerX = [this, midiClip, &layout] (int mouseX, bool centerTimelineView)
    {
        if (edit == nullptr || layout.rulerArea.isEmpty())
            return;

        const int rulerLeft = layout.rulerArea.getX();
        const int rulerRight = layout.rulerArea.getRight();
        const int clampedX = juce::jlimit (rulerLeft, rulerRight, mouseX);
        const int rulerWidth = juce::jmax (1, layout.rulerArea.getWidth());
        const double normalised = (double) (clampedX - rulerLeft) / (double) rulerWidth;
        const double visibleRangeBeats = juce::jmax (1.0 / 16.0, pianoRollViewLengthBeats);
        const double rawBeat = pianoRollViewStartBeat + normalised * visibleRangeBeats;
        const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClip));
        const double beat = juce::jlimit (0.0, clipLengthBeats, rawBeat);

        edit->getTransport().setPosition (midiClip->getTimeOfRelativeBeat (te::BeatDuration::fromBeats (beat)));
        if (centerTimelineView)
            centerPlayheadInView();
        updateTransportInfoLabel();
    };

    if (e.mods.isLeftButtonDown() && ! e.mods.isRightButtonDown() && layout.rulerArea.contains (e.getPosition()))
    {
        scrubPlayheadToRulerX (e.x, e.mods.isShiftDown());
        pianoRollRulerScrubActive = true;
        pianoRollPanDragActive = false;
        resetPianoRollNoteDragState();
        midiPianoRoll.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        return;
    }

    pianoRollRulerScrubActive = false;

    const bool startPanDrag = e.mods.isMiddleButtonDown()
                              || (e.mods.isLeftButtonDown() && e.mods.isCommandDown() && ! e.mods.isAltDown());
    if (startPanDrag && layout.frame.contains (e.getPosition()))
    {
        pianoRollPanDragActive = true;
        pianoRollPanDragStart = e.getPosition();
        pianoRollPanStartBeat = pianoRollViewStartBeat;
        pianoRollPanStartLowestNote = pianoRollViewLowestNote;
        pianoRollRulerScrubActive = false;
        resetPianoRollNoteDragState();
        midiPianoRoll.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }

    if (layout.gridArea.isEmpty())
        return;

    const auto gridPos = e.getPosition() - layout.gridArea.getPosition();
    const bool pointerInGrid = layout.gridArea.contains (e.getPosition());
    const int gridWidth = layout.gridArea.getWidth();
    const int gridHeight = layout.gridArea.getHeight();

    auto* note = pointerInGrid ? getPianoRollNoteAt (gridPos.x, gridPos.y, gridWidth, gridHeight) : nullptr;
    auto& sequence = midiClip->getSequence();
    auto& undoManager = edit->getUndoManager();
    auto* clipTrack = getMidiClipOwnerTrack (midiClip);
    const auto activeTool = getTimelineEditTool();
    const bool isDoubleClick = e.getNumberOfClicks() >= 2 && e.mods.isLeftButtonDown();

    if (e.mods.isRightButtonDown())
    {
        enum PianoRollMenuIds
        {
            pianoRollMenuDeleteNote = 1,
            pianoRollMenuDuplicateNote,
            pianoRollMenuEnsureInstrument,
            pianoRollMenuAddExternalInstrument,
            pianoRollMenuOpenInstrumentUi,
            pianoRollMenuPreparePlayback,
            pianoRollMenuQuantizeClip,
            pianoRollMenuLegatoClip,
            pianoRollMenuBounceClip,
            pianoRollMenuGenChords,
            pianoRollMenuGenArp,
            pianoRollMenuGenBass,
            pianoRollMenuGenDrums
        };

        juce::PopupMenu menu;
        menu.addSectionHeader ("Piano Roll");

        if (note != nullptr)
        {
            menu.addItem (pianoRollMenuDeleteNote, "Delete Note");
            menu.addItem (pianoRollMenuDuplicateNote, "Duplicate Note (Next Grid)");
            menu.addSeparator();
        }

        menu.addItem (pianoRollMenuEnsureInstrument, "Ensure Instrument (Prefer VST3)", clipTrack != nullptr);
        menu.addItem (pianoRollMenuAddExternalInstrument, "Add AU/VST3 Instrument...", clipTrack != nullptr);
        menu.addItem (pianoRollMenuOpenInstrumentUi, "Open Instrument UI",
                      clipTrack != nullptr && getFirstEnabledInstrumentPlugin (*clipTrack) != nullptr);
        menu.addSeparator();
        menu.addItem (pianoRollMenuPreparePlayback, "Prepare Playback Routing");
        menu.addItem (pianoRollMenuQuantizeClip, "Quantize Clip");
        menu.addItem (pianoRollMenuLegatoClip, "Legato Notes");
        menu.addItem (pianoRollMenuBounceClip, "Bounce Clip To Audio");
        menu.addSeparator();
        menu.addSectionHeader ("Generate");
        menu.addItem (pianoRollMenuGenChords, "Generate Chords");
        menu.addItem (pianoRollMenuGenArp, "Generate Arp");
        menu.addItem (pianoRollMenuGenBass, "Generate Bassline");
        menu.addItem (pianoRollMenuGenDrums, "Generate Drum Pattern");

        const int selected = menu.showMenu (juce::PopupMenu::Options().withTargetComponent (&midiPianoRoll));
        if (selected <= 0)
            return;

        switch (selected)
        {
            case pianoRollMenuDeleteNote:
            {
                if (note != nullptr)
                {
                    sequence.removeNote (*note, &undoManager);
                    setStatus ("Removed MIDI note.");
                }
                break;
            }

            case pianoRollMenuDuplicateNote:
            {
                if (note != nullptr)
                {
                    const auto grid = getPianoRollGridBeats();
                    constexpr double minimumLengthBeats = 1.0 / 128.0;
                    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
                    const double sourceLength = juce::jmax (minimumLengthBeats, note->getLengthBeats().inBeats());
                    const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - minimumLengthBeats);
                    const double duplicatedStartBeat = juce::jlimit (0.0,
                                                                     maxStartBeat,
                                                                     note->getStartBeat().inBeats() + grid.inBeats());
                    const double maxLengthBeats = juce::jmax (minimumLengthBeats, clipLengthBeats - duplicatedStartBeat);
                    const double duplicatedLengthBeats = juce::jlimit (minimumLengthBeats, maxLengthBeats, sourceLength);

                    auto* duplicated = sequence.addNote (note->getNoteNumber(),
                                                         te::BeatPosition::fromBeats (duplicatedStartBeat),
                                                         te::BeatDuration::fromBeats (duplicatedLengthBeats),
                                                         note->getVelocity(),
                                                         note->getColour(),
                                                         &undoManager);
                    if (duplicated != nullptr)
                        setStatus ("Duplicated MIDI note to next grid.");
                }
                break;
            }

            case pianoRollMenuEnsureInstrument:
            {
                if (clipTrack != nullptr)
                {
                    const bool changed = ensureTrackHasInstrumentForMidiPlayback (*clipTrack);
                    if (! changed)
                        setStatus ("Instrument already available for MIDI playback.");
                }
                break;
            }

            case pianoRollMenuAddExternalInstrument:
            {
                if (clipTrack != nullptr)
                {
                    selectionManager.selectOnly (clipTrack);
                    addExternalInstrumentPluginToSelectedTrack();
                    selectionManager.selectOnly (midiClip);
                }
                break;
            }

            case pianoRollMenuOpenInstrumentUi:
            {
                if (clipTrack != nullptr)
                {
                    if (auto* instrument = getFirstEnabledInstrumentPlugin (*clipTrack))
                    {
                        instrument->showWindowExplicitly();
                        setStatus ("Opened instrument UI: " + instrument->getName());
                    }
                }
                break;
            }

            case pianoRollMenuPreparePlayback:
            {
                prepareEditForPluginPlayback (true);
                break;
            }

            case pianoRollMenuQuantizeClip:
            {
                selectionManager.selectOnly (midiClip);
                quantizeSelectedMidiClip();
                break;
            }

            case pianoRollMenuLegatoClip:
            {
                selectionManager.selectOnly (midiClip);
                legatoSelectedMidiNotes();
                break;
            }

            case pianoRollMenuBounceClip:
            {
                selectionManager.selectOnly (midiClip);
                bounceSelectedMidiClipToAudio();
                break;
            }

            case pianoRollMenuGenChords:
            {
                selectionManager.selectOnly (midiClip);
                generateMidiChordProgression();
                break;
            }

            case pianoRollMenuGenArp:
            {
                selectionManager.selectOnly (midiClip);
                generateMidiArpeggioPattern();
                break;
            }

            case pianoRollMenuGenBass:
            {
                selectionManager.selectOnly (midiClip);
                generateMidiBasslinePattern();
                break;
            }

            case pianoRollMenuGenDrums:
            {
                selectionManager.selectOnly (midiClip);
                generateMidiDrumPattern();
                break;
            }

            default:
                break;
        }

        resetPianoRollNoteDragState();
        updateButtonsFromState();
        midiPianoRoll.repaint();
        return;
    }

    if (isDoubleClick)
    {
        if (note != nullptr)
        {
            sequence.removeNote (*note, &undoManager);
            setStatus ("Deleted MIDI note.");
            resetPianoRollNoteDragState();
            updateButtonsFromState();
            midiPianoRoll.repaint();
            return;
        }

        if (! pointerInGrid)
            return;

        if (clipTrack != nullptr && ! trackHasInstrumentPlugin (*clipTrack))
            ensureTrackHasInstrumentForMidiPlayback (*clipTrack);

        constexpr double minimumLengthBeats = 1.0 / 128.0;
        const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
        const double rawStartBeat = pianoRollXToBeat (gridPos.x, gridWidth);
        const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - minimumLengthBeats);
        const double startBeat = juce::jlimit (0.0, maxStartBeat, rawStartBeat);
        const int noteNumber = pianoRollYToNoteNumber (gridPos.y, gridHeight);
        const double baseLengthBeats = juce::jmax (minimumLengthBeats, getPianoRollGridBeats().inBeats());
        const double maxLengthBeats = juce::jmax (minimumLengthBeats, clipLengthBeats - startBeat);
        const auto length = te::BeatDuration::fromBeats (juce::jlimit (minimumLengthBeats, maxLengthBeats, baseLengthBeats));

        if (sequence.addNote (noteNumber,
                              te::BeatPosition::fromBeats (startBeat),
                              length,
                              100,
                              0,
                              &undoManager) != nullptr)
        {
            setStatus ("Created MIDI note.");
            updateButtonsFromState();
            midiPianoRoll.repaint();
        }

        return;
    }

    if (note != nullptr)
    {
        if (e.mods.isAltDown() && activeTool != TimelineEditTool::scissors)
        {
            constexpr double minimumLengthBeats = 1.0 / 128.0;
            const auto grid = getPianoRollGridBeats();
            const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
            const double sourceLengthBeats = juce::jmax (minimumLengthBeats, note->getLengthBeats().inBeats());
            const double duplicatedStartBeat = juce::jlimit (0.0,
                                                             juce::jmax (0.0, clipLengthBeats - minimumLengthBeats),
                                                             note->getStartBeat().inBeats() + grid.inBeats());
            const double maxLengthBeats = juce::jmax (minimumLengthBeats, clipLengthBeats - duplicatedStartBeat);
            const double duplicatedLengthBeats = juce::jlimit (minimumLengthBeats, maxLengthBeats, sourceLengthBeats);

            if (auto* duplicated = sequence.addNote (note->getNoteNumber(),
                                                     te::BeatPosition::fromBeats (duplicatedStartBeat),
                                                     te::BeatDuration::fromBeats (duplicatedLengthBeats),
                                                     note->getVelocity(),
                                                     note->getColour(),
                                                     &undoManager))
            {
                note = duplicated;
                setStatus ("Duplicated MIDI note (Alt-drag).");
            }
        }

        constexpr int resizeHandleWidth = 6;
        const auto noteRect = getPianoRollNoteBounds (*note, gridWidth, gridHeight);
        const bool nearStartHandle = gridPos.x <= noteRect.getX() + resizeHandleWidth;
        const bool nearEndHandle = gridPos.x >= noteRect.getRight() - resizeHandleWidth;

        if (activeTool == TimelineEditTool::scissors)
        {
            constexpr double minimumLengthBeats = 1.0 / 128.0;
            const double grid = juce::jmax (1.0 / 32.0, getPianoRollGridBeats().inBeats());
            const double sourceStart = note->getStartBeat().inBeats();
            const double sourceLength = juce::jmax (minimumLengthBeats, note->getLengthBeats().inBeats());
            const double sourceEnd = sourceStart + sourceLength;

            double splitBeat = pianoRollXToBeat (gridPos.x, gridWidth);
            splitBeat = std::round (splitBeat / grid) * grid;
            splitBeat = juce::jlimit (sourceStart + minimumLengthBeats,
                                      sourceEnd - minimumLengthBeats,
                                      splitBeat);

            if (splitBeat > sourceStart + minimumLengthBeats && splitBeat < sourceEnd - minimumLengthBeats)
            {
                const auto firstLength = te::BeatDuration::fromBeats (splitBeat - sourceStart);
                const auto secondLength = te::BeatDuration::fromBeats (sourceEnd - splitBeat);
                note->setStartAndLength (te::BeatPosition::fromBeats (sourceStart), firstLength, &undoManager);
                sequence.addNote (note->getNoteNumber(),
                                  te::BeatPosition::fromBeats (splitBeat),
                                  secondLength,
                                  note->getVelocity(),
                                  note->getColour(),
                                  &undoManager);
                setStatus ("Split MIDI note.");
            }

            midiPianoRoll.repaint();
            updateButtonsFromState();
            return;
        }

        pianoRollDraggedNote = note;
        pianoRollDragStart = e.getPosition();
        pianoRollDraggedNoteStartBeats = note->getStartBeat().inBeats();
        pianoRollDraggedNoteLengthBeats = juce::jmax (1.0 / 32.0, note->getLengthBeats().inBeats());
        pianoRollDraggedNotePitch = note->getNoteNumber();
        pianoRollDraggedNoteVelocity = note->getVelocity();
        if (activeTool == TimelineEditTool::resize)
            pianoRollDragMode = nearStartHandle ? PianoRollDragMode::resizeStart : PianoRollDragMode::resizeEnd;
        else if (activeTool == TimelineEditTool::pencil)
            pianoRollDragMode = PianoRollDragMode::resizeEnd;
        else if (e.mods.isShiftDown())
            pianoRollDragMode = PianoRollDragMode::velocity;
        else if (nearStartHandle)
            pianoRollDragMode = PianoRollDragMode::resizeStart;
        else if (nearEndHandle)
            pianoRollDragMode = PianoRollDragMode::resizeEnd;
        else
            pianoRollDragMode = PianoRollDragMode::move;
        pianoRollAddMode = false;
        handleMidiPianoRollMouseMove (e, width, height);
        return;
    }

    if (! pointerInGrid)
    {
        resetPianoRollNoteDragState();
        handleMidiPianoRollMouseMove (e, width, height);
        return;
    }

    if (activeTool != TimelineEditTool::pencil)
    {
        resetPianoRollNoteDragState();
        handleMidiPianoRollMouseMove (e, width, height);
        return;
    }

    if (clipTrack != nullptr && ! trackHasInstrumentPlugin (*clipTrack))
        ensureTrackHasInstrumentForMidiPlayback (*clipTrack);

    constexpr double minimumLengthBeats = 1.0 / 128.0;
    const double clipLengthBeats = getMidiClipLengthBeats (*midiClip);
    const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - minimumLengthBeats);
    const double startBeat = juce::jlimit (0.0, maxStartBeat, pianoRollXToBeat (gridPos.x, gridWidth));
    const int noteNumber = pianoRollYToNoteNumber (gridPos.y, gridHeight);
    const double rawLengthBeats = juce::jmax (minimumLengthBeats, getPianoRollGridBeats().inBeats());
    const double maxLengthBeats = juce::jmax (minimumLengthBeats, clipLengthBeats - startBeat);
    const auto length = te::BeatDuration::fromBeats (juce::jlimit (minimumLengthBeats, maxLengthBeats, rawLengthBeats));
    if (auto* newNote = sequence.addNote (noteNumber,
                                          te::BeatPosition::fromBeats (startBeat),
                                          length,
                                          100,
                                          0,
                                          &undoManager))
    {
        pianoRollDraggedNote = newNote;
        pianoRollDragStart = e.getPosition();
        pianoRollDraggedNoteStartBeats = newNote->getStartBeat().inBeats();
        pianoRollDraggedNoteLengthBeats = juce::jmax (1.0 / 32.0, newNote->getLengthBeats().inBeats());
        pianoRollDraggedNotePitch = newNote->getNoteNumber();
        pianoRollDraggedNoteVelocity = newNote->getVelocity();
        pianoRollDragMode = PianoRollDragMode::resizeEnd;
        pianoRollAddMode = true;
        updateButtonsFromState();
        handleMidiPianoRollMouseMove (e, width, height);
    }
}

void BeatMakerNoRecord::handleMidiPianoRollMouseDrag (const juce::MouseEvent& e, int width, int height)
{
    if (edit == nullptr)
        return;

    const auto layout = getPianoRollGeometry ({ 0, 0, width, height });
    if (layout.gridArea.isEmpty() || layout.frame.isEmpty())
        return;

    syncPianoRollViewportToSelection (false);

    if (pianoRollRulerScrubActive)
    {
        auto* midiClipForRuler = getSelectedMidiClip();
        if (midiClipForRuler == nullptr || layout.rulerArea.isEmpty())
            return;

        const int rulerLeft = layout.rulerArea.getX();
        const int rulerRight = layout.rulerArea.getRight();
        const int clampedX = juce::jlimit (rulerLeft, rulerRight, e.x);
        const int rulerWidth = juce::jmax (1, layout.rulerArea.getWidth());
        const double normalised = (double) (clampedX - rulerLeft) / (double) rulerWidth;
        const double visibleRangeBeats = juce::jmax (1.0 / 16.0, pianoRollViewLengthBeats);
        const double rawBeat = pianoRollViewStartBeat + normalised * visibleRangeBeats;
        const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClipForRuler));
        const double beat = juce::jlimit (0.0, clipLengthBeats, rawBeat);

        edit->getTransport().setPosition (midiClipForRuler->getTimeOfRelativeBeat (te::BeatDuration::fromBeats (beat)));
        if (e.mods.isShiftDown())
            centerPlayheadInView();
        updateTransportInfoLabel();
        return;
    }

    if (pianoRollPanDragActive)
    {
        const int gridWidth = juce::jmax (1, layout.gridArea.getWidth());
        const int gridHeight = juce::jmax (1, layout.gridArea.getHeight());
        const int visibleNoteCount = juce::jmax (1, pianoRollViewNoteCount);
        const double beatsPerPixel = pianoRollViewLengthBeats / (double) gridWidth;
        const double rowHeight = (double) gridHeight / (double) visibleNoteCount;
        const double beatDelta = (double) (pianoRollPanDragStart.x - e.x) * beatsPerPixel;
        const int noteDelta = juce::roundToInt ((double) (e.y - pianoRollPanDragStart.y) / juce::jmax (1.0, rowHeight));

        auto* midiClipForPan = getSelectedMidiClip();
        if (midiClipForPan == nullptr)
            return;

        const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClipForPan));
        const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - pianoRollViewLengthBeats);
        const int maxLowestNote = pianoRollMaxNote - pianoRollViewNoteCount + 1;

        pianoRollViewStartBeat = juce::jlimit (0.0, maxStartBeat, pianoRollPanStartBeat + beatDelta);
        pianoRollViewLowestNote = juce::jlimit (pianoRollMinNote, maxLowestNote, pianoRollPanStartLowestNote + noteDelta);
        updatePianoRollScrollbarsFromViewport();
        midiPianoRoll.repaint();
        return;
    }

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr || pianoRollDraggedNote == nullptr)
        return;

    const int gridWidth = layout.gridArea.getWidth();
    const int gridHeight = layout.gridArea.getHeight();

    auto& undoManager = edit->getUndoManager();
    const int noteCount = juce::jmax (1, pianoRollViewNoteCount);
    const int rowHeight = juce::jmax (1, gridHeight / noteCount);
    const int pitchDelta = (pianoRollDragStart.y - e.y) / rowHeight;
    const int noteNumber = juce::jlimit (pianoRollMinNote, pianoRollMaxNote, pianoRollDraggedNotePitch + pitchDelta);

    const double maxBeat = getMidiClipLengthBeats (*midiClip);
    const double deltaBeats = (double) (e.x - pianoRollDragStart.x)
                              / juce::jmax (1, gridWidth)
                              * juce::jmax (1.0 / 16.0, pianoRollViewLengthBeats);
    const double grid = juce::jmax (1.0 / 32.0, getPianoRollGridBeats().inBeats());

    switch (pianoRollDragMode)
    {
        case PianoRollDragMode::move:
        {
            const double snappedStart = pianoRollDraggedNoteStartBeats + std::round (deltaBeats / grid) * grid;
            const double noteLengthBeats = juce::jmax (1.0 / 128.0, pianoRollDraggedNote->getLengthBeats().inBeats());
            const double maxStartBeat = juce::jmax (0.0, maxBeat - noteLengthBeats);
            const double startBeat = juce::jlimit (0.0, maxStartBeat, snappedStart);

            pianoRollDraggedNote->setStartAndLength (te::BeatPosition::fromBeats (startBeat),
                                                     pianoRollDraggedNote->getLengthBeats(),
                                                     &undoManager);
            pianoRollDraggedNote->setNoteNumber (noteNumber, &undoManager);
            break;
        }

        case PianoRollDragMode::resizeEnd:
        {
            const double minimumLength = juce::jmax (1.0 / 32.0, grid);
            const double rawLength = pianoRollDraggedNoteLengthBeats + deltaBeats;
            const double snappedLength = std::round (rawLength / grid) * grid;
            const double maxLength = juce::jmax (minimumLength, maxBeat - juce::jmax (0.0, pianoRollDraggedNoteStartBeats));
            const double lengthBeats = juce::jlimit (minimumLength, maxLength, juce::jmax (minimumLength, snappedLength));

            pianoRollDraggedNote->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (0.0, pianoRollDraggedNoteStartBeats)),
                                                     te::BeatDuration::fromBeats (lengthBeats),
                                                     &undoManager);
            break;
        }

        case PianoRollDragMode::resizeStart:
        {
            const double minimumLength = juce::jmax (1.0 / 32.0, grid);
            const double clipEnd = pianoRollDraggedNoteStartBeats + juce::jmax (minimumLength, pianoRollDraggedNoteLengthBeats);
            const double rawStart = pianoRollDraggedNoteStartBeats + deltaBeats;
            const double snappedStart = std::round (rawStart / grid) * grid;
            const double maxStart = juce::jmax (0.0, clipEnd - minimumLength);
            const double startBeat = juce::jlimit (0.0, maxStart, snappedStart);
            const double lengthBeats = juce::jmax (minimumLength, clipEnd - startBeat);

            pianoRollDraggedNote->setStartAndLength (te::BeatPosition::fromBeats (startBeat),
                                                     te::BeatDuration::fromBeats (lengthBeats),
                                                     &undoManager);
            break;
        }

        case PianoRollDragMode::velocity:
        {
            const int velocityDelta = pianoRollDragStart.y - e.y;
            const int velocity = juce::jlimit (1, 127, pianoRollDraggedNoteVelocity + velocityDelta);
            pianoRollDraggedNote->setVelocity (velocity, &undoManager);
            break;
        }

        case PianoRollDragMode::none:
        default:
            break;
    }

    handleMidiPianoRollMouseMove (e, width, height);
}

void BeatMakerNoRecord::handleMidiPianoRollMouseUp (const juce::MouseEvent& e)
{
    if (pianoRollRulerScrubActive)
    {
        clearPianoRollNavigationInteraction();
        setStatus ("Moved playhead from piano roll ruler.");
        handleMidiPianoRollMouseMove (e, midiPianoRoll.getWidth(), midiPianoRoll.getHeight());
        return;
    }

    if (pianoRollPanDragActive)
    {
        clearPianoRollNavigationInteraction();
        handleMidiPianoRollMouseMove (e, midiPianoRoll.getWidth(), midiPianoRoll.getHeight());
        return;
    }

    if (pianoRollDraggedNote != nullptr)
    {
        if (pianoRollAddMode)
            setStatus ("Added MIDI note.");
        else if (pianoRollDragMode == PianoRollDragMode::resizeEnd || pianoRollDragMode == PianoRollDragMode::resizeStart)
            setStatus ("Resized MIDI note.");
        else if (pianoRollDragMode == PianoRollDragMode::velocity)
            setStatus ("Adjusted MIDI note velocity.");
        else if (pianoRollDragMode == PianoRollDragMode::move)
            setStatus ("Moved MIDI note.");
    }

    resetPianoRollNoteDragState();
    updateButtonsFromState();
    handleMidiPianoRollMouseMove (e, midiPianoRoll.getWidth(), midiPianoRoll.getHeight());
}

void BeatMakerNoRecord::handleMidiPianoRollMouseMove (const juce::MouseEvent& e, int width, int height)
{
    juce::MouseCursor cursor = juce::MouseCursor::NormalCursor;
    const auto tool = getTimelineEditTool();
    auto* midiClip = getSelectedMidiClip();
    const auto layout = getPianoRollGeometry ({ 0, 0, width, height });

    if (pianoRollPanDragActive)
    {
        midiPianoRoll.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }

    if (pianoRollRulerScrubActive)
    {
        midiPianoRoll.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        return;
    }

    if (midiClip == nullptr || edit == nullptr || width <= 0 || height <= 0 || layout.gridArea.isEmpty())
    {
        midiPianoRoll.setMouseCursor (cursor);
        return;
    }

    syncPianoRollViewportToSelection (false);

    if (! layout.gridArea.contains (e.getPosition()))
    {
        if (layout.rulerArea.contains (e.getPosition()))
            cursor = juce::MouseCursor::PointingHandCursor;
        else if (layout.keyboardArea.contains (e.getPosition()))
            cursor = juce::MouseCursor::PointingHandCursor;
        else if (layout.frame.contains (e.getPosition()) && (e.mods.isMiddleButtonDown() || e.mods.isCommandDown()))
            cursor = juce::MouseCursor::DraggingHandCursor;

        midiPianoRoll.setMouseCursor (cursor);
        return;
    }

    const auto gridPos = e.getPosition() - layout.gridArea.getPosition();

    if (tool == TimelineEditTool::pencil || tool == TimelineEditTool::scissors)
        cursor = juce::MouseCursor::CrosshairCursor;
    else if (tool == TimelineEditTool::resize)
        cursor = juce::MouseCursor::LeftRightResizeCursor;
    else if (e.mods.isMiddleButtonDown() || e.mods.isCommandDown())
        cursor = juce::MouseCursor::DraggingHandCursor;

    if (auto* note = getPianoRollNoteAt (gridPos.x, gridPos.y, layout.gridArea.getWidth(), layout.gridArea.getHeight()))
    {
        constexpr int resizeHandleWidth = 6;
        const auto noteRect = getPianoRollNoteBounds (*note, layout.gridArea.getWidth(), layout.gridArea.getHeight());
        const bool nearEdge = gridPos.x <= noteRect.getX() + resizeHandleWidth
                           || gridPos.x >= noteRect.getRight() - resizeHandleWidth;

        if (tool == TimelineEditTool::scissors)
            cursor = juce::MouseCursor::CrosshairCursor;
        else if (e.mods.isAltDown())
            cursor = juce::MouseCursor::CopyingCursor;
        else if (tool == TimelineEditTool::resize || nearEdge)
            cursor = juce::MouseCursor::LeftRightResizeCursor;
        else if (tool == TimelineEditTool::select && e.mods.isShiftDown())
            cursor = juce::MouseCursor::UpDownResizeCursor;
        else if (tool == TimelineEditTool::pencil)
            cursor = juce::MouseCursor::CrosshairCursor;
        else
            cursor = juce::MouseCursor::DraggingHandCursor;
    }

    midiPianoRoll.setMouseCursor (cursor);
}

void BeatMakerNoRecord::handleMidiPianoRollMouseWheel (const juce::MouseEvent& e,
                                                       const juce::MouseWheelDetails& wheel,
                                                       int width,
                                                       int height)
{
    if (edit == nullptr || width <= 0 || height <= 0)
        return;

    auto* midiClip = getSelectedMidiClip();
    if (midiClip == nullptr)
        return;

    const auto layout = getPianoRollGeometry ({ 0, 0, width, height });
    if (layout.gridArea.isEmpty())
        return;

    const bool inEditorArea = layout.gridArea.contains (e.getPosition())
                              || layout.keyboardArea.contains (e.getPosition())
                              || layout.rulerArea.contains (e.getPosition())
                              || layout.velocityArea.contains (e.getPosition());
    if (! inEditorArea)
        return;

    syncPianoRollViewportToSelection (false);

    const double primaryDelta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
    const double strength = juce::jlimit (0.2, 2.0, std::abs (primaryDelta) * 2.4);

    const int gridWidth = juce::jmax (1, layout.gridArea.getWidth());
    const int gridHeight = juce::jmax (1, layout.gridArea.getHeight());
    const int cursorGridX = juce::jlimit (0, gridWidth, e.x - layout.gridArea.getX());
    const int cursorGridY = juce::jlimit (0, gridHeight - 1, e.y - layout.gridArea.getY());
    const double anchorBeat = pianoRollViewStartBeat
                              + (double) cursorGridX / (double) gridWidth * pianoRollViewLengthBeats;
    const int anchorNote = pianoRollYToNoteNumber (cursorGridY, gridHeight);

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double factor = primaryDelta >= 0.0 ? std::pow (0.86, strength) : std::pow (1.16, strength);
        zoomPianoRollViewportTime (factor, anchorBeat);
        return;
    }

    if (e.mods.isAltDown())
    {
        const double factor = primaryDelta >= 0.0 ? std::pow (0.88, strength) : std::pow (1.14, strength);
        zoomPianoRollViewportPitch (factor, anchorNote);
        return;
    }

    if (e.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        const double beatDelta = -primaryDelta * pianoRollViewLengthBeats * 0.22;
        panPianoRollViewport (beatDelta, 0);
        return;
    }

    const int noteStep = juce::jmax (1, juce::roundToInt ((double) pianoRollViewNoteCount * 0.10));
    const int noteDelta = juce::roundToInt (wheel.deltaY * (double) noteStep);
    if (noteDelta != 0)
    {
        panPianoRollViewport (0.0, noteDelta);
        return;
    }

    if (wheel.deltaX != 0.0)
        panPianoRollViewport (-wheel.deltaX * pianoRollViewLengthBeats * 0.18, 0);
}

void BeatMakerNoRecord::applyTrackHeightFromUI()
{
    if (updatingViewControls || editComponent == nullptr)
        return;

    editComponent->getEditViewState().trackHeight = trackHeightSlider.getValue();
    syncViewControlsFromState();
}

void BeatMakerNoRecord::applyHorizontalScrollFromUI()
{
    if (updatingViewControls || editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const double visibleLength = (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds();
    const double totalLength = getTimelineTotalLengthSeconds();
    const double maxStart = juce::jmax (0.0, totalLength - visibleLength);

    const double startTime = horizontalScrollSlider.getValue() * maxStart;
    viewState.viewX1 = te::TimePosition::fromSeconds (startTime);
    viewState.viewX2 = te::TimePosition::fromSeconds (startTime + visibleLength);
    timelineRuler.repaint();
}

void BeatMakerNoRecord::applyVerticalScrollFromUI()
{
    if (updatingViewControls || editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const double fallbackTrackHeight = juce::jmax (28.0, viewState.trackHeight.get());
    const double contentHeight = edit != nullptr ? getVisibleTrackContentHeight (*edit, viewState)
                                                 : getVisibleTrackCount() * (fallbackTrackHeight + 2.0);
    const double viewportHeight = juce::jmax (1, lastEditViewportBounds.getHeight());
    const double minY = juce::jmin (0.0, viewportHeight - contentHeight);
    const double newY = juce::jmap (verticalScrollSlider.getValue(), 0.0, 1.0, 0.0, minY);

    viewState.viewY = newY;
}

void BeatMakerNoRecord::syncViewControlsFromState()
{
    if (editComponent == nullptr)
        return;

    updatingViewControls = true;

    auto& viewState = editComponent->getEditViewState();

    const double visibleLength = (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds();
    const double totalLength = getTimelineTotalLengthSeconds();
    const double maxStart = juce::jmax (0.0, totalLength - visibleLength);
    const double start = juce::jmax (0.0, viewState.viewX1.get().inSeconds());
    const double h = maxStart > 0.0 ? juce::jlimit (0.0, 1.0, start / maxStart) : 0.0;
    horizontalScrollSlider.setValue (h, juce::dontSendNotification);

    const double fallbackTrackHeight = juce::jmax (28.0, viewState.trackHeight.get());
    const double contentHeight = edit != nullptr ? getVisibleTrackContentHeight (*edit, viewState)
                                                 : getVisibleTrackCount() * (fallbackTrackHeight + 2.0);
    const double viewportHeight = juce::jmax (1, lastEditViewportBounds.getHeight());
    const double minY = juce::jmin (0.0, viewportHeight - contentHeight);
    const double y = juce::jlimit (minY, 0.0, viewState.viewY.get());
    if (std::abs (y - viewState.viewY.get()) > 1.0e-6)
        viewState.viewY = y;
    const double v = minY < 0.0 ? juce::jlimit (0.0, 1.0, (0.0 - y) / (0.0 - minY)) : 0.0;
    verticalScrollSlider.setValue (v, juce::dontSendNotification);
    verticalScrollSlider.setEnabled (minY < 0.0);

    trackHeightSlider.setValue (viewState.trackHeight.get(), juce::dontSendNotification);
    timelineRuler.repaint();
    mixerArea.repaint();
    updateTransportInfoLabel();

    updatingViewControls = false;
}

void BeatMakerNoRecord::refreshChannelRackInspector()
{
    updatingChannelRackControls = true;

    channelRackTrackBox.clear (juce::dontSendNotification);
    channelRackPluginBox.clear (juce::dontSendNotification);

    auto* selectedTrack = getSelectedTrackOrFirst();

    if (edit != nullptr)
    {
        auto tracks = te::getAudioTracks (*edit);
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto* track = tracks.getUnchecked (i);
            if (track == nullptr)
                continue;

            juce::String trackLabel = juce::String (i + 1) + ". " + track->getName();
            if (track->isMuted (false))
                trackLabel << " [M]";
            if (track->isSolo (false))
                trackLabel << " [S]";

            channelRackTrackBox.addItem (trackLabel, i + 1);
            if (track == selectedTrack)
                channelRackTrackBox.setSelectedId (i + 1, juce::dontSendNotification);
        }
    }

    if (selectedTrack != nullptr)
    {
        auto plugins = selectedTrack->pluginList.getPlugins();
        int selectedPluginId = 0;
        int itemId = 1;
        for (auto* plugin : plugins)
        {
            if (plugin == nullptr)
                continue;

            juce::String pluginName = plugin->getName().trim();
            if (pluginName.isEmpty())
                pluginName = plugin->getPluginType();
            if (! plugin->isEnabled())
                pluginName = "[Bypassed] " + pluginName;

            channelRackPluginBox.addItem (pluginName, itemId);

            if (auto* selectedPlugin = getSelectedTrackPlugin(); selectedPlugin == plugin)
                selectedPluginId = itemId;

            ++itemId;
        }

        if (selectedPluginId == 0 && channelRackPluginBox.getNumItems() > 0)
            selectedPluginId = juce::jlimit (1, channelRackPluginBox.getNumItems(), fxChainBox.getSelectedId());

        if (selectedPluginId > 0)
            channelRackPluginBox.setSelectedId (selectedPluginId, juce::dontSendNotification);

        const auto trackState = getMixerTrackUiState (*selectedTrack);
        inspectorTrackNameLabel.setText ("Track: " + selectedTrack->getName(), juce::dontSendNotification);
        inspectorRouteLabel.setText ("Routing: Vol " + juce::String (trackState.volumeDb, 1)
                                     + " dB | Pan " + juce::String (trackState.pan, 2)
                                     + " | FX " + juce::String (trackState.userFxCount),
                                     juce::dontSendNotification);

        if (auto* selectedPlugin = getSelectedTrackPlugin())
            inspectorPluginLabel.setText ("Plugin: " + selectedPlugin->getName(), juce::dontSendNotification);
        else
            inspectorPluginLabel.setText ("Plugin: none", juce::dontSendNotification);
    }
    else
    {
        inspectorTrackNameLabel.setText ("Track: -", juce::dontSendNotification);
        inspectorRouteLabel.setText ("Routing: -", juce::dontSendNotification);
        inspectorPluginLabel.setText ("Plugin: -", juce::dontSendNotification);
    }

    const float safeOutGain = juce::jmax (1.0e-5f, outputMeterSmoothed);
    const float outDb = juce::Decibels::gainToDecibels (safeOutGain, -100.0f);
    inspectorMeterLabel.setText ("OUT: " + juce::String (outDb, 1) + " dB", juce::dontSendNotification);

    updatingChannelRackControls = false;
}

void BeatMakerNoRecord::refreshBuiltInSynthControlsFromSelection()
{
    updatingBuiltInSynthControls = true;

    auto* plugin = getSelectedBuiltInSynthPlugin();
    if (plugin != nullptr)
    {
        juce::String targetText = "4OSC: ";
        if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (plugin->getOwnerTrack()))
            targetText << ownerTrack->getName();
        else
            targetText << plugin->getName();

        if (! plugin->isEnabled())
            targetText << " [Bypassed]";

        builtInSynthTargetLabel.setText (targetText, juce::dontSendNotification);
        builtInSynthCutoffSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "filterFreq", 0.82f), juce::dontSendNotification);
        builtInSynthResonanceSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "filterResonance", 0.12f), juce::dontSendNotification);
        builtInSynthEnvSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "filterAmount", 0.58f), juce::dontSendNotification);
        builtInSynthDriveSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "distortion", 0.04f), juce::dontSendNotification);
        builtInSynthAttackSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "ampAttack", 0.02f), juce::dontSendNotification);
        builtInSynthReleaseSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "ampRelease", 0.14f), juce::dontSendNotification);
        builtInSynthReverbSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "reverbMix", 0.08f), juce::dontSendNotification);
        builtInSynthDelaySlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "delayMix", 0.00f), juce::dontSendNotification);
        builtInSynthOsc1Slider.setValue (getBuiltInSynthParameterNormalised (*plugin, "level1", 0.88f), juce::dontSendNotification);
        builtInSynthOsc2Slider.setValue (getBuiltInSynthParameterNormalised (*plugin, "level2", 0.68f), juce::dontSendNotification);
        builtInSynthOsc3Slider.setValue (getBuiltInSynthParameterNormalised (*plugin, "level3", 0.54f), juce::dontSendNotification);
        builtInSynthOsc4Slider.setValue (getBuiltInSynthParameterNormalised (*plugin, "level4", 0.36f), juce::dontSendNotification);
        builtInSynthUnisonSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "detune2", 0.28f), juce::dontSendNotification);
        const float pan2 = getBuiltInSynthParameterNormalised (*plugin, "pan2", 0.66f);
        builtInSynthWidthSlider.setValue (juce::jlimit (0.0f, 1.0f, std::abs (pan2 - 0.5f) * 2.0f), juce::dontSendNotification);
        builtInSynthChorusSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "chorusMix", 0.18f), juce::dontSendNotification);
        builtInSynthMasterSlider.setValue (getBuiltInSynthParameterNormalised (*plugin, "masterLevel", 0.74f), juce::dontSendNotification);
    }
    else
    {
        builtInSynthTargetLabel.setText ("4OSC: none on selected track", juce::dontSendNotification);
        builtInSynthCutoffSlider.setValue (0.82, juce::dontSendNotification);
        builtInSynthResonanceSlider.setValue (0.12, juce::dontSendNotification);
        builtInSynthEnvSlider.setValue (0.58, juce::dontSendNotification);
        builtInSynthDriveSlider.setValue (0.04, juce::dontSendNotification);
        builtInSynthAttackSlider.setValue (0.02, juce::dontSendNotification);
        builtInSynthReleaseSlider.setValue (0.14, juce::dontSendNotification);
        builtInSynthReverbSlider.setValue (0.08, juce::dontSendNotification);
        builtInSynthDelaySlider.setValue (0.00, juce::dontSendNotification);
        builtInSynthOsc1Slider.setValue (0.88, juce::dontSendNotification);
        builtInSynthOsc2Slider.setValue (0.68, juce::dontSendNotification);
        builtInSynthOsc3Slider.setValue (0.54, juce::dontSendNotification);
        builtInSynthOsc4Slider.setValue (0.36, juce::dontSendNotification);
        builtInSynthUnisonSlider.setValue (0.28, juce::dontSendNotification);
        builtInSynthWidthSlider.setValue (0.66, juce::dontSendNotification);
        builtInSynthChorusSlider.setValue (0.18, juce::dontSendNotification);
        builtInSynthMasterSlider.setValue (0.74, juce::dontSendNotification);
    }

    updatingBuiltInSynthControls = false;
}

void BeatMakerNoRecord::updateTrackControlsFromSelection()
{
    updatingTrackControls = true;

    if (auto* track = getSelectedTrackOrFirst())
    {
        selectedTrackLabel.setText ("Track: " + track->getName(), juce::dontSendNotification);
        trackMuteButton.setToggleState (track->isMuted (false), juce::dontSendNotification);
        trackSoloButton.setToggleState (track->isSolo (false), juce::dontSendNotification);

        if (auto* volumePlugin = track->getVolumePlugin())
        {
            trackVolumeSlider.setValue (volumePlugin->getVolumeDb(), juce::dontSendNotification);
            trackPanSlider.setValue (volumePlugin->getPan(), juce::dontSendNotification);
        }
    }
    else
    {
        selectedTrackLabel.setText ("Track: none", juce::dontSendNotification);
        trackMuteButton.setToggleState (false, juce::dontSendNotification);
        trackSoloButton.setToggleState (false, juce::dontSendNotification);
        trackVolumeSlider.setValue (0.0, juce::dontSendNotification);
        trackPanSlider.setValue (0.0, juce::dontSendNotification);
    }

    if (edit != nullptr)
        if (auto* firstTempo = edit->tempoSequence.getTempo (0))
            tempoSlider.setValue (firstTempo->getBpm(), juce::dontSendNotification);

    if (auto* midiClip = getSelectedMidiClip())
    {
        const auto typeName = midiClip->getQuantisation().getType (false);

        for (int i = 0; i < quantizeTypeBox.getNumItems(); ++i)
        {
            const int id = i + 1;
            if (quantizeTypeBox.getItemText (i) == typeName)
            {
                quantizeTypeBox.setSelectedId (id, juce::dontSendNotification);
                break;
            }
        }
    }

    updatingTrackControls = false;
    mixerArea.repaint();
    refreshSelectedTrackPluginList();
    refreshChannelRackInspector();
    refreshBuiltInSynthControlsFromSelection();
}

te::Clip* BeatMakerNoRecord::getSelectedClip() const
{
    return dynamic_cast<te::Clip*> (selectionManager.getSelectedObject (0));
}

te::MidiClip* BeatMakerNoRecord::getSelectedMidiClip() const
{
    auto* selectedObject = selectionManager.getSelectedObject (0);

    if (auto* selectedMidiClip = dynamic_cast<te::MidiClip*> (selectedObject))
    {
        activeMidiClipID = selectedMidiClip->itemID;
        return selectedMidiClip;
    }

    if (dynamic_cast<te::Clip*> (selectedObject) != nullptr)
    {
        // Explicit non-MIDI clip selection should disable direct MIDI editing,
        // but keep the cached MIDI context for quick recovery when selection
        // returns to track/plugin scope.
        return nullptr;
    }

    auto getCachedMidiClip = [this] () -> te::MidiClip*
    {
        if (edit == nullptr || ! activeMidiClipID.isValid())
            return nullptr;

        if (auto* cached = dynamic_cast<te::MidiClip*> (te::findClipForID (*edit, activeMidiClipID)))
            return cached;

        // Cached clip no longer exists (delete/undo/track mutations).
        activeMidiClipID = {};
        return nullptr;
    };

    auto* selectedTrack = dynamic_cast<te::AudioTrack*> (selectedObject);
    if (selectedTrack == nullptr)
        if (auto* selectedPlugin = dynamic_cast<te::Plugin*> (selectedObject))
            selectedTrack = dynamic_cast<te::AudioTrack*> (selectedPlugin->getOwnerTrack());

    if (selectedTrack != nullptr)
    {
        if (auto* cachedMidiClip = getCachedMidiClip())
            if (dynamic_cast<te::AudioTrack*> (cachedMidiClip->getTrack()) == selectedTrack)
                return cachedMidiClip;

        const auto playhead = edit != nullptr ? edit->getTransport().getPosition()
                                              : te::TimePosition::fromSeconds (0.0);

        if (auto* midiClip = getPreferredMidiClipOnTrack (*selectedTrack, playhead))
        {
            activeMidiClipID = midiClip->itemID;
            return midiClip;
        }

        activeMidiClipID = {};
        return nullptr;
    }

    if (auto* cachedMidiClip = getCachedMidiClip())
        return cachedMidiClip;

    if (activeMidiClipID.isValid())
        activeMidiClipID = {};

    return nullptr;
}

te::AudioClipBase* BeatMakerNoRecord::getSelectedAudioClip() const
{
    return dynamic_cast<te::AudioClipBase*> (selectionManager.getSelectedObject (0));
}

te::AudioTrack* BeatMakerNoRecord::getSelectedTrackOrFirst() const
{
    if (edit == nullptr)
        return nullptr;

    if (auto* selectedTrack = dynamic_cast<te::AudioTrack*> (selectionManager.getSelectedObject (0)))
        return selectedTrack;

    if (auto* selectedPlugin = dynamic_cast<te::Plugin*> (selectionManager.getSelectedObject (0)))
        if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (selectedPlugin->getOwnerTrack()))
            return ownerTrack;

    if (auto* selectedClip = getSelectedClip())
        if (auto* clipTrack = dynamic_cast<te::AudioTrack*> (selectedClip->getTrack()))
            return clipTrack;

    auto tracks = te::getAudioTracks (*edit);
    return tracks.isEmpty() ? nullptr : tracks.getFirst();
}

void BeatMakerNoRecord::updateButtonsFromState()
{
    const bool hasEdit = (edit != nullptr);
    const bool hasSelection = selectionManager.getSelectedObjects().isNotEmpty();
    const bool hasClip = (getSelectedClip() != nullptr);
    const bool hasMidiClip = (getSelectedMidiClip() != nullptr);
    const bool hasAudioClip = (getSelectedAudioClip() != nullptr);
    const bool hasTrack = (getSelectedTrackOrFirst() != nullptr);
    auto* midiContextTrack = [this] () -> te::AudioTrack*
    {
        if (auto* midiClip = getSelectedMidiClip())
            if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
                return ownerTrack;

        return getSelectedTrackOrFirst();
    }();
    const bool hasMidiContextTrack = (midiContextTrack != nullptr);
    const bool midiContextHasInstrument = hasMidiContextTrack && trackHasInstrumentPlugin (*midiContextTrack);
    const bool hasBuiltInSynth = (getSelectedBuiltInSynthPlugin() != nullptr);
    auto* selectedFxPlugin = getSelectedTrackPlugin();
    const bool hasSelectedFx = (selectedFxPlugin != nullptr);
    const bool canBypassSelectedFx = hasSelectedFx && selectedFxPlugin->canBeDisabled();
    const bool canDeleteSelectedFx = hasSelectedFx
        && dynamic_cast<te::VolumeAndPanPlugin*> (selectedFxPlugin) == nullptr
        && dynamic_cast<te::LevelMeterPlugin*> (selectedFxPlugin) == nullptr;

    bool shouldRelayout = false;
    auto setVisibleWithLayout = [&shouldRelayout] (juce::Component& c, bool visible)
    {
        if (c.isVisible() != visible)
            shouldRelayout = true;

        c.setVisible (visible);
    };

    const bool showClipTools = hasEdit && hasClip;
    const bool showMidiTools = hasEdit;
    const bool showMidiDirectoryPage = showMidiTools && midiToolsTabs.getCurrentTabIndex() == 1;
    const bool showMidiEditorPage = showMidiTools && ! showMidiDirectoryPage;
    const bool showAudioTools = hasEdit && hasAudioClip;

    setVisibleWithLayout (clipEditGroup, showClipTools);
    setVisibleWithLayout (copyButton, showClipTools);
    setVisibleWithLayout (cutButton, showClipTools);
    setVisibleWithLayout (pasteButton, showClipTools);
    setVisibleWithLayout (deleteButton, showClipTools);
    setVisibleWithLayout (duplicateButton, showClipTools);
    setVisibleWithLayout (splitButton, showClipTools);
    setVisibleWithLayout (trimStartButton, showClipTools);
    setVisibleWithLayout (trimEndButton, showClipTools);
    setVisibleWithLayout (moveStartToCursorButton, showClipTools);
    setVisibleWithLayout (moveEndToCursorButton, showClipTools);
    setVisibleWithLayout (nudgeLeftButton, showClipTools);
    setVisibleWithLayout (nudgeRightButton, showClipTools);
    setVisibleWithLayout (slipLeftButton, showClipTools);
    setVisibleWithLayout (slipRightButton, showClipTools);
    setVisibleWithLayout (moveToPrevButton, showClipTools);
    setVisibleWithLayout (moveToNextButton, showClipTools);
    setVisibleWithLayout (toggleClipLoopButton, showClipTools);
    setVisibleWithLayout (renameClipButton, showClipTools);
    setVisibleWithLayout (selectAllButton, showClipTools);
    setVisibleWithLayout (deselectAllButton, showClipTools);

    setVisibleWithLayout (midiEditGroup, showMidiTools);
    setVisibleWithLayout (midiToolsTabs, showMidiTools);
    setVisibleWithLayout (quantizeTypeBox, showMidiEditorPage);
    setVisibleWithLayout (quantizeButton, showMidiEditorPage);
    setVisibleWithLayout (gridLabel, showMidiEditorPage);
    setVisibleWithLayout (gridBox, showMidiEditorPage);
    setVisibleWithLayout (midiTransposeDownButton, showMidiEditorPage);
    setVisibleWithLayout (midiTransposeUpButton, showMidiEditorPage);
    setVisibleWithLayout (midiOctaveDownButton, showMidiEditorPage);
    setVisibleWithLayout (midiOctaveUpButton, showMidiEditorPage);
    setVisibleWithLayout (midiVelocityDownButton, showMidiEditorPage);
    setVisibleWithLayout (midiVelocityUpButton, showMidiEditorPage);
    setVisibleWithLayout (midiHumanizeTimingButton, showMidiEditorPage);
    setVisibleWithLayout (midiHumanizeVelocityButton, showMidiEditorPage);
    setVisibleWithLayout (midiLegatoButton, showMidiEditorPage);
    setVisibleWithLayout (midiBounceToAudioButton, showMidiEditorPage);
    setVisibleWithLayout (midiGenerateChordsButton, showMidiEditorPage);
    setVisibleWithLayout (midiGenerateArpButton, showMidiEditorPage);
    setVisibleWithLayout (midiGenerateBassButton, showMidiEditorPage);
    setVisibleWithLayout (midiGenerateDrumsButton, showMidiEditorPage);
    setVisibleWithLayout (chordDirectoryRootLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryRootBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryScaleLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryScaleBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryProgressionLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryProgressionBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryBarsLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryBarsBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryTimeSignatureLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryTimeSignatureBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryOctaveLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryOctaveBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryVoicingLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryVoicingBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryDensityLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryDensityBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryPreviewPresetLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryPreviewPresetBox, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryVelocityLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryVelocitySlider, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectorySwingLabel, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectorySwingSlider, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryPreviewButton, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryApplyButton, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryExportMidiButton, showMidiDirectoryPage);
    setVisibleWithLayout (chordDirectoryExportWavButton, showMidiDirectoryPage);

    setVisibleWithLayout (audioEditGroup, showAudioTools);
    setVisibleWithLayout (audioGainDownButton, showAudioTools);
    setVisibleWithLayout (audioGainUpButton, showAudioTools);
    setVisibleWithLayout (audioFadeInButton, showAudioTools);
    setVisibleWithLayout (audioFadeOutButton, showAudioTools);
    setVisibleWithLayout (audioClearFadesButton, showAudioTools);
    setVisibleWithLayout (audioReverseButton, showAudioTools);
    setVisibleWithLayout (audioSpeedDownButton, showAudioTools);
    setVisibleWithLayout (audioSpeedUpButton, showAudioTools);
    setVisibleWithLayout (audioPitchDownButton, showAudioTools);
    setVisibleWithLayout (audioPitchUpButton, showAudioTools);
    setVisibleWithLayout (audioAutoTempoButton, showAudioTools);
    setVisibleWithLayout (audioWarpButton, showAudioTools);
    setVisibleWithLayout (audioAlignToBarButton, showAudioTools);
    setVisibleWithLayout (audioMake2BarLoopButton, showAudioTools);
    setVisibleWithLayout (audioMake4BarLoopButton, showAudioTools);
    setVisibleWithLayout (audioFillTransportLoopButton, showAudioTools);

    if (shouldRelayout)
        resized();

    int markerCount = 0;
    bool hasArrangerSections = false;

    if (hasEdit)
    {
        markerCount = edit->getMarkerManager().getMarkers().size();

        if (auto* arrangerTrack = edit->getArrangerTrack())
            for (auto* clip : arrangerTrack->getClips())
                if (dynamic_cast<te::ArrangerClip*> (clip) != nullptr)
                {
                    hasArrangerSections = true;
                    break;
                }
    }

    saveButton.setEnabled (hasEdit);
    saveAsButton.setEnabled (hasEdit);
    undoButton.setEnabled (hasEdit && edit->getUndoManager().canUndo());
    redoButton.setEnabled (hasEdit && edit->getUndoManager().canRedo());
    helpButton.setEnabled (true);
    focusSelectionButton.setEnabled (editComponent != nullptr);
    centerPlayheadButton.setEnabled (hasEdit && editComponent != nullptr);
    fitProjectButton.setEnabled (editComponent != nullptr);

    playPauseButton.setEnabled (hasEdit);
    stopButton.setEnabled (hasEdit);
    returnToStartButton.setEnabled (hasEdit);
    transportLoopButton.setEnabled (hasEdit);
    setLoopToSelectionButton.setEnabled (hasClip);
    jumpPrevBarButton.setEnabled (hasEdit);
    jumpNextBarButton.setEnabled (hasEdit);
    zoomInButton.setEnabled (editComponent != nullptr);
    zoomOutButton.setEnabled (editComponent != nullptr);
    zoomResetButton.setEnabled (editComponent != nullptr);
    showMarkerTrackButton.setEnabled (editComponent != nullptr);
    showArrangerTrackButton.setEnabled (editComponent != nullptr);
    addMarkerButton.setEnabled (hasEdit);
    prevMarkerButton.setEnabled (markerCount > 0);
    nextMarkerButton.setEnabled (markerCount > 0);
    loopMarkersButton.setEnabled (markerCount > 1);
    addSectionButton.setEnabled (hasEdit);
    prevSectionButton.setEnabled (hasArrangerSections);
    nextSectionButton.setEnabled (hasArrangerSections);
    loopSectionButton.setEnabled (hasArrangerSections);

    addTrackButton.setEnabled (hasEdit);
    addMidiTrackButton.setEnabled (hasEdit);
    addFloatingSynthTrackButton.setEnabled (hasEdit);
    moveTrackUpButton.setEnabled (hasTrack);
    moveTrackDownButton.setEnabled (hasTrack);
    duplicateTrackButton.setEnabled (hasTrack);
    colorTrackButton.setEnabled (hasTrack);
    renameTrackButton.setEnabled (hasTrack);
    importAudioButton.setEnabled (hasEdit);
    importMidiButton.setEnabled (hasEdit);
    createMidiClipButton.setEnabled (hasEdit);
    editToolSelectButton.setEnabled (hasEdit);
    editToolPencilButton.setEnabled (hasEdit);
    editToolScissorsButton.setEnabled (hasEdit);
    editToolResizeButton.setEnabled (hasEdit);
    refreshTimelineEditToolButtons();
    editToolLabel.setText ("Edit Tool (" + getTimelineEditToolName (getTimelineEditTool()) + ")", juce::dontSendNotification);
    defaultSynthModeBox.setEnabled (hasEdit);
    insertBarButton.setEnabled (hasEdit);
    deleteBarButton.setEnabled (hasEdit);

    copyButton.setEnabled (hasSelection);
    cutButton.setEnabled (hasSelection);
    pasteButton.setEnabled (hasEdit);
    deleteButton.setEnabled (hasSelection);
    duplicateButton.setEnabled (hasClip);
    splitButton.setEnabled (hasClip);
    trimStartButton.setEnabled (hasClip);
    trimEndButton.setEnabled (hasClip);
    moveStartToCursorButton.setEnabled (hasClip);
    moveEndToCursorButton.setEnabled (hasClip);
    nudgeLeftButton.setEnabled (hasClip);
    nudgeRightButton.setEnabled (hasClip);
    slipLeftButton.setEnabled (hasClip);
    slipRightButton.setEnabled (hasClip);
    moveToPrevButton.setEnabled (hasClip);
    moveToNextButton.setEnabled (hasClip);
    toggleClipLoopButton.setEnabled (hasClip);
    renameClipButton.setEnabled (hasClip);
    splitAllTracksButton.setEnabled (hasEdit);
    selectAllButton.setEnabled (hasEdit);
    deselectAllButton.setEnabled (hasSelection);
    quantizeTypeBox.setEnabled (hasMidiClip);
    quantizeButton.setEnabled (hasMidiClip);
    midiTransposeDownButton.setEnabled (hasMidiClip);
    midiTransposeUpButton.setEnabled (hasMidiClip);
    midiOctaveDownButton.setEnabled (hasMidiClip);
    midiOctaveUpButton.setEnabled (hasMidiClip);
    midiVelocityDownButton.setEnabled (hasMidiClip);
    midiVelocityUpButton.setEnabled (hasMidiClip);
    midiHumanizeTimingButton.setEnabled (hasMidiClip);
    midiHumanizeVelocityButton.setEnabled (hasMidiClip);
    midiLegatoButton.setEnabled (hasMidiClip);
    midiBounceToAudioButton.setEnabled (hasMidiClip);
    midiGenerateChordsButton.setEnabled (hasEdit);
    midiGenerateArpButton.setEnabled (hasEdit);
    midiGenerateBassButton.setEnabled (hasEdit);
    midiGenerateDrumsButton.setEnabled (hasEdit);
    chordDirectoryRootBox.setEnabled (hasEdit);
    chordDirectoryScaleBox.setEnabled (hasEdit);
    chordDirectoryProgressionBox.setEnabled (hasEdit);
    chordDirectoryBarsBox.setEnabled (hasEdit);
    chordDirectoryTimeSignatureBox.setEnabled (hasEdit);
    chordDirectoryOctaveBox.setEnabled (hasEdit);
    chordDirectoryVoicingBox.setEnabled (hasEdit);
    chordDirectoryDensityBox.setEnabled (hasEdit);
    chordDirectoryPreviewPresetBox.setEnabled (hasEdit);
    chordDirectoryVelocitySlider.setEnabled (hasEdit);
    chordDirectorySwingSlider.setEnabled (hasEdit);
    chordDirectoryPreviewButton.setEnabled (hasEdit);
    chordDirectoryApplyButton.setEnabled (hasEdit);
    chordDirectoryExportMidiButton.setEnabled (hasEdit);
    chordDirectoryExportWavButton.setEnabled (hasEdit);
    audioGainDownButton.setEnabled (hasAudioClip);
    audioGainUpButton.setEnabled (hasAudioClip);
    audioFadeInButton.setEnabled (hasAudioClip);
    audioFadeOutButton.setEnabled (hasAudioClip);
    audioClearFadesButton.setEnabled (hasAudioClip);
    audioReverseButton.setEnabled (hasAudioClip);
    audioSpeedDownButton.setEnabled (hasAudioClip);
    audioSpeedUpButton.setEnabled (hasAudioClip);
    audioPitchDownButton.setEnabled (hasAudioClip);
    audioPitchUpButton.setEnabled (hasAudioClip);
    audioAutoTempoButton.setEnabled (hasAudioClip);
    audioWarpButton.setEnabled (hasAudioClip);
    audioAlignToBarButton.setEnabled (hasClip);
    audioMake2BarLoopButton.setEnabled (hasClip);
    audioMake4BarLoopButton.setEnabled (hasClip);
    audioFillTransportLoopButton.setEnabled (hasClip && hasEdit && ! edit->getTransport().getLoopRange().isEmpty());
    gridBox.setEnabled (hasEdit);
    midiToolsTabs.setEnabled (hasEdit);
    fxChainLabel.setEnabled (hasTrack);
    fxChainBox.setEnabled (hasTrack);
    fxRefreshButton.setEnabled (hasTrack);
    fxScanButton.setEnabled (hasEdit);
    fxScanSkippedButton.setEnabled (hasEdit && skippedPluginScanEntries.size() > 0);
    fxPrepPlaybackButton.setEnabled (hasEdit);
    fxAddSamplerButton.setEnabled (hasTrack);
    fxAddSynthButton.setEnabled (hasTrack);
    fxAddEQButton.setEnabled (hasTrack);
    fxAddCompButton.setEnabled (hasTrack);
    fxAddReverbButton.setEnabled (hasTrack);
    fxAddDelayButton.setEnabled (hasTrack);
    fxAddExternalInstrumentButton.setEnabled (hasTrack);
    fxAddExternalButton.setEnabled (hasTrack);
    fxOpenEditorButton.setEnabled (hasSelectedFx);
    fxMoveUpButton.setEnabled (hasSelectedFx);
    fxMoveDownButton.setEnabled (hasSelectedFx);
    fxBypassButton.setEnabled (canBypassSelectedFx);
    fxDeleteButton.setEnabled (canDeleteSelectedFx);

    trackMuteButton.setEnabled (hasTrack);
    trackSoloButton.setEnabled (hasTrack);
    trackVolumeSlider.setEnabled (hasTrack);
    trackPanSlider.setEnabled (hasTrack);
    tempoSlider.setEnabled (hasEdit);
    trackHeightSlider.setEnabled (editComponent != nullptr);
    horizontalScrollSlider.setEnabled (editComponent != nullptr);
    commandToolbar.setVisible (true);
    timelineRuler.setVisible (editComponent != nullptr);
    trackAreaToolbar.setVisible (editComponent != nullptr);
    mixerToolsToolbar.setVisible (editComponent != nullptr);
    mixerArea.setVisible (editComponent != nullptr);
    mixerArea.setEnabled (hasEdit);
    channelRackGroup.setVisible (editComponent != nullptr);
    inspectorGroup.setVisible (editComponent != nullptr);
    builtInSynthGroup.setVisible (editComponent != nullptr);
    channelRackTrackLabel.setVisible (editComponent != nullptr);
    channelRackTrackBox.setVisible (editComponent != nullptr);
    channelRackPluginLabel.setVisible (editComponent != nullptr);
    channelRackPluginBox.setVisible (editComponent != nullptr);
    channelRackAddInstrumentButton.setVisible (editComponent != nullptr);
    channelRackAddFxButton.setVisible (editComponent != nullptr);
    channelRackOpenPluginButton.setVisible (editComponent != nullptr);
    inspectorTrackNameLabel.setVisible (editComponent != nullptr);
    inspectorRouteLabel.setVisible (editComponent != nullptr);
    inspectorPluginLabel.setVisible (editComponent != nullptr);
    inspectorMeterLabel.setVisible (editComponent != nullptr);
    builtInSynthTargetLabel.setVisible (editComponent != nullptr);
    builtInSynthPresetInitButton.setVisible (editComponent != nullptr);
    builtInSynthPresetWarmPadButton.setVisible (editComponent != nullptr);
    builtInSynthPresetPunchBassButton.setVisible (editComponent != nullptr);
    builtInSynthPresetBrightPluckButton.setVisible (editComponent != nullptr);
    builtInSynthCutoffLabel.setVisible (editComponent != nullptr);
    builtInSynthCutoffSlider.setVisible (editComponent != nullptr);
    builtInSynthResonanceLabel.setVisible (editComponent != nullptr);
    builtInSynthResonanceSlider.setVisible (editComponent != nullptr);
    builtInSynthEnvLabel.setVisible (editComponent != nullptr);
    builtInSynthEnvSlider.setVisible (editComponent != nullptr);
    builtInSynthDriveLabel.setVisible (editComponent != nullptr);
    builtInSynthDriveSlider.setVisible (editComponent != nullptr);
    builtInSynthAttackLabel.setVisible (editComponent != nullptr);
    builtInSynthAttackSlider.setVisible (editComponent != nullptr);
    builtInSynthReleaseLabel.setVisible (editComponent != nullptr);
    builtInSynthReleaseSlider.setVisible (editComponent != nullptr);
    builtInSynthReverbLabel.setVisible (editComponent != nullptr);
    builtInSynthReverbSlider.setVisible (editComponent != nullptr);
    builtInSynthDelayLabel.setVisible (editComponent != nullptr);
    builtInSynthDelaySlider.setVisible (editComponent != nullptr);
    builtInSynthOsc1Label.setVisible (editComponent != nullptr);
    builtInSynthOsc1Slider.setVisible (editComponent != nullptr);
    builtInSynthOsc2Label.setVisible (editComponent != nullptr);
    builtInSynthOsc2Slider.setVisible (editComponent != nullptr);
    builtInSynthOsc3Label.setVisible (editComponent != nullptr);
    builtInSynthOsc3Slider.setVisible (editComponent != nullptr);
    builtInSynthOsc4Label.setVisible (editComponent != nullptr);
    builtInSynthOsc4Slider.setVisible (editComponent != nullptr);
    builtInSynthUnisonLabel.setVisible (editComponent != nullptr);
    builtInSynthUnisonSlider.setVisible (editComponent != nullptr);
    builtInSynthWidthLabel.setVisible (editComponent != nullptr);
    builtInSynthWidthSlider.setVisible (editComponent != nullptr);
    builtInSynthChorusLabel.setVisible (editComponent != nullptr);
    builtInSynthChorusSlider.setVisible (editComponent != nullptr);
    builtInSynthMasterLabel.setVisible (editComponent != nullptr);
    builtInSynthMasterSlider.setVisible (editComponent != nullptr);
    channelRackTrackBox.setEnabled (hasTrack);
    channelRackPluginBox.setEnabled (hasTrack && channelRackPluginBox.getNumItems() > 0);
    channelRackAddInstrumentButton.setEnabled (hasTrack);
    channelRackAddFxButton.setEnabled (hasTrack);
    channelRackOpenPluginButton.setEnabled (hasSelectedFx);
    builtInSynthTargetLabel.setEnabled (hasBuiltInSynth);
    builtInSynthPresetInitButton.setEnabled (hasBuiltInSynth);
    builtInSynthPresetWarmPadButton.setEnabled (hasBuiltInSynth);
    builtInSynthPresetPunchBassButton.setEnabled (hasBuiltInSynth);
    builtInSynthPresetBrightPluckButton.setEnabled (hasBuiltInSynth);
    builtInSynthCutoffSlider.setEnabled (hasBuiltInSynth);
    builtInSynthResonanceSlider.setEnabled (hasBuiltInSynth);
    builtInSynthEnvSlider.setEnabled (hasBuiltInSynth);
    builtInSynthDriveSlider.setEnabled (hasBuiltInSynth);
    builtInSynthAttackSlider.setEnabled (hasBuiltInSynth);
    builtInSynthReleaseSlider.setEnabled (hasBuiltInSynth);
    builtInSynthReverbSlider.setEnabled (hasBuiltInSynth);
    builtInSynthDelaySlider.setEnabled (hasBuiltInSynth);
    builtInSynthOsc1Slider.setEnabled (hasBuiltInSynth);
    builtInSynthOsc2Slider.setEnabled (hasBuiltInSynth);
    builtInSynthOsc3Slider.setEnabled (hasBuiltInSynth);
    builtInSynthOsc4Slider.setEnabled (hasBuiltInSynth);
    builtInSynthUnisonSlider.setEnabled (hasBuiltInSynth);
    builtInSynthWidthSlider.setEnabled (hasBuiltInSynth);
    builtInSynthChorusSlider.setEnabled (hasBuiltInSynth);
    builtInSynthMasterSlider.setEnabled (hasBuiltInSynth);
    const auto pianoEditorMode = getPianoEditorLayoutModeSelection();
    const bool hasPianoEditor = (editComponent != nullptr);
    const bool showStepEditor = hasPianoEditor && pianoEditorMode != PianoEditorLayoutMode::pianoRoll;
    const bool showPianoEditor = hasPianoEditor && pianoEditorMode != PianoEditorLayoutMode::stepSequencer;
    const bool showSplitEditorDivider = hasPianoEditor && pianoEditorMode == PianoEditorLayoutMode::split;
    stepSequencerGroup.setVisible (showStepEditor);
    pianoRollGroup.setVisible (showPianoEditor);
    pianoStepSplitter.setVisible (showSplitEditorDivider);
    pianoStepSplitter.setEnabled (showSplitEditorDivider);
    pianoFloatToggleButton.setVisible (editComponent != nullptr);
    pianoEnsureInstrumentButton.setVisible (editComponent != nullptr);
    pianoOpenInstrumentButton.setVisible (editComponent != nullptr);
    pianoAlwaysOnTopButton.setVisible (editComponent != nullptr);
    pianoEditorModeTabs.setVisible (editComponent != nullptr);
    pianoRollToolbar.setVisible (showPianoEditor);
    stepSequencerToolbar.setVisible (showStepEditor);
    pianoRollHorizontalScrollBar.setVisible (showPianoEditor);
    pianoRollVerticalScrollBar.setVisible (showPianoEditor);
    stepSequencerHorizontalScrollBar.setVisible (showStepEditor);
    stepSequencer.setVisible (showStepEditor);
    stepSequencer.setEnabled (showStepEditor && hasMidiClip);
    midiPianoRoll.setVisible (showPianoEditor);
    midiPianoRoll.setEnabled (showPianoEditor && hasMidiClip);
    pianoFloatToggleButton.setEnabled (editComponent != nullptr);
    pianoEnsureInstrumentButton.setEnabled (hasMidiContextTrack);
    pianoOpenInstrumentButton.setEnabled (hasMidiContextTrack && midiContextHasInstrument);
    pianoAlwaysOnTopButton.setEnabled (isSectionFloating (FloatSection::piano));
    pianoEditorModeTabs.setEnabled (editComponent != nullptr);
    pianoRollToolbar.setEnabled (showPianoEditor && editComponent != nullptr);
    stepSequencerToolbar.setEnabled (showStepEditor && editComponent != nullptr);
    pianoRollHorizontalScrollBar.setEnabled (showPianoEditor && hasMidiClip);
    pianoRollVerticalScrollBar.setEnabled (showPianoEditor && hasMidiClip);
    stepSequencerHorizontalScrollBar.setEnabled (showStepEditor && hasMidiClip);
    refreshPianoFloatingWindowUi();
    commandToolbar.setEnabled (true);
    trackAreaToolbar.setEnabled (editComponent != nullptr);
    mixerToolsToolbar.setEnabled (editComponent != nullptr);
    refreshAllToolbarStates();
    updatePianoRollScrollbarsFromViewport();
    updateStepSequencerScrollbarFromPageContext();
    mixerArea.repaint();
    stepSequencer.repaint();

    updateTrackModeButtons();
    updateAudioClipModeButtons();
    updateContextHint();
    updateTransportInfoLabel();
    updateWorkflowStateLabel();
    refreshChannelRackInspector();
    refreshBuiltInSynthControlsFromSelection();
    refreshScaffoldState();
}

void BeatMakerNoRecord::updatePlayButtonText()
{
    if (edit != nullptr)
        playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Pause" : "Play");
}

void BeatMakerNoRecord::updateTransportLoopButton()
{
    if (edit != nullptr)
        transportLoopButton.setButtonText (edit->getTransport().looping ? "Loop: On" : "Loop: Off");
}

void BeatMakerNoRecord::updateTransportInfoLabel()
{
    if (edit == nullptr)
    {
        const auto text = "No edit loaded | REC OFF";
        if (transportInfoLabel.getText() != text)
            transportInfoLabel.setText (text, juce::dontSendNotification);
        return;
    }

    const auto playhead = edit->getTransport().getPosition();
    const int totalMs = juce::jmax (0, juce::roundToInt (playhead.inSeconds() * 1000.0));
    const int minutes = totalMs / 60000;
    const int seconds = (totalMs / 1000) % 60;
    const int milliseconds = totalMs % 1000;

    const auto barsAndBeats = edit->tempoSequence.toBarsAndBeats (playhead);
    const int barNumber = juce::jmax (1, barsAndBeats.bars + 1);
    const double beatInBar = juce::jmax (0.0, barsAndBeats.beats.inBeats());
    const double beatFraction = beatInBar - std::floor (beatInBar);
    const int beatNumber = juce::jmax (1, (int) std::floor (beatInBar) + 1);
    const int beatFine = juce::jlimit (0, 999, juce::roundToInt (beatFraction * 1000.0));

    const double bpm = edit->tempoSequence.getBeatsPerSecondAt (playhead, true) * 60.0;

    double zoomPercent = 100.0;
    if (editComponent != nullptr)
    {
        const auto& viewState = editComponent->getEditViewState();
        const double visibleSeconds = juce::jmax (0.001, (viewState.viewX2.get() - viewState.viewX1.get()).inSeconds());
        const double totalSeconds = juce::jmax (visibleSeconds, getTimelineTotalLengthSeconds());
        zoomPercent = juce::jlimit (1.0, 4000.0, (totalSeconds / visibleSeconds) * 100.0);
    }

    const juce::String transportState = edit->getTransport().isPlaying() ? "Playing" : "Stopped";
    const juce::String loopState = edit->getTransport().looping ? "Loop On" : "Loop Off";

    juce::String text;
    text << transportState
         << " | "
         << juce::String::formatted ("%02d:%02d.%03d", minutes, seconds, milliseconds)
         << " | Bar " << barNumber << "." << beatNumber << "." << juce::String (beatFine).paddedLeft ('0', 3)
         << " | " << juce::String (bpm, 1) << " BPM"
         << " | Zoom " << juce::String (zoomPercent, 0) << "%"
         << " | " << loopState
         << " | REC OFF";

    if (transportInfoLabel.getText() != text)
        transportInfoLabel.setText (text, juce::dontSendNotification);
}

void BeatMakerNoRecord::focusSelectedClipInView()
{
    if (editComponent == nullptr)
        return;

    if (auto* clip = getSelectedClip())
    {
        auto& viewState = editComponent->getEditViewState();
        const auto range = clip->getEditTimeRange();
        const double start = range.getStart().inSeconds();
        const double end = range.getEnd().inSeconds();
        const double length = juce::jmax (0.001, end - start);
        const double padding = juce::jmax (0.25, length * 0.35);

        double newStart = juce::jmax (0.0, start - padding);
        double newEnd = juce::jmax (newStart + 1.0, end + padding);
        const double maxEnd = juce::jmax (newEnd, getTimelineTotalLengthSeconds() + 2.0);
        if (newEnd > maxEnd)
        {
            newEnd = maxEnd;
            newStart = juce::jmax (0.0, newEnd - juce::jmax (1.0, length + padding * 2.0));
        }

        viewState.viewX1 = te::TimePosition::fromSeconds (newStart);
        viewState.viewX2 = te::TimePosition::fromSeconds (newEnd);
        syncViewControlsFromState();
        setStatus ("Focused timeline on selected clip.");
        return;
    }

    centerPlayheadInView();
}

void BeatMakerNoRecord::centerPlayheadInView()
{
    if (edit == nullptr || editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const double currentStart = viewState.viewX1.get().inSeconds();
    const double currentEnd = viewState.viewX2.get().inSeconds();
    const double width = juce::jmax (1.0, currentEnd - currentStart);
    const double playhead = edit->getTransport().getPosition().inSeconds();

    double newStart = juce::jmax (0.0, playhead - width * 0.5);
    double newEnd = newStart + width;
    const double maxEnd = juce::jmax (width, getTimelineTotalLengthSeconds() + 2.0);

    if (newEnd > maxEnd)
    {
        newEnd = maxEnd;
        newStart = juce::jmax (0.0, newEnd - width);
    }

    viewState.viewX1 = te::TimePosition::fromSeconds (newStart);
    viewState.viewX2 = te::TimePosition::fromSeconds (newEnd);
    syncViewControlsFromState();
    setStatus ("Centered timeline around playhead.");
}

void BeatMakerNoRecord::fitProjectInView()
{
    if (editComponent == nullptr)
        return;

    auto& viewState = editComponent->getEditViewState();
    const double totalLength = juce::jmax (0.0, getTimelineTotalLengthSeconds());
    const double viewSeconds = juce::jmax (16.0, totalLength + 2.0);
    viewState.viewX1 = te::TimePosition::fromSeconds (0.0);
    viewState.viewX2 = te::TimePosition::fromSeconds (viewSeconds);
    syncViewControlsFromState();
    setStatus ("Fitted timeline to project length.");
}

void BeatMakerNoRecord::setTimelineEditToolFromUi (TimelineEditTool tool, bool announceStatus)
{
    setTimelineEditTool (tool);
    refreshTimelineEditToolButtons();
    refreshAllToolbarStates();
    if (tool == TimelineEditTool::pencil || tool == TimelineEditTool::scissors)
        midiPianoRoll.setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else if (tool == TimelineEditTool::resize)
        midiPianoRoll.setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        midiPianoRoll.setMouseCursor (juce::MouseCursor::NormalCursor);
    if (editComponent != nullptr)
        editComponent->repaint();
    midiPianoRoll.repaint();
    updateContextHint();

    if (announceStatus)
        setStatus ("Edit tool set to " + getTimelineEditToolName (tool) + ".");
}

void BeatMakerNoRecord::refreshTimelineEditToolButtons()
{
    const auto tool = getTimelineEditTool();
    editToolSelectButton.setToggleState (tool == TimelineEditTool::select, juce::dontSendNotification);
    editToolPencilButton.setToggleState (tool == TimelineEditTool::pencil, juce::dontSendNotification);
    editToolScissorsButton.setToggleState (tool == TimelineEditTool::scissors, juce::dontSendNotification);
    editToolResizeButton.setToggleState (tool == TimelineEditTool::resize, juce::dontSendNotification);
}

void BeatMakerNoRecord::updateWorkflowStateLabel()
{
    if (edit == nullptr)
    {
        const juce::String text ("Workflow: No project loaded.");
        if (workflowStateLabel.getText() != text)
            workflowStateLabel.setText (text, juce::dontSendNotification);
        return;
    }

    juce::String trackState ("Track: none");
    juce::String clipState ("No clip selected");
    juce::String pluginState ("Inst: none | FX: 0");

    if (auto* track = getSelectedTrackOrFirst())
    {
        trackState = "Track: " + track->getName();

        int userFxCount = 0;
        for (auto* plugin : track->pluginList.getPlugins())
        {
            if (plugin == nullptr)
                continue;

            if (! isMixerUtilityPlugin (*plugin))
                ++userFxCount;
        }

        auto* instrument = getFirstEnabledInstrumentPlugin (*track);
        pluginState = "Inst: " + getInstrumentWorkflowLabel (instrument) + " | FX: " + juce::String (userFxCount);
    }

    if (getSelectedMidiClip() != nullptr)
        clipState = "MIDI clip selected";
    else if (getSelectedAudioClip() != nullptr)
        clipState = "Audio clip selected";
    else if (getSelectedClip() != nullptr)
        clipState = "Clip selected";

    const auto text = trackState + " | " + clipState + " | " + pluginState;
    if (workflowStateLabel.getText() != text)
        workflowStateLabel.setText (text, juce::dontSendNotification);
}

void BeatMakerNoRecord::updateTrackModeButtons()
{
    bool showMarkers = false;
    bool showArranger = false;

    if (editComponent != nullptr)
    {
        const auto& viewState = editComponent->getEditViewState();
        showMarkers = viewState.showMarkerTrack.get();
        showArranger = viewState.showArrangerTrack.get();
    }

    showMarkerTrackButton.setButtonText (showMarkers ? "Markers: On" : "Markers: Off");
    showArrangerTrackButton.setButtonText (showArranger ? "Arranger: On" : "Arranger: Off");
}

void BeatMakerNoRecord::updateAudioClipModeButtons()
{
    if (auto* clip = getSelectedAudioClip())
    {
        audioReverseButton.setButtonText (clip->getIsReversed() ? "Reverse: On" : "Reverse: Off");
        audioAutoTempoButton.setButtonText (clip->getAutoTempo() ? "AutoTempo: On" : "AutoTempo: Off");
        audioWarpButton.setButtonText (clip->getWarpTime() ? "Warp: On" : "Warp: Off");
        return;
    }

    audioReverseButton.setButtonText ("Reverse: Off");
    audioAutoTempoButton.setButtonText ("AutoTempo: Off");
    audioWarpButton.setButtonText ("Warp: Off");
}

void BeatMakerNoRecord::updateContextHint()
{
    juce::String hint = "Tip: Tools 1-4 = Select/Pencil/Scissors/Resize. Shift-drag ruler sets loop, Alt-drag pans timeline, and drag panel splitters to resize workspace.";

    if (getSelectedMidiClip() != nullptr)
    {
        if (auto* track = getSelectedTrackOrFirst(); track != nullptr && ! trackHasInstrumentPlugin (*track))
            hint = "MIDI Tip: Add an instrument plugin (Add AU/VST3 Instrument) or click Prep Playback to auto-fix playback chain.";
        else
            hint = "MIDI Tip: Alt-drag duplicates notes, double-click adds/deletes notes, Shift-drag edits velocity, and , . / - = adjust selected notes.";
    }
    else if (getSelectedAudioClip() != nullptr)
        hint = "Audio Tip: Align To Bar, Make 2/4-Bar Loop, or Fill Transport Loop for rapid beat-building.";
    else if (getSelectedTrackOrFirst() != nullptr)
        hint = "Track Tip: Up/Down scroll track area, Shift+Up/Down moves tracks, and right-click mixer strips for quick actions.";

    hint << " | Tool: " << getTimelineEditToolName (getTimelineEditTool());
    contextHintLabel.setText (hint, juce::dontSendNotification);
}

void BeatMakerNoRecord::setStatus (const juce::String& message)
{
    const auto lower = message.toLowerCase();
    juce::Colour statusColour = juce::Colour::fromRGB (167, 219, 255);

    const bool isError = lower.contains ("failed")
                      || lower.contains ("error")
                      || lower.contains ("unable")
                      || lower.contains ("cannot")
                      || lower.contains ("warning");

    const bool isWarning = lower.contains ("cancel")
                        || lower.contains ("nothing")
                        || lower.startsWith ("no ");

    const bool isSuccess = lower.contains ("saved")
                        || lower.contains ("created")
                        || lower.contains ("opened")
                        || lower.contains ("imported")
                        || lower.contains ("added")
                        || lower.contains ("applied")
                        || lower.contains ("enabled")
                        || lower.contains ("renamed")
                        || lower.contains ("updated")
                        || lower.contains ("moved")
                        || lower.contains ("deleted");

    if (isError)
        statusColour = juce::Colour::fromRGB (244, 129, 129);
    else if (isWarning)
        statusColour = juce::Colour::fromRGB (245, 206, 120);
    else if (isSuccess)
        statusColour = juce::Colour::fromRGB (161, 236, 178);

    statusLabel.setColour (juce::Label::textColourId, statusColour);
    statusLabel.setText (message, juce::dontSendNotification);
}
