# Sampledex DAW (Windows Prototype)

This repository is an active prototype, not a finished DAW release.

## Current Status

- Prototype quality: expect incomplete workflows and occasional breakage.
- Not fully functional yet: core areas are still under active refactor.
- This build targets third-party plugins only (VST3/AU hosting). There are no internal instruments or internal effects.

## Known Drawbacks

- Some UI workflows are still unstable (especially around complex panel/floating layouts).
- Piano roll, step sequencing, and arrangement interactions are still being iterated and may regress.
- Build stability depends on third-party dependency compatibility and may require updates over time.
- Session behavior and defaults are still evolving and are not guaranteed to be stable between revisions.

## Run From GitHub Actions (Recommended)

1. Open the repository Actions tab.
2. Run the `Windows Manual Build` workflow with `Release`.
3. Download the artifact named `TheSampledexWorkflow-Release`.
4. Extract and run `TheSampledexWorkflow.exe`.

## Build Locally on Windows

### Prerequisites

- Windows 10/11
- Visual Studio 2022 with Desktop development for C++
- CMake 3.21 or newer
- Git

### Build Commands

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022-release --config Release
ctest --preset windows-vs2022-release-tests --output-on-failure -C Release
```

The executable is produced under `build/windows-vs2022` (search for `TheSampledexWorkflow.exe`).

## Repository Layout

- `BeatMakerApp/Source/BeatMakerNoRecord` - DAW application code
- `BeatMakerApp/Source/common` - shared UI and helper components
- `ThirdParty/tracktion_engine` - Tracktion Engine sources and CMake support
- `ThirdParty/JUCE` - JUCE sources (used when local JUCE CMake support is complete)
