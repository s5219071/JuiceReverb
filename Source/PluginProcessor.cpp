#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // 44.1kHz 기준 Freeverb 계열 튜닝값입니다.
    // 샘플레이트가 바뀌면 prepare()에서 비율에 맞게 늘리거나 줄입니다.
    constexpr std::array<int, 8> combTunings    { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    constexpr std::array<int, 4> allpassTunings { 556, 441, 341, 225 };

    constexpr double referenceSampleRate = 44100.0;
    constexpr int stereoSpreadSamples = 23;
    constexpr double maxPreDelaySeconds = 0.25;

}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
JuiceReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto percentRange = juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f);

    auto decayRange = juce::NormalisableRange<float> (0.5f, 12.0f, 0.01f);
    decayRange.setSkewForCentre (3.5f);

    auto preDelayRange = juce::NormalisableRange<float> (0.0f, 180.0f, 0.1f);
    preDelayRange.setSkewForCentre (35.0f);

    auto lowCutRange = juce::NormalisableRange<float> (20.0f, 600.0f, 0.1f);
    lowCutRange.setSkewForCentre (150.0f);

    auto highCutRange = juce::NormalisableRange<float> (2000.0f, 20000.0f, 1.0f);
    highCutRange.setSkewForCentre (9000.0f);

    auto midGainRange = juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f);
    auto widthRange = juce::NormalisableRange<float> (50.0f, 200.0f, 0.1f);

    // Mix: 최종 dry/wet 비율입니다. 0%는 원음만, 100%는 리버브만 들립니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::mix, 1 }, "Mix", percentRange, 35.0f));

    // Decay: 리버브 꼬리가 얼마나 오래 남는지 정합니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::decay, 1 }, "Decay", decayRange, 4.5f));

    // Size: 공간의 체감 크기와 확산감에 영향을 줍니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::size, 1 }, "Size", percentRange, 78.0f));

    // Pre Delay: 원음 뒤에 리버브가 살짝 늦게 붙게 해서 트랜스 킥/리드의 앞부분을 살립니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::preDelay, 1 }, "Pre Delay", preDelayRange, 32.0f));

    // Low Cut: 리버브 wet에만 걸리는 저역 정리 필터입니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::lowCut, 1 }, "Low Cut", lowCutRange, 150.0f));

    // Mid Gain: wet 리버브의 1.5kHz 존재감을 조절합니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::midGain, 1 }, "Mid Gain", midGainRange, 0.0f));

    // Hi Cut: wet 리버브의 밝기를 정리하는 low-pass 필터입니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::highCut, 1 }, "Hi Cut", highCutRange, 12000.0f));

    // Width: wet 리버브의 Mid-Side 스테레오 폭입니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::width, 1 }, "Width", widthRange, 132.0f));

    // Damping: 리버브 꼬리의 고역이 얼마나 부드럽게 줄어드는지 정합니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::damping, 1 }, "Damping", percentRange, 42.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
#ifndef JucePlugin_PreferredChannelConfigurations

JuiceReverbAudioProcessor::JuiceReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
        #if ! JucePlugin_IsMidiEffect
         #if ! JucePlugin_IsSynth
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
         #endif
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        #endif
      ),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

#else

JuiceReverbAudioProcessor::JuiceReverbAudioProcessor()
    : apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

#endif

JuiceReverbAudioProcessor::~JuiceReverbAudioProcessor() = default;

//==============================================================================
const juce::String JuiceReverbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JuiceReverbAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool JuiceReverbAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool JuiceReverbAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double JuiceReverbAudioProcessor::getTailLengthSeconds() const
{
    // 리버브 플러그인은 입력이 멈춘 뒤에도 꼬리가 남습니다.
    return 14.0;
}

int JuiceReverbAudioProcessor::getNumPrograms()
{
    return 1;
}

int JuiceReverbAudioProcessor::getCurrentProgram()
{
    return 0;
}

void JuiceReverbAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String JuiceReverbAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void JuiceReverbAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void JuiceReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    sampleRateHz = isFiniteAndPositive (sampleRate) ? sampleRate : 44100.0;

    // 프리딜레이는 최대 250ms까지만 내부 버퍼를 잡습니다.
    // 실제 노브 범위는 180ms라서 여유분이 있습니다.
    maxPreDelaySamples = juce::jmax (1, static_cast<int> (sampleRateHz * maxPreDelaySeconds));
    preDelayBuffer.setSize (2, maxPreDelaySamples + 1);

    reverbTank.prepare (sampleRateHz);
    resetSmoothers();
    clearDspState();
}

void JuiceReverbAudioProcessor::releaseResources()
{
    clearDspState();
}

#ifndef JucePlugin_PreferredChannelConfigurations

bool JuiceReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (mainOutput != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
}

#endif

//==============================================================================
void JuiceReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = juce::jmin (2, juce::jmin (numInputChannels, numOutputChannels));

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (channelsToProcess <= 0 || numSamples <= 0)
        return;

    mixSmoother.setTargetValue (getParameterValue (ParameterIDs::mix) / 100.0f);
    decaySmoother.setTargetValue (getParameterValue (ParameterIDs::decay));
    sizeSmoother.setTargetValue (getParameterValue (ParameterIDs::size) / 100.0f);
    preDelaySmoother.setTargetValue (getParameterValue (ParameterIDs::preDelay));
    lowCutSmoother.setTargetValue (getParameterValue (ParameterIDs::lowCut));
    midGainSmoother.setTargetValue (getParameterValue (ParameterIDs::midGain));
    highCutSmoother.setTargetValue (getParameterValue (ParameterIDs::highCut));
    widthSmoother.setTargetValue (getParameterValue (ParameterIDs::width) / 100.0f);
    dampingSmoother.setTargetValue (getParameterValue (ParameterIDs::damping) / 100.0f);

    float blockMeter = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryLeft = buffer.getSample (0, sample);
        const float dryRight = channelsToProcess > 1 ? buffer.getSample (1, sample) : dryLeft;

        const float mix = juce::jlimit (0.0f, 1.0f, mixSmoother.getNextValue());
        const float decaySeconds = decaySmoother.getNextValue();
        const float size = juce::jlimit (0.0f, 1.0f, sizeSmoother.getNextValue());
        const float preDelayMs = preDelaySmoother.getNextValue();
        const float lowCutHz = lowCutSmoother.getNextValue();
        const float midGainDb = midGainSmoother.getNextValue();
        const float highCutHz = highCutSmoother.getNextValue();
        const float width = juce::jlimit (0.5f, 2.0f, widthSmoother.getNextValue());
        const float damping = juce::jlimit (0.0f, 1.0f, dampingSmoother.getNextValue());

        const int preDelaySamples = juce::jlimit (
            0,
            maxPreDelaySamples - 1,
            static_cast<int> (preDelayMs * 0.001f * static_cast<float> (sampleRateHz)));

        // Dry를 바로 리버브에 넣지 않고 프리딜레이를 거칩니다.
        // 이렇게 하면 원음의 첫 타격감이 앞으로 살아 있습니다.
        const float preDelayedLeft = processPreDelay (0, dryLeft, preDelaySamples);
        const float preDelayedRight = processPreDelay (1, dryRight, preDelaySamples);
        advancePreDelay();

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        reverbTank.process (preDelayedLeft,
                            preDelayedRight,
                            decaySeconds,
                            damping,
                            size,
                            wetLeft,
                            wetRight);

        // 리버브에만 12dB/oct 정도의 로우컷을 적용합니다.
        // 원음 저역은 건드리지 않으므로 킥과 베이스의 중심이 덜 무너집니다.
        const float highPassCoefficient = calculateHighPassCoefficient (lowCutHz, sampleRateHz);
        wetLeft = processHighPass (lowCutStageA[0], wetLeft, highPassCoefficient);
        wetLeft = processHighPass (lowCutStageB[0], wetLeft, highPassCoefficient);
        wetRight = processHighPass (lowCutStageA[1], wetRight, highPassCoefficient);
        wetRight = processHighPass (lowCutStageB[1], wetRight, highPassCoefficient);

        // Wet 전용 Mid EQ와 Hi Cut입니다.
        const auto midCoefficients = calculateMidPeakCoefficients (midGainDb, sampleRateHz);
        wetLeft = processBiquad (midFilterState[0], wetLeft, midCoefficients);
        wetRight = processBiquad (midFilterState[1], wetRight, midCoefficients);

        const float lowPassCoefficient = calculateLowPassCoefficient (highCutHz, sampleRateHz);
        wetLeft = processLowPass (highCutStageA[0], wetLeft, lowPassCoefficient);
        wetLeft = processLowPass (highCutStageB[0], wetLeft, lowPassCoefficient);
        wetRight = processLowPass (highCutStageA[1], wetRight, lowPassCoefficient);
        wetRight = processLowPass (highCutStageB[1], wetRight, lowPassCoefficient);

        // Wet 전용 Mid-Side Width입니다.
        // Dry에는 적용하지 않으므로 원음의 중앙 이미지는 더 안정적입니다.
        const float mid = (wetLeft + wetRight) * 0.5f;
        const float side = (wetLeft - wetRight) * 0.5f * width;
        wetLeft = mid + side;
        wetRight = mid - side;

        // Equal-power에 가까운 dry/wet 크로스페이드입니다.
        const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin (mix * juce::MathConstants<float>::halfPi);

        float outputLeft = dryLeft * dryGain + wetLeft * wetGain;
        float outputRight = dryRight * dryGain + wetRight * wetGain;

        outputLeft = softLimitSample (outputLeft);
        outputRight = softLimitSample (outputRight);

        if (channelsToProcess > 1)
        {
            buffer.setSample (0, sample, outputLeft);
            buffer.setSample (1, sample, outputRight);
        }
        else
        {
            buffer.setSample (0, sample, (outputLeft + outputRight) * 0.5f);
        }

        blockMeter = juce::jmax (blockMeter, std::abs (wetLeft * wetGain));
        blockMeter = juce::jmax (blockMeter, std::abs (wetRight * wetGain));
    }

    const float previousMeter = visualLevel.load();
    const float targetMeter = juce::jlimit (0.0f, 1.0f, blockMeter * 2.2f);
    const float smoothedMeter = targetMeter > previousMeter
        ? targetMeter
        : previousMeter * 0.86f + targetMeter * 0.14f;

    visualLevel.store (smoothedMeter);
}

