#include "../BeatMakerNoRecord.h"
#include "SharedLayoutSystem.h"

namespace
{
beatmaker::layout::DensityMode getFxDensity (int densityMode)
{
    if (densityMode == 0)
        return beatmaker::layout::DensityMode::compact;
    if (densityMode == 2)
        return beatmaker::layout::DensityMode::accessible;
    return beatmaker::layout::DensityMode::comfortable;
}
}

void BeatMakerNoRecord::setupFxCallbacks()
{
    audioGainDownButton.onClick = [this] { adjustSelectedAudioClipGain (-1.0f); };
    audioGainUpButton.onClick = [this] { adjustSelectedAudioClipGain (1.0f); };
    audioFadeInButton.onClick = [this] { setSelectedAudioClipFade (true); };
    audioFadeOutButton.onClick = [this] { setSelectedAudioClipFade (false); };
    audioClearFadesButton.onClick = [this] { clearSelectedAudioClipFades(); };
    audioReverseButton.onClick = [this] { toggleSelectedAudioClipReverse(); };
    audioSpeedDownButton.onClick = [this] { scaleSelectedAudioClipSpeed (0.5); };
    audioSpeedUpButton.onClick = [this] { scaleSelectedAudioClipSpeed (2.0); };
    audioPitchDownButton.onClick = [this] { adjustSelectedAudioClipPitch (-1.0f); };
    audioPitchUpButton.onClick = [this] { adjustSelectedAudioClipPitch (1.0f); };
    audioAutoTempoButton.onClick = [this] { toggleSelectedAudioClipAutoTempo(); };
    audioWarpButton.onClick = [this] { toggleSelectedAudioClipWarp(); };
    audioAlignToBarButton.onClick = [this] { alignSelectedClipToBar(); };
    audioMake2BarLoopButton.onClick = [this] { makeSelectedClipLoop (2); };
    audioMake4BarLoopButton.onClick = [this] { makeSelectedClipLoop (4); };
    audioFillTransportLoopButton.onClick = [this] { fillTransportLoopWithSelectedClip(); };

    fxRefreshButton.onClick = [this] { refreshSelectedTrackPluginList(); };
    fxScanButton.onClick = [this] { openPluginScanDialog(); };
    fxScanSkippedButton.onClick = [this] { scanSkippedPlugins(); };
    fxPrepPlaybackButton.onClick = [this] { prepareEditForPluginPlayback (true); };
    fxAddExternalInstrumentButton.onClick = [this] { addExternalInstrumentPluginToSelectedTrack(); };
    fxAddExternalButton.onClick = [this] { addExternalPluginToSelectedTrack(); };
    fxOpenEditorButton.onClick = [this] { openSelectedTrackPluginEditor(); };
    fxMoveUpButton.onClick = [this] { moveSelectedTrackPlugin (false); };
    fxMoveDownButton.onClick = [this] { moveSelectedTrackPlugin (true); };
    fxBypassButton.onClick = [this] { toggleSelectedTrackPluginBypass(); };
    fxDeleteButton.onClick = [this] { deleteSelectedTrackPlugin(); };
    fxChainBox.onChange = [this]
    {
        if (updatingTrackControls)
            return;

        if (auto* plugin = getSelectedTrackPlugin())
            selectionManager.selectOnly (plugin);

        updateButtonsFromState();
    };
}

void BeatMakerNoRecord::layoutFxPanel (juce::Rectangle<int> bounds, bool detachedMode)
{
    const int densityOrdinal = uiDensityMode == UiDensityMode::compact ? 0
                             : (uiDensityMode == UiDensityMode::accessible ? 2 : 1);
    auto metrics = beatmaker::layout::makeMetrics (bounds,
                                                   getUiDensityScale(),
                                                   getFxDensity (densityOrdinal),
                                                   detachedMode);
    auto area = beatmaker::layout::groupContent (fxGroup,
                                                 beatmaker::layout::insetSection (bounds, metrics),
                                                 metrics);

    auto row1 = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 1);
    fxChainLabel.setBounds (row1.removeFromLeft (54).reduced (0, 1));
    row1.removeFromLeft (6);
    auto actions = row1.removeFromRight (juce::jmax (detachedMode ? 176 : 140, row1.getWidth() / 2));
    fxChainBox.setBounds (row1.reduced (0, 1));
    beatmaker::layout::layoutButtonRow (actions, metrics,
                                        { &fxRefreshButton, &fxScanButton, &fxScanSkippedButton, &fxPrepPlaybackButton });

    beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                     beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 2, 2)),
                                        metrics,
                                        { &fxAddExternalInstrumentButton, &fxAddExternalButton });

    beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                     beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 5, 2)),
                                        metrics,
                                        { &fxOpenEditorButton, &fxMoveUpButton, &fxMoveDownButton, &fxBypassButton, &fxDeleteButton });
}
