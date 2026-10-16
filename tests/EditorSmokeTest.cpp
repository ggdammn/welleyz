#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WelleyzAssets.h"
#include <cstdio>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    for (const auto* font : {"Synevariable_ttf", "Outfitvariable_ttf", "SarinaRegular_ttf"}) {
        int size = 0;
        const auto* bytes = WelleyzAssets::getNamedResource(font, size);
        if (bytes == nullptr || size <= 0 ||
            juce::Typeface::createSystemTypefaceFor(bytes, (size_t)size) == nullptr) {
            std::fprintf(stderr, "Embedded font failed to load: %s\n", font);
            return 1;
        }
    }
    for (const auto* name :
         {"logo_svg", "soundtypes_icons_svg", "back_icon_svg", "colors_icon_svg", "fx_icon_svg",
          "like_preset_icon_svg", "load_icon_svg", "manual_icon_svg", "metronome_icon_svg",
          "osc_icon_svg", "random_icon_svg", "record_icon_svg", "reset_icon_svg", "save__icon_svg",
          "scale_icon_svg", "settings_icon_svg"}) {
        int size = 0;
        const auto* bytes = WelleyzAssets::getNamedResource(name, size);
        if (bytes == nullptr || size <= 0)
            return 1;
        auto svg = juce::String::fromUTF8(bytes, size).replace("currentColor", "#f542b9");
        auto xml = juce::XmlDocument::parse(svg);
        if (xml == nullptr || juce::Drawable::createFromSVG(*xml) == nullptr) {
            std::fprintf(stderr, "Embedded SVG failed to parse: %s\n", name);
            return 1;
        }
    }
    int manualSize = 0;
    if (WelleyzAssets::getNamedResource("Manual_md", manualSize) == nullptr || manualSize < 100) {
        std::fprintf(stderr, "Embedded manual is missing\n");
        return 1;
    }
    WelleyzProcessor synth;
    if (synth.getName() != "welleyz")
        return 1;
    synth.setRateAndBufferSizeDetails(44100.0, 256);
    synth.prepareToPlay(44100.0, 256);
    WelleyzEditor editor(synth);
    editor.setZoom(1.0f);
    editor.setVisible(true);
    std::printf("Editor size %d x %d\n", editor.getWidth(), editor.getHeight());
    if (synth.mUiPage != 0)
        return 1;
    if (WelleyzEditor::initialZoomForArea({0, 0, 1366, 768}, 1.0f, false) != 0.75f ||
        WelleyzEditor::initialZoomForArea({0, 0, 1920, 1080}, 1.0f, false) != 1.0f ||
        WelleyzEditor::initialZoomForArea({0, 0, 2560, 1440}, 1.0f, false) != 1.0f ||
        WelleyzEditor::initialZoomForArea({0, 0, 2560, 1440}, 0.75f, true) != 0.75f) {
        std::fprintf(stderr, "Adaptive editor zoom failed\n");
        return 1;
    }
    auto* historyButton =
        dynamic_cast<juce::TextButton*>(editor.findChildWithID("backPresetButton"));
    if (historyButton == nullptr || historyButton->isEnabled())
        return 1;
    auto* randomButton =
        dynamic_cast<juce::TextButton*>(editor.findChildWithID("randomSoundButton"));
    if (randomButton == nullptr || !randomButton->onClick || !historyButton->onClick ||
        !randomButton->isVisible())
        return 1;
    auto* browser = editor.findChildWithID("presetBrowser");
    auto* types = editor.findChildWithID("soundTypePanel");
    if (browser == nullptr || types == nullptr ||
        browser->findChildWithID("presetSearch") == nullptr ||
        types->findChildWithID("randomSoundType5") == nullptr)
        return 1;
    auto* likes = dynamic_cast<juce::TextButton*>(browser->findChildWithID("presetFilterLikes"));
    if (likes == nullptr || !likes->onClick) {
        std::fprintf(stderr, "Likes filter is missing\n");
        return 1;
    }
    likes->onClick();
    if (!likes->getToggleState()) {
        std::fprintf(stderr, "Likes filter did not activate\n");
        return 1;
    }
    auto* keyboardView =
        dynamic_cast<juce::MidiKeyboardComponent*>(editor.findChildWithID("pianoKeyboard"));
    if (keyboardView == nullptr || keyboardView->getLowestVisibleKey() != 72) {
        std::fprintf(stderr, "Expected C5 as first visible note, got %d\n",
                     keyboardView != nullptr ? keyboardView->getLowestVisibleKey() : -1);
        return 1;
    }
    auto* bass = dynamic_cast<juce::Button*>(types->findChildWithID("randomSoundType5"));
    if (bass == nullptr || bass->onClick == nullptr)
        return 1;
    bass->onClick();
    if (!bass->getToggleState())
        return 1;
    if (synth.mUiSoundType != 5) {
        std::fprintf(stderr, "Sound type selection was not stored in the processor\n");
        return 1;
    }
    if (synth.selectedSoundType() != 5)
        return 1;
    {
        WelleyzEditor reopened(synth);
        auto* reopenedTypes = reopened.findChildWithID("soundTypePanel");
        auto* reopenedBass =
            reopenedTypes != nullptr
                ? dynamic_cast<juce::Button*>(reopenedTypes->findChildWithID("randomSoundType5"))
                : nullptr;
        if (reopenedBass == nullptr || !reopenedBass->getToggleState()) {
            std::fprintf(stderr, "Reopened editor lost the selected sound type\n");
            return 1;
        }
    }
    randomButton->onClick();
    if (!historyButton->isEnabled())
        return 1;
    historyButton->onClick();
    if (historyButton->isEnabled() || synth.canRestorePreviousSound())
        return 1;
    if (!editor.keyPressed(juce::KeyPress('a', juce::ModifierKeys(), 'a')) ||
        editor.keyPressed(juce::KeyPress('a', juce::ModifierKeys::ctrlModifier, 'a'))) {
        std::fprintf(stderr, "Computer keyboard mapping or shortcut handling failed\n");
        return 1;
    }

    auto* tab = dynamic_cast<juce::TextButton*>(editor.findChildWithID("fxRackTab"));
    if (tab == nullptr)
        return 1;
    auto* back = dynamic_cast<juce::TextButton*>(editor.findChildWithID("synthTab"));
    if (back == nullptr)
        return 1;
    auto* home = dynamic_cast<juce::TextButton*>(editor.findChildWithID("randomTab"));
    if (home == nullptr || tab->onClick == nullptr || back->onClick == nullptr ||
        home->onClick == nullptr)
        return 1;
    auto* colors = dynamic_cast<juce::TextButton*>(editor.findChildWithID("colorsButton"));
    auto* settings = dynamic_cast<juce::TextButton*>(editor.findChildWithID("settingsButton"));
    auto* zoom = dynamic_cast<juce::TextButton*>(editor.findChildWithID("zoomButton"));
    auto* manualHeader = dynamic_cast<juce::TextButton*>(editor.findChildWithID("manualButton"));
    if (colors == nullptr || settings == nullptr || colors->getButtonText() != "COLORS" ||
        settings->getButtonText() != "SETTINGS" || zoom == nullptr || manualHeader == nullptr ||
        colors->getWidth() != colors->getHeight() ||
        colors->getLocalBounds() != settings->getLocalBounds() ||
        colors->getLocalBounds() != zoom->getLocalBounds() ||
        colors->getLocalBounds() != manualHeader->getLocalBounds() ||
        colors->getY() != settings->getY() || settings->getY() != zoom->getY() ||
        zoom->getY() != manualHeader->getY())
        return 1;
    back->onClick();
    if (synth.mUiPage != 1 || randomButton->isVisible())
        return 1;
    auto* firstStrip = editor.findChildWithID("osc1Strip");
    juce::Slider* fineTune = nullptr;
    if (firstStrip != nullptr)
        for (int i = 0; i < firstStrip->getNumChildComponents(); ++i)
            fineTune =
                fineTune != nullptr
                    ? fineTune
                    : dynamic_cast<juce::Slider*>(
                          firstStrip->getChildComponent(i)->findChildWithID("oscillator1FineTune"));
    if (fineTune == nullptr || fineTune->getTextFromValue(fineTune->getValue()) != "0.0 ct") {
        std::fprintf(stderr, "Fine-tune value label is not formatted correctly: %s (%f)\n",
                     fineTune != nullptr
                         ? fineTune->getTextFromValue(fineTune->getValue()).toRawUTF8()
                         : "missing",
                     fineTune != nullptr ? fineTune->getValue() : 0.0);
        return 1;
    }
    bool displayedFineTune = false;
    for (int i = 0; i < fineTune->getNumChildComponents(); ++i)
        if (auto* label = dynamic_cast<juce::Label*>(fineTune->getChildComponent(i)))
            displayedFineTune |= label->getText() == "0.0 ct";
    if (!displayedFineTune) {
        std::fprintf(stderr, "Fine-tune textbox did not refresh after layout\n");
        return 1;
    }
    auto* noisePower = dynamic_cast<juce::Button*>(editor.findChildWithID("noiseOnPower"));
    auto* noiseType = dynamic_cast<juce::ComboBox*>(editor.findChildWithID("noiseTypeSelector"));
    auto* noiseAmount = editor.findChildWithID("noiseAmountKnob");
    if (noisePower == nullptr || noiseType == nullptr || noiseType->getNumItems() < 8 ||
        noiseAmount == nullptr || !noisePower->isVisible())
        return 1;
    for (int i = 1; i <= 3; ++i) {
        const auto name = "osc" + juce::String(i);
        auto* strip = editor.findChildWithID(name + "Strip");
        auto* wave = strip != nullptr ? strip->findChildWithID(name + "WaveSelector") : nullptr;
        auto* knob = strip != nullptr ? strip->findChildWithID(name + "OctKnob") : nullptr;
        if (strip == nullptr || wave == nullptr || knob == nullptr || wave->getY() < 65 ||
            knob->getY() < 100) {
            std::fprintf(stderr, "OSC controls were not moved down: %s, wave=%d, knob=%d\n",
                         name.toRawUTF8(), wave != nullptr ? wave->getY() : -1,
                         knob != nullptr ? knob->getY() : -1);
            return 1;
        }
    }
    auto* masterSection = editor.findChildWithID("masterArpSection");
    auto* arpSelector =
        masterSection != nullptr ? masterSection->findChildWithID("arpModeSelector") : nullptr;
    auto* portaSelector =
        masterSection != nullptr ? masterSection->findChildWithID("portaModeSelector") : nullptr;
    if (masterSection == nullptr || arpSelector == nullptr || portaSelector == nullptr ||
        arpSelector->getY() > 180 || portaSelector->getY() != arpSelector->getY())
        return 1;
    juce::Button* oscillatorPower = nullptr;
    for (const auto* id : {"osc1Power", "osc2Power", "osc3Power", "amplitudeEnabledPower",
                           "filterEnabledPower", "tubeEnabledPower"}) {
        juce::Button* light = nullptr;
        for (int i = 0; i < editor.getNumChildComponents(); ++i)
            if (auto* child = editor.getChildComponent(i))
                if (auto* found = dynamic_cast<juce::Button*>(child->findChildWithID(id)))
                    light = found;
        if (light == nullptr || !light->getToggleState()) {
            std::fprintf(stderr, "OSC power indicator is missing or off by default: %s\n", id);
            return 1;
        }
        if (juce::String(id) == "osc1Power")
            oscillatorPower = light;
    }
    oscillatorPower->setToggleState(false, juce::sendNotification);
    if (synth.mApvts.getRawParameterValue("oscillator1Enabled")->load() != 0.0f)
        return 1;
    oscillatorPower->setToggleState(true, juce::sendNotification);
    if (synth.mApvts.getRawParameterValue("oscillator1Enabled")->load() != 1.0f)
        return 1;
    tab->onClick();
    if (synth.mUiPage != 2)
        return 1;
    home->onClick();
    if (synth.mUiPage != 0 || !randomButton->isVisible())
        return 1;
    if (argc > 1) {
        juce::PNGImageFormat png;
        const auto destination = juce::File(juce::String::fromUTF8(argv[1]));
        for (const auto& page : {std::pair<juce::TextButton*, const char*>{home, "random"},
                                 {back, "osc"},
                                 {tab, "fx"}}) {
            std::fprintf(stderr, "Capturing %s\n", page.second);
            page.first->onClick();
            const auto imageFile = destination.getSiblingFile(
                destination.getFileNameWithoutExtension() + "-" + page.second + ".png");
            auto output = imageFile.createOutputStream();
            if (output == nullptr ||
                !png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),
                                        *output))
                return 1;
        }
        home->onClick();
    }

    const auto originalAccent = ThemeManager::instance().accent();
    ThemeManager::instance().setAccent(juce::Colour(0xff8562f5));
    editor.applyTheme(0);
    if (ThemeManager::instance().accent() != juce::Colour(0xff8562f5))
        return 1;
    ThemeManager::instance().setAccent(originalAccent);
    editor.applyTheme(0);

    editor.setZoom(0.5f);
    if (editor.getWidth() != 640 || editor.getHeight() != 380) {
        std::fprintf(stderr, "50%% zoom produced unexpected size %d x %d\n", editor.getWidth(),
                     editor.getHeight());
        return 1;
    }
    if (keyboardView->getLowestVisibleKey() != 72) {
        std::fprintf(stderr, "Zoom changed the selected keyboard octave\n");
        return 1;
    }

    auto* manual = dynamic_cast<juce::TextButton*>(editor.findChildWithID("manualButton"));
    if (manual == nullptr || !manual->onClick)
        return 1;
    manual->onClick();
    auto* manualPanel = editor.findChildWithID("manualPanel");
    if (manualPanel == nullptr || !manualPanel->isVisible())
        return 1;

    std::printf("Random, Synth and FX pages switch correctly\n");
    return 0;
}
