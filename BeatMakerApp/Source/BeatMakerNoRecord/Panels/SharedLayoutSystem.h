#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace beatmaker::layout
{
enum class DensityMode
{
    compact,
    comfortable,
    accessible
};

struct Metrics
{
    bool compactLayout = false;
    bool denseLayout = false;
    int defaultRowHeight = 26;
    int rowGap = 4;
    int groupInset = 9;
    int groupTitleInset = 19;
    int sectionInset = 4;
    int buttonGapX = 6;
    int buttonGapY = 3;
    int minButtonWidth = 60;
    int splitterThickness = 10;
};

Metrics makeMetrics (juce::Rectangle<int> bounds,
                     float uiScale,
                     DensityMode densityMode,
                     bool detachedMode);

juce::Rectangle<int> insetSection (juce::Rectangle<int> bounds, const Metrics& metrics);
juce::Rectangle<int> nextRow (juce::Rectangle<int>& area, const Metrics& metrics, int height = 0);
juce::Rectangle<int> groupContent (juce::GroupComponent& group, juce::Rectangle<int> area, const Metrics& metrics);
juce::Rectangle<int> groupContentPlain (juce::GroupComponent& group, juce::Rectangle<int> area, const Metrics& metrics);
int estimateGroupHeight (const Metrics& metrics, std::initializer_list<int> rowHeights);
int adaptiveButtonRowHeight (const Metrics& metrics, int rowWidth, int buttonCount, int maxRows = 2);
void layoutButtonRow (juce::Rectangle<int> row, const Metrics& metrics, std::initializer_list<juce::Button*> buttons);
}
