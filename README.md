# Sampledex DAW Professional Structure

This folder is a clean, production-style layout where DAW app source is separated from third-party dependencies.

## Layout

- `BeatMakerApp/Source/BeatMakerNoRecord` - Your DAW application code
- `BeatMakerApp/Source/common` - Shared example utility components used by the DAW UI
- `BeatMakerApp/BundledSynths/SampledexNovaSynth` - Bundled synth plugin source
- `ThirdParty/tracktion_engine` - Tracktion Engine dependency sources (modules + CMake support)
- `ThirdParty/JUCE` - JUCE source used by CMake when available locally

## Build

```bash
cd BeatMakerApp
./run_beatmaker.sh
```

This configures and builds `TheSampledexWorkflow` in `BeatMakerApp/build`.
