# Sampledex DAW (Windows Prototype)

![Made with Codex](https://img.shields.io/badge/Made%20with-Codex-0A7CFF?style=flat-square)
![Built with JUCE](https://img.shields.io/badge/Built%20with-JUCE-8A2BE2?style=flat-square)
![Powered by Tracktion Engine](https://img.shields.io/badge/Powered%20by-Tracktion%20Engine-2F8D46?style=flat-square)

<img width="1919" height="962" alt="Screenshot 2026-03-03 at 2 42 36 PM" src="https://github.com/user-attachments/assets/cd4d41cf-75c1-4c1c-8f48-6e0c26f616d2" />
<img width="1919" height="962" alt="Screenshot 2026-03-03 at 2 42 45 PM" src="https://github.com/user-attachments/assets/66c047f4-5543-4e51-8bc3-671d3d714240" />
<img width="1919" height="962" alt="Screenshot 2026-03-03 at 2 43 47 PM" src="https://github.com/user-attachments/assets/e9f73461-68d1-49f2-9b99-bee69321da24" />

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

## License

This repository is licensed under MIT (see `LICENSE`), except third-party code
in `ThirdParty/`, which remains under its original licenses.
