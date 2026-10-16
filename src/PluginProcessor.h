#pragma once
#include <JuceHeader.h>
#include "DSP.h"
#include "FxEngine.h"
#include "BackgroundNoise.h"

class WelleyzProcessor : public juce::AudioProcessor, private juce::MidiKeyboardState::Listener {
  public:
    WelleyzProcessor();
    ~WelleyzProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "welleyz"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState mApvts;
    juce::AudioProcessorValueTreeState& getApvts() { return mApvts; }
    juce::MidiKeyboardState mKeyboardState;
    juce::MidiKeyboardState& getKeyboardState() { return mKeyboardState; }
    void flushKeyboardDisplay();
    void setUiPitchBend(int value) {
        mUiPitchValue.store(juce::jlimit(0, 16383, value));
        mUiPitchRevision.fetch_add(1);
    }
    void setUiModWheel(int value) {
        mUiModValue.store(juce::jlimit(0, 127, value));
        mUiModRevision.fetch_add(1);
    }

    // Вызывается кнопкой "RANDOM" из UI — музыкальная рандомизация
    void randomizeParameters(int category, bool factoryPreset = false);
    void resetToDefault();
    bool canRestorePreviousSound() const { return !mSoundHistory.empty(); }
    bool restorePreviousSound();

    // Sound presets use the JSON .wlyz format.
    bool savePresetToFile(const juce::File& file);
    bool loadPresetFromFile(const juce::File& file);
    bool loadPresetFromData(const void* data, size_t size);
    static juce::File presetDirectory();
    static juce::File recordingDirectory();
    bool startRecording();
    juce::File stopRecording();
    bool recordingFailed() const { return mRecordingOverflow.load(); }
    bool isRecording() const { return mRecordingWriter.load() != nullptr; }
    void setStandaloneBpm(int bpm) { mStandaloneBpm.store(juce::jlimit(40, 240, bpm)); }
    int standaloneBpm() const { return mStandaloneBpm.load(); }
    void setMetronomeEnabled(bool enabled) { mMetronomeEnabled.store(enabled); }
    bool metronomeEnabled() const { return mMetronomeEnabled.load(); }

    // Window state is stored separately from the automatable sound parameters.
    int mUiPage = 0;      // 0: Random, 1: Synth, 2: FX Rack
    int mUiSoundType = 1; // Selected SOUND TYPE category, must survive editor close/reopen.
    void selectSoundType(int category);
    int selectedSoundType() const;
    float mUiZoom = 1.0f;
    bool mUiZoomCustomized = false;
    juce::String mUiPreset;
    void applyStateTree(const juce::ValueTree& tree);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  private:
    std::atomic<int> mStandaloneBpm{120};
    std::atomic<bool> mMetronomeEnabled{false};
    double mBeatPosition = 0.0;
    int mClickSamples = 0;
    int mBeatIndex = 0;
    float mClickAmplitude = 0.0f;
    std::unique_ptr<juce::TimeSliceThread> mRecordingThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> mRecorder;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> mRecordingWriter{nullptr};
    std::atomic<int> mActiveRecordCallbacks{0};
    std::atomic<bool> mRecordingOverflow{false};
    juce::File mRecordingTempFile;
    struct KeyboardEvent {
        int mNote = 0;
        float mVelocity = 0.0f;
        bool mOn = false;
    };
    static constexpr int kKeyboardQueueCapacity = 512;
    juce::AbstractFifo mKeyboardInputFifo{kKeyboardQueueCapacity};
    juce::AbstractFifo mKeyboardDisplayFifo{kKeyboardQueueCapacity};
    std::array<KeyboardEvent, kKeyboardQueueCapacity> mKeyboardInput{};
    std::array<KeyboardEvent, kKeyboardQueueCapacity> mKeyboardDisplay{};
    bool mMirroringHostMidi = false;
    std::atomic<bool> mKeyboardInputOverflow{false};
    std::atomic<bool> mKeyboardDisplayOverflow{false};
    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float velocity) override;
    void enqueueDisplay(int note, float velocity, bool on);
    // --- Один голос полифонии ---
    struct Voice {
        bool mActive = false;
        bool mHeld = false;
        int mNote = -1;
        float mVelocity = 0.0f;
        std::array<std::array<welleyz::dsp::OscState, 8>, 3> mOsc;
        welleyz::dsp::Adsr mEnvelope;
        double mAge = 0.0; // для voice stealing (старший гасится)
        double mCurrentPitch = 60.0;
        double mPitchStep = 0.0;
        int mGlideSamples = 0;
    };
    static constexpr int kMaxVoices = 16;
    std::array<Voice, kMaxVoices> mVoices;
    double mVoiceClock = 0.0;
    int mLastMidiNote = -1;

    // Глобальный LFO (вибрато) + фильтр + лампа + крашер
    double mVibratoPhase = 0.0;
    juce::dsp::StateVariableTPTFilter<float> mFilter;
    WelleyzFx mEffects;
    juce::dsp::ProcessSpec mProcessSpec{};
    welleyz::dsp::TubeStage mTubeLeft, mTubeRight;
    welleyz::dsp::BitCrusher mCrusher;
    welleyz::backgroundNoise::Generator mNoiseLeft, mNoiseRight;
    float mCrushHoldLeft = 0.0f, mCrushHoldRight = 0.0f;
    int mCrushCounter = 0;

    // Простой арпеджиатор: запоминаем held-ноты, циклично триггерим
    juce::Array<int> mHeldNotes;
    juce::Array<int> mArpOrder;
    int mArpIndex = 0;
    double mArpPhase = 0.0; // в сэмплах до следующего шага
    int mArpCurrentNote = -1;

    // Сглаженные значения
    juce::SmoothedValue<float> mSmoothMaster, mSmoothDriveMix;
    juce::SmoothedValue<float> mSmoothPitchBend, mSmoothModWheel;
    std::atomic<int> mUiPitchValue{8192}, mUiModValue{0};
    std::atomic<unsigned> mUiPitchRevision{0}, mUiModRevision{0};
    unsigned mConsumedPitchRevision = 0, mConsumedModRevision = 0;

    struct SoundSnapshot {
        juce::ValueTree mState;
        juce::String mPreset;
        int mSoundType = 1;
    };
    std::vector<SoundSnapshot> mSoundHistory;
    void rememberCurrentSound();

    Voice* findFreeVoice(int note);
    void noteOnInternal(int note, float vel, bool legato = false, int previousHeldNote = -1);
    void noteOffInternal(int note);
    void allNotesOff();
    void resetVoiceState();
    void resetPlaybackState();

    float getParam(juce::StringRef id) const;
    int getParamChoice(juce::StringRef id) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WelleyzProcessor)
};
