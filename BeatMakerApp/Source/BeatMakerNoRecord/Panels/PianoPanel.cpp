#include "../BeatMakerNoRecord.h"
#include "SharedLayoutSystem.h"

void BeatMakerNoRecord::setupPianoCallbacks()
{
    pianoFloatToggleButton.setClickingTogglesState (true);
    pianoFloatToggleButton.onClick = [this]
    {
        toggleSectionFloating (FloatSection::piano);
        refreshPianoFloatingWindowUi();
    };
    pianoEnsureInstrumentButton.onClick = [this]
    {
        if (edit == nullptr)
            return;

        auto* track = [this] () -> te::AudioTrack*
        {
            if (auto* midiClip = getSelectedMidiClip())
                if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
                    return ownerTrack;

            return getSelectedTrackOrFirst();
        }();

        if (track == nullptr)
        {
            setStatus ("Select a MIDI clip or track first.");
            return;
        }

        const bool transportWasPlaying = edit != nullptr && edit->getTransport().isPlaying();
        const bool hadInstrumentBefore = trackHasInstrumentPlugin (*track);
        const bool changed = ensureTrackHasInstrumentForMidiPlayback (*track);
        const bool hasInstrumentAfter = trackHasInstrumentPlugin (*track);
        if (! changed && ! transportWasPlaying && hadInstrumentBefore && hasInstrumentAfter)
            setStatus ("Instrument already ready for MIDI playback on " + track->getName() + ".");

        refreshPianoFloatingWindowUi();
    };
    pianoOpenInstrumentButton.onClick = [this]
    {
        if (edit == nullptr)
            return;

        auto* track = [this] () -> te::AudioTrack*
        {
            if (auto* midiClip = getSelectedMidiClip())
                if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
                    return ownerTrack;

            return getSelectedTrackOrFirst();
        }();

        if (track == nullptr)
        {
            setStatus ("Select a MIDI clip or track first.");
            return;
        }

        ensureTrackHasInstrumentForMidiPlayback (*track);

        for (auto* plugin : track->pluginList.getPlugins())
        {
            if (plugin != nullptr && plugin->isSynth() && plugin->isEnabled())
            {
                plugin->showWindowExplicitly();
                setStatus ("Opened instrument UI: " + plugin->getName());
                refreshPianoFloatingWindowUi();
                return;
            }
        }

        setStatus ("No instrument UI available on selected track.");
    };
    pianoAlwaysOnTopButton.onClick = [this]
    {
        pianoFloatingAlwaysOnTop = pianoAlwaysOnTopButton.getToggleState();
        engine.getPropertyStorage().getPropertiesFile().setValue ("pianoFloatAlwaysOnTop", pianoFloatingAlwaysOnTop);

        if (pianoFloatingWindow != nullptr)
        {
            pianoFloatingWindow->setAlwaysOnTop (pianoFloatingAlwaysOnTop);
            pianoFloatingWindow->toFront (true);
        }

        refreshPianoFloatingWindowUi();
    };
}