//==============================================================================
bool JuiceReverbAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* JuiceReverbAudioProcessor::createEditor()
{
    return new JuiceReverbAudioProcessorEditor (*this);
}

//==============================================================================
void JuiceReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
    else
        destData.setSize (0);
}

void JuiceReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
float JuiceReverbAudioProcessor::getVisualLevel() const noexcept
{
    return visualLevel.load();
}

float JuiceReverbAudioProcessor::getParameterValue (const char* parameterID) const noexcept
{
    if (auto* value = apvts.getRawParameterValue (parameterID))
        return value->load();

    jassertfalse;
    return 0.0f;
}

void JuiceReverbAudioProcessor::resetSmoothers()
{
    auto initialise = [this] (auto& smoother, const char* parameterID, float scale)
    {
        smoother.reset (sampleRateHz, 0.045);
        smoother.setCurrentAndTargetValue (getParameterValue (parameterID) * scale);
    };

    initialise (mixSmoother,        ParameterIDs::mix,        0.01f);
    initialise (decaySmoother,      ParameterIDs::decay,      1.0f);
    initialise (sizeSmoother,       ParameterIDs::size,       0.01f);
    initialise (preDelaySmoother,   ParameterIDs::preDelay,   1.0f);
    initialise (lowCutSmoother,     ParameterIDs::lowCut,     1.0f);
    initialise (midGainSmoother,    ParameterIDs::midGain,    1.0f);
    initialise (highCutSmoother,    ParameterIDs::highCut,    1.0f);
    initialise (widthSmoother,      ParameterIDs::width,      0.01f);
    initialise (dampingSmoother,    ParameterIDs::damping,    0.01f);
}

