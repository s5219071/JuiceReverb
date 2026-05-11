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

    constexpr float minimumDuckingGain = 0.18f;
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

    auto widthRange = juce::NormalisableRange<float> (50.0f, 200.0f, 0.1f);

    // Mix: 최종 dry/wet 비율입니다. 0%는 원음만, 100%는 리버브만 들립니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::mix, 1 }, "Mix", percentRange, 35.0f));

    // Decay: 리버브 꼬리가 얼마나 오래 남는지 정합니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::decay, 1 }, "Decay", decayRange, 4.5f));

    // Size: 공간의 체감 크기입니다. feedback과 확산감에 함께 영향을 줍니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::size, 1 }, "Size", percentRange, 78.0f));

    // Pre Delay: 원음 뒤에 리버브가 살짝 늦게 붙게 해서 트랜스 킥/리드의 앞부분을 살립니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::preDelay, 1 }, "Pre Delay", preDelayRange, 32.0f));

    // Low Cut: 리버브 wet에만 걸리는 저역 정리 필터입니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::lowCut, 1 }, "Low Cut", lowCutRange, 150.0f));

    // Ducking: 입력이 강할 때 wet만 자동으로 줄이는 내부 사이드체인 양입니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::ducking, 1 }, "Ducking", percentRange, 45.0f));

    // Saturation: 리버브 꼬리에 따뜻한 배음을 더하는 Juice 노브입니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIDs::saturation, 1 }, "Juice", percentRange, 24.0f));

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
    duckingSmoother.setTargetValue (getParameterValue (ParameterIDs::ducking) / 100.0f);
    saturationSmoother.setTargetValue (getParameterValue (ParameterIDs::saturation) / 100.0f);
    widthSmoother.setTargetValue (getParameterValue (ParameterIDs::width) / 100.0f);
    dampingSmoother.setTargetValue (getParameterValue (ParameterIDs::damping) / 100.0f);

    const float attackCoefficient = std::exp (-1.0f / static_cast<float> (0.004 * sampleRateHz));
    const float releaseCoefficient = std::exp (-1.0f / static_cast<float> (0.180 * sampleRateHz));

    float blockMeter = 0.0f;
    float lastDuckingDepth = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryLeft = buffer.getSample (0, sample);
        const float dryRight = channelsToProcess > 1 ? buffer.getSample (1, sample) : dryLeft;

        const float mix = juce::jlimit (0.0f, 1.0f, mixSmoother.getNextValue());
        const float decaySeconds = decaySmoother.getNextValue();
        const float size = juce::jlimit (0.0f, 1.0f, sizeSmoother.getNextValue());
        const float preDelayMs = preDelaySmoother.getNextValue();
        const float lowCutHz = lowCutSmoother.getNextValue();
        const float ducking = juce::jlimit (0.0f, 1.0f, duckingSmoother.getNextValue());
        const float saturation = juce::jlimit (0.0f, 1.0f, saturationSmoother.getNextValue());
        const float width = juce::jlimit (0.5f, 2.0f, widthSmoother.getNextValue());
        const float damping = juce::jlimit (0.0f, 1.0f, dampingSmoother.getNextValue());

        // decaySeconds를 직접 feedback으로 쓰면 불안정해질 수 있으므로 안전한 범위로 변환합니다.
        const float decayNormalised = juce::jlimit (0.0f, 1.0f, (decaySeconds - 0.5f) / 11.5f);
        const float feedback = juce::jlimit (0.62f, 0.965f,
                                             0.62f + decayNormalised * 0.285f + size * 0.06f);

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
        reverbTank.process (preDelayedLeft, preDelayedRight, feedback, damping, size, wetLeft, wetRight);

        wetLeft = saturateSample (wetLeft, saturation);
        wetRight = saturateSample (wetRight, saturation);

        // 리버브에만 12dB/oct 정도의 로우컷을 적용합니다.
        // 원음 저역은 건드리지 않으므로 킥과 베이스의 중심이 덜 무너집니다.
        const float highPassCoefficient = calculateHighPassCoefficient (lowCutHz, sampleRateHz);
        wetLeft = processHighPass (lowCutStageA[0], wetLeft, highPassCoefficient);
        wetLeft = processHighPass (lowCutStageB[0], wetLeft, highPassCoefficient);
        wetRight = processHighPass (lowCutStageA[1], wetRight, highPassCoefficient);
        wetRight = processHighPass (lowCutStageB[1], wetRight, highPassCoefficient);

        // Wet 전용 Mid-Side Width입니다.
        // Dry에는 적용하지 않으므로 원음의 중앙 이미지는 더 안정적입니다.
        const float mid = (wetLeft + wetRight) * 0.5f;
        const float side = (wetLeft - wetRight) * 0.5f * width;
        wetLeft = mid + side;
        wetRight = mid - side;

        // Internal Ducking:
        // 입력이 커지면 wet만 내려서 보컬/리드/킥의 앞부분이 리버브에 묻히지 않게 합니다.
        const float inputPeak = juce::jmax (std::abs (dryLeft), std::abs (dryRight));

        if (inputPeak > duckingEnvelope)
            duckingEnvelope = attackCoefficient * duckingEnvelope + (1.0f - attackCoefficient) * inputPeak;
        else
            duckingEnvelope = releaseCoefficient * duckingEnvelope + (1.0f - releaseCoefficient) * inputPeak;

        const float duckingDetector = smoothStep (juce::jlimit (0.0f, 1.0f, (duckingEnvelope - 0.035f) / 0.40f));
        const float duckingGain = juce::jlimit (minimumDuckingGain, 1.0f,
                                                1.0f - ducking * duckingDetector * 0.72f);

        lastDuckingDepth = 1.0f - duckingGain;

        // Equal-power에 가까운 dry/wet 크로스페이드입니다.
        const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin (mix * juce::MathConstants<float>::halfPi) * duckingGain;

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
    visualDuckingDepth.store (juce::jlimit (0.0f, 1.0f, lastDuckingDepth));
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

