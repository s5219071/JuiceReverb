#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    const juce::Colour labBackground = juce::Colour::fromString ("ff050805");
    const juce::Colour labPanel = juce::Colour::fromString ("ff0b120c");
    const juce::Colour juiceGreen = juce::Colour::fromString ("ff00ff41");
    const juce::Colour labText = juce::Colour::fromString ("ddc9ffd4");
    const juce::Colour labMutedText = juce::Colour::fromString ("8896b89d");

    constexpr float rotaryStartAngle = juce::MathConstants<float>::pi * 1.20f;
    constexpr float rotaryEndAngle = juce::MathConstants<float>::pi * 2.80f;
}

//==============================================================================
JuiceReverbAudioProcessorEditor::MadLabLookAndFeel::MadLabLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, labText);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, labText);
}

void JuiceReverbAudioProcessorEditor::MadLabLookAndFeel::drawRotarySlider
(
    juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStart,
    float rotaryEnd,
    juce::Slider& slider
)
{
    juce::ignoreUnused (slider);

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height))
                                                .reduced (9.0f);

    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float angle = rotaryStart + sliderPosProportional * (rotaryEnd - rotaryStart);

    // 금속 베이스입니다.
    g.setColour (juce::Colour::fromString ("ff111812"));
    g.fillEllipse (bounds);

    g.setColour (juce::Colour::fromString ("ff263027"));
    g.drawEllipse (bounds, 2.0f);

    g.setColour (juiceGreen.withAlpha (0.18f));
    g.drawEllipse (bounds.reduced (5.0f), 1.0f);

    // 계측기 눈금처럼 보이는 작은 틱 마크입니다.
    for (int tick = 0; tick <= 10; ++tick)
    {
        const float proportion = static_cast<float> (tick) / 10.0f;
        const float tickAngle = rotaryStart + proportion * (rotaryEnd - rotaryStart);
        const float inner = radius - 11.0f;
        const float outer = radius - 5.0f;

        const juce::Point<float> p1 (centreX + std::cos (tickAngle) * inner,
                                     centreY + std::sin (tickAngle) * inner);
        const juce::Point<float> p2 (centreX + std::cos (tickAngle) * outer,
                                     centreY + std::sin (tickAngle) * outer);

        g.setColour (tick % 5 == 0 ? juiceGreen.withAlpha (0.58f)
                                   : labMutedText.withAlpha (0.35f));
        g.drawLine (juce::Line<float> (p1, p2), tick % 5 == 0 ? 1.4f : 1.0f);
    }

    // 현재 값 구간을 형광 액체가 차오르는 링처럼 그립니다.
    juce::Path valueArc;
    valueArc.addCentredArc (centreX,
                            centreY,
                            radius - 15.0f,
                            radius - 15.0f,
                            0.0f,
                            rotaryStart,
                            angle,
                            true);

    g.setColour (juiceGreen.withAlpha (0.22f));
    g.strokePath (valueArc, juce::PathStrokeType (8.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    g.setColour (juiceGreen);
    g.strokePath (valueArc, juce::PathStrokeType (3.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // 괴짜 실험실 장비 느낌의 포인터입니다.
    juce::Path pointer;
    pointer.addRoundedRectangle (-3.0f,
                                 -radius + 20.0f,
                                 6.0f,
                                 radius * 0.62f,
                                 3.0f);

    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                            .translated (centreX, centreY));

    g.setColour (juiceGreen.withAlpha (0.92f));
    g.fillPath (pointer);

    g.setColour (labBackground);
    g.fillEllipse (centreX - radius * 0.31f,
                   centreY - radius * 0.31f,
                   radius * 0.62f,
                   radius * 0.62f);

    g.setColour (juiceGreen.withAlpha (0.70f));
    g.drawEllipse (centreX - radius * 0.31f,
                   centreY - radius * 0.31f,
                   radius * 0.62f,
                   radius * 0.62f,
                   1.2f);
}

//==============================================================================
void JuiceReverbAudioProcessorEditor::JuiceTank::setState (float newLevel,
                                                           float newDucking,
                                                           float newPhase) noexcept
{
    level = juce::jlimit (0.0f, 1.0f, newLevel);
    ducking = juce::jlimit (0.0f, 1.0f, newDucking);
    phase = newPhase;
    repaint();
}

void JuiceReverbAudioProcessorEditor::JuiceTank::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (labPanel);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (juiceGreen.withAlpha (0.20f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.2f);

    auto glass = bounds.reduced (24.0f, 18.0f);
    const float liquidHeight = glass.getHeight() * (0.22f + level * 0.70f);
    const float liquidTop = glass.getBottom() - liquidHeight;

    g.setColour (juce::Colour::fromString ("3318ff58"));
    g.fillRoundedRectangle (glass, 8.0f);

    // 액체 표면 파동입니다.
    juce::Path liquidPath;
    const float amplitude = 4.0f + level * 18.0f;
    const int steps = 48;

    liquidPath.startNewSubPath (glass.getX(), glass.getBottom());
    liquidPath.lineTo (glass.getX(), liquidTop);

    for (int i = 0; i <= steps; ++i)
    {
        const float x = glass.getX() + glass.getWidth() * static_cast<float> (i) / static_cast<float> (steps);
        const float waveA = std::sin (phase + static_cast<float> (i) * 0.38f) * amplitude;
        const float waveB = std::sin (phase * 0.63f + static_cast<float> (i) * 0.91f) * amplitude * 0.35f;
        const float y = liquidTop + waveA + waveB;
        liquidPath.lineTo (x, y);
    }

    liquidPath.lineTo (glass.getRight(), glass.getBottom());
    liquidPath.closeSubPath();

    g.setColour (juiceGreen.withAlpha (0.82f));
    g.fillPath (liquidPath);

    g.setColour (juiceGreen.withAlpha (0.28f));
    g.drawHorizontalLine (static_cast<int> (liquidTop),
                          glass.getX(),
                          glass.getRight());

    // Ducking이 강하게 걸릴수록 위쪽 압력 게이지가 더 어둡게 잠깁니다.
    const auto pressure = glass.removeFromTop (14.0f).reduced (6.0f, 3.0f);
    g.setColour (juiceGreen.withAlpha (0.18f));
    g.fillRoundedRectangle (pressure, 4.0f);

    g.setColour (juiceGreen.withAlpha (0.75f));
    g.fillRoundedRectangle (pressure.withWidth (pressure.getWidth() * (1.0f - ducking)), 4.0f);

    g.setColour (juiceGreen.withAlpha (0.82f));
    g.drawRoundedRectangle (bounds.reduced (20.0f, 14.0f), 8.0f, 2.0f);

    // 유리 하이라이트입니다.
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (bounds.getX() + 34.0f,
                bounds.getY() + 28.0f,
                bounds.getX() + 62.0f,
                bounds.getBottom() - 30.0f,
                2.0f);
}

//==============================================================================
JuiceReverbAudioProcessorEditor::JuiceReverbAudioProcessorEditor (JuiceReverbAudioProcessor& processor)
    : AudioProcessorEditor (&processor),
      audioProcessor (processor)
{
    setLookAndFeel (&madLabLookAndFeel);
    setResizable (true, true);
    setResizeLimits (680, 500, 1040, 760);
    setSize (820, 540);

    addAndMakeVisible (juiceTank);

    setupKnob (mixSlider, mixLabel, "MIX", " %");
    setupKnob (decaySlider, decayLabel, "DECAY", " s");
    setupKnob (sizeSlider, sizeLabel, "SIZE", " %");
    setupKnob (preDelaySlider, preDelayLabel, "PRE", " ms");
    setupKnob (lowCutSlider, lowCutLabel, "LOW CUT", " Hz");
    setupKnob (duckingSlider, duckingLabel, "DUCK", " %");
    setupKnob (saturationSlider, saturationLabel, "JUICE", " %");
    setupKnob (widthSlider, widthLabel, "WIDTH", " %");
    setupKnob (dampingSlider, dampingLabel, "DAMP", " %");

    mixAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "mix", mixSlider);
    decayAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "decay", decaySlider);
    sizeAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "size", sizeSlider);
    preDelayAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "preDelay", preDelaySlider);
    lowCutAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "lowCut", lowCutSlider);
    duckingAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "ducking", duckingSlider);
    saturationAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "saturation", saturationSlider);
    widthAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "width", widthSlider);
    dampingAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "damping", dampingSlider);

    startTimerHz (30);
}

