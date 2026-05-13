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

## macOS 설치

`JuiceReverb-macOS-Universal-VST3.zip`을 내려받아 압축을 풀면 `JuiceReverb.vst3` 번들이 나옵니다.

이 파일을 아래 폴더 중 하나에 넣으면 VST3를 지원하는 macOS DAW에서 스캔할 수 있습니다.

```text
~/Library/Audio/Plug-Ins/VST3
/Library/Audio/Plug-Ins/VST3
```

이 빌드는 Apple Silicon과 Intel Mac을 모두 지원하는 Universal VST3입니다. Logic Pro는 VST3가 아니라 Audio Unit을 사용하므로, Logic용 플러그인이 필요하면 AU 포맷을 추가해야 합니다.

## macOS 보안 경고와 notarization

macOS에서 "악성 소프트웨어를 확인할 수 없음", "개발자를 확인할 수 없음", "업데이트가 필요함" 같은 경고가 뜨면, 플러그인이 Apple Developer ID로 서명되고 Apple notarization을 통과한 빌드인지 확인해야 합니다.

GitHub Actions는 Apple 인증서가 없을 때 ad-hoc 서명 빌드를 만듭니다. 이 빌드는 개발 테스트에는 쓸 수 있지만, 공개 배포용으로는 부족하며 macOS Gatekeeper가 막을 수 있습니다.

공개 배포용 macOS VST3를 만들려면 GitHub 저장소의 `Settings > Secrets and variables > Actions`에 아래 secrets를 추가합니다.

```text
APPLE_CERTIFICATE_BASE64
APPLE_CERTIFICATE_PASSWORD
APPLE_CODESIGN_IDENTITY
APPLE_ID
APPLE_TEAM_ID
APPLE_APP_SPECIFIC_PASSWORD
```

`APPLE_CERTIFICATE_BASE64`는 Apple Developer 계정에서 만든 `Developer ID Application` 인증서를 `.p12`로 내보낸 뒤 base64로 변환한 값입니다. `APPLE_APP_SPECIFIC_PASSWORD`는 Apple ID의 app-specific password입니다.

secrets를 추가한 뒤 `Build JuiceReverb VST3` 워크플로우를 다시 실행하면 macOS job이 Developer ID 서명, `notarytool` 제출, `stapler` 티켓 첨부를 시도합니다. 이 과정을 통과한 `JuiceReverb-macOS-Universal-VST3.zip`을 배포해야 macOS Gatekeeper 경고를 줄일 수 있습니다.

태그로 릴리스를 만들고 싶다면:

```bash
git tag v1.0.0
git push origin v1.0.0
```

macOS 공개 배포에는 Apple Developer ID 인증서와 notarization이 추가로 필요합니다.
