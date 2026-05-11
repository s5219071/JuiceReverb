# BeatRepeater

BeatRepeater is a JUCE-based Windows/macOS VST3 audio effect plugin.

Core features:

- Host BPM and PPQ synced beat repeat
- Grid length selection from 1/1 to 1/32
- Circular buffer based audio capture and repeat playback
- Softness/Warmth control using short crossfade and low-pass filtering
- Dark neon blue plugin editor UI
- GitHub Actions workflow for Windows and macOS VST3 cloud builds

## Repository Layout

```text
BeatRepeater/
├─ CMakeLists.txt
├─ Source/
│  ├─ PluginProcessor.h
│  ├─ PluginProcessor.cpp
│  ├─ PluginEditor.h
│  └─ PluginEditor.cpp
└─ .github/
   └─ workflows/
      └─ build.yml
```

## Cloud Build and Release

Open the repository on GitHub, go to `Actions`, choose `Build and Release VST3`,
then press `Run workflow`.

The workflow now builds:

- `BeatRepeater-Windows-VST3.zip`
- `BeatRepeater-macOS-Universal-VST3.zip`

To publish both builds as a GitHub Release, push a version tag such as:

```bash
git tag v1.0.0
git push origin v1.0.0
```

macOS note:

The macOS VST3 build is ad-hoc signed in GitHub Actions. For commercial public
distribution without Gatekeeper warnings, an Apple Developer ID certificate and
notarization step should be added later.
