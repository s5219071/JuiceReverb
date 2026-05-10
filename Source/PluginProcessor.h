#pragma once

#include <JuceHeader.h>

//==============================================================================
// BeatRepeaterAudioProcessor
//
// 이 클래스는 플러그인의 "두뇌이자 엔진"입니다.
//
// 쉽게 말하면:
// - DAW에서 오디오를 받아오고
// - 일정 길이만큼 오디오를 기억해두고
// - DAW 박자에 맞춰 그 조각을 반복 재생하고
// - Softness 값에 따라 클릭 노이즈를 줄이고 따뜻한 질감을 만듭니다.
//
// UI, 즉 버튼/노브/콤보박스는 PluginEditor에서 만들지만,
// 실제 소리 처리는 전부 이 Processor에서 일어납니다.
//==============================================================================
class BeatRepeaterAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    BeatRepeaterAudioProcessor();
    ~BeatRepeaterAudioProcessor() override;

    //==============================================================================
    // DAW가 플러그인에게 "이제 오디오 처리를 시작할 준비를 해라"라고 알려줄 때 호출됩니다.
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    // DAW가 플러그인을 멈추거나 리소스를 정리할 때 호출됩니다.
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    // 플러그인이 어떤 채널 구성을 지원하는지 검사합니다.
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    //==============================================================================
    // 실제 오디오가 처리되는 가장 중요한 함수입니다.
    //
    // DAW는 오디오를 아주 작은 덩어리(block) 단위로 계속 보내줍니다.
    // processBlock은 그 덩어리를 받아서 가공한 뒤 다시 DAW로 돌려보냅니다.
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    // UI Editor 생성 관련 함수입니다.
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    // 플러그인의 기본 정보입니다.
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    // 기본 프리셋 관련 함수입니다.
    //
    // 이 예제에서는 별도의 프리셋 프로그램을 만들지 않고,
    // APVTS(AudioProcessorValueTreeState)가 파라미터 저장을 담당합니다.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    // DAW 프로젝트 저장/불러오기 때 파라미터 값을 저장하고 복원합니다.
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // APVTS는 플러그인의 파라미터 관리자입니다.
    //
    // Grid, Softness 같은 값을 한 곳에서 관리합니다.
    // 나중에 PluginEditor에서 ComboBox, Slider를 연결할 때도 이 apvts를 사용합니다.
    //
    // public으로 둔 이유:
    // PluginEditor에서 audioProcessor.apvts 형태로 접근해 UI와 연결하기 쉽도록 하기 위해서입니다.
    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    // APVTS에 등록할 파라미터 목록을 만듭니다.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Grid 콤보박스 인덱스를 PPQ 길이로 바꿉니다.
    //
    // PPQ는 "4분음표 기준 박자 위치"입니다.
    // 1.0 PPQ = 4분음표 1개입니다.
    static double gridIndexToPpqLength (int gridIndex);

    // BPM, sampleRate 같은 값이 정상적인 숫자인지 검사합니다.
    static bool isFiniteAndPositive (double value) noexcept;

    // Circular Buffer 인덱스가 범위를 벗어났을 때 다시 정상 범위로 감싸줍니다.
    int wrapBufferIndex (int index) const noexcept;

    // 내부 재생 상태를 안전하게 초기화합니다.
    void resetRepeaterState() noexcept;

    //==============================================================================
    // Circular Buffer
    //
    // Circular Buffer는 "빙글빙글 도는 녹음 테이프"라고 생각하면 됩니다.
    //
    // 예를 들어 버퍼 크기가 10000 samples라면:
    // - 0번 위치부터 9999번 위치까지 오디오를 저장합니다.
    // - 끝에 도착하면 다시 0번 위치부터 덮어씁니다.
    //
    // Beat Repeater는 이 버퍼에 과거 오디오를 계속 저장해두고,
    // DAW 박자선에 맞춰 특정 구간을 다시 읽어서 반복 재생합니다.
    juce::AudioBuffer<float> circularBuffer;

    // Circular Buffer의 전체 길이입니다.
    int circularBufferSize = 0;

    // 지금 입력 오디오를 Circular Buffer의 어느 위치에 쓸지 나타냅니다.
    int writePosition = 0;

    // 반복 재생할 오디오 조각이 Circular Buffer 안에서 시작되는 위치입니다.
    int repeatStartPosition = 0;

    // 현재 Grid 설정을 sample 단위로 바꾼 길이입니다.
    //
    // 예:
    // 120 BPM, 44.1kHz에서 1/4 note는 약 22050 samples입니다.
    int repeatLengthSamples = 1;

    // 반복 조각 안에서 현재 몇 번째 sample을 읽고 있는지 나타냅니다.
    int repeatPhase = 0;

    //==============================================================================
    // DAW와 동기화하기 위한 상태값입니다.

    // 현재 sample rate입니다. 예: 44100, 48000, 96000
    double sampleRateHz = 44100.0;

    // 이전 processBlock에서 확인한 Grid 칸 번호입니다.
    //
    // DAW의 PPQ 위치를 Grid 길이로 나누면 현재 몇 번째 Grid 칸인지 알 수 있습니다.
    // 이 값이 바뀌는 순간이 "새 반복 조각을 잡아야 하는 시점"입니다.
    double lastGridCell = -1.0;

    // 사용자가 Grid 파라미터를 바꿨는지 감지하기 위한 값입니다.
    int lastGridParameterIndex = -1;

    // 플러그인이 지금까지 Circular Buffer에 써온 총 sample 수입니다.
    //
    // 아직 충분한 오디오가 쌓이지 않았는데 반복을 시도하면 쓰레기 값이 나올 수 있으므로,
    // 이 값을 이용해 "반복할 만큼 과거 오디오가 충분한지" 확인합니다.
    int64_t totalSamplesWritten = 0;

    // DAW가 PPQ 위치를 제공하지 않는 예외 상황을 위한 임시 sample counter입니다.
    //
    // 대부분의 DAW는 PPQ를 제공하지만,
    // 일부 환경이나 테스트 상황에서는 값이 없을 수 있습니다.
    int64_t fallbackSampleCounter = 0;

    //==============================================================================
    // Soft/Warm Low-pass Filter 상태입니다.
    //
    // 1-pole LPF는 바로 이전 출력값을 기억해야 합니다.
    // 채널마다 이전 값이 다르므로 vector로 채널 수만큼 보관합니다.
    std::vector<float> lowpassState;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatRepeaterAudioProcessor)
};

