#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// 이 anonymous namespace 안의 값들은 이 cpp 파일 안에서만 사용됩니다.
// 전역 상수처럼 쓰지만, 다른 파일과 이름이 충돌하지 않게 보호됩니다.
namespace
{
    // 파라미터 ID는 DAW automation, preset 저장, UI 연결에 사용되는 내부 이름입니다.
    //
    // 한 번 배포한 뒤에는 ID를 바꾸지 않는 것이 좋습니다.
    // ID가 바뀌면 기존 DAW 프로젝트에서 저장된 automation이 끊길 수 있습니다.
    constexpr const char* gridParamID     = "grid";
    constexpr const char* softnessParamID = "softness";

    // Circular Buffer에 저장할 최대 시간입니다.
    //
    // 느린 BPM에서 1/1 Grid를 사용해도 충분히 과거 오디오를 보관하기 위해
    // 넉넉하게 20초로 설정합니다.
    constexpr double maxCircularBufferSeconds = 20.0;

    // 비정상적인 BPM 값이 들어왔을 때 사용할 기본 BPM입니다.
    constexpr double fallbackBpm = 120.0;

    // 너무 느리거나 빠른 BPM 때문에 계산이 망가지지 않도록 제한합니다.
    constexpr double minSafeBpm = 20.0;
    constexpr double maxSafeBpm = 300.0;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BeatRepeaterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Grid는 반복될 오디오 조각의 길이를 정하는 파라미터입니다.
    //
    // 1/1  = 온음표 길이
    // 1/2  = 2분음표 길이
    // 1/4  = 4분음표 길이
    // 1/8  = 8분음표 길이
    // 1/16 = 16분음표 길이
    // 1/32 = 32분음표 길이
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        gridParamID,
        "Grid",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" },
        3)); // 기본값은 1/8입니다.

    // Softness는 클릭 노이즈 방지와 따뜻한 질감의 양입니다.
    //
    // 0%:
    // - 반복음이 비교적 선명합니다.
    // - Crossfade와 LPF 영향이 적습니다.
    //
    // 100%:
    // - 반복 구간 연결부가 더 부드럽습니다.
    // - Low-pass Filter가 더 강하게 적용되어 고역이 살짝 둥글어집니다.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        softnessParamID,
        "Softness",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        35.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
#ifndef JucePlugin_PreferredChannelConfigurations

BeatRepeaterAudioProcessor::BeatRepeaterAudioProcessor()
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

BeatRepeaterAudioProcessor::BeatRepeaterAudioProcessor()
    : apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

#endif

BeatRepeaterAudioProcessor::~BeatRepeaterAudioProcessor()
{
}

//==============================================================================
const juce::String BeatRepeaterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BeatRepeaterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool BeatRepeaterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool BeatRepeaterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double BeatRepeaterAudioProcessor::getTailLengthSeconds() const
{
    // 이 플러그인은 입력을 받아 즉시 반복 처리하는 타입입니다.
    // Reverb나 Delay처럼 재생이 멈춘 뒤 긴 꼬리가 남는 구조는 아니므로 0초로 둡니다.
    return 0.0;
}

int BeatRepeaterAudioProcessor::getNumPrograms()
{
    return 1;
}

int BeatRepeaterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BeatRepeaterAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String BeatRepeaterAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void BeatRepeaterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void BeatRepeaterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // sampleRate가 비정상적인 값으로 들어오는 경우를 방어합니다.
    // 일반적으로는 44100, 48000, 96000 같은 정상 값이 들어옵니다.
    if (isFiniteAndPositive (sampleRate))
        sampleRateHz = sampleRate;
    else
        sampleRateHz = 44100.0;

    const int numChannels = juce::jmax (getTotalNumInputChannels(),
                                        getTotalNumOutputChannels());

    // 채널 수가 0이면 오디오 처리를 할 수 없습니다.
    // 그래도 플러그인이 죽지 않도록 최소 1채널 버퍼를 준비합니다.
    const int safeNumChannels = juce::jmax (1, numChannels);

    // 최대 20초 분량의 Circular Buffer를 만듭니다.
    circularBufferSize = static_cast<int> (sampleRateHz * maxCircularBufferSeconds);

    // 혹시 sampleRate 계산이 이상해서 버퍼 길이가 너무 작아지는 것을 방어합니다.
    circularBufferSize = juce::jmax (1, circularBufferSize);

    circularBuffer.setSize (safeNumChannels, circularBufferSize);
    circularBuffer.clear();

    // Low-pass Filter 상태도 채널 수에 맞춰 준비합니다.
    lowpassState.assign (static_cast<size_t> (safeNumChannels), 0.0f);

    writePosition = 0;
    repeatStartPosition = 0;
    repeatLengthSamples = 1;
    repeatPhase = 0;

    resetRepeaterState();
}

