#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// BeatRepeaterAudioProcessorEditor
//
// 이 클래스는 플러그인의 "화면"을 담당합니다.
//
// 쉽게 말하면:
// - 사용자가 보는 배경색
// - GRID 선택 박스
// - SOFTNESS 노브
// - 글자 색상
// - 노브와 실제 소리 파라미터의 연결
//
// 이런 UI 관련 작업은 모두 여기에서 처리합니다.
//==============================================================================
class BeatRepeaterAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit BeatRepeaterAudioProcessorEditor (BeatRepeaterAudioProcessor&);
    ~BeatRepeaterAudioProcessorEditor() override;

    // 화면을 그리는 함수입니다.
    void paint (juce::Graphics&) override;

    // 창 크기가 바뀌거나 처음 만들어질 때,
    // 각 UI 요소의 위치와 크기를 정하는 함수입니다.
    void resized() override;

private:
    //==============================================================================
    // 커스텀 LookAndFeel
    //
    // JUCE의 ComboBox와 Slider는 기본 모양이 정해져 있습니다.
    // 우리가 원하는 다크 테마와 네온 블루 스타일을 만들기 위해
    // LookAndFeel 클래스를 직접 만들어 색과 모양을 일부 바꿉니다.
    //==============================================================================
    class NeonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        NeonLookAndFeel();

        // 로터리 슬라이더, 즉 노브를 그리는 함수입니다.
        void drawRotarySlider (juce::Graphics& g,
                               int x,
                               int y,
                               int width,
                               int height,
                               float sliderPosProportional,
                               float rotaryStartAngle,
                               float rotaryEndAngle,
                               juce::Slider& slider) override;

        // ComboBox의 테두리와 배경을 그립니다.
        void drawComboBox (juce::Graphics& g,
                           int width,
                           int height,
                           bool isButtonDown,
                           int buttonX,
                           int buttonY,
                           int buttonW,
                           int buttonH,
                           juce::ComboBox& box) override;
    };

    //==============================================================================
    BeatRepeaterAudioProcessor& audioProcessor;

    NeonLookAndFeel neonLookAndFeel;

    // GRID 라벨과 선택 박스입니다.
    juce::Label gridLabel;
    juce::ComboBox gridComboBox;

    // SOFTNESS 라벨과 로터리 슬라이더입니다.
    juce::Label softnessLabel;
    juce::Slider softnessSlider;

    //==============================================================================
    // Attachment는 UI와 Processor의 파라미터를 연결하는 케이블입니다.
    //
    // 예:
    // - 사용자가 Softness 노브를 돌림
    // - SliderAttachment가 그 값을 Processor의 "softness" 파라미터에 전달
    // - processBlock에서 바뀐 softness 값을 읽음
    // - 실제 소리가 바뀜
    //
    // 이 Attachment가 없으면 UI는 움직여도 실제 소리는 바뀌지 않습니다.
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboBoxAttachment> gridAttachment;
    std::unique_ptr<SliderAttachment> softnessAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatRepeaterAudioProcessorEditor)
};

