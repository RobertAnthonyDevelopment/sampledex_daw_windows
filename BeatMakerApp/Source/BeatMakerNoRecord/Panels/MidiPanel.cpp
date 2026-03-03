#include "../BeatMakerNoRecord.h"
#include "SharedLayoutSystem.h"

namespace
{
beatmaker::layout::DensityMode getMidiDensity (int densityMode)
{
    if (densityMode == 0)
        return beatmaker::layout::DensityMode::compact;
    if (densityMode == 2)
        return beatmaker::layout::DensityMode::accessible;
    return beatmaker::layout::DensityMode::comfortable;
}

void layoutLabelComboCell (juce::Rectangle<int>& row, juce::Label& label, juce::ComboBox& combo, int labelWidth)
{
    label.setBounds (row.removeFromLeft (labelWidth).reduced (0, 1));
    row.removeFromLeft (4);
    combo.setBounds (row.reduced (0, 1));
}

void layoutLabelSliderCell (juce::Rectangle<int>& row, juce::Label& label, juce::Slider& slider, int labelWidth)
{
    label.setBounds (row.removeFromLeft (labelWidth).reduced (0, 1));
    row.removeFromLeft (4);
    slider.setBounds (row.reduced (0, 1));
}
}

void BeatMakerNoRecord::setupMidiCallbacks()
{
    copyButton.onClick = [this] { copySelection(); };
    cutButton.onClick = [this] { cutSelection(); };
    pasteButton.onClick = [this] { pasteSelection(); };
    deleteButton.onClick = [this] { deleteSelectedItem(); };
    duplicateButton.onClick = [this] { duplicateSelectedClip(); };
    splitButton.onClick = [this] { splitSelectedClipAtPlayhead(); };
    trimStartButton.onClick = [this] { trimSelectedClipStartToPlayhead(); };
    trimEndButton.onClick = [this] { trimSelectedClipEndToPlayhead(); };
    moveStartToCursorButton.onClick = [this] { moveSelectedClipBoundaryToCursor (true); };
    moveEndToCursorButton.onClick = [this] { moveSelectedClipBoundaryToCursor (false); };
    nudgeLeftButton.onClick = [this] { nudgeSelectedClip (false); };
    nudgeRightButton.onClick = [this] { nudgeSelectedClip (true); };
    slipLeftButton.onClick = [this] { slipSelectedClipContent (false); };
    slipRightButton.onClick = [this] { slipSelectedClipContent (true); };
    moveToPrevButton.onClick = [this] { moveSelectedClipToNeighbour (false); };
    moveToNextButton.onClick = [this] { moveSelectedClipToNeighbour (true); };
    toggleClipLoopButton.onClick = [this] { toggleSelectedClipLooping(); };
    renameClipButton.onClick = [this] { renameSelectedClip(); };
    selectAllButton.onClick = [this] { selectAllEditableItems(); };
    deselectAllButton.onClick = [this] { selectionManager.deselectAll(); };
    splitAllTracksButton.onClick = [this] { splitAllTracksAtPlayhead(); };
    insertBarButton.onClick = [this] { insertBarAtPlayhead(); };
    deleteBarButton.onClick = [this] { deleteBarAtPlayhead(); };

    quantizeButton.onClick = [this] { quantizeSelectedMidiClip(); };
    midiTransposeDownButton.onClick = [this] { transposeSelectedMidiNotes (-1); };
    midiTransposeUpButton.onClick = [this] { transposeSelectedMidiNotes (1); };
    midiOctaveDownButton.onClick = [this] { transposeSelectedMidiNotes (-12); };
    midiOctaveUpButton.onClick = [this] { transposeSelectedMidiNotes (12); };
    midiVelocityDownButton.onClick = [this] { adjustSelectedMidiNoteVelocity (-8); };
    midiVelocityUpButton.onClick = [this] { adjustSelectedMidiNoteVelocity (8); };
    midiHumanizeTimingButton.onClick = [this] { humanizeSelectedMidiTiming (0.08); };
    midiHumanizeVelocityButton.onClick = [this] { humanizeSelectedMidiVelocity (10); };
    midiLegatoButton.onClick = [this] { legatoSelectedMidiNotes(); };
    midiBounceToAudioButton.onClick = [this] { bounceSelectedMidiClipToAudio(); };
    midiGenerateChordsButton.onClick = [this] { generateMidiChordProgression(); };
    midiGenerateArpButton.onClick = [this] { generateMidiArpeggioPattern(); };
    midiGenerateBassButton.onClick = [this] { generateMidiBasslinePattern(); };
    midiGenerateDrumsButton.onClick = [this] { generateMidiDrumPattern(); };
    chordDirectoryPreviewButton.onClick = [this] { previewChordScaleDirectoryPattern(); };
    chordDirectoryApplyButton.onClick = [this] { generateMidiChordScaleDirectoryPattern(); };
    chordDirectoryExportMidiButton.onClick = [this] { exportChordScaleDirectorySelectionAsMidi(); };
    chordDirectoryExportWavButton.onClick = [this] { exportChordScaleDirectorySelectionAsWav(); };
}