void BeatRepeaterAudioProcessor::releaseResources()
{
    // 여기서 버퍼를 반드시 해제할 필요는 없습니다.
    // DAW가 다시 prepareToPlay를 호출하면 새 sampleRate에 맞춰 다시 준비됩니다.
    resetRepeaterState();
}

#ifndef JucePlugin_PreferredChannelConfigurations

bool BeatRepeaterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 이 예제는 mono 또는 stereo 출력만 허용합니다.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    // 일반 오디오 이펙트는 입력 채널 구성과 출력 채널 구성이 같아야 안전합니다.
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
}

#endif

//==============================================================================
void BeatRepeaterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    // 아주 작은 부동소수점 값 때문에 CPU 사용량이 튀는 문제를 방지합니다.
    juce::ScopedNoDenormals noDenormals;

    const int numInputChannels  = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numChannels       = juce::jmin (numInputChannels, numOutputChannels);
    const int numSamples        = buffer.getNumSamples();

    // 출력 채널이 입력 채널보다 많으면 남는 채널을 깨끗하게 비웁니다.
    for (int ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // 버퍼가 아직 준비되지 않았거나 채널 구성이 맞지 않으면,
    // 무리하게 처리하지 않고 원본 소리를 그대로 통과시킵니다.
    //
    // 오디오 플러그인에서는 "소리가 이상하게 나더라도 계속 실행"보다
    // "위험하면 안전하게 통과"가 훨씬 중요합니다.
    if (numChannels <= 0
        || numSamples <= 0
        || circularBufferSize <= 0
        || circularBuffer.getNumChannels() < numChannels
        || static_cast<int> (lowpassState.size()) < numChannels)
    {
        return;
    }

    const int gridIndex = static_cast<int> (apvts.getRawParameterValue (gridParamID)->load());
    const float softnessPercent = apvts.getRawParameterValue (softnessParamID)->load();

    const float softness = juce::jlimit (0.0f, 1.0f, softnessPercent / 100.0f);

    // 사용자가 Grid를 바꾸면 기존 반복 위치와 새 Grid 위치가 어긋날 수 있습니다.
    // 그래서 Grid 변경을 감지하면 다음 sample에서 기준을 다시 잡습니다.
    if (gridIndex != lastGridParameterIndex)
    {
        lastGridParameterIndex = gridIndex;
        lastGridCell = -1.0;
        repeatPhase = 0;
    }

    const double gridPpq = gridIndexToPpqLength (gridIndex);

    double bpm = fallbackBpm;
    double blockStartPpq = 0.0;
    bool hasHostPpq = false;
    bool isPlaying = true;

    // DAW의 PlayHead에서 BPM과 PPQ 위치를 가져옵니다.
    //
    // BPM:
    // - 곡의 템포입니다.
    //
    // PPQ:
    // - DAW 타임라인의 박자 위치입니다.
    // - 1.0 PPQ는 4분음표 하나입니다.
    //
    // Beat Repeater가 정확한 박자에 맞으려면 PPQ가 매우 중요합니다.
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;

            if (auto ppq = position->getPpqPosition())
            {
                blockStartPpq = *ppq;
                hasHostPpq = true;
            }

            isPlaying = position->getIsPlaying();
        }
    }

    if (! isFiniteAndPositive (bpm))
        bpm = fallbackBpm;

    bpm = juce::jlimit (minSafeBpm, maxSafeBpm, bpm);

    // DAW가 PPQ를 제공하지 않는 경우를 위한 예비 동기화 방식입니다.
    // sample 수와 BPM만으로 대략적인 PPQ를 직접 계산합니다.
    if (! hasHostPpq)
    {
        blockStartPpq = static_cast<double> (fallbackSampleCounter)
                      * bpm / (60.0 * sampleRateHz);
    }

    const double ppqPerSample = bpm / (60.0 * sampleRateHz);

    // Grid 길이를 sample 단위로 변환합니다.
    //
    // 공식:
    // grid seconds = grid PPQ * 60 / BPM
    // grid samples = grid seconds * sampleRate
    repeatLengthSamples = static_cast<int> ((gridPpq * 60.0 / bpm) * sampleRateHz);

    // 너무 작거나 너무 큰 값이 되지 않도록 제한합니다.
    repeatLengthSamples = juce::jlimit (1,
                                        juce::jmax (1, circularBufferSize / 2),
                                        repeatLengthSamples);

    // Softness가 높을수록 crossfade 길이가 길어집니다.
    // 요구사항에 맞춰 약 5~10ms 범위로 설정합니다.
    const float fadeMs = juce::jmap (softness, 5.0f, 10.0f);

    int fadeSamples = static_cast<int> (sampleRateHz * fadeMs * 0.001f);

    // 반복 조각보다 fade가 길면 오히려 소리가 이상해질 수 있으므로,
    // 반복 길이의 절반 이하로 제한합니다.
    fadeSamples = juce::jlimit (1,
                                juce::jmax (1, repeatLengthSamples / 2),
                                fadeSamples);

    // Softness가 높을수록 cutoff를 낮춰서 더 따뜻한 질감을 만듭니다.
    //
    // 0%   -> 약 20000Hz, 거의 원본에 가까움
    // 100% -> 약 3500Hz, 고역이 둥글게 정리됨
    float cutoffHz = juce::jmap (softness, 20000.0f, 3500.0f);

    // sampleRate가 낮은 환경에서도 cutoff가 Nyquist 주파수를 넘지 않도록 방어합니다.
    const float nyquistHz = static_cast<float> (sampleRateHz * 0.5);
    cutoffHz = juce::jlimit (20.0f, nyquistHz * 0.9f, cutoffHz);

    // 1-pole Low-pass Filter 계수입니다.
    //
    // coeff가 1에 가까울수록 새 sample을 빨리 따라가고,
    // coeff가 작을수록 더 부드럽게 움직입니다.
    const float lowpassCoeff = 1.0f - std::exp (
        -juce::MathConstants<float>::twoPi * cutoffHz / static_cast<float> (sampleRateHz));

    // DAW가 멈춰 있을 때는 반복 효과를 만들지 않고 입력을 그대로 통과시킵니다.
    // 하지만 Circular Buffer에는 계속 입력을 저장해둡니다.
    //
    // 이렇게 하면 사용자가 재생을 시작했을 때 내부 상태가 깨끗하게 이어집니다.
    if (! isPlaying)
    {
        resetRepeaterState();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float input = buffer.getSample (ch, sample);
                circularBuffer.setSample (ch, writePosition, input);
            }

            writePosition = wrapBufferIndex (writePosition + 1);
            ++totalSamplesWritten;
        }

        fallbackSampleCounter += numSamples;
        return;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const double currentPpq = blockStartPpq + static_cast<double> (sample) * ppqPerSample;

        // 현재 DAW 위치가 몇 번째 Grid 칸에 있는지 계산합니다.
        //
        // 예:
        // Grid가 1/4라면 gridPpq는 1.0입니다.
        // PPQ 0.0~0.999는 0번 칸,
        // PPQ 1.0~1.999는 1번 칸,
        // PPQ 2.0~2.999는 2번 칸입니다.
        const double currentGridCell = std::floor (currentPpq / gridPpq);

        if (lastGridCell < 0.0)
            lastGridCell = currentGridCell;

        // Grid 칸이 바뀌는 순간이 새 반복 조각을 잡는 시점입니다.
        //
        // 바로 직전 Grid 길이만큼의 오디오를 반복 대상으로 선택합니다.
        if (currentGridCell != lastGridCell)
        {
            repeatStartPosition = wrapBufferIndex (writePosition - repeatLengthSamples);
            repeatPhase = 0;
            lastGridCell = currentGridCell;

            // 새 반복 구간을 시작할 때 LPF 상태를 반복 조각의 첫 sample 근처로 맞춥니다.
            // 이렇게 하면 필터가 갑자기 튀는 것을 줄일 수 있습니다.
            for (int ch = 0; ch < numChannels; ++ch)
                lowpassState[static_cast<size_t> (ch)] =
                    circularBuffer.getSample (ch, repeatStartPosition);
        }

        const bool hasEnoughAudio = totalSamplesWritten >= repeatLengthSamples;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float input = buffer.getSample (ch, sample);

            // 현재 입력을 먼저 Circular Buffer에 저장합니다.
            //
            // 이 저장된 오디오는 "다음 Grid 칸"에서 반복 재생 재료가 됩니다.
            circularBuffer.setSample (ch, writePosition, input);

            float output = input;

            if (hasEnoughAudio)
            {
                const int phase = repeatPhase % repeatLengthSamples;
                const int readPosition = wrapBufferIndex (repeatStartPosition + phase);

                float repeatedSample = circularBuffer.getSample (ch, readPosition);

                //==========================================================================
                // Crossfade 처리
                //
                // 반복 구간이 끝나고 다시 처음으로 돌아갈 때 파형 값이 갑자기 바뀌면
                // "딱", "틱" 같은 클릭 노이즈가 생깁니다.
                //
                // 이를 줄이기 위해 반복 구간의 시작 부분에서:
                // - 반복 구간 끝부분의 소리
                // - 반복 구간 시작부분의 소리
                // 를 아주 짧게 섞습니다.
                //
                // 사람 귀에는 "끝과 시작이 자연스럽게 붙은 것"처럼 들립니다.
                //==========================================================================
                if (phase < fadeSamples && repeatLengthSamples > fadeSamples * 2)
                {
                    const int tailPosition = wrapBufferIndex (
                        repeatStartPosition + repeatLengthSamples - fadeSamples + phase);

                    const float tailSample = circularBuffer.getSample (ch, tailPosition);

                    float fade = static_cast<float> (phase) / static_cast<float> (fadeSamples);

                    // smoothstep 곡선입니다.
                    // 단순 직선 fade보다 시작과 끝이 더 자연스럽습니다.
                    fade = fade * fade * (3.0f - 2.0f * fade);

                    repeatedSample = tailSample * (1.0f - fade) + repeatedSample * fade;
                }

                //==========================================================================
                // Soft/Warm Low-pass Filter
                //
                // LPF는 높은 주파수, 즉 날카로운 성분을 살짝 줄이는 필터입니다.
                // Softness가 높을수록 필터된 소리를 더 많이 섞습니다.
                //==========================================================================
                float& filterMemory = lowpassState[static_cast<size_t> (ch)];

                filterMemory += lowpassCoeff * (repeatedSample - filterMemory);

                const float filteredSample = filterMemory;

                // Softness 0%   -> repeatedSample 위주
                // Softness 100% -> filteredSample 위주
                output = repeatedSample * (1.0f - softness)
                       + filteredSample  * softness;
            }

            buffer.setSample (ch, sample, output);
        }

        writePosition = wrapBufferIndex (writePosition + 1);
        repeatPhase = (repeatPhase + 1) % repeatLengthSamples;

        ++totalSamplesWritten;
    }

    fallbackSampleCounter += numSamples;
}

