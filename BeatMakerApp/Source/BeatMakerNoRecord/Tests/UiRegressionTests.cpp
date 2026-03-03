#include "../Panels/SharedLayoutSystem.h"
#include "../UI/BeatMakerNoRecordCommandRouting.h"

#include <cstdlib>
#include <iostream>

namespace
{
int failures = 0;

void expectTrue (bool condition, const char* message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAILED: " << message << std::endl;
    }
}

void testViewMenuRouting()
{
    using namespace beatmaker::routing;

    const auto floatWorkspace = routeViewMenuCommand (menuViewFloatWorkspace);
    expectTrue (floatWorkspace.action == ViewMenuRouteAction::invokeAppCommand,
                "menuViewFloatWorkspace should route to app command invocation");
    expectTrue (floatWorkspace.appCommandId == appCommandToggleFloatWorkspace,
                "menuViewFloatWorkspace should map to appCommandToggleFloatWorkspace");

    const auto floatMixer = routeViewMenuCommand (menuViewFloatMixer);
    expectTrue (floatMixer.action == ViewMenuRouteAction::invokeAppCommand,
                "menuViewFloatMixer should route to app command invocation");
    expectTrue (floatMixer.appCommandId == appCommandToggleFloatMixer,
                "menuViewFloatMixer should map to appCommandToggleFloatMixer");

    const auto floatPiano = routeViewMenuCommand (menuViewFloatPiano);
    expectTrue (floatPiano.action == ViewMenuRouteAction::invokeAppCommand,
                "menuViewFloatPiano should route to app command invocation");
    expectTrue (floatPiano.appCommandId == appCommandToggleFloatPiano,
                "menuViewFloatPiano should map to appCommandToggleFloatPiano");

    const auto dockAll = routeViewMenuCommand (menuViewDockAllPanels);
    expectTrue (dockAll.action == ViewMenuRouteAction::invokeAppCommand,
                "menuViewDockAllPanels should route to app command invocation");
    expectTrue (dockAll.appCommandId == appCommandDockAllPanels,
                "menuViewDockAllPanels should map to appCommandDockAllPanels");

    const auto uiCompact = routeViewMenuCommand (menuViewUiCompact);
    expectTrue (uiCompact.action == ViewMenuRouteAction::setUiDensity,
                "menuViewUiCompact should route to density update");
    expectTrue (uiCompact.density == UiDensityRoute::compact,
                "menuViewUiCompact should map to compact density");

    const auto uiComfortable = routeViewMenuCommand (menuViewUiComfortable);
    expectTrue (uiComfortable.action == ViewMenuRouteAction::setUiDensity,
                "menuViewUiComfortable should route to density update");
    expectTrue (uiComfortable.density == UiDensityRoute::comfortable,
                "menuViewUiComfortable should map to comfortable density");

    const auto uiAccessible = routeViewMenuCommand (menuViewUiAccessible);
    expectTrue (uiAccessible.action == ViewMenuRouteAction::setUiDensity,
                "menuViewUiAccessible should route to density update");
    expectTrue (uiAccessible.density == UiDensityRoute::accessible,
                "menuViewUiAccessible should map to accessible density");

    expectTrue (isFloatingLayoutCommand (appCommandToggleFloatWorkspace),
                "workspace floating command should be treated as layout floating command");
    expectTrue (isFloatingLayoutCommand (appCommandDockAllPanels),
                "dock all panels command should be treated as layout floating command");
    expectTrue (! isFloatingLayoutCommand (22001),
                "save command should not be treated as layout floating command");
}

