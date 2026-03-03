#include "BeatMakerNoRecordCommandRouting.h"

namespace beatmaker::routing
{
ViewMenuRoute routeViewMenuCommand (int menuItemId)
{
    switch (menuItemId)
    {
        case menuViewFloatWorkspace:
            return { ViewMenuRouteAction::invokeAppCommand, appCommandToggleFloatWorkspace, UiDensityRoute::comfortable };
        case menuViewFloatMixer:
            return { ViewMenuRouteAction::invokeAppCommand, appCommandToggleFloatMixer, UiDensityRoute::comfortable };
        case menuViewFloatPiano:
            return { ViewMenuRouteAction::invokeAppCommand, appCommandToggleFloatPiano, UiDensityRoute::comfortable };
        case menuViewDockAllPanels:
            return { ViewMenuRouteAction::invokeAppCommand, appCommandDockAllPanels, UiDensityRoute::comfortable };
        case menuViewUiCompact:
            return { ViewMenuRouteAction::setUiDensity, 0, UiDensityRoute::compact };
        case menuViewUiComfortable:
            return { ViewMenuRouteAction::setUiDensity, 0, UiDensityRoute::comfortable };
        case menuViewUiAccessible:
            return { ViewMenuRouteAction::setUiDensity, 0, UiDensityRoute::accessible };
        default:
            return {};
    }
}

bool isFloatingLayoutCommand (juce::CommandID commandId)
{
    return commandId == appCommandToggleFloatWorkspace
        || commandId == appCommandToggleFloatMixer
        || commandId == appCommandToggleFloatPiano
        || commandId == appCommandDockAllPanels;
}

SelectionPanelVisibility resolveSelectionPanelVisibility (const SelectionPanelVisibilityInput& input)
{
    SelectionPanelVisibility state;
    state.showClipPanel = input.hasEdit && input.clipPanelVisible;
    state.showAudioPanel = input.hasEdit && input.audioPanelVisible;
    state.showClipPanelDocked = state.showClipPanel && ! input.clipFloating;
    state.showAudioPanelDocked = state.showAudioPanel && ! input.audioFloating;
    state.clipActionsEnabled = state.showClipPanel && input.hasClipSelection;
    state.audioActionsEnabled = state.showAudioPanel && input.hasAudioSelection;
    return state;
}
}
