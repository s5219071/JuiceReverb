#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// 색상 상수입니다.
//
// 코드 곳곳에 색상 숫자를 직접 쓰면 나중에 수정하기 어렵습니다.
// 이렇게 이름을 붙여두면 "네온 블루를 조금 바꾸고 싶다" 같은 상황에서
// 한 곳만 수정하면 됩니다.
namespace
{
    const juce::Colour backgroundColour = juce::Colour::fromString ("ff121212");
    const juce::Colour neonBlueColour   = juce::Colour::fromString ("ff00fbff");
    const juce::Colour darkPanelColour  = juce::Colour::fromString ("ff1b1b1b");
    const juce::Colour softTextColour   = juce::Colour::fromString ("cc00fbff");
}

//==============================================================================
BeatRepeaterAudioProcessorEditor::NeonLookAndFeel::NeonLookAndFeel()
{
    // ComboBox 기본 색상 설정입니다.
    setColour (juce::ComboBox::backgroundColourId, darkPanelColour);
    setColour (juce::ComboBox::outlineColourId, neonBlueColour);
    setColour (juce::ComboBox::textColourId, neonBlueColour);
    setColour (juce::ComboBox::arrowColourId, neonBlueColour);

    // ComboBox를 클릭했을 때 뜨는 메뉴의 색상입니다.
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour::fromString ("ff181818"));
    setColour (juce::PopupMenu::textColourId, neonBlueColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, neonBlueColour);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);

    // Slider 기본 색상 설정입니다.
    setColour (juce::Slider::rotarySliderFillColourId, neonBlueColour);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromString ("ff303030"));
    setColour (juce::Slider::thumbColourId, neonBlueColour);
    setColour (juce::Slider::textBoxTextColourId, neonBlueColour);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void BeatRepeaterAudioProcessorEditor::NeonLookAndFeel::drawRotarySlider
(
    juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider
)
{
    juce::ignoreUnused (slider);

    // 노브는 주어진 사각형 안에 들어가는 원으로 그립니다.
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height))
                                                .reduced (8.0f);

    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();

    // 현재 슬라이더 값이 어느 각도인지 계산합니다.
    const float angle = rotaryStartAngle
                      + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // 노브 바깥쪽 어두운 원입니다.
    g.setColour (juce::Colour::fromString ("ff202020"));
    g.fillEllipse (bounds);

    // 네온 느낌을 위한 얇은 외곽선입니다.
    g.setColour (neonBlueColour.withAlpha (0.9f));
    g.drawEllipse (bounds, 2.0f);

    // 값이 차오른 구간을 arc로 그립니다.
    juce::Path valueArc;
    valueArc.addCentredArc (centreX,
                            centreY,
                            radius - 7.0f,
                            radius - 7.0f,
                            0.0f,
                            rotaryStartAngle,
                            angle,
                            true);

    g.setColour (neonBlueColour);
    g.strokePath (valueArc, juce::PathStrokeType (4.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // 노브 포인터입니다.
    // 작은 선 하나가 현재 값을 가리킵니다.
    juce::Path pointer;
    const float pointerLength = radius * 0.58f;
    const float pointerThickness = 3.0f;

    pointer.addRectangle (-pointerThickness * 0.5f,
                          -radius + 10.0f,
                          pointerThickness,
                          pointerLength);

    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                            .translated (centreX, centreY));

    g.setColour (neonBlueColour);
    g.fillPath (pointer);

    // 네온 블루가 너무 평면적으로 보이지 않도록 중심에 작은 어두운 원을 더합니다.
    const float innerRadius = radius * 0.34f;
    g.setColour (backgroundColour);
    g.fillEllipse (centreX - innerRadius,
                   centreY - innerRadius,
                   innerRadius * 2.0f,
                   innerRadius * 2.0f);
}