void JuiceReverbAudioProcessor::clearDspState()
{
    reverbTank.clear();
    preDelayBuffer.clear();
    preDelayWritePosition = 0;

    for (auto& state : lowCutStageA)
        state.clear();

    for (auto& state : lowCutStageB)
        state.clear();

    for (auto& state : highCutStageA)
        state.clear();

    for (auto& state : highCutStageB)
        state.clear();

    for (auto& state : midFilterState)
        state.clear();

    visualLevel.store (0.0f);
}

float JuiceReverbAudioProcessor::processPreDelay (int channel, float input, int delaySamples) noexcept
{
    if (preDelayBuffer.getNumSamples() <= 0)
        return input;

    const int safeChannel = juce::jlimit (0, preDelayBuffer.getNumChannels() - 1, channel);
    const int bufferSize = preDelayBuffer.getNumSamples();

    // 0ms에서는 원형 버퍼의 오래된 값이 아니라 현재 입력을 즉시 통과시켜야 합니다.
    // 기존 구현은 0ms에서 writePosition을 먼저 읽어 최대 버퍼 길이만큼 늦게 들렸습니다.
    if (delaySamples <= 0)
    {
        preDelayBuffer.setSample (safeChannel, preDelayWritePosition, input);
        return input;
    }

    int readPosition = preDelayWritePosition - delaySamples;

    while (readPosition < 0)
        readPosition += bufferSize;

    const float output = preDelayBuffer.getSample (safeChannel, readPosition);
    preDelayBuffer.setSample (safeChannel, preDelayWritePosition, input);

    return output;
}

void JuiceReverbAudioProcessor::advancePreDelay() noexcept
{
    if (preDelayBuffer.getNumSamples() <= 0)
        return;

    preDelayWritePosition = (preDelayWritePosition + 1) % preDelayBuffer.getNumSamples();
}

float JuiceReverbAudioProcessor::processHighPass (HighPassState& state,
                                                  float input,
                                                  float coefficient) noexcept
{
    const float output = coefficient * (state.previousOutput + input - state.previousInput);

    state.previousInput = input;
    state.previousOutput = output;

    return output;
}

float JuiceReverbAudioProcessor::processLowPass (LowPassState& state,
                                                 float input,
                                                 float coefficient) noexcept
{
    state.memory += coefficient * (input - state.memory);
    return state.memory;
}

float JuiceReverbAudioProcessor::processBiquad (
    BiquadState& state,
    float input,
    const BiquadCoefficients& coefficients) noexcept
{
    const float output = coefficients.b0 * input + state.z1;
    state.z1 = coefficients.b1 * input - coefficients.a1 * output + state.z2;
    state.z2 = coefficients.b2 * input - coefficients.a2 * output;
    return output;
}

float JuiceReverbAudioProcessor::calculateHighPassCoefficient (float cutoffHz, double sampleRate) noexcept
{
    const float safeCutoff = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.45), cutoffHz);
    const float dt = 1.0f / static_cast<float> (sampleRate);
    const float rc = 1.0f / (juce::MathConstants<float>::twoPi * safeCutoff);

    return rc / (rc + dt);
}

float JuiceReverbAudioProcessor::calculateLowPassCoefficient (float cutoffHz,
                                                              double sampleRate) noexcept
{
    const float safeCutoff = juce::jlimit (200.0f,
                                           static_cast<float> (sampleRate * 0.45),
                                           cutoffHz);
    return 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                            * safeCutoff / static_cast<float> (sampleRate));
}

