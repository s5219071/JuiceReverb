# BeatRepeater

BeatRepeater is a JUCE-based Windows VST3 audio effect plugin.

Core features:

- Host BPM and PPQ synced beat repeat
- Grid length selection from 1/1 to 1/32
- Circular buffer based audio capture and repeat playback
- Softness/Warmth control using short crossfade and low-pass filtering
- Dark neon blue plugin editor UI
- GitHub Actions workflow for Windows VST3 cloud builds

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

## Cloud Build

Open the repository on GitHub, go to `Actions`, choose `Build Windows VST3`,
then press `Run workflow`.