JuiceReverbAudioProcessorEditor::~JuiceReverbAudioProcessorEditor()
{
    stopTimer();

    dampingAttachment = nullptr;
    widthAttachment = nullptr;
    saturationAttachment = nullptr;
    duckingAttachment = nullptr;
    lowCutAttachment = nullptr;
    preDelayAttachment = nullptr;
    sizeAttachment = nullptr;
    decayAttachment = nullptr;
    mixAttachment = nullptr;

    setLookAndFeel (nullptr);
}

//==============================================================================
void JuiceReverbAudioProcessorEditor::setupKnob (juce::Slider& slider,
                                                 juce::Label& label,
                                                 const juce::String& labelText,
                                                 const juce::String& suffix)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, labText);
    label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (rotaryStartAngle, rotaryEndAngle, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 22);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::textBoxTextColourId, labText);
    addAndMakeVisible (slider);
}

void JuiceReverbAudioProcessorEditor::layoutKnob (juce::Slider& slider,
                                                  juce::Label& label,
                                                  juce::Rectangle<int> bounds)
{
    label.setBounds (bounds.removeFromTop (22));
    slider.setBounds (bounds);
}

void JuiceReverbAudioProcessorEditor::timerCallback()
{
    const float targetLevel = audioProcessor.getVisualLevel();
    tankLevel = targetLevel > tankLevel
        ? targetLevel
        : tankLevel * 0.90f + targetLevel * 0.10f;

    tankPhase += 0.12f + tankLevel * 0.18f;

    if (tankPhase > juce::MathConstants<float>::twoPi)
        tankPhase -= juce::MathConstants<float>::twoPi;

    juiceTank.setState (tankLevel, audioProcessor.getDuckingDepth(), tankPhase);
}

