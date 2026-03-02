#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
inline float wrapPhase (float phase)
{
    while (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;

    while (phase < 0.0f)
        phase += juce::MathConstants<float>::twoPi;

    return phase;
}

inline float sawWave (float phase)
{
    return (phase / juce::MathConstants<float>::pi) - 1.0f;
}

inline float squareWave (float phase)
{
    return phase < juce::MathConstants<float>::pi ? 1.0f : -1.0f;
}

inline float lerp (float a, float b, float alpha)
{
    return a + (b - a) * alpha;
}
}

std::vector<SampledexNovaSynthAudioProcessor::Preset> SampledexNovaSynthAudioProcessor::createFactoryPresets()
{
    return {
        { "Nova Init",     0.35f,  2.0f,  5200.0f, 0.36f, 0.010f, 0.35f, 0.62f, 0.75f, 0.12f, 0.72f, 0.24f, 0.68f, 0.18f },
        { "Warm Pad",      0.48f,  6.0f,  3200.0f, 0.44f, 0.120f, 0.80f, 0.72f, 1.60f, 0.16f, 0.70f, 0.62f, 0.90f, 0.36f },
        { "Bright Pluck",  0.24f,  1.0f, 11000.0f, 0.28f, 0.001f, 0.11f, 0.22f, 0.18f, 0.08f, 0.74f, 0.14f, 0.36f, 0.10f },
        { "Deep Bass",     0.70f, -2.0f,  1200.0f, 0.58f, 0.003f, 0.22f, 0.52f, 0.28f, 0.30f, 0.78f, 0.08f, 0.20f, 0.00f },
        { "Wide Lead",     0.58f,  4.0f,  7600.0f, 0.32f, 0.004f, 0.20f, 0.64f, 0.42f, 0.24f, 0.72f, 0.46f, 1.00f, 0.28f }
    };
}

class SampledexNovaSynthAudioProcessor::NovaSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override      { return true; }
    bool appliesToChannel (int) override   { return true; }
};

class SampledexNovaSynthAudioProcessor::NovaVoice : public juce::SynthesiserVoice
{
public:
    explicit NovaVoice (const ParameterRefs& refsToUse)
        : refs (refsToUse)
    {
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<NovaSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        noteFrequencyHz = (float) juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        gain = juce::jlimit (0.0f, 1.0f, velocity);

        const float noteNormal = juce::jmap ((float) (midiNoteNumber % 12), 0.0f, 11.0f, -1.0f, 1.0f);
        voicePan = noteNormal * 0.35f;

        phaseA = 0.0f;
        phaseB = juce::MathConstants<float>::halfPi;
        lfoPhase = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
        lfoRateHz = juce::jmap (juce::Random::getSystemRandom().nextFloat(), 0.12f, 0.50f);

        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
            return;
        }

        adsr.reset();
        clearCurrentNote();
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (! isVoiceActive() || getSampleRate() <= 0.0)
            return;

        juce::ADSR::Parameters adsrParams;
        adsrParams.attack = juce::jmax (0.001f, refs.attackSeconds->load());
        adsrParams.decay = juce::jmax (0.001f, refs.decaySeconds->load());
        adsrParams.sustain = juce::jlimit (0.0f, 1.0f, refs.sustainLevel->load());
        adsrParams.release = juce::jmax (0.001f, refs.releaseSeconds->load());
        adsr.setParameters (adsrParams);

        const auto sr = (float) getSampleRate();
        const float mix = juce::jlimit (0.0f, 1.0f, refs.oscMix->load());
        const float detuneSemitones = refs.detuneSemitones->load();
        const float unisonDepth = juce::jlimit (0.0f, 1.0f, refs.unison->load());
        const float stereoWidth = juce::jlimit (0.0f, 1.0f, refs.stereoWidth->load());
        const float chorusDepth = juce::jlimit (0.0f, 1.0f, refs.chorus->load());
        const float driveAmount = juce::jlimit (0.0f, 1.0f, refs.drive->load());
        const float masterGain = juce::jlimit (0.0f, 1.0f, refs.master->load());

        const float pitchRatio = std::pow (2.0f, (detuneSemitones + unisonDepth * 10.0f) / 12.0f);
        const float freqA = noteFrequencyHz;
        const float freqB = juce::jlimit (20.0f, 22000.0f, noteFrequencyHz * pitchRatio);

        float cutoff = juce::jlimit (20.0f, 22000.0f, refs.cutoffHz->load());
        cutoff = juce::jmin (cutoff, sr * 0.45f);
        filter.setCutoffFrequency (cutoff);
        filter.setResonance (juce::jlimit (0.2f, 1.2f, refs.resonance->load()));