JuiceReverbAudioProcessor::BiquadCoefficients
JuiceReverbAudioProcessor::calculateMidPeakCoefficients (float gainDb,
                                                         double sampleRate) noexcept
{
    constexpr float centreFrequencyHz = 1500.0f;
    constexpr float q = 0.75f;

    const float safeGain = juce::jlimit (-12.0f, 12.0f, gainDb);
    const float omega = juce::MathConstants<float>::twoPi
                        * centreFrequencyHz / static_cast<float> (sampleRate);
    const float alpha = std::sin (omega) / (2.0f * q);
    const float amplitude = std::pow (10.0f, safeGain / 40.0f);
    const float a0 = 1.0f + alpha / amplitude;

    BiquadCoefficients coefficients;
    coefficients.b0 = (1.0f + alpha * amplitude) / a0;
    coefficients.b1 = (-2.0f * std::cos (omega)) / a0;
    coefficients.b2 = (1.0f - alpha * amplitude) / a0;
    coefficients.a1 = (-2.0f * std::cos (omega)) / a0;
    coefficients.a2 = (1.0f - alpha / amplitude) / a0;
    return coefficients;
}

float JuiceReverbAudioProcessor::softLimitSample (float input) noexcept
{
    if (! std::isfinite (input))
        return 0.0f;

    constexpr float ceiling = 0.985f;
    return ceiling * std::tanh (input / ceiling);
}

bool JuiceReverbAudioProcessor::isFiniteAndPositive (double value) noexcept
{
    return std::isfinite (value) && value > 0.0;
}

//==============================================================================
void JuiceReverbAudioProcessor::HighPassState::clear() noexcept
{
    previousInput = 0.0f;
    previousOutput = 0.0f;
}

void JuiceReverbAudioProcessor::LowPassState::clear() noexcept
{
    memory = 0.0f;
}

void JuiceReverbAudioProcessor::BiquadState::clear() noexcept
{
    z1 = 0.0f;
    z2 = 0.0f;
}

//==============================================================================
void JuiceReverbAudioProcessor::CombFilter::prepare (int delaySamples, double sampleRate)
{
    delayBuffer.assign (static_cast<size_t> (juce::jmax (1, delaySamples)), 0.0f);
    writeIndex = 0;
    dampingMemory = 0.0f;
    delaySeconds = static_cast<float> (delayBuffer.size() / juce::jmax (1.0, sampleRate));
}