//==============================================================================
void JuiceReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (labBackground);

    // 배경의 얇은 실험실 그리드입니다.
    g.setColour (juiceGreen.withAlpha (0.055f));

    for (int x = 0; x < getWidth(); x += 24)
        g.drawVerticalLine (x, 0.0f, static_cast<float> (getHeight()));

    for (int y = 0; y < getHeight(); y += 24)
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));

    auto titleArea = getLocalBounds().removeFromTop (58);

    g.setColour (juiceGreen);
    g.setFont (juce::Font (juce::FontOptions (25.0f, juce::Font::bold)));
    g.drawText ("JuiceReverb", titleArea.removeFromTop (34), juce::Justification::centred);

    g.setColour (labMutedText);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("TRANCE LUSHNESS  |  INTERNAL DUCKING  |  MAD LAB DSP",
                titleArea,
                juce::Justification::centred);

    g.setColour (juiceGreen.withAlpha (0.25f));
    g.drawLine (34.0f, 62.0f, static_cast<float> (getWidth() - 34), 62.0f, 1.0f);

    g.setColour (labMutedText.withAlpha (0.72f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("Made by Kino",
                getLocalBounds().reduced (24).removeFromBottom (24),
                juce::Justification::centredRight);
}

void JuiceReverbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24);
    area.removeFromTop (54);

    const int knobWidth = 132;
    const int knobHeight = 136;
    const int gap = 14;

    auto body = area.removeFromTop (300);

    const int leftX = body.getX();
    const int rightX = body.getRight() - knobWidth;
    const int firstY = body.getY() + 6;
    const int secondY = firstY + knobHeight + gap;

    layoutKnob (mixSlider, mixLabel, { leftX, firstY, knobWidth, knobHeight });
    layoutKnob (decaySlider, decayLabel, { leftX, secondY, knobWidth, knobHeight });

    layoutKnob (duckingSlider, duckingLabel, { rightX, firstY, knobWidth, knobHeight });
    layoutKnob (saturationSlider, saturationLabel, { rightX, secondY, knobWidth, knobHeight });

    const int tankWidth = juce::jlimit (280, 390, body.getWidth() - knobWidth * 2 - 64);
    const int tankHeight = 258;

    juiceTank.setBounds (body.getCentreX() - tankWidth / 2,
                         body.getY() + 18,
                         tankWidth,
                         tankHeight);

    auto bottom = area.reduced (8, 0);
    const int bottomKnobWidth = juce::jlimit (104, 132, (bottom.getWidth() - gap * 4) / 5);
    const int bottomY = bottom.getY() + 14;

    juce::Array<juce::Rectangle<int>> bottomSlots;

    const int totalBottomWidth = bottomKnobWidth * 5 + gap * 4;
    int x = bottom.getCentreX() - totalBottomWidth / 2;

    for (int index = 0; index < 5; ++index)
    {
        bottomSlots.add ({ x, bottomY, bottomKnobWidth, knobHeight });
        x += bottomKnobWidth + gap;
    }

    layoutKnob (sizeSlider, sizeLabel, bottomSlots[0]);
    layoutKnob (preDelaySlider, preDelayLabel, bottomSlots[1]);
    layoutKnob (lowCutSlider, lowCutLabel, bottomSlots[2]);
    layoutKnob (widthSlider, widthLabel, bottomSlots[3]);
    layoutKnob (dampingSlider, dampingLabel, bottomSlots[4]);
}
