#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

//==============================================================================
// JuiceReverbAudioProcessor
//
// 이 클래스는 플러그인의 "오디오 엔진"입니다.
// DAW가 보내는 오디오 블록을 받아서 리버브를 만들고, 다시 DAW로 돌려보냅니다.
//
// 설계 철학:
// - Dry 경로는 최대한 보존해서 원음의 펀치가 무너지지 않게 합니다.
// - Wet 경로에만 리버브, 로우컷, 새추레이션, 스테레오 확장을 적용합니다.
// - 입력 신호가 강할 때 wet만 자동으로 내려주는 Internal Ducking을 넣습니다.
// - 모든 노브 값은 AudioProcessorValueTreeState(APVTS)로 관리합니다.
//==============================================================================
class JuiceReverbAudioProcessor final : public juce::AudioProcessor
{
public:
    JuiceReverbAudioProcessor();
    ~JuiceReverbAudioProcessor() override;

    // DAW가 재생을 시작하거나 샘플레이트가 바뀔 때 호출됩니다.
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    // DAW가 플러그인 리소스를 정리할 때 호출됩니다.
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    // 실제 오디오 처리가 일어나는 가장 중요한 함수입니다.
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    // 플러그인 UI 생성 관련 함수입니다.
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    // DAW 프로젝트 저장/불러오기 때 APVTS 상태를 저장하고 복원합니다.
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // UI가 슬라이더와 파라미터를 연결할 수 있도록 public으로 둡니다.
    juce::AudioProcessorValueTreeState apvts;

    // UI의 Juice Tank가 읽는 간단한 미터 값입니다.
    // 오디오 스레드와 UI 스레드가 동시에 접근하므로 atomic을 사용합니다.
    float getVisualLevel() const noexcept;
    float getDuckingDepth() const noexcept;

private:
    //==============================================================================
    // 파라미터 ID
    //
    // 이 문자열은 DAW 자동화와 프리셋 저장에 쓰입니다.
    // 한 번 배포한 뒤에는 바꾸지 않는 것이 좋습니다.
    struct ParameterIDs
    {
        static constexpr const char* mix        = "mix";
        static constexpr const char* decay      = "decay";
        static constexpr const char* size       = "size";
        static constexpr const char* preDelay   = "preDelay";
        static constexpr const char* lowCut     = "lowCut";
        static constexpr const char* ducking    = "ducking";
        static constexpr const char* saturation = "saturation";
        static constexpr const char* width      = "width";
        static constexpr const char* damping    = "damping";
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // 작은 DSP 부품들
    //
    // CombFilter와 AllpassFilter는 고전적인 Schroeder/Freeverb 계열 리버브의
    // 기본 재료입니다. 여러 지연선이 서로 다른 시간으로 울리며 풍성한 꼬리를 만듭니다.
    class CombFilter
    {
    public:
        void prepare (int delaySamples);
        void clear() noexcept;
        float process (float input, float feedback, float damping) noexcept;

    private:
        std::vector<float> delayBuffer;
        int writeIndex = 0;
        float dampingMemory = 0.0f;
    };

    class AllpassFilter
    {
    public:
        void prepare (int delaySamples);
        void clear() noexcept;
        float process (float input, float feedback) noexcept;

    private:
        std::vector<float> delayBuffer;
        int writeIndex = 0;
    };

    // 한 채널의 리버브 탱크입니다.
    // Comb 여러 개를 합친 뒤 Allpass로 퍼뜨려 밀도 높은 공간감을 만듭니다.
    class ChannelReverbTank
    {
    public:
        void prepare (double sampleRate, int stereoSpreadSamples);
        void clear() noexcept;
        float process (float input, float feedback, float damping, float diffusion) noexcept;

    private:
        std::array<CombFilter, 8> combs;
        std::array<AllpassFilter, 4> allpasses;
    };

    // 좌우 두 개의 탱크를 묶은 스테레오 리버브입니다.
    class StereoReverbTank
    {
    public:
        void prepare (double sampleRate);
        void clear() noexcept;

        void process (float inputLeft,
                      float inputRight,
                      float feedback,
                      float damping,
                      float size,
                      float& outputLeft,
                      float& outputRight) noexcept;

    private:
        ChannelReverbTank leftTank;
        ChannelReverbTank rightTank;

        double currentSampleRate = 44100.0;
        float lfoPhase = 0.0f;
    };

    // 1-pole high-pass 필터 상태입니다.
    // Wet 리버브의 저역 뭉침을 줄이기 위해 두 번 직렬로 사용합니다.
    struct HighPassState
    {
        float previousInput = 0.0f;
        float previousOutput = 0.0f;

        void clear() noexcept;
    };

    //==============================================================================
    // 내부 처리 함수
    float getParameterValue (const char* parameterID) const noexcept;
    void resetSmoothers();
    void clearDspState();

    float processPreDelay (int channel, float input, int delaySamples) noexcept;
    void advancePreDelay() noexcept;

    static float processHighPass (HighPassState& state, float input, float coefficient) noexcept;
    static float calculateHighPassCoefficient (float cutoffHz, double sampleRate) noexcept;
    static float saturateSample (float input, float amount) noexcept;
    static float softLimitSample (float input) noexcept;
    static float smoothStep (float value) noexcept;
    static bool isFiniteAndPositive (double value) noexcept;

    //==============================================================================
    // DSP 상태
    StereoReverbTank reverbTank;

    juce::AudioBuffer<float> preDelayBuffer;
    int preDelayWritePosition = 0;
    int maxPreDelaySamples = 1;

    std::array<HighPassState, 2> lowCutStageA;
    std::array<HighPassState, 2> lowCutStageB;

    double sampleRateHz = 44100.0;
    float duckingEnvelope = 0.0f;

    // 파라미터가 갑자기 튀면 클릭이 생길 수 있으므로 짧게 부드럽게 움직입니다.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> decaySmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sizeSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preDelaySmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowCutSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> duckingSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> saturationSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dampingSmoother;

    std::atomic<float> visualLevel { 0.0f };
    std::atomic<float> visualDuckingDepth { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceReverbAudioProcessor)
};