//==============================================================================
BeatRepeaterAudioProcessorEditor::BeatRepeaterAudioProcessorEditor
(
    BeatRepeaterAudioProcessor& p
)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    // 플러그인 창 크기를 400x300으로 고정합니다.
    setSize (400, 300);

    //==========================================================================
    // LENGTH 라벨 설정
    //==========================================================================
    lengthLabel.setText ("LENGTH", juce::dontSendNotification);
    lengthLabel.setJustificationType (juce::Justification::centred);
    lengthLabel.setColour (juce::Label::textColourId, neonBlueColour);
    lengthLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    addAndMakeVisible (lengthLabel);

    //==========================================================================
    // LENGTH Slider 설정
    //
    // 내부 파라미터는 기존 "grid"입니다.
    // 단, 화면에서는 ComboBox 대신 로터리 노브로 보여줍니다.
    //
    // 값 대응:
    // 0 = 1/1
    // 1 = 1/2
    // 2 = 1/4
    // 3 = 1/8
    // 4 = 1/16
    // 5 = 1/32
    //
    // Length가 짧아질수록 Processor 안에서 마스터링 매크로가 자동으로 더 많이 동작합니다.
    //==========================================================================
    lengthSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    lengthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 24);
    lengthSlider.setLookAndFeel (&neonLookAndFeel);
    lengthSlider.setColour (juce::Slider::textBoxTextColourId, neonBlueColour);
    lengthSlider.setNumDecimalPlacesToDisplay (0);
    lengthSlider.setRange (0.0, 5.0, 1.0);
    lengthSlider.textFromValueFunction = [] (double value)
    {
        switch (juce::jlimit (0, 5, static_cast<int> (std::round (value))))
        {
            case 0:  return juce::String ("1/1");
            case 1:  return juce::String ("1/2");
            case 2:  return juce::String ("1/4");
            case 3:  return juce::String ("1/8");
            case 4:  return juce::String ("1/16");
            default: return juce::String ("1/32");
        }
    };

    lengthSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        if (text == "1/1")  return 0.0;
        if (text == "1/2")  return 1.0;
        if (text == "1/4")  return 2.0;
        if (text == "1/8")  return 3.0;
        if (text == "1/16") return 4.0;
        return 5.0;
    };

    addAndMakeVisible (lengthSlider);

    //==========================================================================
    // SOFTNESS 라벨 설정
    //==========================================================================
    softnessLabel.setText ("SOFTNESS", juce::dontSendNotification);
    softnessLabel.setJustificationType (juce::Justification::centred);
    softnessLabel.setColour (juce::Label::textColourId, neonBlueColour);
    softnessLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    addAndMakeVisible (softnessLabel);

    //==========================================================================
    // SOFTNESS Slider 설정
    //
    // RotaryVerticalDrag:
    // - 노브 형태로 보입니다.
    // - 마우스를 위아래로 드래그해서 값을 바꿉니다.
    //
    // TextBoxBelow:
    // - 노브 아래에 현재 값이 숫자로 보입니다.
    //==========================================================================
    softnessSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    softnessSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 24);
    softnessSlider.setTextValueSuffix (" %");
    softnessSlider.setLookAndFeel (&neonLookAndFeel);
    softnessSlider.setColour (juce::Slider::textBoxTextColourId, neonBlueColour);
    addAndMakeVisible (softnessSlider);

    //==========================================================================
    // Attachment 연결
    //
    // 이것이 UI와 실제 오디오 엔진을 연결하는 핵심입니다.
    //
    // "grid"와 "softness"라는 문자열은 Processor에서 만든 파라미터 ID와
    // 정확히 일치해야 합니다.
    //==========================================================================
    lengthAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts,
                                                           "grid",
                                                           lengthSlider);

    softnessAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts,
                                                             "softness",
                                                             softnessSlider);
}

BeatRepeaterAudioProcessorEditor::~BeatRepeaterAudioProcessorEditor()
{
    // Attachment를 먼저 해제합니다.
    // UI 컴포넌트가 사라지기 전에 연결을 끊어야 안전합니다.
    softnessAttachment = nullptr;
    lengthAttachment = nullptr;

    // LookAndFeel 연결을 해제합니다.
    // JUCE에서는 커스텀 LookAndFeel 객체가 사라진 뒤에도 컴포넌트가
    // 그 객체를 바라보고 있으면 문제가 생길 수 있습니다.
    softnessSlider.setLookAndFeel (nullptr);
    lengthSlider.setLookAndFeel (nullptr);
}

//==============================================================================
void BeatRepeaterAudioProcessorEditor::paint (juce::Graphics& g)
{
    // 전체 배경을 아주 어두운 차콜색으로 채웁니다.
    g.fillAll (backgroundColour);

    // 플러그인 이름을 위쪽 중앙에 표시합니다.
    g.setColour (neonBlueColour);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("BEAT REPEATER",
                0,
                18,
                getWidth(),
                32,
                juce::Justification::centred,
                false);

    // 얇은 네온 라인을 추가해 전체 UI에 중심선을 만들어줍니다.
    g.setColour (neonBlueColour.withAlpha (0.35f));
    g.drawLine (40.0f, 62.0f, static_cast<float> (getWidth() - 40), 62.0f, 1.0f);

    // 하단의 작은 상태 텍스트입니다.
    g.setColour (softTextColour);
    g.setFont (juce::Font (12.0f));
    g.drawText ("HOST SYNCED MASTER REPEATER",
                0,
                getHeight() - 34,
                getWidth(),
                20,
                juce::Justification::centred,
                false);
}

void BeatRepeaterAudioProcessorEditor::resized()
{
    // 전체 창 크기: 400 x 300
    //
    // 왼쪽 영역:
    // - LENGTH 라벨
    // - Length 노브
    //
    // 오른쪽 영역:
    // - SOFTNESS 라벨
    // - Softness 노브
    const int topAreaY = 78;
    const int labelHeight = 24;

    const int leftColumnX = 42;
    const int rightColumnX = 222;

    const int columnWidth = 136;

    lengthLabel.setBounds (leftColumnX, topAreaY, columnWidth, labelHeight);
    lengthSlider.setBounds (leftColumnX, topAreaY + 34, columnWidth, 142);

    softnessLabel.setBounds (rightColumnX, topAreaY, columnWidth, labelHeight);
    softnessSlider.setBounds (rightColumnX, topAreaY + 34, columnWidth, 142);
}