void testSharedLayoutModes()
{
    using namespace beatmaker::layout;

    const auto dockComfortable = makeMetrics ({ 0, 0, 1200, 800 }, 1.0f, DensityMode::comfortable, false);
    const auto dockCompact = makeMetrics ({ 0, 0, 1200, 800 }, 1.0f, DensityMode::compact, false);
    const auto dockAccessible = makeMetrics ({ 0, 0, 1200, 800 }, 1.0f, DensityMode::accessible, false);
    expectTrue (! dockComfortable.denseLayout,
                "large docked layout should not be dense");
    expectTrue (! dockComfortable.compactLayout,
                "large docked layout should not be compact");
    expectTrue (dockCompact.defaultRowHeight < dockComfortable.defaultRowHeight,
                "compact density should reduce row height");
    expectTrue (dockAccessible.defaultRowHeight > dockComfortable.defaultRowHeight,
                "accessible density should increase row height");
    expectTrue (dockAccessible.splitterThickness >= dockComfortable.splitterThickness,
                "accessible density should not shrink splitter thickness");

    const auto floatComfortable = makeMetrics ({ 0, 0, 1200, 800 }, 1.0f, DensityMode::comfortable, true);
    expectTrue (floatComfortable.sectionInset >= dockComfortable.sectionInset,
                "detached layout should keep equal or larger section inset");

    const auto thresholdDocked = makeMetrics ({ 0, 0, 930, 600 }, 1.0f, DensityMode::comfortable, false);
    const auto thresholdFloating = makeMetrics ({ 0, 0, 930, 600 }, 1.0f, DensityMode::comfortable, true);
    expectTrue (thresholdDocked.compactLayout,
                "mid-width docked layout should become compact");
    expectTrue (! thresholdFloating.compactLayout,
                "same bounds in detached mode should stay non-compact");

    const auto smallDocked = makeMetrics ({ 0, 0, 700, 380 }, 1.0f, DensityMode::comfortable, false);
    const auto smallFloating = makeMetrics ({ 0, 0, 700, 380 }, 1.0f, DensityMode::comfortable, true);
    expectTrue (smallDocked.denseLayout,
                "small docked layout should be dense");
    expectTrue (smallFloating.denseLayout,
                "small detached layout should be dense");
}

void testSelectionPanelVisibilityRouting()
{
    using namespace beatmaker::routing;

    const SelectionPanelVisibilityInput baseInput {
        true,   // hasEdit
        true,   // clipPanelVisible
        true,   // audioPanelVisible
        false,  // clipFloating
        false,  // audioFloating
        true,   // hasClipSelection
        true    // hasAudioSelection
    };

    const auto dockedWithSelections = resolveSelectionPanelVisibility (baseInput);
    expectTrue (dockedWithSelections.showClipPanel,
                "clip panel should remain visible when enabled and edit exists");
    expectTrue (dockedWithSelections.showAudioPanel,
                "audio panel should remain visible when enabled and edit exists");
    expectTrue (dockedWithSelections.showClipPanelDocked,
                "clip panel should be docked-visible when not floating");
    expectTrue (dockedWithSelections.showAudioPanelDocked,
                "audio panel should be docked-visible when not floating");
    expectTrue (dockedWithSelections.clipActionsEnabled,
                "clip actions should be enabled with clip selection");
    expectTrue (dockedWithSelections.audioActionsEnabled,
                "audio actions should be enabled with audio selection");

    auto noSelection = baseInput;
    noSelection.hasClipSelection = false;
    noSelection.hasAudioSelection = false;
    const auto dockedWithoutSelections = resolveSelectionPanelVisibility (noSelection);
    expectTrue (dockedWithoutSelections.showClipPanel,
                "clip panel should stay visible when selection is cleared");
    expectTrue (dockedWithoutSelections.showAudioPanel,
                "audio panel should stay visible when selection is cleared");
    expectTrue (! dockedWithoutSelections.clipActionsEnabled,
                "clip actions should disable when clip selection is cleared");
    expectTrue (! dockedWithoutSelections.audioActionsEnabled,
                "audio actions should disable when audio selection is cleared");

    auto floating = noSelection;
    floating.clipFloating = true;
    floating.audioFloating = true;
    const auto floatingWithoutSelections = resolveSelectionPanelVisibility (floating);
    expectTrue (floatingWithoutSelections.showClipPanel,
                "clip panel should remain visible when detached");
    expectTrue (floatingWithoutSelections.showAudioPanel,
                "audio panel should remain visible when detached");
    expectTrue (! floatingWithoutSelections.showClipPanelDocked,
                "clip panel docked visibility should be false when detached");
    expectTrue (! floatingWithoutSelections.showAudioPanelDocked,
                "audio panel docked visibility should be false when detached");

    auto hiddenPanels = floating;
    hiddenPanels.clipPanelVisible = false;
    hiddenPanels.audioPanelVisible = false;
    const auto hidden = resolveSelectionPanelVisibility (hiddenPanels);
    expectTrue (! hidden.showClipPanel,
                "clip panel should hide when panel visibility is disabled");
    expectTrue (! hidden.showAudioPanel,
                "audio panel should hide when panel visibility is disabled");
    expectTrue (! hidden.clipActionsEnabled,
                "clip actions should stay disabled when clip panel is hidden");
    expectTrue (! hidden.audioActionsEnabled,
                "audio actions should stay disabled when audio panel is hidden");
}

