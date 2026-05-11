# JuiceReverb

JuiceReverb는 JUCE/C++ 기반의 현대적인 VST3 리버브 플러그인입니다.

핵심 목표는 트랜스 음악에 어울리는 넓고 투명한 리버브 꼬리, 원음 펀치를 지키는 Internal Ducking, 따뜻한 Juice Saturation, wet 전용 mastering-grade low-cut, Mid-Side width 컨트롤입니다.

## 주요 기능

- APVTS 기반 파라미터 관리와 UI 자동 연결
- 프리딜레이가 있는 스테레오 리버브 탱크
- 입력이 강할 때 wet만 내려주는 Internal Ducking
- 리버브 꼬리에 배음을 더하는 Juice Saturation
- wet 전용 12dB/oct Low Cut
- wet 전용 Mid-Side Width
- 아주 어두운 실험실 배경과 형광 초록 Juice Tank UI
- CMake FetchContent로 JUCE 자동 다운로드
- GitHub Actions Windows/macOS VST3 클라우드 빌드

## 파일 구조

```text
JuiceReverb/
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

## 로컬 빌드

```bash
cmake -S . -B build
cmake --build build --config Release --target JuiceReverb_VST3
```

## GitHub Actions 빌드

GitHub 저장소에서 `Actions` 탭으로 이동한 뒤 `Build JuiceReverb VST3` 워크플로우를 실행하면 다음 산출물이 만들어집니다.

- `JuiceReverb-Windows-VST3.zip`
- `JuiceReverb-macOS-Universal-VST3.zip`

태그로 릴리스를 만들고 싶다면:

```bash
git tag v1.0.0
git push origin v1.0.0
```

macOS 공개 배포에는 Apple Developer ID 인증서와 notarization이 추가로 필요합니다.
