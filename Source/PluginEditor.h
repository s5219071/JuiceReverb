#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// JuiceReverbAudioProcessorEditor
//
// 이 클래스는 플러그인의 화면입니다.
// 소리는 Processor에서 만들고, Editor는 노브/라벨/시각화만 담당합니다.
//==============================================================================
class JuiceReverbAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit JuiceReverbAudioProcessorEditor (JuiceReverbAudioProcessor&);
    ~JuiceReverbAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // Mad Lab 테마용 LookAndFeel입니다.
    //
    // JUCE 기본 노브 대신 어두운 금속 베젤, 형광 초록 아크, 계측기 포인터를 그립니다.
    class MadLabLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        MadLabLookAndFeel();

        void drawRotarySlider (juce::Graphics& g,
                               int x,
                               int y,
                               int width,
                               int height,
                               float sliderPosProportional,
                               float rotaryStartAngle,
                               float rotaryEndAngle,
                               juce::Slider& slider) override;
    };

    //==============================================================================
    // 중앙 Juice Tank 시각화입니다.
    //
    // Processor가 제공하는 리버브 레벨과 ducking 값을 받아 초록 액체 높이와
    // 표면 파동을 그립니다. 실제 오디오를 분석하지는 않고, 안전한 atomic 미터만 읽습니다.
    class JuiceTank final : public juce::Component
    {
    public:
        void setState (float newLevel, float newDucking, float newPhase) noexcept;
        void paint (juce::Graphics& g) override;

    private:
        float level = 0.0f;
        float ducking = 0.0f;
        float phase = 0.0f;
    };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider& slider,
                    juce::Label& label,
                    const juce::String& labelText,
                    const juce::String& suffix);

    void layoutKnob (juce::Slider& slider,
                     juce::Label& label,
                     juce::Rectangle<int> bounds);

    //==============================================================================
    JuiceReverbAudioProcessor& audioProcessor;
    MadLabLookAndFeel madLabLookAndFeel;
    JuiceTank juiceTank;

    juce::Slider mixSlider;
    juce::Slider decaySlider;
    juce::Slider sizeSlider;
    juce::Slider preDelaySlider;
    juce::Slider lowCutSlider;
    juce::Slider duckingSlider;
    juce::Slider saturationSlider;
    juce::Slider widthSlider;
    juce::Slider dampingSlider;

    juce::Label mixLabel;
    juce::Label decayLabel;
    juce::Label sizeLabel;
    juce::Label preDelayLabel;
    juce::Label lowCutLabel;
    juce::Label duckingLabel;
    juce::Label saturationLabel;
    juce::Label widthLabel;
    juce::Label dampingLabel;

    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> sizeAttachment;
    std::unique_ptr<SliderAttachment> preDelayAttachment;
    std::unique_ptr<SliderAttachment> lowCutAttachment;
    std::unique_ptr<SliderAttachment> duckingAttachment;
    std::unique_ptr<SliderAttachment> saturationAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> dampingAttachment;

    float tankLevel = 0.0f;
    float tankPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceReverbAudioProcessorEditor)
};
