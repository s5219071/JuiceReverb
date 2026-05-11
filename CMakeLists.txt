cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# BeatRepeater CMake 프로젝트 설정 파일
#
# 이 파일은 Projucer나 Visual Studio 프로젝트 파일 없이도
# GitHub Actions 같은 클라우드 빌드 서버에서 JUCE 플러그인을 빌드하게 해줍니다.
#
# 아주 쉽게 말하면:
# - GitHub 서버가 이 파일을 읽습니다.
# - JUCE를 인터넷에서 내려받습니다.
# - Source 폴더 안의 PluginProcessor / PluginEditor 코드를 컴파일합니다.
# - 최종적으로 Windows/macOS용 BeatRepeater.vst3 파일을 만듭니다.
# ==============================================================================

project(BeatRepeater VERSION 1.0.0 LANGUAGES CXX)

# JUCE 8은 C++17 이상을 요구합니다.
# C++17은 "이 프로젝트는 최소 이 정도 문법 수준으로 컴파일하겠다"는 뜻입니다.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# JUCE 자동 다운로드 설정
#
# 사용자의 PC에 JUCE가 설치되어 있지 않아도 빌드되게 하기 위해 FetchContent를 사용합니다.
# GitHub Actions가 빌드할 때 JUCE GitHub 저장소에서 지정한 버전을 자동으로 받아옵니다.
# ==============================================================================

include(FetchContent)

# 예제 프로젝트와 Projucer 같은 부가 프로그램은 빌드하지 않습니다.
# 우리는 BeatRepeater VST3만 필요하므로 빌드 시간을 줄이기 위한 설정입니다.
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Do not build JUCE examples" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Do not build JUCE extras such as Projucer" FORCE)

# 현재 확인된 최신 안정 릴리스 기준입니다.
# 나중에 JUCE를 올리고 싶으면 GitHub 릴리스 페이지에서 새 태그를 확인한 뒤
# 아래 값만 예: 8.0.13 처럼 바꾸면 됩니다.
set(BEATREPEATER_JUCE_TAG "8.0.12" CACHE STRING "JUCE version tag to download")

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${BEATREPEATER_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(JUCE)

# ==============================================================================
# BeatRepeater 플러그인 타겟 생성
#
# juce_add_plugin은 JUCE가 제공하는 CMake 함수입니다.
# 이 함수가 VST3 빌드에 필요한 내부 설정을 대부분 자동으로 만들어줍니다.
# ==============================================================================

juce_add_plugin(BeatRepeater
    VERSION 1.0.0

    # 플러그인 제작자/회사 이름입니다.
    # 개인 프로젝트라면 원하는 이름으로 바꿔도 됩니다.
    COMPANY_NAME "BeatRepeaterLab"

    # 현재 프로젝트는 VST3 포맷을 빌드합니다.
    # Windows에서는 Windows VST3, macOS에서는 macOS VST3 번들이 만들어집니다.
    FORMATS VST3

    # 이 플러그인은 악기 신스가 아니라 입력 오디오를 바꾸는 이펙트입니다.
    IS_SYNTH FALSE

    # 현재 버전은 MIDI를 사용하지 않습니다.
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    # 에디터 화면에서 키보드 입력 포커스를 특별히 요구하지 않습니다.
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # GitHub Actions에서는 시스템 VST3 폴더에 복사할 필요가 없습니다.
    # 빌드 결과물 폴더에서 Artifact로 꺼내올 것이므로 FALSE가 안전합니다.
    COPY_PLUGIN_AFTER_BUILD FALSE

    # JUCE 플러그인 고유 ID입니다.
    # 각각 4글자여야 합니다.
    # 이미 배포한 뒤에는 바꾸지 않는 것이 좋습니다.
    PLUGIN_MANUFACTURER_CODE Btrl
    PLUGIN_CODE Btrp

    # 최종 플러그인 파일 이름입니다.
    PRODUCT_NAME "BeatRepeater"
)

# ==============================================================================
# 컴파일할 실제 소스 파일 목록
#
# GitHub 저장소에는 Source 폴더를 만들고 그 안에 아래 네 파일을 넣어야 합니다.
# - Source/PluginProcessor.h
# - Source/PluginProcessor.cpp
# - Source/PluginEditor.h
# - Source/PluginEditor.cpp
# ==============================================================================

target_sources(BeatRepeater
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

# ==============================================================================
# JUCE 모듈과 권장 빌드 옵션 연결
#
# juce_audio_utils는 오디오 플러그인, GUI, 파라미터 처리에 필요한
# 여러 JUCE 모듈을 함께 끌고 옵니다.
# ==============================================================================

target_link_libraries(BeatRepeater
    PRIVATE
        juce::juce_audio_utils

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ==============================================================================
# Projucer 스타일 코드 호환 설정
#
# 우리가 작성한 코드가 #include <JuceHeader.h>를 사용하므로,
# CMake 빌드에서도 JuceHeader.h를 자동 생성해야 합니다.
# ==============================================================================

juce_generate_juce_header(BeatRepeater)

# ==============================================================================
# 불필요한 기능 끄기
#
# 웹브라우저, CURL 같은 기능은 이 플러그인에 필요하지 않습니다.
# 꺼두면 빌드가 단순해지고 문제가 생길 가능성이 줄어듭니다.
# ==============================================================================

target_compile_definitions(BeatRepeater
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