float JuiceReverbAudioProcessor::getDuckingDepth() const noexcept
{
    return visualDuckingDepth.load();
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
    initialise (duckingSmoother,    ParameterIDs::ducking,    0.01f);
    initialise (saturationSmoother, ParameterIDs::saturation, 0.01f);
    initialise (widthSmoother,      ParameterIDs::width,      0.01f);
    initialise (dampingSmoother,    ParameterIDs::damping,    0.01f);
}

void JuiceReverbAudioProcessor::clearDspState()
{
    reverbTank.clear();
    preDelayBuffer.clear();
    preDelayWritePosition = 0;
    duckingEnvelope = 0.0f;

    for (auto& state : lowCutStageA)
        state.clear();

    for (auto& state : lowCutStageB)
        state.clear();

    visualLevel.store (0.0f);
    visualDuckingDepth.store (0.0f);
}

float JuiceReverbAudioProcessor::processPreDelay (int channel, float input, int delaySamples) noexcept
{
    if (preDelayBuffer.getNumSamples() <= 0)
        return input;

    const int safeChannel = juce::jlimit (0, preDelayBuffer.getNumChannels() - 1, channel);
    const int bufferSize = preDelayBuffer.getNumSamples();

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

float JuiceReverbAudioProcessor::calculateHighPassCoefficient (float cutoffHz, double sampleRate) noexcept
{
    const float safeCutoff = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.45), cutoffHz);
    const float dt = 1.0f / static_cast<float> (sampleRate);
    const float rc = 1.0f / (juce::MathConstants<float>::twoPi * safeCutoff);

    return rc / (rc + dt);
}

