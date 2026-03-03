# Sampledex DAW Professional Structure

This folder is a clean, production-style layout where DAW app source is separated from third-party dependencies.

## Layout

- `BeatMakerApp/Source/BeatMakerNoRecord` - Your DAW application code
- `BeatMakerApp/Source/common` - Shared example utility components used by the DAW UI
- `ThirdParty/tracktion_engine` - Tracktion Engine dependency sources (modules + CMake support)
- `ThirdParty/JUCE` - JUCE source used by CMake when available locally

The DAW is configured for third-party AU/VST3 instruments and effects only (no internal or bundled instruments/effects).

## Build

```bash
cd BeatMakerApp
./run_beatmaker.sh
```

This configures and builds `TheSampledexWorkflow` in `BeatMakerApp/build`.
