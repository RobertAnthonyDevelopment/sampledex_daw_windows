#include "../BeatMakerNoRecord.h"
#include "SharedLayoutSystem.h"

namespace
{
beatmaker::layout::DensityMode getArrangementDensity (int densityMode)
{
    if (densityMode == 0)
        return beatmaker::layout::DensityMode::compact;
    if (densityMode == 2)
        return beatmaker::layout::DensityMode::accessible;
    return beatmaker::layout::DensityMode::comfortable;
}
}

void BeatMakerNoRecord::setupArrangementCallbacks()
{
    showMarkerTrackButton.onClick = [this] { toggleMarkerTrackVisibility(); };
    showArrangerTrackButton.onClick = [this] { toggleArrangerTrackVisibility(); };
    addMarkerButton.onClick = [this] { addMarkerAtPlayhead(); };
    prevMarkerButton.onClick = [this] { jumpToMarker (false); };
    nextMarkerButton.onClick = [this] { jumpToMarker (true); };
    loopMarkersButton.onClick = [this] { setLoopBetweenNearestMarkers(); };
    addSectionButton.onClick = [this] { addArrangerSectionAtPlayhead(); };
    prevSectionButton.onClick = [this] { jumpToArrangerSection (false); };
    nextSectionButton.onClick = [this] { jumpToArrangerSection (true); };
    loopSectionButton.onClick = [this] { setLoopToCurrentArrangerSection(); };
}

void BeatMakerNoRecord::layoutArrangementPanel (juce::Rectangle<int> bounds, bool detachedMode)
{
    const int densityOrdinal = uiDensityMode == UiDensityMode::compact ? 0
                             : (uiDensityMode == UiDensityMode::accessible ? 2 : 1);
    auto metrics = beatmaker::layout::makeMetrics (bounds,
                                                   getUiDensityScale(),
                                                   getArrangementDensity (densityOrdinal),
                                                   detachedMode);
    auto area = beatmaker::layout::groupContent (arrangementGroup,
                                                 beatmaker::layout::insetSection (bounds, metrics),
                                                 metrics);
    const int rowHeight = juce::jmax (metrics.defaultRowHeight + 2,
                                      beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 6, 2));

    beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics, rowHeight), metrics,
                                        { &showMarkerTrackButton, &showArrangerTrackButton, &addMarkerButton,
                                          &prevMarkerButton, &nextMarkerButton, &loopMarkersButton });
    beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics, rowHeight), metrics,
                                        { &addSectionButton, &prevSectionButton, &nextSectionButton,
                                          &loopSectionButton, &jumpPrevBarButton, &jumpNextBarButton });
}
