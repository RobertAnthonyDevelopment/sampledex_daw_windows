#include "../BeatMakerNoRecord.h"
#include "SharedLayoutSystem.h"

namespace
{
beatmaker::layout::DensityMode getMixerDensity (int densityMode)
{
    if (densityMode == 0)
        return beatmaker::layout::DensityMode::compact;
    if (densityMode == 2)
        return beatmaker::layout::DensityMode::accessible;
    return beatmaker::layout::DensityMode::comfortable;
}
}

void BeatMakerNoRecord::setupMixerCallbacks()
{
    zoomVerticalInButton.onClick = [this]
    {
        zoomTimelineVertically (1.12);
    };
    zoomVerticalOutButton.onClick = [this]
    {
        zoomTimelineVertically (0.89);
    };
    zoomVerticalResetButton.onClick = [this]
    {
        resetVerticalZoom();
    };

    trackMuteButton.onClick = [this] { applyTrackMixerFromUI(); };
    trackSoloButton.onClick = [this] { applyTrackMixerFromUI(); };
    trackVolumeSlider.onValueChange = [this] { applyTrackMixerFromUI(); };
    trackPanSlider.onValueChange = [this] { applyTrackMixerFromUI(); };
    tempoSlider.onValueChange = [this] { applyTempoFromUI(); };
    trackHeightSlider.onValueChange = [this] { applyTrackHeightFromUI(); };

    channelRackTrackBox.onChange = [this]
    {
        if (updatingChannelRackControls || edit == nullptr)
            return;

        auto tracks = te::getAudioTracks (*edit);
        const int selectedIndex = channelRackTrackBox.getSelectedId() - 1;
        if (selectedIndex < 0 || selectedIndex >= tracks.size())
            return;

        if (auto* track = tracks.getUnchecked (selectedIndex))
            selectionManager.selectOnly (track);

        updateTrackControlsFromSelection();
        updateButtonsFromState();
    };
    channelRackPluginBox.onChange = [this]
    {
        if (updatingChannelRackControls)
            return;

        auto* track = getSelectedTrackOrFirst();
        if (track == nullptr)
            return;

        auto plugins = track->pluginList.getPlugins();
        const int selectedIndex = channelRackPluginBox.getSelectedId() - 1;
        if (selectedIndex < 0 || selectedIndex >= channelRackPluginIndexMap.size())
            return;

        const int pluginIndex = channelRackPluginIndexMap.getUnchecked (selectedIndex);
        if (pluginIndex < 0 || pluginIndex >= plugins.size())
            return;

        if (auto* plugin = plugins[pluginIndex].get())
        {
            selectionManager.selectOnly (plugin);
            fxChainBox.setSelectedId (pluginIndex + 1, juce::dontSendNotification);
        }

        updateButtonsFromState();
    };
    channelRackAddInstrumentButton.onClick = [this] { addExternalInstrumentPluginToSelectedTrack(); };
    channelRackAddFxButton.onClick = [this] { addExternalPluginToSelectedTrack(); };
    channelRackOpenPluginButton.onClick = [this] { openSelectedTrackPluginEditor(); };
}

void BeatMakerNoRecord::layoutMixerPanel (juce::Rectangle<int> bounds, bool detachedMode)
{
    const int densityOrdinal = uiDensityMode == UiDensityMode::compact ? 0
                             : (uiDensityMode == UiDensityMode::accessible ? 2 : 1);
    auto metrics = beatmaker::layout::makeMetrics (bounds,
                                                   getUiDensityScale(),
                                                   getMixerDensity (densityOrdinal),
                                                   detachedMode);
    auto area = beatmaker::layout::groupContent (mixerGroup,
                                                 beatmaker::layout::insetSection (bounds, metrics),
                                                 metrics);

    auto row1 = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
    selectedTrackLabel.setBounds (row1.removeFromLeft (juce::jmax (detachedMode ? 130 : 120, row1.getWidth() / 2)).reduced (0, 1));
    row1.removeFromLeft (6);
    trackMuteButton.setBounds (row1.removeFromLeft (64).reduced (0, 1));
    row1.removeFromLeft (6);
    trackSoloButton.setBounds (row1.removeFromLeft (64).reduced (0, 1));
    row1.removeFromLeft (detachedMode ? 8 : 10);
    trackHeightLabel.setBounds (row1.removeFromLeft (detachedMode ? 60 : 58).reduced (0, 1));
    row1.removeFromLeft (6);

    const int minTrackHeightSliderWidth = 108;
    const int verticalZoomButtonsWidth = juce::jmin (juce::jmax (132, row1.getWidth() / 3),
                                                     juce::jmax (0, row1.getWidth() - minTrackHeightSliderWidth));
    auto zoomArea = row1.removeFromRight (verticalZoomButtonsWidth);
    if (zoomArea.getWidth() > 0)
        row1.removeFromRight (6);
    trackHeightSlider.setBounds (row1.reduced (0, 1));
    if (zoomArea.getWidth() > 0)
    {
        beatmaker::layout::layoutButtonRow (zoomArea, metrics,
                                            { &zoomVerticalInButton, &zoomVerticalOutButton, &zoomVerticalResetButton });
    }
    else
    {
        zoomVerticalInButton.setBounds ({});
        zoomVerticalOutButton.setBounds ({});
        zoomVerticalResetButton.setBounds ({});
    }

    auto row2 = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
    trackVolumeLabel.setBounds (row2.removeFromLeft (detachedMode ? 36 : 34).reduced (0, 1));
    row2.removeFromLeft (6);
    trackVolumeSlider.setBounds (row2.removeFromLeft (juce::jmax (detachedMode ? 110 : 96, row2.getWidth() / 2)).reduced (0, 1));
    row2.removeFromLeft (6);
    trackPanLabel.setBounds (row2.removeFromLeft (detachedMode ? 34 : 32).reduced (0, 1));
    row2.removeFromLeft (6);
    trackPanSlider.setBounds (row2.reduced (0, 1));
}