void testSelectionPanelVisibilityTransitions()
{
    using namespace beatmaker::routing;

    SelectionPanelVisibilityInput state;
    state.hasEdit = true;
    state.clipPanelVisible = true;
    state.audioPanelVisible = true;
    state.hasClipSelection = true;
    state.hasAudioSelection = true;

    const auto dockedSelected = resolveSelectionPanelVisibility (state);
    expectTrue (dockedSelected.showClipPanelDocked && dockedSelected.showAudioPanelDocked,
                "both panels should be docked-visible in baseline state");
    expectTrue (dockedSelected.clipActionsEnabled && dockedSelected.audioActionsEnabled,
                "actions should start enabled with valid selection");

    state.hasClipSelection = false;
    state.hasAudioSelection = false;
    const auto dockedNoSelection = resolveSelectionPanelVisibility (state);
    expectTrue (dockedNoSelection.showClipPanel && dockedNoSelection.showAudioPanel,
                "clearing selection should not hide visible docked panels");
    expectTrue (! dockedNoSelection.clipActionsEnabled && ! dockedNoSelection.audioActionsEnabled,
                "clearing selection should disable actions");

    state.clipFloating = true;
    state.audioFloating = true;
    const auto floatingNoSelection = resolveSelectionPanelVisibility (state);
    expectTrue (floatingNoSelection.showClipPanel && floatingNoSelection.showAudioPanel,
                "floating panels should remain visible without selection");
    expectTrue (! floatingNoSelection.showClipPanelDocked && ! floatingNoSelection.showAudioPanelDocked,
                "floating panels should no longer be reported as docked-visible");

    state.hasEdit = false;
    const auto noEdit = resolveSelectionPanelVisibility (state);
    expectTrue (! noEdit.showClipPanel && ! noEdit.showAudioPanel,
                "panel visibility should turn off when edit is unavailable");
    expectTrue (! noEdit.clipActionsEnabled && ! noEdit.audioActionsEnabled,
                "actions should be disabled when edit is unavailable");
}

void testSelectionPanelMixedFloatingVisibility()
{
    using namespace beatmaker::routing;

    SelectionPanelVisibilityInput state;
    state.hasEdit = true;
    state.clipPanelVisible = true;
    state.audioPanelVisible = true;
    state.clipFloating = true;
    state.audioFloating = false;
    state.hasClipSelection = false;
    state.hasAudioSelection = true;

    const auto mixed = resolveSelectionPanelVisibility (state);
    expectTrue (mixed.showClipPanel && mixed.showAudioPanel,
                "panel visibility should not depend on current clip/audio selection");
    expectTrue (! mixed.showClipPanelDocked && mixed.showAudioPanelDocked,
                "floating state should be reflected per panel");
    expectTrue (! mixed.clipActionsEnabled && mixed.audioActionsEnabled,
                "action enablement should depend on panel-specific selection");

    state.clipPanelVisible = false;
    const auto clipHidden = resolveSelectionPanelVisibility (state);
    expectTrue (! clipHidden.showClipPanel && clipHidden.showAudioPanel,
                "clip panel hide state should not affect audio panel visibility");
    expectTrue (! clipHidden.clipActionsEnabled && clipHidden.audioActionsEnabled,
                "hidden clip panel should keep clip actions disabled while audio remains enabled");

    state.audioPanelVisible = false;
    const auto bothHidden = resolveSelectionPanelVisibility (state);
    expectTrue (! bothHidden.showClipPanel && ! bothHidden.showAudioPanel,
                "disabling both panels should hide both clip and audio panels");
    expectTrue (! bothHidden.clipActionsEnabled && ! bothHidden.audioActionsEnabled,
                "disabling both panels should disable all panel actions");
}
}

int main()
{
    testViewMenuRouting();
    testSharedLayoutModes();
    testSelectionPanelVisibilityRouting();
    testSelectionPanelVisibilityTransitions();
    testSelectionPanelMixedFloatingVisibility();

    if (failures > 0)
    {
        std::cerr << failures << " UI regression checks failed." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "UI regression checks passed." << std::endl;
    return EXIT_SUCCESS;
}
