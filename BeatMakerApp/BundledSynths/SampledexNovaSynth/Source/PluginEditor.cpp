#include "PluginEditor.h"

SampledexNovaSynthAudioProcessorEditor::SampledexNovaSynthAudioProcessorEditor (SampledexNovaSynthAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      keyboardComponent (audioProcessor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    auto addControl = [this] (const juce::String& name, const juce::String& paramID)
    {
        Control control;
        control.label = std::make_unique<juce::Label>();
        control.slider = std::make_unique<juce::Slider>();

        control.label->setText (name, juce::dontSendNotification);
        control.label->setJustificationType (juce::Justification::centred);

        control.slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        control.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        control.slider->setPopupDisplayEnabled (true, true, this);

        control.attachment = std::make_unique<APVTS::SliderAttachment> (audioProcessor.parameters, paramID, *control.slider);

        addAndMakeVisible (*control.label);
        addAndMakeVisible (*control.slider);
        controls.push_back (std::move (control));
    };

    addControl ("OSC Mix", SampledexNovaSynthParams::oscMix);
    addControl ("Detune", SampledexNovaSynthParams::detune);
    addControl ("Cutoff", SampledexNovaSynthParams::cutoff);
    addControl ("Resonance", SampledexNovaSynthParams::resonance);
    addControl ("Attack", SampledexNovaSynthParams::attack);
    addControl ("Decay", SampledexNovaSynthParams::decay);
    addControl ("Sustain", SampledexNovaSynthParams::sustain);
    addControl ("Release", SampledexNovaSynthParams::release);
    addControl ("Drive", SampledexNovaSynthParams::drive);
    addControl ("Master", SampledexNovaSynthParams::master);
    addControl ("Unison", SampledexNovaSynthParams::unison);
    addControl ("Stereo", SampledexNovaSynthParams::stereoWidth);
    addControl ("Chorus", SampledexNovaSynthParams::chorus);

    titleLabel.setText ("Sampledex Nova Synth", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::Font (juce::FontOptions (27.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("by sampledex.com | Bundled instrument for TheSampledexWorkflow", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::plain)));
    addAndMakeVisible (subtitleLabel);

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);

    presetBox.addListener (this);
    presetBox.setTooltip ("Factory presets for fast sound-starting points.");
    rebuildPresetMenu();
    addAndMakeVisible (presetBox);

    keyboardComponent.setAvailableRange (24, 108);
    keyboardComponent.setKeyWidth (18.0f);
    keyboardComponent.setScrollButtonsVisible (true);
    keyboardComponent.setWantsKeyboardFocus (true);
    addAndMakeVisible (keyboardComponent);
    setWantsKeyboardFocus (true);

    startTimerHz (12);
    setSize (920, 460);
}

SampledexNovaSynthAudioProcessorEditor::~SampledexNovaSynthAudioProcessorEditor()
{
    stopTimer();
    presetBox.removeListener (this);
}

void SampledexNovaSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour::fromRGB (20, 26, 35),
                             0.0f,
                             0.0f,
                             juce::Colour::fromRGB (8, 12, 20),
                             (float) getWidth(),
                             (float) getHeight(),
                             false);
    bg.addColour (0.54, juce::Colour::fromRGB (24, 40, 64));
    g.setGradientFill (bg);
    g.fillAll();

    auto panel = getLocalBounds().reduced (16, 14).toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (panel, 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (panel, 14.0f, 1.0f);
}

void SampledexNovaSynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (24, 16);

    auto header = bounds.removeFromTop (56);
    titleLabel.setBounds (header.removeFromTop (32));
    subtitleLabel.setBounds (header);

    bounds.removeFromTop (8);
    auto presetRow = bounds.removeFromTop (28);
    presetLabel.setBounds (presetRow.removeFromLeft (64));
    presetBox.setBounds (presetRow.removeFromLeft (300));
    bounds.removeFromTop (8);

    auto keyboardArea = bounds.removeFromBottom (88);
    keyboardComponent.setBounds (keyboardArea);
    bounds.removeFromBottom (8);

    constexpr int columns = 7;
    constexpr int rowGap = 10;
    constexpr int colGap = 8;
    constexpr int labelHeight = 18;

    const int numRows = juce::jmax (1, (int) std::ceil ((double) controls.size() / (double) columns));
    const int rowHeight = juce::jmax (112, (bounds.getHeight() - (numRows - 1) * rowGap) / numRows);
    const int colWidth = juce::jmax (94, (bounds.getWidth() - (columns - 1) * colGap) / columns);

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const int row = (int) i / columns;
        const int col = (int) i % columns;

        const int x = bounds.getX() + col * (colWidth + colGap);
        const int y = bounds.getY() + row * (rowHeight + rowGap);
        auto cell = juce::Rectangle<int> (x, y, colWidth, rowHeight);

        controls[i].label->setBounds (cell.removeFromTop (labelHeight));
        controls[i].slider->setBounds (cell);
    }
}

void SampledexNovaSynthAudioProcessorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged != &presetBox)
        return;

    const int selectedPreset = presetBox.getSelectedItemIndex();
    if (selectedPreset < 0)
        return;

    if (selectedPreset == audioProcessor.getCurrentProgram())
        return;

    audioProcessor.setCurrentProgram (selectedPreset);
    lastKnownProgram = selectedPreset;
}

void SampledexNovaSynthAudioProcessorEditor::timerCallback()
{
    rebuildPresetMenu();

    const int currentProgram = audioProcessor.getCurrentProgram();
    if (currentProgram == lastKnownProgram)
        return;

    lastKnownProgram = currentProgram;
    presetBox.setSelectedItemIndex (currentProgram, juce::dontSendNotification);
}

void SampledexNovaSynthAudioProcessorEditor::rebuildPresetMenu()
{
    const auto& presetNames = audioProcessor.getPresetNames();
    if (presetNames == cachedPresetNames)
        return;

    cachedPresetNames = presetNames;
    presetBox.clear (juce::dontSendNotification);

    int presetItemId = 1;
    for (const auto& presetName : cachedPresetNames)
        presetBox.addItem (presetName, presetItemId++);

    lastKnownProgram = audioProcessor.getCurrentProgram();
    presetBox.setSelectedItemIndex (lastKnownProgram, juce::dontSendNotification);
}