        const float drive = juce::jmap (driveAmount, 1.0f, 8.0f);
        const float level = masterGain * gain * 0.28f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float lfo = std::sin (lfoPhase);
            lfoPhase = wrapPhase (lfoPhase + juce::MathConstants<float>::twoPi * lfoRateHz / sr);

            const float chorusFreqOffset = 1.0f + (chorusDepth * 0.012f * lfo);
            const float phaseIncA = juce::MathConstants<float>::twoPi * freqA / sr;
            const float phaseIncB = juce::MathConstants<float>::twoPi * (freqB * chorusFreqOffset) / sr;

            phaseA = wrapPhase (phaseA + phaseIncA);
            phaseB = wrapPhase (phaseB + phaseIncB);

            const float oscA = sawWave (phaseA);
            const float oscB = squareWave (phaseB);
            float voiceSample = lerp (oscA, oscB, mix);

            voiceSample = filter.processSample (0, voiceSample);
            voiceSample = std::tanh (voiceSample * drive);
            voiceSample *= adsr.getNextSample() * level;

            const float pan = juce::jlimit (-1.0f, 1.0f, voicePan * stereoWidth);
            const float leftPan = std::sqrt (0.5f * (1.0f - pan));
            const float rightPan = std::sqrt (0.5f * (1.0f + pan));

            outputBuffer.addSample (0, startSample + sample, voiceSample * leftPan);

            if (outputBuffer.getNumChannels() > 1)
                outputBuffer.addSample (1, startSample + sample, voiceSample * rightPan);
        }

        if (! adsr.isActive())
            clearCurrentNote();
    }

private:
    const ParameterRefs& refs;

    juce::ADSR adsr;
    juce::dsp::StateVariableTPTFilter<float> filter;

    float noteFrequencyHz = 440.0f;
    float phaseA = 0.0f;
    float phaseB = 0.0f;
    float lfoPhase = 0.0f;
    float lfoRateHz = 0.25f;
    float gain = 1.0f;
    float voicePan = 0.0f;
};

SampledexNovaSynthAudioProcessor::SampledexNovaSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "NovaSynthParams", createParameterLayout()),
      factoryPresets (createFactoryPresets())
{
    paramRefs.oscMix = parameters.getRawParameterValue (SampledexNovaSynthParams::oscMix);
    paramRefs.detuneSemitones = parameters.getRawParameterValue (SampledexNovaSynthParams::detune);
    paramRefs.cutoffHz = parameters.getRawParameterValue (SampledexNovaSynthParams::cutoff);
    paramRefs.resonance = parameters.getRawParameterValue (SampledexNovaSynthParams::resonance);
    paramRefs.attackSeconds = parameters.getRawParameterValue (SampledexNovaSynthParams::attack);
    paramRefs.decaySeconds = parameters.getRawParameterValue (SampledexNovaSynthParams::decay);
    paramRefs.sustainLevel = parameters.getRawParameterValue (SampledexNovaSynthParams::sustain);
    paramRefs.releaseSeconds = parameters.getRawParameterValue (SampledexNovaSynthParams::release);
    paramRefs.drive = parameters.getRawParameterValue (SampledexNovaSynthParams::drive);
    paramRefs.master = parameters.getRawParameterValue (SampledexNovaSynthParams::master);
    paramRefs.unison = parameters.getRawParameterValue (SampledexNovaSynthParams::unison);
    paramRefs.stereoWidth = parameters.getRawParameterValue (SampledexNovaSynthParams::stereoWidth);
    paramRefs.chorus = parameters.getRawParameterValue (SampledexNovaSynthParams::chorus);

    synth.clearVoices();
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new NovaVoice (paramRefs));

    synth.clearSounds();
    synth.addSound (new NovaSound());

    refreshPresetNameCache();
    if (! factoryPresets.empty())
        applyPreset (factoryPresets.front(), false);
}

SampledexNovaSynthAudioProcessor::~SampledexNovaSynthAudioProcessor() = default;

void SampledexNovaSynthAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    synth.allNotesOff (0, false);
    keyboardState.reset();
}

void SampledexNovaSynthAudioProcessor::releaseResources()
{
    synth.allNotesOff (0, false);
    keyboardState.reset();
}

bool SampledexNovaSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    if (output.isDisabled())
        return false;

    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void SampledexNovaSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);
    buffer.clear();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

double SampledexNovaSynthAudioProcessor::getTailLengthSeconds() const
{
    return juce::jmax (0.05, (double) parameters.getRawParameterValue (SampledexNovaSynthParams::release)->load());
}

int SampledexNovaSynthAudioProcessor::getNumPrograms()
{
    return juce::jmax (1, (int) factoryPresets.size());
}

int SampledexNovaSynthAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void SampledexNovaSynthAudioProcessor::setCurrentProgram (int index)
{
    if (factoryPresets.empty())
        return;

    const int clamped = juce::jlimit (0, getNumPrograms() - 1, index);
    currentProgram = clamped;
    applyPreset (factoryPresets[(size_t) clamped], true);
}

const juce::String SampledexNovaSynthAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= presetNames.size())
        return {};

    return presetNames[index];
}

void SampledexNovaSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    if (index < 0 || (size_t) index >= factoryPresets.size())
        return;

    const auto trimmed = newName.trim();
    if (trimmed.isEmpty())
        return;

    factoryPresets[(size_t) index].name = trimmed;
    refreshPresetNameCache();
}

void SampledexNovaSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("currentProgram", currentProgram, nullptr);

    for (int i = 0; i < presetNames.size(); ++i)
        state.setProperty ("programName" + juce::String (i), presetNames[i], nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SampledexNovaSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    if (! xmlState->hasTagName (parameters.state.getType()))
        return;

    auto restored = juce::ValueTree::fromXml (*xmlState);
    parameters.replaceState (restored);

    for (size_t i = 0; i < factoryPresets.size(); ++i)
    {
        const auto key = "programName" + juce::String ((int) i);
        if (restored.hasProperty (key))
            factoryPresets[i].name = restored[key].toString();
    }

    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) restored.getProperty ("currentProgram", 0));
    refreshPresetNameCache();
}

juce::AudioProcessorEditor* SampledexNovaSynthAudioProcessor::createEditor()
{
    return new SampledexNovaSynthAudioProcessorEditor (*this);
}

SampledexNovaSynthAudioProcessor::APVTS::ParameterLayout SampledexNovaSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::oscMix, "OSC Mix", 0.0f, 1.0f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::detune, "Detune", -12.0f, 12.0f, 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        SampledexNovaSynthParams::cutoff, "Cutoff", juce::NormalisableRange<float> (40.0f, 18000.0f, 0.01f, 0.28f), 5200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::resonance, "Resonance", 0.2f, 1.2f, 0.36f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::attack, "Attack", 0.001f, 2.0f, 0.010f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::decay, "Decay", 0.01f, 3.0f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::sustain, "Sustain", 0.0f, 1.0f, 0.62f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::release, "Release", 0.01f, 4.0f, 0.75f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::drive, "Drive", 0.0f, 1.0f, 0.12f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::master, "Master", 0.0f, 1.0f, 0.72f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::unison, "Unison", 0.0f, 1.0f, 0.24f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::stereoWidth, "Stereo Width", 0.0f, 1.0f, 0.68f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (SampledexNovaSynthParams::chorus, "Chorus", 0.0f, 1.0f, 0.18f));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SampledexNovaSynthAudioProcessor();
}

void SampledexNovaSynthAudioProcessor::setParameterValue (const juce::String& paramID, float value, bool notifyHost)
{
    if (auto* param = parameters.getParameter (paramID))
    {
        const auto normalised = param->convertTo0to1 (value);

        if (notifyHost)
            param->beginChangeGesture();

        if (notifyHost)
            param->setValueNotifyingHost (normalised);
        else
            param->setValue (normalised);

        if (notifyHost)
            param->endChangeGesture();
    }
}

void SampledexNovaSynthAudioProcessor::applyPreset (const Preset& preset, bool notifyHost)
{
    setParameterValue (SampledexNovaSynthParams::oscMix, preset.oscMix, notifyHost);
    setParameterValue (SampledexNovaSynthParams::detune, preset.detune, notifyHost);
    setParameterValue (SampledexNovaSynthParams::cutoff, preset.cutoff, notifyHost);
    setParameterValue (SampledexNovaSynthParams::resonance, preset.resonance, notifyHost);
    setParameterValue (SampledexNovaSynthParams::attack, preset.attack, notifyHost);
    setParameterValue (SampledexNovaSynthParams::decay, preset.decay, notifyHost);
    setParameterValue (SampledexNovaSynthParams::sustain, preset.sustain, notifyHost);
    setParameterValue (SampledexNovaSynthParams::release, preset.release, notifyHost);
    setParameterValue (SampledexNovaSynthParams::drive, preset.drive, notifyHost);
    setParameterValue (SampledexNovaSynthParams::master, preset.master, notifyHost);
    setParameterValue (SampledexNovaSynthParams::unison, preset.unison, notifyHost);
    setParameterValue (SampledexNovaSynthParams::stereoWidth, preset.stereoWidth, notifyHost);
    setParameterValue (SampledexNovaSynthParams::chorus, preset.chorus, notifyHost);
}

void SampledexNovaSynthAudioProcessor::refreshPresetNameCache()
{
    presetNames.clearQuick();
    for (const auto& preset : factoryPresets)
        presetNames.add (preset.name);
}