void JuiceReverbAudioProcessor::CombFilter::clear() noexcept
{
    std::fill (delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writeIndex = 0;
    dampingMemory = 0.0f;
}

float JuiceReverbAudioProcessor::CombFilter::process (float input,
                                                      float decaySeconds,
                                                      float damping) noexcept
{
    if (delayBuffer.empty())
        return input;

    const float delayed = delayBuffer[static_cast<size_t> (writeIndex)];

    // dampingMemory는 comb 내부 feedback의 고역을 조금씩 줄입니다.
    // 값이 클수록 꼬리가 어두워지고 부드러워집니다.
    dampingMemory = delayed * (1.0f - damping) + dampingMemory * damping;
    // -60dB(0.001)까지 줄어드는 시간을 Decay 노브의 초 값과 맞춥니다.
    const float safeDecaySeconds = juce::jlimit (0.1f, 30.0f, decaySeconds);
    const float feedback = std::pow (0.001f, delaySeconds / safeDecaySeconds);
    delayBuffer[static_cast<size_t> (writeIndex)] = input + dampingMemory * feedback;

    if (++writeIndex >= static_cast<int> (delayBuffer.size()))
        writeIndex = 0;

    return delayed;
}

//==============================================================================
void JuiceReverbAudioProcessor::AllpassFilter::prepare (int delaySamples)
{
    delayBuffer.assign (static_cast<size_t> (juce::jmax (1, delaySamples)), 0.0f);
    writeIndex = 0;
}

void JuiceReverbAudioProcessor::AllpassFilter::clear() noexcept
{
    std::fill (delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writeIndex = 0;
}

float JuiceReverbAudioProcessor::AllpassFilter::process (float input, float feedback) noexcept
{
    if (delayBuffer.empty())
        return input;

    const float delayed = delayBuffer[static_cast<size_t> (writeIndex)];
    const float output = delayed - input;

    delayBuffer[static_cast<size_t> (writeIndex)] = input + delayed * feedback;

    if (++writeIndex >= static_cast<int> (delayBuffer.size()))
        writeIndex = 0;

    return output;
}

//==============================================================================
void JuiceReverbAudioProcessor::ChannelReverbTank::prepare (double sampleRate, int stereoSpread)
{
    const double scale = sampleRate / referenceSampleRate;

    for (size_t index = 0; index < combs.size(); ++index)
    {
        const int delay = static_cast<int> (std::round ((combTunings[index] + stereoSpread) * scale));
        combs[index].prepare (delay, sampleRate);
    }

    for (size_t index = 0; index < allpasses.size(); ++index)
    {
        const int delay = static_cast<int> (std::round ((allpassTunings[index] + stereoSpread) * scale));
        allpasses[index].prepare (delay);
    }
}

void JuiceReverbAudioProcessor::ChannelReverbTank::clear() noexcept
{
    for (auto& comb : combs)
        comb.clear();

    for (auto& allpass : allpasses)
        allpass.clear();
}

float JuiceReverbAudioProcessor::ChannelReverbTank::process (float input,
                                                             float decaySeconds,
                                                             float damping,
                                                             float diffusion) noexcept
{
    float sum = 0.0f;

    for (auto& comb : combs)
        sum += comb.process (input, decaySeconds, damping);

    // 여러 comb가 합쳐지면 레벨이 커지므로 적당히 낮춥니다.
    float output = sum * 0.125f;

    for (auto& allpass : allpasses)
        output = allpass.process (output, diffusion);

    return output;
}

//==============================================================================
void JuiceReverbAudioProcessor::StereoReverbTank::prepare (double sampleRate)
{
    currentSampleRate = isFiniteAndPositive (sampleRate) ? sampleRate : 44100.0;

    leftTank.prepare (currentSampleRate, 0);
    rightTank.prepare (currentSampleRate, stereoSpreadSamples);
    clear();
}

void JuiceReverbAudioProcessor::StereoReverbTank::clear() noexcept
{
    leftTank.clear();
    rightTank.clear();
    lfoPhase = 0.0f;
}

void JuiceReverbAudioProcessor::StereoReverbTank::process (float inputLeft,
                                                           float inputRight,
                                                           float decaySeconds,
                                                           float damping,
                                                           float size,
                                                           float& outputLeft,
                                                           float& outputRight) noexcept
{
    const float safeSize = juce::jlimit (0.0f, 1.0f, size);

    // 아주 느린 LFO로 좌우 diffusion을 미세하게 흔들어 고정된 꼬리 느낌을 줄입니다.
    const float lfoSpeedHz = 0.055f + safeSize * 0.07f;
    lfoPhase += juce::MathConstants<float>::twoPi * lfoSpeedHz / static_cast<float> (currentSampleRate);

    if (lfoPhase > juce::MathConstants<float>::twoPi)
        lfoPhase -= juce::MathConstants<float>::twoPi;

    const float modulation = std::sin (lfoPhase);
    const float diffusion = juce::jlimit (0.42f, 0.72f, 0.50f + safeSize * 0.12f + modulation * 0.018f);

    const float mono = (inputLeft + inputRight) * 0.5f;
    const float leftInput = inputLeft * 0.62f + mono * 0.38f;
    const float rightInput = inputRight * 0.62f + mono * 0.38f;

    const float leftDecay = juce::jlimit (0.5f, 12.0f, decaySeconds * (1.0f + modulation * 0.012f));
    const float rightDecay = juce::jlimit (0.5f, 12.0f, decaySeconds * (1.0f - modulation * 0.012f));
    const float rawLeft = leftTank.process (leftInput, leftDecay, damping, diffusion);
    const float rawRight = rightTank.process (rightInput, rightDecay, damping, diffusion);

    // 약한 crossfeed가 좌우 탱크를 붙여서 큰 홀처럼 느껴지게 합니다.
    outputLeft = rawLeft * 0.88f + rawRight * 0.12f;
    outputRight = rawRight * 0.88f + rawLeft * 0.12f;
}

//==============================================================================
// JUCE가 플러그인 인스턴스를 만들 때 호출하는 진입점입니다.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuiceReverbAudioProcessor();
}
