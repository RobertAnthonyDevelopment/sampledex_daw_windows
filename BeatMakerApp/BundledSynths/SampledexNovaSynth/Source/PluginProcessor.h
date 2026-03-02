#pragma once

#include <JuceHeader.h>

namespace SampledexNovaSynthParams
{
inline constexpr auto oscMix = "oscMix";
inline constexpr auto detune = "detune";
inline constexpr auto cutoff = "cutoff";
inline constexpr auto resonance = "resonance";
inline constexpr auto attack = "attack";
inline constexpr auto decay = "decay";
inline constexpr auto sustain = "sustain";
inline constexpr auto release = "release";
inline constexpr auto drive = "drive";
inline constexpr auto master = "master";
inline constexpr auto unison = "unison";
inline constexpr auto stereoWidth = "stereoWidth";
inline constexpr auto chorus = "chorus";
}

class SampledexNovaSynthAudioProcessor  : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Preset
    {
        juce::String name;
        float oscMix = 0.35f;
        float detune = 2.0f;
        float cutoff = 5200.0f;
        float resonance = 0.36f;
        float attack = 0.01f;
        float decay = 0.35f;
        float sustain = 0.62f;
        float release = 0.75f;
        float drive = 0.12f;
        float master = 0.72f;
        float unison = 0.24f;
        float stereoWidth = 0.68f;
        float chorus = 0.18f;
    };

    SampledexNovaSynthAudioProcessor();
    ~SampledexNovaSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sampledex Nova Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    APVTS parameters;
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    const juce::StringArray& getPresetNames() const noexcept { return presetNames; }

    static APVTS::ParameterLayout createParameterLayout();

private:
    struct ParameterRefs
    {
        std::atomic<float>* oscMix = nullptr;
        std::atomic<float>* detuneSemitones = nullptr;
        std::atomic<float>* cutoffHz = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* attackSeconds = nullptr;
        std::atomic<float>* decaySeconds = nullptr;
        std::atomic<float>* sustainLevel = nullptr;
        std::atomic<float>* releaseSeconds = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* master = nullptr;
        std::atomic<float>* unison = nullptr;
        std::atomic<float>* stereoWidth = nullptr;
        std::atomic<float>* chorus = nullptr;
    };

    class NovaSound;
    class NovaVoice;

    static std::vector<Preset> createFactoryPresets();
    void applyPreset (const Preset&, bool notifyHost);
    void setParameterValue (const juce::String& paramID, float value, bool notifyHost);
    void refreshPresetNameCache();

    ParameterRefs paramRefs;
    juce::Synthesiser synth;
    juce::MidiKeyboardState keyboardState;
    std::vector<Preset> factoryPresets;
    juce::StringArray presetNames;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampledexNovaSynthAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
