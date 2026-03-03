/*
    Beatmaker-style DAW example.
    Editing + playback only. Recording paths are intentionally disabled.
*/

#pragma once

#include <JuceHeader.h>
#include "../common/Utilities.h"
#include "../common/Components.h"
#include "DawScaffold.h"
#include <atomic>
#include <array>
#include <memory>

namespace te = tracktion;
std::unique_ptr<te::UIBehaviour> createBeatMakerUiBehaviour();
std::unique_ptr<juce::KnownPluginList::CustomScanner> createTimedPluginScanCustomScanner();
void setTimedPluginScanTimeoutMs (int timeoutMs);
juce::StringArray consumeTimedOutPluginScanEntries();

class BeatMakerNoRecord  : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::MenuBarModel,
                           private juce::ChangeListener,
                           private juce::ScrollBar::Listener,
                           private juce::ApplicationCommandTarget
{
public:
    BeatMakerNoRecord();
    ~BeatMakerNoRecord() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    enum class FloatSection
    {
        workspace,
        mixer,
        piano
    };

    enum class DetachedPanel
    {
        arrangement = 0,
        tracks,
        clip,
        midi,
        audio,
        fx,
        trackMixer,
        mixerArea,
        channelRack,
        inspector,
        pianoRoll,
        stepSequencer,
        count
    };

    enum class PianoRollDragMode
    {
        none,
        move,
        resizeStart,
        resizeEnd,
        velocity
    };

    enum class MixerDragMode
    {
        none,
        volume,
        pan,
        sendLevel
    };

    enum class StepSequencerDragMode
    {
        none,
        add,
        remove
    };

    enum class ProjectStartTemplate
    {
        defaultInstrument,
        audioTrack,
        audioMidiHybrid
    };

    enum class DefaultInstrumentMode
    {
        autoPreferExternal,
        forceExternalVst3
    };

    enum class UiDensityMode
    {
        compact,
        comfortable,
        accessible
    };

    enum class LeftDockPanelMode
    {
        all,
        project,
        editing,
        sound
    };

    enum class PianoEditorLayoutMode
    {
        split,
        pianoRoll,
        stepSequencer
    };

    struct StepSequencerPageContext
    {
        te::MidiClip* midiClip = nullptr;
        te::BeatDuration stepLength = te::BeatDuration::fromBeats (0.25);
        double pageStartBeat = 0.0;
        double stepLengthBeats = 0.25;
        double clipLengthBeats = 1.0;
    };

    class SectionContainer : public juce::Component
    {
    public:
        SectionContainer (BeatMakerNoRecord& ownerToUse, FloatSection sectionToUse);

    private:
        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        BeatMakerNoRecord& owner;
        FloatSection section;
    };

    class FloatingSectionWindow : public juce::DocumentWindow
    {
    public:
        FloatingSectionWindow (const juce::String& windowTitle,
                               std::function<void()> closeHandler);

    private:
        void closeButtonPressed() override;

        std::function<void()> onClosePressed;
    };

    class DetachedPanelContainer : public juce::Component
    {
    public:
        DetachedPanelContainer (BeatMakerNoRecord& ownerToUse, DetachedPanel panelToUse);

    private:
        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        BeatMakerNoRecord& owner;
        DetachedPanel panel;
    };

    class LayoutSplitter : public juce::Component
    {
    public:
        explicit LayoutSplitter (bool isVerticalToUse);

        std::function<void (int deltaPixels)> onDeltaDrag;

    private:
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseEnter (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;

        bool isVertical = true;
        juce::Point<int> dragStartScreen;
    };

    class TimelineRulerComponent : public juce::Component,
                                   private juce::Timer
    {
    public:
        explicit TimelineRulerComponent (BeatMakerNoRecord& ownerToUse);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    private:
        void timerCallback() override;

        BeatMakerNoRecord& owner;
    };

    class MidiPianoRollComponent : public juce::Component,
                                   private juce::Timer
    {
    public:
        explicit MidiPianoRollComponent (BeatMakerNoRecord& ownerToUse);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    private:
        void timerCallback() override;

        BeatMakerNoRecord& owner;
    };

    class StepSequencerComponent : public juce::Component,
                                   private juce::Timer
    {
    public:
        explicit StepSequencerComponent (BeatMakerNoRecord& ownerToUse);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        void timerCallback() override;

        BeatMakerNoRecord& owner;
    };

    class MixerAreaComponent : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit MixerAreaComponent (BeatMakerNoRecord& ownerToUse);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        void timerCallback() override;

        BeatMakerNoRecord& owner;
    };

    class ChannelRackPreviewComponent : public juce::Component
    {
    public:
        explicit ChannelRackPreviewComponent (BeatMakerNoRecord& ownerToUse);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;

    private:
        BeatMakerNoRecord& owner;
    };

    te::Engine engine { "TheSampledexWorkflow", createBeatMakerUiBehaviour(), nullptr };
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<EditComponent> editComponent;
    std::unique_ptr<juce::LookAndFeel_V4> modernLookAndFeel;
    juce::File currentEditFile;

    bool updatingTrackControls = false;
    bool updatingViewControls = false;
    bool lastTransportPlaying = false;
    bool playbackSafetyWasPlaying = false;
    te::TimePosition playbackSafetyLastPosition = te::TimePosition::fromSeconds (0.0);
    te::EditItemID playbackSafetyActiveClipID;
    double playbackSafetyLastActiveClipBeat = -1.0;
    juce::Rectangle<int> lastEditViewportBounds;
    bool timelineLoopDragActive = false;
    int timelineLoopDragStartX = 0;
    bool timelinePanDragActive = false;
    int timelinePanDragLastX = 0;
    double lastScaffoldRefreshMs = 0.0;
    te::MidiNote* pianoRollDraggedNote = nullptr;
    PianoRollDragMode pianoRollDragMode = PianoRollDragMode::none;
    juce::Point<int> pianoRollDragStart;
    double pianoRollDraggedNoteStartBeats = 0.0;
    double pianoRollDraggedNoteLengthBeats = 0.25;
    int pianoRollDraggedNotePitch = 60;
    int pianoRollDraggedNoteVelocity = 100;
    bool pianoRollAddMode = false;
    te::EditItemID pianoRollViewportClipID;
    double pianoRollViewStartBeat = 0.0;
    double pianoRollViewLengthBeats = 4.0;
    int pianoRollViewLowestNote = 36;
    int pianoRollViewNoteCount = 36;
    bool pianoRollPanDragActive = false;
    juce::Point<int> pianoRollPanDragStart;
    double pianoRollPanStartBeat = 0.0;
    int pianoRollPanStartLowestNote = 36;
    bool pianoRollRulerScrubActive = false;
    mutable te::EditItemID activeMidiClipID;
    int mixerDragTrackIndex = -1;
    int mixerDragSendSlot = -1;
    MixerDragMode mixerDragMode = MixerDragMode::none;
    StepSequencerDragMode stepSequencerDragMode = StepSequencerDragMode::none;
    int stepSequencerDragLastLane = -1;
    int stepSequencerDragLastStep = -1;
    bool stepSequencerDragChangedAnyCell = false;
    double stepSequencerDragPageStartBeat = 0.0;
    te::BeatDuration stepSequencerDragStepLength = te::BeatDuration::fromBeats (0.25);
    te::EditItemID stepSequencerViewportClipID;
    double stepSequencerViewportStartBeat = 0.0;
    bool stepSequencerManualPageOverrideActive = false;
    std::array<juce::File, 8> stepSequencerLaneSampleFiles;
    std::array<te::EditItemID, 8> stepSequencerLaneTrackIDs;
    bool updatingEditorScrollBars = false;
    bool playbackRoutingNeedsPreparation = true;
    bool fileDragOverlayActive = false;
    juce::AudioDeviceManager::LevelMeter::Ptr outputLevelMeter;
    float outputMeterSmoothed = 0.0f;
    bool updatingChannelRackControls = false;
    juce::Array<int> channelRackPluginIndexMap;
    juce::StringArray skippedPluginScanEntries;
    UiDensityMode uiDensityMode = UiDensityMode::comfortable;

    juce::TextButton newEditButton { "New Project" };
    juce::TextButton openEditButton { "Open" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton saveAsButton { "Save As" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton helpButton { "Shortcuts" };
    juce::TextButton beatmakerSpaceButton { "Workspace Focus" };
    juce::TextButton startBeatQuickButton { "Add Instrument (AU/VST3)" };
    juce::TextButton focusSelectionButton { "Focus Selection" };
    juce::TextButton centerPlayheadButton { "Center Playhead" };
    juce::TextButton fitProjectButton { "Fit Project" };

    juce::TextButton playPauseButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton returnToStartButton { "Return 0" };
    juce::TextButton transportLoopButton { "Loop: Off" };
    juce::TextButton setLoopToSelectionButton { "Loop Selected" };
    juce::TextButton jumpPrevBarButton { "< Bar" };
    juce::TextButton jumpNextBarButton { "Bar >" };
    juce::TextButton zoomInButton { "H +" };
    juce::TextButton zoomOutButton { "H -" };
    juce::TextButton zoomResetButton { "H 1:1" };
    juce::TextButton zoomVerticalInButton { "V +" };
    juce::TextButton zoomVerticalOutButton { "V -" };
    juce::TextButton zoomVerticalResetButton { "V 1:1" };
    juce::TextButton showMarkerTrackButton { "Markers: Off" };
    juce::TextButton showArrangerTrackButton { "Arranger: Off" };
    juce::TextButton addMarkerButton { "Add Marker" };
    juce::TextButton prevMarkerButton { "< Marker" };
    juce::TextButton nextMarkerButton { "Marker >" };
    juce::TextButton loopMarkersButton { "Loop Markers" };
    juce::TextButton addSectionButton { "Add Section" };
    juce::TextButton prevSectionButton { "< Section" };
    juce::TextButton nextSectionButton { "Section >" };
    juce::TextButton loopSectionButton { "Loop Section" };

    juce::TextButton addTrackButton { "Add Audio Track" };
    juce::TextButton addMidiTrackButton { "Add MIDI Track" };
    juce::TextButton moveTrackUpButton { "Track Up" };
    juce::TextButton moveTrackDownButton { "Track Down" };
    juce::TextButton duplicateTrackButton { "Dup Track" };
    juce::TextButton colorTrackButton { "Color" };
    juce::TextButton renameTrackButton { "Rename Track" };
    juce::TextButton addFloatingInstrumentTrackButton { "New Instrument Track" };
    juce::TextButton importAudioButton { "Import Audio" };
    juce::TextButton importMidiButton { "Import MIDI" };
    juce::TextButton createMidiClipButton { "Create MIDI" };
    juce::Label editToolLabel { {}, "Edit Tool" };
    juce::TextButton editToolSelectButton { "Select" };
    juce::TextButton editToolPencilButton { "Pencil" };
    juce::TextButton editToolScissorsButton { "Scissors" };
    juce::TextButton editToolResizeButton { "Resize" };
    juce::TabbedButtonBar leftDockPanelTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Label leftDockPanelModeLabel { {}, "Panels" };
    juce::ComboBox leftDockPanelModeBox;
    juce::Label defaultInstrumentModeLabel { {}, "Default Instrument" };
    juce::ComboBox defaultInstrumentModeBox;

    juce::TextButton copyButton { "Copy" };
    juce::TextButton cutButton { "Cut" };
    juce::TextButton pasteButton { "Paste" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton duplicateButton { "Duplicate" };
    juce::TextButton splitButton { "Split" };
    juce::TextButton trimStartButton { "Trim Start" };
    juce::TextButton trimEndButton { "Trim End" };
    juce::TextButton moveStartToCursorButton { "Move Start->Cursor" };
    juce::TextButton moveEndToCursorButton { "Move End->Cursor" };
    juce::TextButton nudgeLeftButton { "Nudge <-" };
    juce::TextButton nudgeRightButton { "Nudge ->" };
    juce::TextButton slipLeftButton { "Slip <-" };
    juce::TextButton slipRightButton { "Slip ->" };
    juce::TextButton moveToPrevButton { "To Prev" };
    juce::TextButton moveToNextButton { "To Next" };
    juce::TextButton toggleClipLoopButton { "Clip Loop" };
    juce::TextButton renameClipButton { "Rename Clip" };
    juce::TextButton selectAllButton { "Select All" };
    juce::TextButton deselectAllButton { "Deselect" };
    juce::TextButton splitAllTracksButton { "Split All @Cursor" };
    juce::TextButton insertBarButton { "Insert Bar" };
    juce::TextButton deleteBarButton { "Delete Bar" };
    juce::ComboBox quantizeTypeBox;
    juce::TextButton quantizeButton { "Quantize" };
    juce::TextButton midiTransposeDownButton { "Pitch -1" };
    juce::TextButton midiTransposeUpButton { "Pitch +1" };
    juce::TextButton midiOctaveDownButton { "Pitch -12" };
    juce::TextButton midiOctaveUpButton { "Pitch +12" };
    juce::TextButton midiVelocityDownButton { "Vel -8" };
    juce::TextButton midiVelocityUpButton { "Vel +8" };
    juce::TextButton midiHumanizeTimingButton { "Humanize T" };
    juce::TextButton midiHumanizeVelocityButton { "Humanize V" };
    juce::TextButton midiLegatoButton { "Legato MIDI" };
    juce::TextButton midiBounceToAudioButton { "Bounce MIDI->Audio" };
    juce::TextButton midiGenerateChordsButton { "Gen Chords" };
    juce::TextButton midiGenerateArpButton { "Gen Arp" };
    juce::TextButton midiGenerateBassButton { "Gen Bass" };
    juce::TextButton midiGenerateDrumsButton { "Gen Drums" };
    juce::TabbedButtonBar midiToolsTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Label chordDirectoryRootLabel { {}, "Key" };
    juce::ComboBox chordDirectoryRootBox;
    juce::Label chordDirectoryScaleLabel { {}, "Scale" };
    juce::ComboBox chordDirectoryScaleBox;
    juce::Label chordDirectoryProgressionLabel { {}, "Progression" };
    juce::ComboBox chordDirectoryProgressionBox;
    juce::Label chordDirectoryBarsLabel { {}, "Bars" };
    juce::ComboBox chordDirectoryBarsBox;
    juce::Label chordDirectoryTimeSignatureLabel { {}, "Time Sig" };
    juce::ComboBox chordDirectoryTimeSignatureBox;
    juce::Label chordDirectoryOctaveLabel { {}, "Octave" };
    juce::ComboBox chordDirectoryOctaveBox;
    juce::Label chordDirectoryVoicingLabel { {}, "Voicing" };
    juce::ComboBox chordDirectoryVoicingBox;
    juce::Label chordDirectoryDensityLabel { {}, "Density" };
    juce::ComboBox chordDirectoryDensityBox;
    juce::Label chordDirectoryPreviewPresetLabel { {}, "Preview Patch" };
    juce::ComboBox chordDirectoryPreviewPresetBox;
    juce::Label chordDirectoryVelocityLabel { {}, "Velocity" };
    juce::Slider chordDirectoryVelocitySlider;
    juce::Label chordDirectorySwingLabel { {}, "Swing" };
    juce::Slider chordDirectorySwingSlider;
    juce::TextButton chordDirectoryPreviewButton { "Preview Directory" };
    juce::TextButton chordDirectoryApplyButton { "Apply To Clip" };
    juce::TextButton chordDirectoryExportMidiButton { "Export MIDI" };
    juce::TextButton chordDirectoryExportWavButton { "Export WAV" };
    juce::TextButton audioGainDownButton { "Gain -1dB" };
    juce::TextButton audioGainUpButton { "Gain +1dB" };
    juce::TextButton audioFadeInButton { "Fade In Grid" };
    juce::TextButton audioFadeOutButton { "Fade Out Grid" };
    juce::TextButton audioClearFadesButton { "Clear Fades" };
    juce::TextButton audioReverseButton { "Reverse: Off" };
    juce::TextButton audioSpeedDownButton { "Speed /2" };
    juce::TextButton audioSpeedUpButton { "Speed x2" };
    juce::TextButton audioPitchDownButton { "Pitch -1" };
    juce::TextButton audioPitchUpButton { "Pitch +1" };
    juce::TextButton audioAutoTempoButton { "AutoTempo: Off" };
    juce::TextButton audioWarpButton { "Warp: Off" };
    juce::TextButton audioAlignToBarButton { "Align To Bar" };
    juce::TextButton audioMake2BarLoopButton { "Make 2-Bar Loop" };
    juce::TextButton audioMake4BarLoopButton { "Make 4-Bar Loop" };
    juce::TextButton audioFillTransportLoopButton { "Fill Transport Loop" };
    juce::Label gridLabel { {}, "Grid" };
    juce::ComboBox gridBox;
    juce::Label fxChainLabel { {}, "FX Chain" };
    juce::ComboBox fxChainBox;
    juce::TextButton fxRefreshButton { "Refresh FX" };
    juce::TextButton fxScanButton { "Scan Plugins" };
    juce::TextButton fxScanSkippedButton { "Scan Skipped" };
    juce::TextButton fxPrepPlaybackButton { "Prep Playback" };
    juce::TextButton fxAddExternalInstrumentButton { "Add AU/VST3 Instrument" };
    juce::TextButton fxAddExternalButton { "Add AU/VST3 FX" };
    juce::TextButton fxOpenEditorButton { "Open Plugin UI" };
    juce::TextButton fxMoveUpButton { "FX Up" };
    juce::TextButton fxMoveDownButton { "FX Down" };
    juce::TextButton fxBypassButton { "Bypass FX" };
    juce::TextButton fxDeleteButton { "Delete FX" };

    juce::ToggleButton trackMuteButton { "Mute" };
    juce::ToggleButton trackSoloButton { "Solo" };
    juce::Label trackVolumeLabel { {}, "Vol" };
    juce::Label trackPanLabel { {}, "Pan" };
    juce::Label tempoLabel { {}, "Tempo" };
    juce::Slider trackVolumeSlider;
    juce::Slider trackPanSlider;
    juce::Slider tempoSlider;

    juce::Label editNameLabel;
    juce::Label transportInfoLabel { {}, "Stopped | 00:00.000 | Bar 1.1 | 120.0 BPM | Zoom 100% | REC OFF" };
    juce::Label workflowStateLabel { {}, "Workflow: No project loaded." };
    juce::Label selectedTrackLabel { {}, "Track: none" };
    juce::Label statusLabel { {}, "No recording path is enabled." };
    juce::Label contextHintLabel { {}, "Tip: Arrow keys move playhead. Shift-drag ruler = loop, Alt-drag = pan. Press ? for shortcuts." };
    juce::Label trackHeightLabel { {}, "Track H" };
    juce::Slider trackHeightSlider;
    juce::Slider leftDockScrollSlider;
    juce::Slider horizontalZoomSlider;
    juce::Slider verticalZoomSlider;
    juce::Slider horizontalScrollSlider;
    juce::Slider verticalScrollSlider;
    LayoutSplitter leftDockSplitter { true };
    LayoutSplitter workspaceMixerSplitter { true };
    LayoutSplitter workspaceBottomSplitter { false };
    LayoutSplitter mixerPianoSplitter { false };
    LayoutSplitter pianoStepSplitter { false };
    LayoutSplitter mixerRackSplitter { false };
    LayoutSplitter rackInspectorSplitter { true };
    LayoutSplitter channelRackControlsSplitter { false };
    SectionContainer workspaceSection { *this, FloatSection::workspace };
    SectionContainer mixerSection { *this, FloatSection::mixer };
    SectionContainer pianoSection { *this, FloatSection::piano };
    float leftDockWidthRatio = 0.20f;
    float workspaceMixerWidthRatio = 0.74f;
    float workspaceBottomHeightRatio = 0.36f;
    float mixerPianoHeightRatio = 0.42f;
    float pianoStepHeightRatio = 0.44f;
    float mixerRackHeightRatio = 0.56f;
    float rackInspectorWidthRatio = 0.58f;
    float channelRackControlsHeightRatio = 0.36f;
    int currentBodyWidthForResize = 0;
    int currentRightDockWidthForResize = 0;
    int currentRightDockHeightForResize = 0;
    int currentBottomDockHeightForResize = 0;
    int currentPianoSectionHeightForResize = 0;
    int currentMixerSectionHeightForResize = 0;
    int currentRackSectionWidthForResize = 0;
    int currentChannelRackSectionHeightForResize = 0;
    juce::Rectangle<int> leftDockViewportBounds;
    bool leftDockUsesDualColumn = false;
    int leftDockDualColumnDividerX = 0;
    // Beatmaker-first defaults keep startup focused on timeline + instrument workflow.
    bool windowPanelWorkspaceVisible = true;
    bool windowPanelMixerVisible = false;
    bool windowPanelPianoVisible = true;
    bool windowPanelArrangementVisible = true;
    bool windowPanelTrackVisible = true;
    bool windowPanelClipVisible = false;
    bool windowPanelMidiVisible = false;
    bool windowPanelAudioVisible = false;
    bool windowPanelFxVisible = true;
    bool windowPanelTrackMixerVisible = false;
    bool windowPanelMixerAreaVisible = false;
    bool windowPanelChannelRackVisible = true;
    bool windowPanelInspectorVisible = false;
    bool windowPanelPianoRollVisible = true;
    bool windowPanelStepSequencerVisible = true;
    bool workspaceSectionFloating = false;
    bool mixerSectionFloating = false;
    bool pianoSectionFloating = false;
    bool pianoFloatingAlwaysOnTop = true;
    bool shuttingDown = false;
    std::unique_ptr<FloatingSectionWindow> workspaceFloatingWindow;
    std::unique_ptr<FloatingSectionWindow> mixerFloatingWindow;
    std::unique_ptr<FloatingSectionWindow> pianoFloatingWindow;
    struct DetachedPanelWindowState
    {
        bool floating = false;
        std::unique_ptr<DetachedPanelContainer> container;
        std::unique_ptr<FloatingSectionWindow> window;
    };
    std::array<DetachedPanelWindowState, static_cast<size_t> (DetachedPanel::count)> detachedPanelWindows;
    juce::GroupComponent sessionGroup { {}, "Session & Transport" };
    juce::GroupComponent arrangementGroup { {}, "Arrangement" };
    juce::GroupComponent trackGroup { {}, "Tracks & Import" };
    juce::GroupComponent clipEditGroup { {}, "Clip Editing" };
    juce::GroupComponent midiEditGroup { {}, "MIDI Editing" };
    juce::GroupComponent audioEditGroup { {}, "Audio Editing" };
    juce::GroupComponent fxGroup { {}, "FX Chain" };
    juce::GroupComponent mixerGroup { {}, "Mixer" };
    juce::GroupComponent workspaceGroup { {}, "Timeline & Track Area" };
    juce::GroupComponent mixerAreaGroup { {}, "Mixer Area" };
    juce::GroupComponent channelRackGroup { {}, "Channel Rack" };
    juce::GroupComponent inspectorGroup { {}, "Inspector" };
    juce::GroupComponent stepSequencerGroup { {}, "Step Sequencer" };
    juce::GroupComponent pianoRollGroup { {}, "Piano Roll" };
    juce::Toolbar trackAreaToolbar;
    std::unique_ptr<juce::ToolbarItemFactory> trackAreaToolbarFactory;
    juce::Toolbar mixerToolsToolbar;
    std::unique_ptr<juce::ToolbarItemFactory> mixerToolsToolbarFactory;
    juce::Toolbar commandToolbar;
    std::unique_ptr<juce::ToolbarItemFactory> commandToolbarFactory;
    juce::TextButton pianoFloatToggleButton { "Float Piano" };
    juce::TextButton pianoEnsureInstrumentButton { "Ensure Instrument" };
    juce::TextButton pianoOpenInstrumentButton { "Open Instrument UI" };
    juce::ToggleButton pianoAlwaysOnTopButton { "Always On Top" };
    juce::TabbedButtonBar pianoEditorModeTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Toolbar pianoRollToolbar;
    std::unique_ptr<juce::ToolbarItemFactory> pianoRollToolbarFactory;
    juce::Toolbar stepSequencerToolbar;
    std::unique_ptr<juce::ToolbarItemFactory> stepSequencerToolbarFactory;
    juce::ApplicationCommandManager commandManager;
    juce::MenuBarComponent topMenuBar { nullptr };
    juce::TooltipWindow tooltipWindow { this, 900 };
    TimelineRulerComponent timelineRuler { *this };
    MixerAreaComponent mixerArea { *this };
    ChannelRackPreviewComponent channelRackPreview { *this };
    juce::Label channelRackTrackLabel { {}, "Track" };
    juce::ComboBox channelRackTrackBox;
    juce::Label channelRackPluginLabel { {}, "Plugin" };
    juce::ComboBox channelRackPluginBox;
    juce::TextButton channelRackAddInstrumentButton { "Add Instrument" };
    juce::TextButton channelRackAddFxButton { "Add Effect" };
    juce::TextButton channelRackOpenPluginButton { "Open Plugin UI" };
    juce::Label inspectorTrackNameLabel { {}, "Track: -" };
    juce::Label inspectorRouteLabel { {}, "Routing: -" };
    juce::Label inspectorPluginLabel { {}, "Plugin: -" };
    juce::Label inspectorMeterLabel { {}, "OUT: -inf dB" };
    StepSequencerComponent stepSequencer { *this };
    MidiPianoRollComponent midiPianoRoll { *this };
    juce::ScrollBar pianoRollHorizontalScrollBar { false };
    juce::ScrollBar pianoRollVerticalScrollBar { true };
    juce::ScrollBar stepSequencerHorizontalScrollBar { false };
    DawScaffold dawScaffold;
    std::atomic<int> pendingUiUpdateFlags { 0 };
    std::atomic<bool> pendingUiUpdatePosted { false };

    enum PendingUiUpdateFlags
    {
        pendingUiUpdateNone = 0,
        pendingUiUpdateTransport = 1 << 0,
        pendingUiUpdateSelection = 1 << 1
    };

    void setupCallbacks();
    void setupSessionHeaderCallbacks();
    void setupArrangementCallbacks();
    void setupTrackCallbacks();
    void setupMidiCallbacks();
    void setupFxCallbacks();
    void setupMixerCallbacks();
    void setupPianoCallbacks();
    void setupSliders();
    static juce::String getLeftDockPanelModeStorageValue (LeftDockPanelMode mode);
    static juce::String getLeftDockPanelModeDisplayName (LeftDockPanelMode mode);
    static int getComboIdForLeftDockPanelMode (LeftDockPanelMode mode);
    static LeftDockPanelMode getLeftDockPanelModeForStorageValue (const juce::String& value);
    static LeftDockPanelMode getLeftDockPanelModeForComboId (int comboId);
    LeftDockPanelMode getLeftDockPanelModeSelection() const;
    void setLeftDockPanelMode (LeftDockPanelMode mode, bool persist, bool announceStatus);
    void applyBeatmakerTrackAreaFocusLayout (bool persist, bool announceStatus);
    static juce::String getPianoEditorLayoutModeStorageValue (PianoEditorLayoutMode mode);
    static int getTabIndexForPianoEditorLayoutMode (PianoEditorLayoutMode mode);
    static PianoEditorLayoutMode getPianoEditorLayoutModeForStorageValue (const juce::String& value);
    static PianoEditorLayoutMode getPianoEditorLayoutModeForTabIndex (int tabIndex);
    PianoEditorLayoutMode getPianoEditorLayoutModeSelection() const;
    void setPianoEditorLayoutMode (PianoEditorLayoutMode mode, bool persist, bool announceStatus);
    static juce::String getUiDensityStorageValue (UiDensityMode mode);
    static juce::String getUiDensityDisplayName (UiDensityMode mode);
    static UiDensityMode getUiDensityModeForStorageValue (const juce::String& value);
    float getUiDensityScale() const;
    void applyUiDensityToControlSizing();
    void setUiDensityMode (UiDensityMode mode, bool persist, bool announceStatus);
    static juce::String getDefaultInstrumentModeStorageValue (DefaultInstrumentMode mode);
    static juce::String getDefaultInstrumentModeDisplayName (DefaultInstrumentMode mode);
    static int getComboIdForDefaultInstrumentMode (DefaultInstrumentMode mode);
    static DefaultInstrumentMode getDefaultInstrumentModeForStorageValue (const juce::String& value);
    static DefaultInstrumentMode getDefaultInstrumentModeForComboId (int comboId);
    DefaultInstrumentMode getDefaultInstrumentModeSelection() const;
    void refreshChannelRackInspector();
    void closeFloatingWindows();
    void closeDetachedPanelWindows();
    void layoutSessionHeaderPanel (juce::Rectangle<int> bounds, bool detachedMode);
    void layoutArrangementPanel (juce::Rectangle<int> bounds, bool detachedMode);
    void layoutTrackPanel (juce::Rectangle<int> bounds, bool detachedMode);
    void layoutMidiPanel (juce::Rectangle<int> bounds, bool detachedMode);
    void layoutFxPanel (juce::Rectangle<int> bounds, bool detachedMode);
    void layoutMixerPanel (juce::Rectangle<int> bounds, bool detachedMode);
    void layoutPianoPanel (juce::Rectangle<int> bounds, bool detachedMode);
    void layoutSectionContent (FloatSection section, juce::Rectangle<int> bounds);
    void layoutDetachedPanelContent (DetachedPanel panel, juce::Rectangle<int> bounds);
    void setupCommandToolbar();
    void refreshCommandToolbarState();
    void setupTrackAreaToolbar();
    void refreshTrackAreaToolbarState();
    void setupMixerToolsToolbar();
    void refreshMixerToolsToolbarState();
    void setupPianoRollToolbar();
    void refreshPianoRollToolbarState();
    void setupStepSequencerToolbar();
    void refreshStepSequencerToolbarState();
    void refreshAllToolbarStates();
    bool isSectionFloating (FloatSection section) const;
    void setSectionFloating (FloatSection section, bool shouldFloat, bool fromWindowClose = false);
    bool isDetachedPanelFloating (DetachedPanel panel) const;
    void setDetachedPanelFloating (DetachedPanel panel, bool shouldFloat, bool fromWindowClose = false);
    void toggleDetachedPanelFloating (DetachedPanel panel);
    juce::String getDetachedPanelFloatingTitle (DetachedPanel panel) const;
    juce::Component* getDetachedPanelDockParent (DetachedPanel panel);
    bool isDetachedPanelVisibleInLayout (DetachedPanel panel) const;
    void forEachDetachedPanelComponent (DetachedPanel panel,
                                        const std::function<void (juce::Component&)>& visitor);
    juce::String getSectionFloatingTitle (FloatSection section) const;
    juce::String getPianoFloatingWindowTitle() const;
    void refreshPianoFloatingWindowUi();
    void toggleSectionFloating (FloatSection section);
    void showShortcutOverlay();
    bool confirmDestructiveAction (const juce::String& title, const juce::String& message);

    bool hasAnyClipInEdit() const;
    bool hasAnyMidiClipInEdit() const;
    bool isNoRecordPolicyActive() const;
    void refreshScaffoldState (bool force = false);

    void configurePlaybackOnlyIO();
    void applyHighQualityAudioMode();
    void applyHighQualitySettingsToAudioClip (te::AudioClipBase& clip);
    int applyHighQualitySettingsToEdit();
    void applyNoRecordPolicyToEdit();
    void markPlaybackRoutingNeedsPreparation();
    bool chooseNewProjectTemplate (ProjectStartTemplate& outTemplate, bool forcePrompt, bool allowPromptDialog);
    void applyProjectStartTemplate (ProjectStartTemplate startTemplate);
    juce::File getProjectsRootDirectory();
    void ensureProjectDirectoryLayout (const juce::File& editFile) const;
    bool saveEditToPath (const juce::File& requestedFile, juce::String& resultMessage, bool updateCurrentFile);
    void createNewEdit (bool promptForTemplateChooser = false);
    void openEdit();
    void setCurrentEdit (std::unique_ptr<te::Edit> newEdit, const juce::File& file, const juce::String& message);
    void buildEditComponent();

    void saveEdit();
    void saveEditAs();
    void addTrack();
    void addMidiTrack();
    void addFloatingInstrumentTrack();
    void openMidiClipInPianoRoll (te::MidiClip& midiClip, bool floatWindow);
    void configureTrackRoleAsAudio (te::AudioTrack& track, int trackNumber);
    void configureTrackRoleAsMidi (te::AudioTrack& track, int trackNumber, bool openInstrumentUi);
    te::AudioTrack* appendTrackWithRole (bool midiRole, bool openInstrumentUi);
    void moveSelectedTrackVertically (bool moveDown);
    void duplicateSelectedTrack();
    void cycleSelectedTrackColour();
    void renameSelectedTrack();
    void toggleMarkerTrackVisibility();
    void toggleArrangerTrackVisibility();
    void addMarkerAtPlayhead();
    void jumpToMarker (bool forward);
    void setLoopBetweenNearestMarkers();
    void addArrangerSectionAtPlayhead();
    void jumpToArrangerSection (bool forward);
    void setLoopToCurrentArrangerSection();

    double getBeatsPerBarAt (te::TimePosition time) const;
    te::TimeDuration getGridDurationAt (te::TimePosition time) const;
    te::TimeDuration getBarDurationAt (te::TimePosition time) const;
    void jumpByBar (bool forward);

    int getHeaderWidth() const;
    int getFooterWidth() const;
    int getVisibleTrackCount() const;
    double getTimelineTotalLengthSeconds() const;
    double getMidiClipLengthBeats (const te::MidiClip& midiClip) const;
    void syncPianoRollViewportToSelection (bool resetForNewClip);
    void focusPianoRollViewportOnClip (const te::MidiClip& midiClip, bool centerOnContent);
    void panPianoRollViewport (double beatDelta, int noteDelta);
    void zoomPianoRollViewportTime (double factor, double anchorBeat);
    void zoomPianoRollViewportPitch (double factor, int anchorNote);
    void updatePianoRollScrollbarsFromViewport();
    void updateStepSequencerScrollbarFromPageContext();
    void setStepSequencerViewportStartBeat (double startBeat, bool markManualOverride);
    void clearStepSequencerViewportOverride();
    int pianoRollYToNoteNumber (int y, int height) const;
    double pianoRollXToBeat (int x, int width) const;
    te::BeatDuration getPianoRollGridBeats() const;
    juce::Rectangle<int> getPianoRollNoteBounds (const te::MidiNote& note, int width, int height) const;
    te::MidiNote* getPianoRollNoteAt (int x, int y, int width, int height) const;
    double getStepSequencerPageStartBeat (const te::MidiClip& midiClip);
    te::BeatDuration getStepSequencerStepLengthBeats (const te::MidiClip& midiClip) const;
    te::MidiNote* getStepSequencerNoteAt (te::MidiClip& midiClip,
                                          int laneIndex,
                                          int stepIndex,
                                          double pageStartBeat,
                                          te::BeatDuration stepLength) const;
    bool setStepSequencerCellEnabled (te::MidiClip& midiClip,
                                      int laneIndex,
                                      int stepIndex,
                                      double pageStartBeat,
                                      te::BeatDuration stepLength,
                                      bool shouldEnable);
    bool buildStepSequencerPageContext (StepSequencerPageContext& context);
    void refreshStepSequencerEditSurfaces();
    juce::String getStepSequencerLaneSampleName (int laneIndex) const;
    juce::String getStepSequencerLaneDisplayLabel (int laneIndex) const;
    bool hasLoadedStepSequencerLaneSample (int laneIndex) const;
    void showStepSequencerDrumPadPopup();
    void loadSampleIntoStepSequencerLane (int laneIndex);
    void clearStepSequencerLaneSample (int laneIndex);
    te::AudioTrack* getOrCreateStepSequencerLaneTrack (int laneIndex);
    void renderStepSequencerPadsToAudioTracks();
    void resetStepSequencerDragState();
    void resetPianoRollNoteDragState();
    void clearPianoRollNavigationInteraction();
    void clearStepSequencerPage();
    void applyStepSequencerFourOnFloorPattern();
    void randomizeStepSequencerPage();
    void shiftStepSequencerPage (int stepDelta);
    void varyStepSequencerPageVelocities (int maxDelta);

    void handleTimelineRulerMouseDown (const juce::MouseEvent& e, int width);
    void handleTimelineRulerMouseDrag (const juce::MouseEvent& e, int width);
    void handleTimelineRulerMouseUp (const juce::MouseEvent& e, int width);
    void handleTimelineRulerMouseWheel (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel, int width);
    void showTimelineRulerContextMenu (const juce::MouseEvent& e, int width);
    void setPlayheadFromRulerX (int x, int width);
    void moveTimelineViewportBySeconds (double deltaSeconds);
    void zoomTimelineAroundTime (double factor, te::TimePosition anchorTime);
    bool shouldAnimateTimelineRuler() const;
    bool shouldAnimateMidiPianoRoll() const;
    bool shouldAnimateStepSequencer() const;
    bool shouldAnimateMixerArea() const;
    void runTransportPlaybackSafetyCheck();
    void paintTimelineRuler (juce::Graphics& g, juce::Rectangle<int> area);
    void paintMixerArea (juce::Graphics& g, juce::Rectangle<int> area);
    void paintChannelRackPreview (juce::Graphics& g, juce::Rectangle<int> area);
    void paintStepSequencer (juce::Graphics& g, juce::Rectangle<int> area);
    void paintMidiPianoRoll (juce::Graphics& g, juce::Rectangle<int> area);
    void handleMidiPianoRollMouseDown (const juce::MouseEvent& e, int width, int height);
    void handleMidiPianoRollMouseDrag (const juce::MouseEvent& e, int width, int height);
    void handleMidiPianoRollMouseUp (const juce::MouseEvent& e);
    void handleMidiPianoRollMouseMove (const juce::MouseEvent& e, int width, int height);
    void handleMidiPianoRollMouseWheel (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel, int width, int height);
    void handleStepSequencerMouseDown (const juce::MouseEvent& e, int width, int height);
    void handleStepSequencerMouseDrag (const juce::MouseEvent& e, int width, int height);
    void handleStepSequencerMouseUp (const juce::MouseEvent& e);
    bool ensureAuxReturnTrackForBus (int busNumber);
    bool assignMixerSendDestination (te::AudioTrack& track, int sendSlot, int busNumber);
    bool setMixerSendLevelDb (te::AudioTrack& track, int sendSlot, float gainDb);
    void showMixerSendDestinationMenu (te::AudioTrack& track, int sendSlot, juce::Rectangle<int> targetBounds);
    void handleMixerAreaMouseDown (const juce::MouseEvent& e, int width, int height);
    void handleMixerAreaMouseDrag (const juce::MouseEvent& e, int width, int height);
    void handleMixerAreaMouseUp (const juce::MouseEvent& e);
    void handleChannelRackPreviewMouseDown (const juce::MouseEvent& e, int width, int height);

    void applyTrackHeightFromUI();
    void applyHorizontalZoomFromUI();
    void applyVerticalZoomFromUI();
    void applyHorizontalScrollFromUI();
    void applyVerticalScrollFromUI();
    void syncViewControlsFromState();

    void importAudioClip();
    void importMidiClip();
    void createMidiClip();
    void scanSkippedPlugins();
    void refreshSelectedTrackPluginList();
    void openPluginScanDialog();
    te::Plugin* getSelectedTrackPlugin() const;
    void addExternalInstrumentPluginToSelectedTrack();
    void addExternalPluginToSelectedTrack();
    void openSelectedTrackPluginEditor();
    void prepareEditForPluginPlayback (bool reorderFxChains);
    bool trackHasMidiContent (te::AudioTrack& track) const;
    bool prepareTrackForMidiPlayback (te::AudioTrack& track, int& enabledInstruments, int& addedInstruments);
    bool enableExistingInstrumentForDefaultMode (te::AudioTrack& track, DefaultInstrumentMode mode, int& enabledInstruments);
    bool insertInstrumentForDefaultMode (te::AudioTrack& track, DefaultInstrumentMode mode, int& addedInstruments);
    te::Plugin* choosePreferredInstrumentPluginForMode (te::AudioTrack& track, DefaultInstrumentMode mode) const;
    bool normalizeTrackInstrumentActivationForMode (te::AudioTrack& track, DefaultInstrumentMode mode, int& enabledInstruments);
    bool normalizeTrackPluginOrderForPlayback (te::AudioTrack& track, int& movedPlugins);
    bool trackHasInstrumentPlugin (te::AudioTrack& track) const;
    int getPluginInsertIndexForTrack (te::AudioTrack& track, bool forInstrument) const;
    bool ensureTrackHasInstrumentForMidiPlayback (te::AudioTrack& track);
    void moveSelectedTrackPlugin (bool moveDown);
    void toggleSelectedTrackPluginBypass();
    void deleteSelectedTrackPlugin();
    void adjustSelectedAudioClipGain (float deltaDb);
    void setSelectedAudioClipFade (bool fadeIn);
    void clearSelectedAudioClipFades();
    void toggleSelectedAudioClipReverse();
    void scaleSelectedAudioClipSpeed (double factor);
    void adjustSelectedAudioClipPitch (float semitones);
    void toggleSelectedAudioClipAutoTempo();
    void toggleSelectedAudioClipWarp();
    void alignSelectedClipToBar();
    void makeSelectedClipLoop (int bars);
    void fillTransportLoopWithSelectedClip();

    void copySelection();
    void cutSelection();
    void pasteSelection();
    void selectAllEditableItems();
    void duplicateSelectedClip();
    void splitAllTracksAtPlayhead();
    void splitSelectedClipAtPlayhead();
    void trimSelectedClipStartToPlayhead();
    void trimSelectedClipEndToPlayhead();
    void moveSelectedClipBoundaryToCursor (bool moveStart);
    void nudgeSelectedClip (bool moveRight);
    void slipSelectedClipContent (bool forward);
    void moveSelectedClipToNeighbour (bool moveToNext);
    void insertBarAtPlayhead();
    void deleteBarAtPlayhead();
    void quantizeSelectedMidiClip();
    void transposeSelectedMidiNotes (int semitones);
    void adjustSelectedMidiNoteVelocity (int delta);
    void humanizeSelectedMidiTiming (double maxJitterBeats);
    void humanizeSelectedMidiVelocity (int maxDelta);
    void legatoSelectedMidiNotes();
    bool applyChordScaleDirectoryToClip (te::MidiClip& midiClip, bool clearExistingNotes, bool previewMode, juce::String* outSummary);
    void generateMidiChordScaleDirectoryPattern();
    void previewChordScaleDirectoryPattern();
    void exportChordScaleDirectorySelectionAsMidi();
    void exportChordScaleDirectorySelectionAsWav();
    void generateMidiChordProgression();
    void generateMidiArpeggioPattern();
    void generateMidiBasslinePattern();
    void generateMidiDrumPattern();
    void bounceSelectedMidiClipToAudio();
    void toggleSelectedClipLooping();
    void renameSelectedClip();
    void deleteSelectedItem();

    void setTransportLoopToSelectedClip();
    void toggleTransportLooping();
    void zoomTimeline (double factor);
    void zoomTimelineVertically (double factor);
    void resetZoom();
    void resetVerticalZoom();
    void focusSelectedClipInView();
    void centerPlayheadInView();
    void fitProjectInView();
    void setTimelineEditToolFromUi (TimelineEditTool tool, bool announceStatus = true);
    void refreshTimelineEditToolButtons();

    void applyTrackMixerFromUI();
    void applyTempoFromUI();
    void updateTrackControlsFromSelection();

    te::Clip* getSelectedClip() const;
    te::MidiClip* getSelectedMidiClip() const;
    te::AudioClipBase* getSelectedAudioClip() const;
    te::AudioTrack* getSelectedTrackOrFirst() const;

    void updateButtonsFromState();
    void updatePlayButtonText();
    void updateTransportLoopButton();
    void updateTransportInfoLabel();
    void updateWorkflowStateLabel();
    void updateTrackModeButtons();
    void updateAudioClipModeButtons();
    void updateContextHint();
    void setStatus (const juce::String& message);

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;
    juce::ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
    bool perform (const InvocationInfo& info) override;
    void scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void postPendingUiUpdatesAsync();
    void drainPendingUiUpdatesAsync();
    void processTransportChangeForUi();
    void processSelectionChangeForUi();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatMakerNoRecord)
};
