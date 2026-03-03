#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace beatmaker::routing
{
enum class UiDensityRoute
{
    compact,
    comfortable,
    accessible
};

enum class ViewMenuRouteAction
{
    none,
    invokeAppCommand,
    setUiDensity
};

struct ViewMenuRoute
{
    ViewMenuRouteAction action = ViewMenuRouteAction::none;
    juce::CommandID appCommandId = 0;
    UiDensityRoute density = UiDensityRoute::comfortable;
};

struct SelectionPanelVisibilityInput
{
    bool hasEdit = false;
    bool clipPanelVisible = false;
    bool audioPanelVisible = false;
    bool clipFloating = false;
    bool audioFloating = false;
    bool hasClipSelection = false;
    bool hasAudioSelection = false;
};

struct SelectionPanelVisibility
{
    bool showClipPanel = false;
    bool showAudioPanel = false;
    bool showClipPanelDocked = false;
    bool showAudioPanelDocked = false;
    bool clipActionsEnabled = false;
    bool audioActionsEnabled = false;
};

inline constexpr int menuViewFloatWorkspace = 1082;
inline constexpr int menuViewFloatMixer = 1083;
inline constexpr int menuViewFloatPiano = 1084;
inline constexpr int menuViewDockAllPanels = 1085;
inline constexpr int menuViewUiCompact = 1092;
inline constexpr int menuViewUiComfortable = 1093;
inline constexpr int menuViewUiAccessible = 1094;

inline constexpr juce::CommandID appCommandToggleFloatWorkspace = 22024;
inline constexpr juce::CommandID appCommandToggleFloatMixer = 22025;
inline constexpr juce::CommandID appCommandToggleFloatPiano = 22026;
inline constexpr juce::CommandID appCommandDockAllPanels = 22027;

ViewMenuRoute routeViewMenuCommand (int menuItemId);
bool isFloatingLayoutCommand (juce::CommandID commandId);
SelectionPanelVisibility resolveSelectionPanelVisibility (const SelectionPanelVisibilityInput& input);
}
