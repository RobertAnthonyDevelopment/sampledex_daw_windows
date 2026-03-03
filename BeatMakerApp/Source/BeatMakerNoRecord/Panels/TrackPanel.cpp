#include "../BeatMakerNoRecord.h"
#include "SharedLayoutSystem.h"

namespace
{
beatmaker::layout::DensityMode getTrackDensity (int densityMode)
{
    if (densityMode == 0)
        return beatmaker::layout::DensityMode::compact;
    if (densityMode == 2)
        return beatmaker::layout::DensityMode::accessible;
    return beatmaker::layout::DensityMode::comfortable;
}
}

void BeatMakerNoRecord::setupTrackCallbacks()
{
    addTrackButton.onClick = [this] { addTrack(); };
    addMidiTrackButton.onClick = [this] { addMidiTrack(); };
    moveTrackUpButton.onClick = [this] { moveSelectedTrackVertically (false); };
    moveTrackDownButton.onClick = [this] { moveSelectedTrackVertically (true); };
    duplicateTrackButton.onClick = [this] { duplicateSelectedTrack(); };
    colorTrackButton.onClick = [this] { cycleSelectedTrackColour(); };
    renameTrackButton.onClick = [this] { renameSelectedTrack(); };
    addFloatingInstrumentTrackButton.onClick = [this] { addFloatingInstrumentTrack(); };
    importAudioButton.onClick = [this] { importAudioClip(); };
    importMidiButton.onClick = [this] { importMidiClip(); };
    createMidiClipButton.onClick = [this] { createMidiClip(); };

    editToolSelectButton.setClickingTogglesState (true);
    editToolPencilButton.setClickingTogglesState (true);
    editToolScissorsButton.setClickingTogglesState (true);
    editToolResizeButton.setClickingTogglesState (true);
    editToolSelectButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::select); };
    editToolPencilButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::pencil); };
    editToolScissorsButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::scissors); };
    editToolResizeButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::resize); };

    defaultInstrumentModeBox.onChange = [this]
    {
        if (defaultInstrumentModeBox.getSelectedId() <= 0)
            return;

        const auto selectedMode = getDefaultInstrumentModeSelection();
        engine.getPropertyStorage().getPropertiesFile().setValue ("defaultInstrumentMode",
                                                                  getDefaultInstrumentModeStorageValue (selectedMode));
        setStatus ("Default instrument mode set to " + getDefaultInstrumentModeDisplayName (selectedMode) + ".");
    };

    leftDockPanelModeBox.onChange = [this]
    {
        if (leftDockPanelModeBox.getSelectedId() <= 0)
            return;

        setLeftDockPanelMode (getLeftDockPanelModeSelection(), true, true);
    };
}

void BeatMakerNoRecord::layoutTrackPanel (juce::Rectangle<int> bounds, bool detachedMode)
{
    const int densityOrdinal = uiDensityMode == UiDensityMode::compact ? 0
                             : (uiDensityMode == UiDensityMode::accessible ? 2 : 1);
    auto metrics = beatmaker::layout::makeMetrics (bounds,
                                                   getUiDensityScale(),
                                                   getTrackDensity (densityOrdinal),
                                                   detachedMode);
    auto area = beatmaker::layout::groupContent (trackGroup,
                                                 beatmaker::layout::insetSection (bounds, metrics),
                                                 metrics);

    const int trackManageRowHeight = beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 8, 2);
    const int trackImportRowHeight = beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 6, 2);

    beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics, trackManageRowHeight), metrics,
                                        { &addTrackButton, &addMidiTrackButton, &addFloatingInstrumentTrackButton,
                                          &moveTrackUpButton, &moveTrackDownButton, &duplicateTrackButton,
                                          &colorTrackButton, &renameTrackButton });
    beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics, trackImportRowHeight), metrics,
                                        { &importAudioButton, &importMidiButton, &createMidiClipButton,
                                          &splitAllTracksButton, &insertBarButton, &deleteBarButton });

    auto editToolRow = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight);
    editToolLabel.setBounds (editToolRow.removeFromLeft (66).reduced (0, 1));
    editToolRow.removeFromLeft (6);
    beatmaker::layout::layoutButtonRow (editToolRow, metrics,
                                        { &editToolSelectButton, &editToolPencilButton,
                                          &editToolScissorsButton, &editToolResizeButton });

    auto instrumentRow = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight);
    defaultInstrumentModeLabel.setBounds (instrumentRow.removeFromLeft (96).reduced (0, 1));
    instrumentRow.removeFromLeft (6);
    defaultInstrumentModeBox.setBounds (instrumentRow.reduced (0, 1));
}