void BeatMakerNoRecord::layoutMidiPanel (juce::Rectangle<int> bounds, bool detachedMode)
{
    const int densityOrdinal = uiDensityMode == UiDensityMode::compact ? 0
                             : (uiDensityMode == UiDensityMode::accessible ? 2 : 1);
    auto metrics = beatmaker::layout::makeMetrics (bounds,
                                                   getUiDensityScale(),
                                                   getMidiDensity (densityOrdinal),
                                                   detachedMode);
    auto area = beatmaker::layout::groupContent (midiEditGroup,
                                                 beatmaker::layout::insetSection (bounds, metrics),
                                                 metrics);

    midiToolsTabs.setBounds (beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2).reduced (0, 1));

    if (midiToolsTabs.getCurrentTabIndex() != 1)
    {
        auto row1 = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
        if (! detachedMode && row1.getWidth() < 420)
        {
            auto left = row1.removeFromLeft (juce::jmax (110, row1.getWidth() / 2));
            quantizeTypeBox.setBounds (left.removeFromLeft (juce::jmax (72, left.getWidth() - 76)).reduced (0, 1));
            left.removeFromLeft (4);
            quantizeButton.setBounds (left.reduced (0, 1));

            gridLabel.setBounds (row1.removeFromLeft (34).reduced (0, 1));
            row1.removeFromLeft (4);
            gridBox.setBounds (row1.reduced (0, 1));

            beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                             beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 2, 2)),
                                                metrics,
                                                { &midiLegatoButton, &midiBounceToAudioButton });
        }
        else
        {
            const int quantizeTypeWidth = juce::jmax (96, row1.getWidth() / 4);
            quantizeTypeBox.setBounds (row1.removeFromLeft (quantizeTypeWidth).reduced (0, 1));
            row1.removeFromLeft (6);
            quantizeButton.setBounds (row1.removeFromLeft (juce::jmax (72, row1.getWidth() / 6)).reduced (0, 1));
            row1.removeFromLeft (6);
            gridLabel.setBounds (row1.removeFromLeft (34).reduced (0, 1));
            row1.removeFromLeft (6);
            gridBox.setBounds (row1.removeFromLeft (juce::jmax (78, row1.getWidth() / 4)).reduced (0, 1));
            row1.removeFromLeft (6);
            const int utilityWidth = juce::jmax (0, (row1.getWidth() - 6) / 2);
            midiLegatoButton.setBounds (row1.removeFromLeft (utilityWidth).reduced (0, 1));
            if (row1.getWidth() > 6)
                row1.removeFromLeft (6);
            midiBounceToAudioButton.setBounds (row1.reduced (0, 1));
        }

        beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                         beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 6, 2)),
                                            metrics,
                                            { &midiTransposeDownButton, &midiTransposeUpButton,
                                              &midiOctaveDownButton, &midiOctaveUpButton,
                                              &midiVelocityDownButton, &midiVelocityUpButton });
        beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                         beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 2, 2)),
                                            metrics,
                                            { &midiHumanizeTimingButton, &midiHumanizeVelocityButton });
        beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                         beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 4, 2)),
                                            metrics,
                                            { &midiGenerateChordsButton, &midiGenerateArpButton,
                                              &midiGenerateBassButton, &midiGenerateDrumsButton });
        return;
    }

    if (! detachedMode && area.getWidth() < 520)
    {
        auto rowA = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
        auto rowALeft = rowA.removeFromLeft (juce::jmax (160, rowA.getWidth() / 2));
        rowA.removeFromLeft (6);
        layoutLabelComboCell (rowALeft, chordDirectoryRootLabel, chordDirectoryRootBox, 46);
        layoutLabelComboCell (rowA, chordDirectoryScaleLabel, chordDirectoryScaleBox, 46);

        auto rowB = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
        layoutLabelComboCell (rowB, chordDirectoryProgressionLabel, chordDirectoryProgressionBox, 90);
    }
    else
    {
        auto rowA = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
        auto keyCell = rowA.removeFromLeft (juce::jmax (130, rowA.getWidth() / 5));
        rowA.removeFromLeft (6);
        auto scaleCell = rowA.removeFromLeft (juce::jmax (170, rowA.getWidth() / 3));
        rowA.removeFromLeft (6);
        auto progressionCell = rowA;
        layoutLabelComboCell (keyCell, chordDirectoryRootLabel, chordDirectoryRootBox, 46);
        layoutLabelComboCell (scaleCell, chordDirectoryScaleLabel, chordDirectoryScaleBox, 46);
        layoutLabelComboCell (progressionCell, chordDirectoryProgressionLabel, chordDirectoryProgressionBox, 92);
    }

    auto rowB = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
    auto barsCell = rowB.removeFromLeft (juce::jmax (112, rowB.getWidth() / 4));
    rowB.removeFromLeft (6);
    auto sigCell = rowB.removeFromLeft (juce::jmax (138, rowB.getWidth() / 3));
    rowB.removeFromLeft (6);
    auto octCell = rowB;
    layoutLabelComboCell (barsCell, chordDirectoryBarsLabel, chordDirectoryBarsBox, 42);
    layoutLabelComboCell (sigCell, chordDirectoryTimeSignatureLabel, chordDirectoryTimeSignatureBox, 58);
    layoutLabelComboCell (octCell, chordDirectoryOctaveLabel, chordDirectoryOctaveBox, 54);

    auto rowC = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
    if (! detachedMode && rowC.getWidth() < 430)
    {
        auto leftCell = rowC.removeFromLeft (juce::jmax (128, rowC.getWidth() / 2));
        rowC.removeFromLeft (6);
        layoutLabelComboCell (leftCell, chordDirectoryVoicingLabel, chordDirectoryVoicingBox, 58);
        layoutLabelComboCell (rowC, chordDirectoryDensityLabel, chordDirectoryDensityBox, 58);

        auto rowCPreset = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
        layoutLabelComboCell (rowCPreset, chordDirectoryPreviewPresetLabel, chordDirectoryPreviewPresetBox, 96);
    }
    else
    {
        auto voicingCell = rowC.removeFromLeft (juce::jmax (132, rowC.getWidth() / 3));
        rowC.removeFromLeft (6);
        auto densityCell = rowC.removeFromLeft (juce::jmax (132, rowC.getWidth() / 3));
        rowC.removeFromLeft (6);
        auto presetCell = rowC;
        layoutLabelComboCell (voicingCell, chordDirectoryVoicingLabel, chordDirectoryVoicingBox, 58);
        layoutLabelComboCell (densityCell, chordDirectoryDensityLabel, chordDirectoryDensityBox, 58);
        layoutLabelComboCell (presetCell, chordDirectoryPreviewPresetLabel, chordDirectoryPreviewPresetBox, 96);
    }

    auto rowD = beatmaker::layout::nextRow (area, metrics, metrics.defaultRowHeight + 2);
    auto velocityCell = rowD.removeFromLeft (juce::jmax (150, rowD.getWidth() / 2));
    rowD.removeFromLeft (6);
    layoutLabelSliderCell (velocityCell, chordDirectoryVelocityLabel, chordDirectoryVelocitySlider, 64);
    layoutLabelSliderCell (rowD, chordDirectorySwingLabel, chordDirectorySwingSlider, 54);

    if (! detachedMode && area.getWidth() < 460)
    {
        beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                         beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 2, 2)),
                                            metrics,
                                            { &chordDirectoryPreviewButton, &chordDirectoryApplyButton });
        beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                         beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 2, 2)),
                                            metrics,
                                            { &chordDirectoryExportMidiButton, &chordDirectoryExportWavButton });
    }
    else
    {
        beatmaker::layout::layoutButtonRow (beatmaker::layout::nextRow (area, metrics,
                                                                         beatmaker::layout::adaptiveButtonRowHeight (metrics, area.getWidth(), 4, 2)),
                                            metrics,
                                            { &chordDirectoryPreviewButton, &chordDirectoryApplyButton,
                                              &chordDirectoryExportMidiButton, &chordDirectoryExportWavButton });
    }
}
