#include "PluginProcessor.h"
#include "ThemeManager.h"
#include "WelleyzAssets.h"
#include <set>
#include <cmath>
#include <cstdio>

juce::AudioProcessorEditor* WelleyzProcessor::createEditor() {
    return nullptr;
}

static int selected(const WelleyzProcessor& p, const char* id) {
    return (int)std::round(p.mApvts.getRawParameterValue(id)->load());
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    WelleyzProcessor synth;
    if (argc > 1 && juce::String(argv[1]) == "--user-presets") {
        juce::Array<juce::File> userFiles;
        WelleyzProcessor::presetDirectory().findChildFiles(userFiles, juce::File::findFiles, false,
                                                           "*.wlyz");
        for (const auto& file : userFiles) {
            WelleyzProcessor candidate;
            if (!candidate.loadPresetFromFile(file)) {
                std::fprintf(stderr, "Could not load converted preset: %s\n",
                             file.getFileName().toRawUTF8());
                return 1;
            }
        }
        std::printf("Loaded %d user JSON presets\n", userFiles.size());
    }
    synth.setRateAndBufferSizeDetails(44100.0, 256);
    synth.prepareToPlay(44100.0, 256);
    auto& randomOptions = ThemeManager::instance();
    struct RestoreRandomizeGroups {
        ThemeManager& preferences;
        bool porta, fx, arp, voices, noise;
        ~RestoreRandomizeGroups() { preferences.setRandomizeGroups(porta, fx, arp, voices, noise); }
    } restoreGroups{randomOptions,
                    randomOptions.randomizePorta(),
                    randomOptions.randomizeFx(),
                    randomOptions.randomizeArp(),
                    randomOptions.randomizeVoices(),
                    randomOptions.randomizeNoise()};
    randomOptions.setRandomizeGroups(true, true, true, true, true);
    std::set<juce::String> global;
    int fxCount = 0;
    for (int category = 0; category < 9; ++category) {
        std::set<juce::String> signatures;
        int sawCount = 0;
        constexpr int patchesPerCategory = 48;
        for (int i = 0; i < patchesPerCategory; ++i) {
            synth.randomizeParameters(category);
            auto p = [&](const char* id) { return synth.mApvts.getRawParameterValue(id)->load(); };
            for (const char* id : {"oscillator1Wave", "oscillator2Wave", "oscillator3Wave"})
                if (selected(synth, id) == 2)
                    ++sawCount;
            const auto signature = juce::String(selected(synth, "oscillator1Wave")) + "/" +
                                   juce::String(selected(synth, "oscillator2Wave")) + "/" +
                                   juce::String(selected(synth, "oscillator3Wave")) + "/" +
                                   juce::String(p("filterCutoff"), 0) + "/" +
                                   juce::String(p("amplitudeAttack"), 3) + "/" +
                                   juce::String(selected(synth, "effectsFilterMode")) + "/" +
                                   juce::String(p("effectsSpaceEnabled") > 0.5f ? 1 : 0) + "/" +
                                   juce::String(p("effectsTapsEnabled") > 0.5f ? 1 : 0);
            signatures.insert(signature);
            global.insert(signature);
            if (p("effectsSpaceEnabled") > 0.5f || p("effectsTapsEnabled") > 0.5f ||
                p("effectsHazeEnabled") > 0.5f || p("effectsFilterEnabled") > 0.5f)
                ++fxCount;
        }
        if (signatures.size() < 30) {
            std::fprintf(stderr, "Random category %d produced only %zu unique patches\n", category,
                         signatures.size());
            return 1;
        }
        if (sawCount > patchesPerCategory * 3 * 0.40) {
            std::fprintf(stderr, "Random category %d overused saw: %d/%d oscillators\n", category,
                         sawCount, patchesPerCategory * 3);
            return 1;
        }
    }
    if (global.size() < 200 || fxCount < 150) {
        std::fprintf(stderr, "Insufficient variety: %zu patches, %d using effects\n", global.size(),
                     fxCount);
        return 1;
    }

    auto setRandomOptionTestValue = [&](const char* id, float value) {
        auto* parameter = synth.mApvts.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    setRandomOptionTestValue("portamentoTime", 0.42f);
    setRandomOptionTestValue("portamentoMode", 2.0f);
    setRandomOptionTestValue("arpeggiatorRate", 13.0f);
    setRandomOptionTestValue("arpeggiatorMode", 2.0f);
    setRandomOptionTestValue("effectsSpaceMix", 0.71f);
    setRandomOptionTestValue("effectsSpaceEnabled", 1.0f);
    setRandomOptionTestValue("oscillator1Voices", 4.0f);
    setRandomOptionTestValue("oscillator1Detune", 17.0f);
    setRandomOptionTestValue("backgroundNoiseEnabled", 1.0f);
    setRandomOptionTestValue("backgroundNoiseType", 4.0f);
    setRandomOptionTestValue("backgroundNoiseLevel", 0.67f);
    randomOptions.setRandomizeGroups(false, false, false, false, false);
    synth.randomizeParameters(2);
    if (std::abs(synth.mApvts.getRawParameterValue("portamentoTime")->load() - 0.42f) > 0.001f ||
        selected(synth, "portamentoMode") != 2 ||
        std::abs(synth.mApvts.getRawParameterValue("arpeggiatorRate")->load() - 13.0f) > 0.01f ||
        selected(synth, "arpeggiatorMode") != 2 ||
        std::abs(synth.mApvts.getRawParameterValue("effectsSpaceMix")->load() - 0.71f) > 0.001f ||
        synth.mApvts.getRawParameterValue("effectsSpaceEnabled")->load() < 0.5f ||
        selected(synth, "oscillator1Voices") != 4 ||
        std::abs(synth.mApvts.getRawParameterValue("oscillator1Detune")->load() - 17.0f) > 0.001f ||
        selected(synth, "backgroundNoiseEnabled") != 1 ||
        selected(synth, "backgroundNoiseType") != 4 ||
        std::abs(synth.mApvts.getRawParameterValue("backgroundNoiseLevel")->load() - 0.67f) >
            0.001f) {
        std::fprintf(stderr, "RANDOMIZE options did not preserve disabled groups\n");
        return 1;
    }
    randomOptions.setRandomizeGroups(true, true, true, true, true);

    // Textures run continuously even without notes and are completely silent
    // when bypassed. Each mode must produce a distinct repeatable signal.
    WelleyzProcessor atmosphere;
    atmosphere.setRateAndBufferSizeDetails(44100.0, 1024);
    atmosphere.prepareToPlay(44100.0, 1024);
    auto setNoise = [&](const char* id, float value) {
        auto* parameter = atmosphere.mApvts.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    juce::AudioBuffer<float> noiseAudio(2, 1024);
    juce::MidiBuffer noNotes;
    atmosphere.processBlock(noiseAudio, noNotes);
    if (noiseAudio.getMagnitude(0, 1024) != 0.0f)
        return 1;
    setNoise("backgroundNoiseEnabled", 1.0f);
    setNoise("backgroundNoiseLevel", 0.75f);
    std::array<float, 9> signatures{};
    for (int type = 0; type < (int)signatures.size(); ++type) {
        setNoise("backgroundNoiseType", (float)type);
        atmosphere.prepareToPlay(44100.0, 1024);
        atmosphere.processBlock(noiseAudio, noNotes);
        double energy = 0.0;
        for (int i = 0; i < 1024; ++i)
            energy += std::abs(noiseAudio.getSample(0, i));
        signatures[(size_t)type] = (float)energy;
        if (energy < 0.01 || energy > 100.0) {
            std::fprintf(stderr, "Noise type %d produced invalid background level\n", type);
            return 1;
        }
    }
    if (std::abs(signatures[0] - signatures[4]) < 0.02f)
        return 1;
    setNoise("backgroundNoiseEnabled", 0.0f);
    atmosphere.processBlock(noiseAudio, noNotes);
    if (noiseAudio.getMagnitude(0, 1024) != 0.0f) {
        std::fprintf(stderr, "Background noise did not switch off\n");
        return 1;
    }

    // A host may resume MIDI immediately after RANDOMIZE, without an extra
    // click in the editor. The generated patch must sound on the next note-on.
    WelleyzProcessor afterRandom;
    afterRandom.setRateAndBufferSizeDetails(44100.0, 256);
    afterRandom.prepareToPlay(44100.0, 256);
    afterRandom.randomizeParameters(2); // Lead: short attack, no waiting for a pad fade-in.
    juce::AudioBuffer<float> generatedAudio(2, 256);
    juce::MidiBuffer generatedMidi;
    generatedMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    double generatedEnergy = 0.0;
    for (int block = 0; block < 12; ++block) {
        afterRandom.processBlock(generatedAudio, generatedMidi);
        generatedMidi.clear();
        for (int channel = 0; channel < generatedAudio.getNumChannels(); ++channel)
            for (int sample = 0; sample < generatedAudio.getNumSamples(); ++sample)
                generatedEnergy += std::abs(generatedAudio.getSample(channel, sample));
    }
    if (generatedEnergy < 0.01) {
        std::fprintf(stderr, "RANDOMIZE produced no audio from the next MIDI note\n");
        return 1;
    }
    WelleyzProcessor keyboardInput;
    keyboardInput.setRateAndBufferSizeDetails(44100.0, 256);
    keyboardInput.prepareToPlay(44100.0, 256);
    juce::AudioBuffer<float> keyboardAudio(2, 256);
    juce::MidiBuffer emptyMidi;
    keyboardInput.getKeyboardState().noteOn(1, 60, 1.0f);
    keyboardInput.processBlock(keyboardAudio, emptyMidi);
    if (keyboardAudio.getMagnitude(0, 256) < 0.0001f) {
        std::fprintf(stderr, "Screen keyboard did not reach the audio callback\n");
        return 1;
    }
    keyboardInput.getKeyboardState().noteOff(1, 60, 0.0f);
    keyboardInput.processBlock(keyboardAudio, emptyMidi);
    keyboardInput.flushKeyboardDisplay();
    afterRandom.randomizeParameters(2);
    double heldEnergy = 0.0;
    for (int block = 0; block < 8; ++block) {
        afterRandom.processBlock(generatedAudio, generatedMidi);
        for (int channel = 0; channel < generatedAudio.getNumChannels(); ++channel)
            for (int sample = 0; sample < generatedAudio.getNumSamples(); ++sample)
                heldEnergy += std::abs(generatedAudio.getSample(channel, sample));
    }
    if (heldEnergy < 0.01) {
        std::fprintf(stderr, "RANDOMIZE silenced a held MIDI note\n");
        return 1;
    }

    int factorySize = 0;
    const auto* factoryData = WelleyzAssets::getNamedResource("arpsynth_wlyz", factorySize);
    if (factoryData == nullptr || factorySize <= 0)
        return 1;
    for (int i = 0; i < WelleyzAssets::namedResourceListSize; ++i) {
        const auto resourceName = juce::String::fromUTF8(WelleyzAssets::originalFilenames[i]);
        if (!resourceName.endsWith(".wlyz"))
            continue;
        int size = 0;
        const auto* data =
            WelleyzAssets::getNamedResource(WelleyzAssets::namedResourceList[i], size);
        WelleyzProcessor factory;
        if (data == nullptr || !factory.loadPresetFromData(data, (size_t)size)) {
            std::fprintf(stderr, "Factory sound failed to load: %s\n", resourceName.toRawUTF8());
            return 1;
        }
    }
    // Missing optional parameters use current defaults, not the previous sound's values.
    synth.randomizeParameters(0);
    for (const char* id : {"oscillator1Enabled", "oscillator2Enabled", "oscillator3Enabled",
                           "amplitudeEnabled", "filterEnabled", "tubeEnabled"})
        synth.mApvts.getParameter(id)->setValueNotifyingHost(0.0f);
    if (!synth.loadPresetFromData(factoryData, (size_t)factorySize))
        return 1;
    for (const char* id : {"effectsFilterEnabled", "effectsHazeEnabled", "effectsTapsEnabled",
                           "effectsSpaceEnabled"})
        if (synth.mApvts.getRawParameterValue(id)->load() != 0.0f) {
            std::fprintf(stderr, "Factory preset left %s enabled\n", id);
            return 1;
        }
    if (selected(synth, "backgroundNoiseEnabled") != 0) {
        std::fprintf(stderr, "Factory preset retained background noise\n");
        return 1;
    }
    for (const char* id :
         {"oscillator1Voices", "oscillator2Voices", "oscillator3Voices", "portamentoMode"})
        if (selected(synth, id) != 0) {
            std::fprintf(stderr, "Factory preset did not reset choice %s\n", id);
            return 1;
        }
    for (const char* id : {"oscillator1Detune", "oscillator2Detune", "oscillator3Detune"})
        if (synth.mApvts.getRawParameterValue(id)->load() != 0.0f) {
            std::fprintf(stderr, "Factory preset did not reset %s\n", id);
            return 1;
        }
    for (const char* id : {"oscillator1Enabled", "oscillator2Enabled", "oscillator3Enabled",
                           "amplitudeEnabled", "filterEnabled", "tubeEnabled"})
        if (synth.mApvts.getRawParameterValue(id)->load() != 1.0f) {
            std::fprintf(stderr, "Factory preset did not enable %s\n", id);
            return 1;
        }

    const auto presetFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getNonexistentChildFile("welleyz-preset-check", ".wlyz", false);
    const bool savedPreset = synth.savePresetToFile(presetFile);
    WelleyzProcessor fromJson;
    const bool roundTrip =
        savedPreset && presetFile.existsAsFile() && fromJson.loadPresetFromFile(presetFile) &&
        std::abs(fromJson.mApvts.getRawParameterValue("oscillator1Wave")->load() -
                 synth.mApvts.getRawParameterValue("oscillator1Wave")->load()) < 0.001f &&
        std::abs(fromJson.mApvts.getRawParameterValue("filterCutoff")->load() -
                 synth.mApvts.getRawParameterValue("filterCutoff")->load()) < 0.01f;
    presetFile.deleteFile();
    const char* invalidPreset =
        R"({"format":"welleyz","version":1,"name":"Broken","parameters":{"unknown":1}})";
    if (!roundTrip || fromJson.loadPresetFromData(invalidPreset, std::strlen(invalidPreset))) {
        std::fprintf(stderr, "JSON preset round trip or invalid-data rejection failed\n");
        return 1;
    }

    WelleyzProcessor bypass;
    bypass.setRateAndBufferSizeDetails(44100.0, 256);
    bypass.prepareToPlay(44100.0, 256);
    auto bypassParam = [&](const char* id, float value) {
        if (auto* p = bypass.mApvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    bypassParam("oscillator1Enabled", 0.0f);
    bypassParam("oscillator2Enabled", 0.0f);
    bypassParam("oscillator3Enabled", 0.0f);
    bypassParam("amplitudeEnabled", 0.0f);
    bypassParam("filterEnabled", 0.0f);
    bypassParam("tubeEnabled", 0.0f);
    juce::AudioBuffer<float> bypassAudio(2, 256);
    juce::MidiBuffer bypassMidi;
    bypassMidi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    bypass.processBlock(bypassAudio, bypassMidi);
    double bypassEnergy = 0.0;
    for (int i = 0; i < bypassAudio.getNumSamples(); ++i)
        bypassEnergy += std::abs(bypassAudio.getSample(0, i));
    if (bypassEnergy > 0.00001) {
        std::fprintf(stderr, "Disabled oscillators still produced sound\n");
        return 1;
    }
    bypassParam("oscillator1Enabled", 1.0f);
    bypassMidi.clear();
    bypass.processBlock(bypassAudio, bypassMidi);
    bypassEnergy = 0.0;
    for (int i = 0; i < bypassAudio.getNumSamples(); ++i)
        bypassEnergy += std::abs(bypassAudio.getSample(0, i));
    if (bypassEnergy < 0.01) {
        std::fprintf(stderr, "Re-enabling OSC 1 did not restore the held note\n");
        return 1;
    }
    bypassMidi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
    bypass.processBlock(bypassAudio, bypassMidi);
    bypassEnergy = 0.0;
    for (int i = 0; i < bypassAudio.getNumSamples(); ++i)
        bypassEnergy += std::abs(bypassAudio.getSample(0, i));
    if (bypassEnergy > 0.00001) {
        std::fprintf(stderr, "Bypassed envelope did not close on note-off\n");
        return 1;
    }

    synth.randomizeParameters(6);
    auto setTestParam = [&](const char* id, float value) {
        if (auto* parameter = synth.mApvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    setTestParam("oscillator1Voices", 5.0f);
    setTestParam("oscillator1Detune", 24.5f);
    setTestParam("portamentoMode", 2.0f);
    setTestParam("portamentoTime", 0.42f);
    setTestParam("oscillator2Enabled", 0.0f);
    setTestParam("tubeEnabled", 0.0f);
    setTestParam("backgroundNoiseEnabled", 1.0f);
    setTestParam("backgroundNoiseType", 6.0f);
    setTestParam("backgroundNoiseLevel", 0.37f);
    synth.mUiPreset = "factory:arpsynth";
    synth.mUiZoom = 0.75f;
    synth.mUiZoomCustomized = true;
    juce::MemoryBlock state;
    synth.getStateInformation(state);
    WelleyzProcessor restored;
    restored.setStateInformation(state.getData(), (int)state.getSize());
    if (restored.mApvts.getRawParameterValue("oscillator2Enabled")->load() != 0.0f ||
        restored.mApvts.getRawParameterValue("tubeEnabled")->load() != 0.0f ||
        selected(restored, "backgroundNoiseEnabled") != 1 ||
        selected(restored, "backgroundNoiseType") != 6 ||
        std::abs(restored.mApvts.getRawParameterValue("backgroundNoiseLevel")->load() - 0.37f) >
            0.001f) {
        std::fprintf(stderr, "Bypass state was not restored from the host project\n");
        return 1;
    }
    if (restored.mUiPreset != synth.mUiPreset) {
        std::fprintf(stderr, "Selected preset identity was not restored\n");
        return 1;
    }
    if (!restored.mUiZoomCustomized || std::abs(restored.mUiZoom - 0.75f) > 0.001f) {
        std::fprintf(stderr, "Explicit zoom choice was not restored\n");
        return 1;
    }
    synth.mUiPage = 2;
    synth.selectSoundType(5);
    synth.getStateInformation(state);
    restored.setStateInformation(state.getData(), (int)state.getSize());
    if (restored.mUiPage != 2) {
        std::fprintf(stderr, "New editor page was not restored\n");
        return 1;
    }
    if (restored.mUiSoundType != 5) {
        std::fprintf(stderr, "Sound type was not restored from the host project\n");
        return 1;
    }
    if (restored.selectedSoundType() != 5 || selected(restored, "randomSoundType") != 5) {
        std::fprintf(stderr, "Host parameter did not retain sound type\n");
        return 1;
    }
    {
        WelleyzProcessor soundTypeHistory;
        soundTypeHistory.selectSoundType(3);
        soundTypeHistory.randomizeParameters(0);
        soundTypeHistory.selectSoundType(7);
        if (!soundTypeHistory.restorePreviousSound() || soundTypeHistory.mUiSoundType != 3) {
            std::fprintf(stderr, "BACK failed to restore the sound type\n");
            return 1;
        }
    }
    if (WelleyzProcessor::presetDirectory().getFileName() != "welleyz presets") {
        std::fprintf(stderr, "Preset directory was not renamed to welleyz presets\n");
        return 1;
    }
    WelleyzProcessor history;
    if (history.canRestorePreviousSound())
        return 1;
    history.mUiPreset = "factory:1bass";
    const auto originalWave = selected(history, "oscillator1Wave");
    history.randomizeParameters(0);
    const auto firstWave = selected(history, "oscillator1Wave");
    history.mUiPreset.clear();
    history.randomizeParameters(1);
    if (!history.restorePreviousSound() || selected(history, "oscillator1Wave") != firstWave ||
        !history.mUiPreset.isEmpty()) {
        std::fprintf(stderr, "BACK failed to restore the previous random patch\n");
        return 1;
    }
    if (!history.restorePreviousSound() || selected(history, "oscillator1Wave") != originalWave ||
        history.mUiPreset != "factory:1bass" || history.canRestorePreviousSound()) {
        std::fprintf(stderr, "BACK failed to restore the selected preset identity\n");
        return 1;
    }
    for (int i = 0; i < 11; ++i)
        history.randomizeParameters(i % 9);
    for (int i = 0; i < 10; ++i)
        if (!history.restorePreviousSound())
            return 1;
    if (history.canRestorePreviousSound()) {
        std::fprintf(stderr, "BACK history exceeded ten snapshots\n");
        return 1;
    }
    history.mUiPreset = "factory:1bass";
    const auto beforeLoad = selected(history, "oscillator1Wave");
    if (!history.loadPresetFromData(factoryData, (size_t)factorySize))
        return 1;
    history.mUiPreset = "factory:arpsynth";
    if (!history.restorePreviousSound() || history.mUiPreset != "factory:1bass" ||
        selected(history, "oscillator1Wave") != beforeLoad) {
        std::fprintf(stderr, "BACK failed after selecting another preset\n");
        return 1;
    }
    for (const char* id :
         {"effectsFilterEnabled", "effectsHazeEnabled", "effectsTapsEnabled", "effectsSpaceEnabled",
          "effectsFilterFrequency", "effectsHazeDepth", "effectsTapsTime", "effectsSpaceSize",
          "oscillator1Voices", "oscillator1Detune", "portamentoMode", "portamentoTime"})
        if (std::abs(synth.mApvts.getRawParameterValue(id)->load() -
                     restored.mApvts.getRawParameterValue(id)->load()) > 0.0001f) {
            std::fprintf(stderr, "FX setting not restored: %s\n", id);
            return 1;
        }

    // Pitch Bend changes oscillator pitch; CC1 and the on-screen Mod wheel
    // change vibrato without introducing APVTS parameters or preset data.
    auto renderWheel = [](bool bend, bool ui, bool mod) {
        WelleyzProcessor instrument;
        instrument.setRateAndBufferSizeDetails(44100.0, 8192);
        instrument.prepareToPlay(44100.0, 8192);
        auto set = [&](const char* id, float value) {
            auto* p = instrument.mApvts.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        for (const char* wave : {"oscillator1Wave", "oscillator2Wave", "oscillator3Wave"})
            set(wave, 0.0f);
        set("oscillator1Level", 0.9f);
        set("oscillator2Level", 0.0f);
        set("oscillator3Level", 0.0f);
        set("amplitudeAttack", 0.001f);
        set("amplitudeSustain", 1.0f);
        set("vibratoDepth", 0.0f);
        set("lofiBitDepth", 16.0f);
        set("lofiRateReduction", 1.0f);
        set("tubeLampiness", 0.0f);
        set("filterCutoff", 15000.0f);
        juce::AudioBuffer<float> output(2, 8192);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
        if (ui && bend)
            instrument.setUiPitchBend(16383);
        if (ui && mod)
            instrument.setUiModWheel(127);
        if (!ui && bend)
            midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
        if (!ui && mod)
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 127), 0);
        instrument.processBlock(output, midi);
        std::vector<float> result(8192);
        for (int i = 0; i < 8192; ++i)
            result[(size_t)i] = output.getSample(0, i);
        return result;
    };
    const auto normal = renderWheel(false, false, false);
    const auto hostPitch = renderWheel(true, false, false);
    const auto uiPitch = renderWheel(true, true, false);
    auto crossings = [](const std::vector<float>& samples) {
        int count = 0;
        for (size_t i = 1025; i < samples.size(); ++i)
            if (samples[i - 1] <= 0.0f && samples[i] > 0.0f)
                ++count;
        return count;
    };
    if (crossings(hostPitch) < crossings(normal) + 7 ||
        crossings(uiPitch) < crossings(normal) + 7) {
        std::fprintf(stderr, "Pitch Bend did not raise the sound's pitch\n");
        return 1;
    }
    for (const auto& modulated :
         {renderWheel(false, false, true), renderWheel(false, true, true)}) {
        double difference = 0.0;
        for (size_t i = 1024; i < normal.size(); ++i)
            difference += std::abs(normal[i] - modulated[i]);
        if (difference < 5.0) {
            std::fprintf(stderr, "Mod wheel did not modulate the sound\n");
            return 1;
        }
    }

    // FL's proprietary slide-note flag is not MIDI, but overlapping ordinary
    // MIDI notes must glide when the synth's Legato portamento is selected.
    WelleyzProcessor gliding;
    gliding.setRateAndBufferSizeDetails(44100.0, 16384);
    gliding.prepareToPlay(44100.0, 16384);
    auto setGlideParam = [&](const char* id, float value) {
        auto* p = gliding.mApvts.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    setGlideParam("oscillator1Wave", 0.0f);
    setGlideParam("oscillator2Level", 0.0f);
    setGlideParam("oscillator3Level", 0.0f);
    setGlideParam("amplitudeAttack", 0.001f);
    setGlideParam("amplitudeSustain", 1.0f);
    setGlideParam("amplitudeRelease", 0.01f);
    setGlideParam("tubeLampiness", 0.0f);
    setGlideParam("lofiBitDepth", 16.0f);
    setGlideParam("lofiRateReduction", 1.0f);
    setGlideParam("filterCutoff", 18000.0f);
    setGlideParam("portamentoMode", 2.0f);
    setGlideParam("portamentoTime", 0.25f);
    juce::AudioBuffer<float> glideAudio(2, 16384);
    juce::MidiBuffer glideMidi;
    glideMidi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    gliding.processBlock(glideAudio, glideMidi);
    glideMidi.clear();
    glideMidi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 0);
    glideMidi.addEvent(juce::MidiMessage::noteOff(1, 60), 1);
    gliding.processBlock(glideAudio, glideMidi);
    auto countRises = [&](int begin, int end) {
        int count = 0;
        for (int i = begin + 1; i < end; ++i)
            if (glideAudio.getSample(0, i - 1) <= 0.0f && glideAudio.getSample(0, i) > 0.0f)
                ++count;
        return count;
    };
    if (countRises(12000, 15000) < countRises(1000, 4000) + 9) {
        std::fprintf(stderr, "Legato note overlap did not glide toward the new pitch\n");
        return 1;
    }
    // The original key ends while the target is held; the sliding voice must
    // keep playing until the target note ends (no retrigger or double voice).
    glideMidi.clear();
    gliding.processBlock(glideAudio, glideMidi);
    double continuedEnergy = 0.0;
    for (int i = 0; i < glideAudio.getNumSamples(); ++i)
        continuedEnergy += std::abs(glideAudio.getSample(0, i));
    if (continuedEnergy < 0.01) {
        std::fprintf(stderr, "Slide voice stopped when source note was released\n");
        return 1;
    }

    WelleyzProcessor lifecycle;
    lifecycle.setRateAndBufferSizeDetails(44100.0, 256);
    lifecycle.prepareToPlay(44100.0, 256);
    for (const char* id : {"oscillator1Wave", "oscillator2Wave", "oscillator3Wave"})
        if (auto* parameter = lifecycle.mApvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(2.0f));
    lifecycle.mApvts.getParameter("effectsSpaceEnabled")->setValueNotifyingHost(1.0f);
    lifecycle.mApvts.getParameter("effectsSpaceMix")
        ->setValueNotifyingHost(
            lifecycle.mApvts.getParameter("effectsSpaceMix")->convertTo0to1(1.0f));
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer note;
    note.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    lifecycle.processBlock(audio, note);
    double noteEnergy = 0.0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            noteEnergy += std::abs(audio.getSample(ch, i));
    if (noteEnergy < 0.01) {
        std::fprintf(stderr, "MIDI note produced no audio in lifecycle test\n");
        return 1;
    }
    if (!lifecycle.loadPresetFromData(factoryData, (size_t)factorySize))
        return 1;
    lifecycle.mApvts.getParameter("effectsSpaceEnabled")->setValueNotifyingHost(1.0f);
    lifecycle.mApvts.getParameter("effectsSpaceMix")
        ->setValueNotifyingHost(
            lifecycle.mApvts.getParameter("effectsSpaceMix")->convertTo0to1(1.0f));
    note.clear();
    audio.clear();
    lifecycle.processBlock(audio, note);
    double stalePatchEnergy = 0.0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            stalePatchEnergy += std::abs(audio.getSample(ch, i));
    if (stalePatchEnergy > 0.0001) {
        std::fprintf(stderr, "Previous patch voice/tail remained after preset load: %f\n",
                     stalePatchEnergy);
        return 1;
    }
    lifecycle.releaseResources();
    lifecycle.prepareToPlay(44100.0, 256);
    audio.clear();
    lifecycle.processBlock(audio, note);
    double resumedEnergy = 0.0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            resumedEnergy += std::abs(audio.getSample(ch, i));
    if (resumedEnergy > 0.0001) {
        std::fprintf(stderr, "Audio continued after device/resource reset: %f\n", resumedEnergy);
        return 1;
    }
    std::printf("Verified %zu varied patches, %d FX combinations, JSON and FX state restore\n",
                global.size(), fxCount);
    return 0;
}