void BeatMakerNoRecord::layoutPianoPanel (juce::Rectangle<int> bounds, bool detachedMode)
{
    juce::ignoreUnused (detachedMode);

    const bool denseLayout = bounds.getWidth() < roundToInt (820.0f * getUiDensityScale())
                             || bounds.getHeight() < roundToInt (420.0f * getUiDensityScale());
    const auto densityMode = uiDensityMode;

    auto topSection = beatmaker::layout::insetSection (bounds,
                                                       beatmaker::layout::makeMetrics (bounds,
                                                                                       getUiDensityScale(),
                                                                                       densityMode == UiDensityMode::compact
                                                                                            ? beatmaker::layout::DensityMode::compact
                                                                                            : (densityMode == UiDensityMode::accessible
                                                                                                   ? beatmaker::layout::DensityMode::accessible
                                                                                                   : beatmaker::layout::DensityMode::comfortable),
                                                                                       false));
    const int minHeaderHeight = denseLayout ? 22 : 24;
    const int maxHeaderHeight = denseLayout ? 34 : 38;
    const int headerHeight = juce::jlimit (minHeaderHeight,
                                           maxHeaderHeight,
                                           juce::jmax (minHeaderHeight, bounds.getHeight() / 4));
    auto headerRow = topSection.removeFromTop (headerHeight).reduced (0, 1);
    topSection.removeFromTop (juce::jlimit (2, 6, bounds.getHeight() / 24));

    const int controlGap = denseLayout ? 4 : 6;
    const bool stackControls = headerRow.getWidth() < 760;

    if (stackControls)
    {
        auto tabsRow = headerRow.removeFromTop (juce::jmax (14, headerRow.getHeight() / 2));
        pianoEditorModeTabs.setBounds (tabsRow.reduced (0, 1));
        if (headerRow.getHeight() > 3)
            headerRow.removeFromTop (3);

        auto controlsRow = headerRow;
        const int half = juce::jmax (0, (controlsRow.getWidth() - controlGap) / 2);
        auto left = controlsRow.removeFromLeft (half);
        if (controlsRow.getWidth() > controlGap)
            controlsRow.removeFromLeft (controlGap);
        auto right = controlsRow;

        const int leftHalf = juce::jmax (0, (left.getWidth() - controlGap) / 2);
        pianoFloatToggleButton.setBounds (left.removeFromLeft (leftHalf).reduced (0, 1));
        if (left.getWidth() > controlGap)
            left.removeFromLeft (controlGap);
        pianoAlwaysOnTopButton.setBounds (left.reduced (0, 1));

        const int rightHalf = juce::jmax (0, (right.getWidth() - controlGap) / 2);
        pianoEnsureInstrumentButton.setBounds (right.removeFromLeft (rightHalf).reduced (0, 1));
        if (right.getWidth() > controlGap)
            right.removeFromLeft (controlGap);
        pianoOpenInstrumentButton.setBounds (right.reduced (0, 1));
    }
    else
    {
        auto tabsArea = headerRow.removeFromLeft (juce::jlimit (210, 320, headerRow.getWidth() / 3));
        pianoEditorModeTabs.setBounds (tabsArea.reduced (0, 1));
        if (headerRow.getWidth() > 8)
            headerRow.removeFromLeft (8);

        const int controlWidth = juce::jmax (0, (headerRow.getWidth() - controlGap * 3) / 4);
        pianoFloatToggleButton.setBounds (headerRow.removeFromLeft (controlWidth).reduced (0, 1));
        if (headerRow.getWidth() > controlGap)
            headerRow.removeFromLeft (controlGap);
        pianoAlwaysOnTopButton.setBounds (headerRow.removeFromLeft (controlWidth).reduced (0, 1));
        if (headerRow.getWidth() > controlGap)
            headerRow.removeFromLeft (controlGap);
        pianoEnsureInstrumentButton.setBounds (headerRow.removeFromLeft (controlWidth).reduced (0, 1));
        if (headerRow.getWidth() > controlGap)
            headerRow.removeFromLeft (controlGap);
        pianoOpenInstrumentButton.setBounds (headerRow.reduced (0, 1));
    }

    const auto effectiveViewMode = getPianoEditorLayoutModeSelection();
    const bool pianoRollFloating = isDetachedPanelFloating (DetachedPanel::pianoRoll);
    const bool stepSequencerFloating = isDetachedPanelFloating (DetachedPanel::stepSequencer);
    const bool allowPianoRollPanel = windowPanelPianoRollVisible && ! pianoRollFloating;
    const bool allowStepSequencerPanel = windowPanelStepSequencerVisible && ! stepSequencerFloating;
    const int splitGap = densityMode == UiDensityMode::accessible ? 12 : (denseLayout ? 8 : 10);
    const int availableSplitHeight = juce::jmax (0, topSection.getHeight() - splitGap);
    auto clearDetachedPanelBounds = [this] (DetachedPanel panel)
    {
        forEachDetachedPanelComponent (panel, [] (juce::Component& component) { component.setBounds ({}); });
    };
    auto clearPianoRollDockBounds = [&]
    {
        if (! pianoRollFloating)
            clearDetachedPanelBounds (DetachedPanel::pianoRoll);
    };
    auto clearStepSequencerDockBounds = [&]
    {
        if (! stepSequencerFloating)
            clearDetachedPanelBounds (DetachedPanel::stepSequencer);
    };
    auto makePanelMetrics = [this, densityMode] (juce::Rectangle<int> area)
    {
        return beatmaker::layout::makeMetrics (area,
                                               getUiDensityScale(),
                                               densityMode == UiDensityMode::compact
                                                   ? beatmaker::layout::DensityMode::compact
                                                   : (densityMode == UiDensityMode::accessible
                                                          ? beatmaker::layout::DensityMode::accessible
                                                          : beatmaker::layout::DensityMode::comfortable),
                                               false);
    };
    auto layoutPianoRollEditor = [this, denseLayout] (juce::Rectangle<int> area)
    {
        if (area.isEmpty())
        {
            pianoRollToolbar.setBounds ({});
            midiPianoRoll.setBounds ({});
            pianoRollHorizontalScrollBar.setBounds ({});
            pianoRollVerticalScrollBar.setBounds ({});
            return;
        }

        const int minToolbarHeight = denseLayout ? 22 : 24;
        const int maxToolbarHeight = denseLayout ? 34 : 38;
        const int toolbarHeight = juce::jlimit (minToolbarHeight,
                                                maxToolbarHeight,
                                                juce::jmax (minToolbarHeight, area.getHeight() / 7));
        auto toolbarRow = area.removeFromTop (juce::jmin (toolbarHeight, area.getHeight()));
        const int toolbarGap = denseLayout ? 3 : 5;
        if (area.getHeight() > toolbarGap)
            area.removeFromTop (toolbarGap);

        pianoRollToolbar.setBounds (toolbarRow.reduced (0, 1));

        const int scrollThickness = denseLayout ? 12 : 14;
        auto editorArea = area;
        auto horizontalBar = editorArea.removeFromBottom (juce::jmin (scrollThickness, editorArea.getHeight()));
        if (editorArea.getHeight() > 2)
            editorArea.removeFromBottom (2);

        auto verticalBar = editorArea.removeFromRight (juce::jmin (scrollThickness, editorArea.getWidth()));
        if (editorArea.getWidth() > 2)
            editorArea.removeFromRight (2);

        pianoRollHorizontalScrollBar.setBounds (horizontalBar.withTrimmedRight (verticalBar.getWidth()));
        pianoRollVerticalScrollBar.setBounds (verticalBar);
        midiPianoRoll.setBounds (editorArea);
    };
    auto layoutStepSequencerEditor = [this, denseLayout] (juce::Rectangle<int> area)
    {
        if (area.isEmpty())
        {
            stepSequencerToolbar.setBounds ({});
            stepSequencer.setBounds ({});
            stepSequencerHorizontalScrollBar.setBounds ({});
            return;
        }

        const int minToolbarHeight = denseLayout ? 22 : 24;
        const int maxToolbarHeight = denseLayout ? 34 : 38;
        const int toolbarHeight = juce::jlimit (minToolbarHeight,
                                                maxToolbarHeight,
                                                juce::jmax (minToolbarHeight, area.getHeight() / 6));
        auto toolbarRow = area.removeFromTop (juce::jmin (toolbarHeight, area.getHeight()));
        const int toolbarGap = denseLayout ? 3 : 5;
        if (area.getHeight() > toolbarGap)
            area.removeFromTop (toolbarGap);

        stepSequencerToolbar.setBounds (toolbarRow.reduced (0, 1));

        const int scrollThickness = denseLayout ? 12 : 14;
        auto editorArea = area;
        auto horizontalBar = editorArea.removeFromBottom (juce::jmin (scrollThickness, editorArea.getHeight()));
        if (editorArea.getHeight() > 2)
            editorArea.removeFromBottom (2);

        stepSequencerHorizontalScrollBar.setBounds (horizontalBar);
        stepSequencer.setBounds (editorArea);
    };

    if (! allowPianoRollPanel && ! allowStepSequencerPanel)
    {
        pianoStepSplitter.setBounds ({});
        clearPianoRollDockBounds();
        clearStepSequencerDockBounds();
        updatePianoRollScrollbarsFromViewport();
        updateStepSequencerScrollbarFromPageContext();
        return;
    }

    if (allowPianoRollPanel && ! allowStepSequencerPanel)
    {
        pianoStepSplitter.setBounds ({});
        clearStepSequencerDockBounds();
        auto pianoArea = beatmaker::layout::groupContentPlain (pianoRollGroup, topSection, makePanelMetrics (topSection));
        layoutPianoRollEditor (pianoArea);
        updatePianoRollScrollbarsFromViewport();
        updateStepSequencerScrollbarFromPageContext();
        return;
    }

    if (! allowPianoRollPanel && allowStepSequencerPanel)
    {
        pianoStepSplitter.setBounds ({});
        clearPianoRollDockBounds();
        auto stepArea = beatmaker::layout::groupContentPlain (stepSequencerGroup, topSection, makePanelMetrics (topSection));
        layoutStepSequencerEditor (stepArea);
        updatePianoRollScrollbarsFromViewport();
        updateStepSequencerScrollbarFromPageContext();
        return;
    }

    if (effectiveViewMode == PianoEditorLayoutMode::pianoRoll)
    {
        pianoStepSplitter.setBounds ({});
        clearStepSequencerDockBounds();
        auto pianoArea = beatmaker::layout::groupContentPlain (pianoRollGroup, topSection, makePanelMetrics (topSection));
        layoutPianoRollEditor (pianoArea);
        updatePianoRollScrollbarsFromViewport();
        updateStepSequencerScrollbarFromPageContext();
        return;
    }

    if (effectiveViewMode == PianoEditorLayoutMode::stepSequencer)
    {
        pianoStepSplitter.setBounds ({});
        clearPianoRollDockBounds();
        auto stepArea = beatmaker::layout::groupContentPlain (stepSequencerGroup, topSection, makePanelMetrics (topSection));
        layoutStepSequencerEditor (stepArea);
        updatePianoRollScrollbarsFromViewport();
        updateStepSequencerScrollbarFromPageContext();
        return;
    }

    const int minStepHeight = juce::jmax (denseLayout ? 78 : 92, availableSplitHeight / 4);
    const int minPianoHeight = juce::jmax (denseLayout ? 120 : 140, availableSplitHeight / 3);

    currentPianoSectionHeightForResize = juce::jmax (1, availableSplitHeight);
    int stepHeight = roundToInt ((float) availableSplitHeight * pianoStepHeightRatio);
    stepHeight = juce::jlimit (minStepHeight,
                               juce::jmax (minStepHeight, availableSplitHeight - minPianoHeight),
                               stepHeight);
    pianoStepHeightRatio = (float) stepHeight / (float) juce::jmax (1, availableSplitHeight);

    auto stepBounds = topSection.removeFromTop (stepHeight);
    if (topSection.getHeight() > splitGap)
        topSection.removeFromTop (splitGap);

    if (! stepBounds.isEmpty() && ! topSection.isEmpty())
    {
        pianoStepSplitter.setBounds (stepBounds.getX(),
                                     stepBounds.getBottom(),
                                     stepBounds.getWidth(),
                                     splitGap);
        pianoStepSplitter.toFront (false);
    }
    else
    {
        pianoStepSplitter.setBounds ({});
    }

    const auto compactMetrics = makePanelMetrics (topSection);
    auto stepArea = beatmaker::layout::groupContentPlain (stepSequencerGroup, stepBounds, compactMetrics);
    auto pianoArea = beatmaker::layout::groupContentPlain (pianoRollGroup, topSection, compactMetrics);
    layoutStepSequencerEditor (stepArea);
    layoutPianoRollEditor (pianoArea);
    updatePianoRollScrollbarsFromViewport();
    updateStepSequencerScrollbarFromPageContext();
}