float JuiceReverbAudioProcessor::saturateSample (float input, float amount) noexcept
{
    const float safeAmount = juce::jlimit (0.0f, 1.0f, amount);

    if (safeAmount <= 0.0001f)
        return input;

    // tanh는 신호가 커질수록 부드럽게 눌러주는 곡선입니다.
    // drive가 클수록 배음과 밀도가 늘어납니다.
    const float drive = 1.0f + safeAmount * 3.2f;
    const float shaped = std::tanh (input * drive) / std::tanh (drive);

    // 원본과 새추레이션 신호를 섞어서 지나치게 찌그러지는 것을 막습니다.
    return input * (1.0f - safeAmount * 0.72f) + shaped * (safeAmount * 0.72f);
}

float JuiceReverbAudioProcessor::softLimitSample (float input) noexcept
{
    if (! std::isfinite (input))
        return 0.0f;

    constexpr float ceiling = 0.985f;
    return ceiling * std::tanh (input / ceiling);
}

float JuiceReverbAudioProcessor::smoothStep (float value) noexcept
{
    const float x = juce::jlimit (0.0f, 1.0f, value);
    return x * x * (3.0f - 2.0f * x);
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

//==============================================================================
void JuiceReverbAudioProcessor::CombFilter::prepare (int delaySamples)
{
    delayBuffer.assign (static_cast<size_t> (juce::jmax (1, delaySamples)), 0.0f);
    writeIndex = 0;
    dampingMemory = 0.0f;
}

void JuiceReverbAudioProcessor::CombFilter::clear() noexcept
{
    std::fill (delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writeIndex = 0;
    dampingMemory = 0.0f;
}

float JuiceReverbAudioProcessor::CombFilter::process (float input,
                                                      float feedback,
                                                      float damping) noexcept
{
    if (delayBuffer.empty())
        return input;

    const float delayed = delayBuffer[static_cast<size_t> (writeIndex)];

    // dampingMemory는 comb 내부 feedback의 고역을 조금씩 줄입니다.
    // 값이 클수록 꼬리가 어두워지고 부드러워집니다.
    dampingMemory = delayed * (1.0f - damping) + dampingMemory * damping;
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
        combs[index].prepare (delay);
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
                                                             float feedback,
                                                             float damping,
                                                             float diffusion) noexcept
{
    float sum = 0.0f;

    for (auto& comb : combs)
        sum += comb.process (input, feedback, damping);

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
                                                           float feedback,
                                                           float damping,
                                                           float size,
                                                           float& outputLeft,
                                                           float& outputRight) noexcept
{
    const float safeSize = juce::jlimit (0.0f, 1.0f, size);

    // 아주 느린 LFO로 좌우 feedback과 diffusion을 미세하게 흔듭니다.
    // 딜레이 시간을 직접 흔드는 방식보다 단순하지만, 꼬리가 고정되어 들리는 느낌을 줄여줍니다.
    const float lfoSpeedHz = 0.055f + safeSize * 0.07f;
    lfoPhase += juce::MathConstants<float>::twoPi * lfoSpeedHz / static_cast<float> (currentSampleRate);

    if (lfoPhase > juce::MathConstants<float>::twoPi)
        lfoPhase -= juce::MathConstants<float>::twoPi;

    const float modulation = std::sin (lfoPhase);
    const float leftFeedback = juce::jlimit (0.5f, 0.975f, feedback + modulation * 0.0045f);
    const float rightFeedback = juce::jlimit (0.5f, 0.975f, feedback - modulation * 0.0045f);
    const float diffusion = juce::jlimit (0.42f, 0.72f, 0.50f + safeSize * 0.12f + modulation * 0.018f);

    const float mono = (inputLeft + inputRight) * 0.5f;
    const float leftInput = inputLeft * 0.62f + mono * 0.38f;
    const float rightInput = inputRight * 0.62f + mono * 0.38f;

    const float rawLeft = leftTank.process (leftInput, leftFeedback, damping, diffusion);
    const float rawRight = rightTank.process (rightInput, rightFeedback, damping, diffusion);

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
