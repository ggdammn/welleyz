#pragma once
#include <JuceHeader.h>
#include "Theme.h"
#include "StudioFonts.h"
#include "BackgroundNoise.h"

class WelleyzProcessor;

// Дизайн-размер (layout считается от него, масштабируется под окно)
constexpr int kEditorDesignWidth = 1280;
constexpr int kEditorDesignHeight = 760;

// ============================================================
// welleyz — studio LookAndFeel driven by DesignTokens and ThemeManager
// ============================================================
struct WelleyzLookAndFeel : public juce::LookAndFeel_V4 {
    WelleyzLookAndFeel();

    void setTheme(const WaveTheme* t) { theme = t; }
    void setUiScale(float s) { uiScale = s; }

    juce::Font display(float h) const { return welleyz::fonts::display(h); }
    juce::Font mono(float h) const { return welleyz::fonts::mono(h); }

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start,
                          float end, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                      juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& bg, bool hover,
                              bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float position, float min,
                          float max, const juce::Slider::SliderStyle, juce::Slider&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;

    const WaveTheme* theme = nullptr;
    float uiScale = 1.0f;
    float randomizeRotation = 0.0f;
    bool mRecordBlinkBright = true;
};

struct PowerDot : juce::Button {
    explicit PowerDot(juce::String name) : juce::Button(name) { setClickingTogglesState(true); }
    void paintButton(juce::Graphics&, bool hover, bool down) override;
};