//==============================================================================
bool BeatRepeaterAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* BeatRepeaterAudioProcessor::createEditor()
{
    return new BeatRepeaterAudioProcessorEditor (*this);
}

//==============================================================================
void BeatRepeaterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // APVTS의 현재 파라미터 상태를 XML로 저장합니다.
    //
    // DAW 프로젝트를 저장할 때 이 함수가 호출됩니다.
    // 여기서 Grid, Softness 값이 함께 저장됩니다.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
    else
        destData.setSize (0);
}

void BeatRepeaterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // DAW 프로젝트를 다시 열 때 저장된 XML을 읽어 APVTS 상태를 복원합니다.
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState == nullptr)
        return;

    if (xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
double BeatRepeaterAudioProcessor::gridIndexToPpqLength (int gridIndex)
{
    // PPQ 기준 길이 변환표입니다.
    //
    // 1.0 PPQ = 4분음표 1개입니다.
    //
    // 1/1  = 온음표   = 4.0 PPQ
    // 1/2  = 2분음표 = 2.0 PPQ
    // 1/4  = 4분음표 = 1.0 PPQ
    // 1/8  = 8분음표 = 0.5 PPQ
    // 1/16 = 16분음표 = 0.25 PPQ
    // 1/32 = 32분음표 = 0.125 PPQ
    switch (gridIndex)
    {
        case 0:  return 4.0;
        case 1:  return 2.0;
        case 2:  return 1.0;
        case 3:  return 0.5;
        case 4:  return 0.25;
        case 5:  return 0.125;
        default: return 0.5;
    }
}

bool BeatRepeaterAudioProcessor::isFiniteAndPositive (double value) noexcept
{
    return std::isfinite (value) && value > 0.0;
}

int BeatRepeaterAudioProcessor::wrapBufferIndex (int index) const noexcept
{
    if (circularBufferSize <= 0)
        return 0;

    index %= circularBufferSize;

    if (index < 0)
        index += circularBufferSize;

    return index;
}

void BeatRepeaterAudioProcessor::resetRepeaterState() noexcept
{
    lastGridCell = -1.0;
    repeatPhase = 0;
}

//==============================================================================
// 이 함수는 JUCE가 플러그인 인스턴스를 만들 때 호출하는 진입점입니다.
// 함수 이름과 반환 타입을 바꾸면 플러그인이 로드되지 않을 수 있습니다.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeatRepeaterAudioProcessor();
}

