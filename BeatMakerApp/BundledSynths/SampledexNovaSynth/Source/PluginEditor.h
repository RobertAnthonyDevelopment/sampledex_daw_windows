#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SampledexNovaSynthAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                                private juce::ComboBox::Listener,
                                                private juce::Timer
{
public:
    explicit SampledexNovaSynthAudioProcessorEditor (SampledexNovaSynthAudioProcessor&);
    ~SampledexNovaSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    using APVTS = juce::AudioProcessorValueTreeState;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void timerCallback() override;
    void rebuildPresetMenu();

    SampledexNovaSynthAudioProcessor& audioProcessor;
    std::vector<Control> controls;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::StringArray cachedPresetNames;
    int lastKnownProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampledexNovaSynthAudioProcessorEditor)
};