class WelleyzEditor : public juce::AudioProcessorEditor,
                      private juce::ChangeListener,
                      private juce::Timer {
  public:
    explicit WelleyzEditor(WelleyzProcessor&);
    ~WelleyzEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    void applyTheme(int idx);
    void setZoom(float z);
    static float initialZoomForArea(juce::Rectangle<int> workArea, float savedZoom,
                                    bool customized);
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override;
    void focusOfChildComponentChanged(FocusChangeType) override;

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshAccent();
    double mRandomizeStartMs = 0.0;
    int mPendingFocusRestores = 0;
    WelleyzProcessor& mProcessor;
    WelleyzLookAndFeel mLookAndFeel;
    WaveTheme mCurrentTheme{makeWaveTheme(ThemeManager::instance())};

    using SliderA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderA>> mSliderAttachments;
    std::vector<std::unique_ptr<ComboA>> mComboAttachments;
    std::vector<std::unique_ptr<ButtonA>> mButtonAttachments;
    std::vector<juce::Slider*> mSliders;

    juce::TextButton mRandomButton{"RANDOMIZE"};
    juce::TextButton mBackButton{"BACK"};
    juce::TextButton mResetButton{"RESET"};
    juce::TextButton mSaveButton{"SAVE"};
    juce::TextButton mLoadButton{"LOAD"};
    juce::TextButton mZoomButton{"100%"}, mSettingsButton{"SETTINGS"}, mColorsButton{"COLORS"};
    juce::TextButton mManualButton{"MANUAL"}, mRecordButton{"REC"}, mMetronomeButton{"MET"};
    struct TempoPanel : juce::Component {
        WelleyzProcessor& mProcessor;
        juce::TextEditor mBpmEditor;
        juce::TextButton mToggleButton{"OFF"}, mCloseButton{"X"};
        std::function<void(bool)> onMetronomeChange;
        explicit TempoPanel(WelleyzProcessor& processor);
        void paint(juce::Graphics&) override;
        void resized() override;
    } mTempoPanel{mProcessor};
    std::unique_ptr<juce::Component> mManualOverlay;
    juce::TextButton mRandomTab{"RANDOM"}, mOscTab{"OSC"}, mFxTab{"FX"};
    juce::Array<juce::File> mPresetFiles;
    struct BundledPreset {
        juce::String name, resource;
    };
    std::vector<BundledPreset> mFactoryPresets;
    std::unique_ptr<juce::FileChooser> mFileChooser;
    struct SoundTypePanel : juce::Component {
        struct Row : juce::Button {
            juce::String name;
            int category;
            Row(juce::String text, int index);
            void paintButton(juce::Graphics&, bool hover, bool down) override;
        };
        std::vector<std::unique_ptr<Row>> rows;
        int selected = 1;
        std::function<void(int)> onSelect;
        SoundTypePanel();
        void paint(juce::Graphics&) override;
        void resized() override;
        void choose(int category);
    } mSoundTypes;

    struct PresetBrowser : juce::Component {
        struct Entry {
            juce::String id, name, resource, category;
            juce::File file;
        };
        struct List : juce::Component {
            PresetBrowser& owner;
            int hovered = -1;
            int focused = 0;
            explicit List(PresetBrowser& b) : owner(b) {
                setName("Preset list");
                setWantsKeyboardFocus(true);
            }
            void paint(juce::Graphics&) override;
            void mouseUp(const juce::MouseEvent&) override;
            void mouseMove(const juce::MouseEvent&) override;
            void mouseExit(const juce::MouseEvent&) override {
                hovered = -1;
                repaint();
            }
            bool keyPressed(const juce::KeyPress&) override;
        } list{*this};
        juce::TextEditor search;
        juce::Viewport viewport;
        std::vector<std::unique_ptr<juce::TextButton>> chips;
        std::vector<Entry> entries;
        std::vector<int> visibleIndices;
        juce::String selectedId, categoryFilter{"All"};
        std::function<void(const Entry&)> onSelect;
        PresetBrowser();
        void setEntries(std::vector<Entry> values);
        void refreshList();
        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    } mBrowser;
    juce::Slider mPitchSlider, mModSlider, mMasterVolume;
    bool mKeyboardInitialPositionPending = true;
    std::unique_ptr<SliderA> mMasterVolumeAttachment;
    void loadBrowserPreset(const PresetBrowser::Entry&);
    void showAccentSettings();
    void showColors();
    void showManual();
    void finishRecording();
    void refreshPresets();
    void refreshBackButton();
    void showPage(int page);

    // --- Ручка: подпись рисуется (гравировка), значение — textbox слайдера ---
    struct Knob : public juce::Component {
        juce::Slider slider;
        juce::String label;
        Knob(const juce::String& name) : label(name) {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 17);
            addAndMakeVisible(slider);
        }
        void paint(juce::Graphics& g) override;
        void resized() override {
            const float scale = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
            const int top = juce::roundToInt(20.0f * scale);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                   juce::jmax(32, juce::roundToInt(64.0f * scale)),
                                   juce::jmax(10, juce::roundToInt(19.0f * scale)));
            slider.setBounds(0, top, getWidth(), getHeight() - top);
            slider.updateText();
        }
    };

    // --- Полоса осциллятора: панель + винты + Wave-список + 5 ручек ---
    struct OscStrip : public juce::Component {
        PowerDot power{"Oscillator power"};
        juce::ComboBox wave;
        juce::ComboBox voices;
        juce::String title;
        std::unique_ptr<Knob> oct, semi, fine, level, pan, detune;
        OscStrip(const juce::String& t);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void updatePowerState();
    };

    // --- Универсальная секция (один layout для всех нижних блоков) ---
    struct Section : public juce::Component {
        PowerDot power{"Section power"};
        juce::String title;
        std::array<juce::Slider*, 4> graphControls{};
        explicit Section(const juce::String& t) : title(t) {
            if (title != "MASTER + ARP")
                addAndMakeVisible(power);
        }
        void paint(juce::Graphics& g) override;
        void resized() override;
        void updatePowerState();
    };

    struct FxModule : public juce::Component {
        juce::String title, subtitle;
        juce::TextButton power{"OFF"};
        juce::ComboBox mode;
        Knob first, second, third;
        FxModule(juce::String title, juce::String subtitle, juce::String a, juce::String b,
                 juce::String c);
        void paint(juce::Graphics&) override;
        void resized() override;
    };

    OscStrip mOscillator1{"OSC 1"}, mOscillator2{"OSC 2"}, mOscillator3{"OSC 3"};
    Section mEnvelopeSection{"ENVELOPE"}, mFilterSection{"FILTER"}, mTubeSection{"TUBE + LO-FI"},
        mMasterSection{"MASTER + ARP"};
    PowerDot mNoisePower{"Background noise power"};
    Knob mNoiseAmount{"LEVEL"};
    juce::ComboBox mNoiseType;
    struct NoiseWave : juce::Component {
        juce::ComboBox& type;
        PowerDot& power;
        NoiseWave(juce::ComboBox& t, PowerDot& p) : type(t), power(p) {}
        void paint(juce::Graphics&) override;
    } mNoiseWave{mNoiseType, mNoisePower};

    Knob mAttackKnob{"ATTACK"}, mDecayKnob{"DECAY"}, mSustainKnob{"SUSTAIN"},
        mReleaseKnob{"RELEASE"};
    Knob mCutoffKnob{"CUTOFF"}, mResoKnob{"RESO"};
    Knob mLampinessKnob{"LAMP"}, mBitsKnob{"BITS"}, mRateKnob{"CRUSH"}, mVibRateKnob{"VIB-R"},
        mVibDepthKnob{"VIB-D"};
    Knob mMasterKnob{"VOLUME"}, mArpRateKnob{"ARP-R"}, mGlideKnob{"PORTA"};
    FxModule mFxFilter{"FILTER", "FREQUENCY / CHARACTER", "CUTOFF", "RESO", "MIX"};
    FxModule mFxHaze{"HAZE", "STEREO MODULATION", "RATE", "DEPTH", "MIX"};
    FxModule mFxTaps{"TAPS", "PING-PONG ECHO", "TIME", "FEEDBACK", "MIX"};
    FxModule mFxSpace{"SPACE", "DIFFUSED REVERB", "SIZE", "DAMP", "MIX"};
    juce::ComboBox mArpMode, mPortaMode;
    juce::Label mArpLabel, mPortaLabel;
    struct StudioKeyboard : public juce::MidiKeyboardComponent, private juce::MultiTimer {
        explicit StudioKeyboard(juce::MidiKeyboardState& s)
            : juce::MidiKeyboardComponent(s, juce::MidiKeyboardComponent::horizontalKeyboard),
              state(s) {
            juce::MultiTimer::startTimer(1, 33);
        }
        bool handlesComputerKey(const juce::KeyPress& key) {
            return juce::MidiKeyboardComponent::keyPressed(key);
        }
        bool handleComputerKeyState(bool down) {
            return juce::MidiKeyboardComponent::keyStateChanged(down);
        }
        void releaseComputerKeys() {
            juce::MidiKeyboardComponent::focusLost(juce::Component::focusChangedDirectly);
        }
        void drawWhiteNote(int, juce::Graphics&, juce::Rectangle<float>, bool, bool, juce::Colour,
                           juce::Colour) override;
        void drawBlackNote(int, juce::Graphics&, juce::Rectangle<float>, bool, bool,
                           juce::Colour) override;
        void timerCallback(int timerId) override;
        juce::MidiKeyboardState& state;
        std::array<float, 128> press{};
    } mKeyboard;

    void attachAll();
    void styleValueBox(juce::Slider& s);
    void refreshValueFonts();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WelleyzEditor)
};
