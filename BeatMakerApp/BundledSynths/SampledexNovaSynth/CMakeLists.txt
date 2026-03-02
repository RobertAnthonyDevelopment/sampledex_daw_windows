cmake_minimum_required(VERSION 3.15)

set (SAMPLEDEX_NOVA_SYNTH_TARGET SampledexNovaSynth)

juce_add_plugin(${SAMPLEDEX_NOVA_SYNTH_TARGET}
    PRODUCT_NAME "Sampledex Nova Synth"
    PLUGIN_NAME "Sampledex Nova Synth"
    DESCRIPTION "Sampledex Nova Synth by sampledex.com"
    VERSION "1.0.0"
    BUILD_VERSION "1.0.0"
    COMPANY_NAME "Sampledex"
    COMPANY_WEBSITE "https://sampledex.com"
    COMPANY_EMAIL "support@sampledex.com"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Sampledex. All rights reserved."
    BUNDLE_ID "com.sampledex.plugins.novasynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_SANDBOX_SAFE TRUE
    AU_MAIN_TYPE "kAudioUnitType_MusicDevice"
    AU_EXPORT_PREFIX "SampledexNovaSynth"
    VST3_CATEGORIES Instrument Synth
    AAX_IDENTIFIER "com.sampledex.plugins.novasynth.aax"
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE "Smdx"
    PLUGIN_CODE "Nvas"
    FORMATS AU VST3 Standalone
    )

juce_generate_juce_header(${SAMPLEDEX_NOVA_SYNTH_TARGET})

target_compile_features(${SAMPLEDEX_NOVA_SYNTH_TARGET} PRIVATE cxx_std_20)

target_sources(${SAMPLEDEX_NOVA_SYNTH_TARGET}
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp)

target_compile_definitions(${SAMPLEDEX_NOVA_SYNTH_TARGET}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(${SAMPLEDEX_NOVA_SYNTH_TARGET}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
