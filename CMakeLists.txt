cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# JuiceReverb
#
# 이 파일은 Projucer 없이 JUCE VST3 플러그인을 빌드하기 위한 CMake 설정입니다.
# 사용자의 PC에 JUCE가 설치되어 있지 않아도, CMake가 GitHub에서 JUCE 소스 코드를
# 자동으로 내려받아 빌드에 사용합니다.
#
# 초보자용 요약:
# - CMake는 "어떤 파일을 어떤 방식으로 컴파일할지" 적어두는 빌드 레시피입니다.
# - GitHub Actions도 이 파일을 그대로 읽어서 Windows/macOS VST3를 만듭니다.
# - 최종 산출물 이름은 JuiceReverb.vst3 입니다.
# ==============================================================================

project(JuiceReverb VERSION 1.0.0 LANGUAGES CXX)

# JUCE 8은 C++17 이상을 요구합니다.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS에서 빌드할 때 너무 최신 macOS 전용 바이너리가 되지 않도록 최소 지원 버전을 정합니다.
# GitHub Actions의 macos-latest에서 빌드해도 macOS 11 Big Sur 이상에서 로드되도록 돕습니다.
if (APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# ==============================================================================
# JUCE 자동 다운로드
#
# JUICEREVERB_JUCE_VERSION 값만 바꾸면 다른 JUCE 버전으로 테스트할 수 있습니다.
# 현재 8.0.12는 JUCE GitHub Releases 기준 최신 안정 릴리스입니다.
# ==============================================================================

include(FetchContent)

set(JUICEREVERB_JUCE_VERSION "8.0.12" CACHE STRING "JUCE version to download")

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Do not build JUCE examples" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Do not build JUCE extras such as Projucer" FORCE)

FetchContent_Declare(
    JUCE
    URL "https://github.com/juce-framework/JUCE/archive/refs/tags/${JUICEREVERB_JUCE_VERSION}.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(JUCE)

# ==============================================================================
# 플러그인 타깃 생성
#
# juce_add_plugin은 JUCE가 제공하는 CMake 함수입니다.
# VST3 래퍼, 플러그인 메타데이터, 빌드 타깃을 한 번에 만들어 줍니다.
# ==============================================================================

juce_add_plugin(JuiceReverb
    VERSION 1.0.0
    COMPANY_NAME "JuiceLab"
    COMPANY_WEBSITE "https://github.com"
    COMPANY_EMAIL "juicelab@example.com"

    FORMATS VST3

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE

    PLUGIN_MANUFACTURER_CODE JLab
    PLUGIN_CODE JRev

    PRODUCT_NAME "JuiceReverb"
    BUNDLE_ID "com.juicelab.juicereverb"
)

target_sources(JuiceReverb
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

# juce_audio_utils는 플러그인, GUI, APVTS, 오디오 유틸리티를 폭넓게 포함합니다.
target_link_libraries(JuiceReverb
    PRIVATE
        juce::juce_audio_utils

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# CMake 기반 JUCE 프로젝트에서 <JuceHeader.h>를 사용할 수 있게 생성합니다.
juce_generate_juce_header(JuiceReverb)

# 플러그인에 필요 없는 기능을 꺼서 빌드 의존성과 문제 가능성을 줄입니다.
target_compile_definitions(JuiceReverb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
