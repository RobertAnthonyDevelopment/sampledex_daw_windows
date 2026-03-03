#include "SharedLayoutSystem.h"

namespace beatmaker::layout
{
Metrics makeMetrics (juce::Rectangle<int> bounds,
                     float uiScale,
                     DensityMode densityMode,
                     bool detachedMode)
{
    Metrics metrics;

    const int compactWidth = detachedMode ? juce::roundToInt (900.0f * uiScale)
                                          : juce::roundToInt (980.0f * uiScale);
    const int compactHeight = juce::roundToInt (520.0f * uiScale);
    const int denseWidth = detachedMode ? juce::roundToInt (760.0f * uiScale)
                                        : juce::roundToInt (820.0f * uiScale);
    const int denseHeight = juce::roundToInt (420.0f * uiScale);

    metrics.compactLayout = bounds.getWidth() < compactWidth || bounds.getHeight() < compactHeight;
    metrics.denseLayout = bounds.getWidth() < denseWidth || bounds.getHeight() < denseHeight;
    metrics.defaultRowHeight = metrics.denseLayout ? (detachedMode ? 24 : 25)
                                                   : (metrics.compactLayout ? (detachedMode ? 25 : 26)
                                                                            : (detachedMode ? 27 : 28));
    metrics.rowGap = metrics.denseLayout ? 3 : 5;
    metrics.groupInset = metrics.denseLayout ? 8 : 10;
    metrics.groupTitleInset = metrics.denseLayout ? 18 : 20;
    metrics.sectionInset = detachedMode ? 6 : (metrics.denseLayout ? 2 : 4);
    metrics.splitterThickness = densityMode == DensityMode::accessible ? 12 : (metrics.denseLayout ? 8 : 10);
    metrics.buttonGapX = bounds.getWidth() < 460 ? 4 : 6;
    metrics.buttonGapY = 3;
    metrics.minButtonWidth = metrics.denseLayout ? 52 : 60;

    switch (densityMode)
    {
        case DensityMode::compact:
            metrics.defaultRowHeight = juce::jmax (detachedMode ? 22 : 23, metrics.defaultRowHeight - 2);
            metrics.rowGap = juce::jmax (2, metrics.rowGap - 1);
            metrics.groupInset = juce::jmax (7, metrics.groupInset - 1);
            metrics.minButtonWidth = juce::jmax (42, metrics.minButtonWidth - 10);
            break;

        case DensityMode::accessible:
            metrics.defaultRowHeight += 3;
            metrics.rowGap += 1;
            metrics.groupInset += 1;
            metrics.groupTitleInset += 2;
            metrics.sectionInset = detachedMode ? 7 : 5;
            metrics.minButtonWidth += 16;
            break;

        case DensityMode::comfortable:
            break;
    }

    return metrics;
}

juce::Rectangle<int> insetSection (juce::Rectangle<int> bounds, const Metrics& metrics)
{
    if (bounds.isEmpty())
        return bounds;

    const int insetX = juce::jmin (metrics.sectionInset, juce::jmax (0, bounds.getWidth() / 9));
    const int insetY = juce::jmin (metrics.sectionInset, juce::jmax (0, bounds.getHeight() / 9));
    return bounds.reduced (insetX, insetY);
}

juce::Rectangle<int> nextRow (juce::Rectangle<int>& area, const Metrics& metrics, int height)
{
    const int targetHeight = juce::jmax (0, juce::jmin (height > 0 ? height : metrics.defaultRowHeight, area.getHeight()));
    auto row = area.removeFromTop (targetHeight);

    if (area.getHeight() > 0)
        area.removeFromTop (juce::jmin (metrics.rowGap, area.getHeight()));

    return row;
}

juce::Rectangle<int> groupContent (juce::GroupComponent& group, juce::Rectangle<int> area, const Metrics& metrics)
{
    group.setBounds (area);

    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return {};

    auto inner = area.reduced (metrics.groupInset);
    if (inner.getHeight() <= metrics.groupTitleInset)
        return {};

    inner.removeFromTop (metrics.groupTitleInset);
    return inner;
}

juce::Rectangle<int> groupContentPlain (juce::GroupComponent& group, juce::Rectangle<int> area, const Metrics& metrics)
{
    group.setBounds (area);

    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return {};

    return area.reduced (metrics.groupInset);
}

int estimateGroupHeight (const Metrics& metrics, std::initializer_list<int> rowHeights)
{
    int contentHeight = 0;
    int rowCount = 0;
    for (const int rowHeight : rowHeights)
    {
        if (rowHeight <= 0)
            continue;

        contentHeight += rowHeight;
        ++rowCount;
    }

    if (rowCount > 1)
        contentHeight += metrics.rowGap * (rowCount - 1);

    return contentHeight + metrics.groupInset * 2 + metrics.groupTitleInset;
}

int adaptiveButtonRowHeight (const Metrics& metrics, int rowWidth, int buttonCount, int maxRows)
{
    if (buttonCount <= 0 || rowWidth <= 0)
        return metrics.defaultRowHeight;

    const int columns = juce::jmax (1, juce::jmin (buttonCount, (rowWidth + metrics.buttonGapX) / (metrics.minButtonWidth + metrics.buttonGapX)));
    int rows = juce::jmax (1, (buttonCount + columns - 1) / columns);
    rows = juce::jmin (juce::jmax (1, maxRows), rows);
    return metrics.defaultRowHeight * rows + juce::jmax (0, rows - 1) * metrics.buttonGapY;
}

void layoutButtonRow (juce::Rectangle<int> row, const Metrics& metrics, std::initializer_list<juce::Button*> buttons)
{
    const int count = (int) buttons.size();
    if (count <= 0 || row.getWidth() <= 0 || row.getHeight() <= 0)
    {
        for (auto* button : buttons)
            if (button != nullptr)
                button->setBounds ({});
        return;
    }

    const int columns = juce::jmax (1, juce::jmin (count, (row.getWidth() + metrics.buttonGapX) / (metrics.minButtonWidth + metrics.buttonGapX)));
    const int rows = juce::jmax (1, (count + columns - 1) / columns);
    const int totalGapY = juce::jmax (0, rows - 1) * metrics.buttonGapY;
    const int buttonHeight = juce::jmax (18, juce::jmin (metrics.defaultRowHeight + 2,
                                                          (row.getHeight() - totalGapY) / rows));

    int index = 0;
    auto area = row;
    for (int r = 0; r < rows; ++r)
    {
        auto line = area.removeFromTop (buttonHeight);
        if (area.getHeight() > 0 && r < rows - 1)
            area.removeFromTop (metrics.buttonGapY);

        const int remaining = count - index;
        const int thisRowCount = juce::jmin (columns, remaining);
        const int totalGapX = juce::jmax (0, thisRowCount - 1) * metrics.buttonGapX;
        const int buttonWidth = juce::jmax (1, (line.getWidth() - totalGapX) / juce::jmax (1, thisRowCount));

        for (int c = 0; c < thisRowCount; ++c)
        {
            if (auto* button = *(buttons.begin() + index))
                button->setBounds (line.removeFromLeft (buttonWidth).reduced (0, 1));

            if (line.getWidth() > metrics.buttonGapX && c < thisRowCount - 1)
                line.removeFromLeft (metrics.buttonGapX);

            ++index;
        }
    }
}
}

