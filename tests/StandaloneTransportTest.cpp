#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <cstdio>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    WelleyzProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    processor.setRateAndBufferSizeDetails(44100.0, 256);
    processor.prepareToPlay(44100.0, 256);
    {
        WelleyzEditor editor(processor);
        editor.setZoom(1.0f);
        auto* record = dynamic_cast<juce::TextButton*>(editor.findChildWithID("recordButton"));
        auto* metro = dynamic_cast<juce::TextButton*>(editor.findChildWithID("metronomeButton"));
        auto* tempo = editor.findChildWithID("tempoPanel");
        auto* bpm = tempo != nullptr
                        ? dynamic_cast<juce::TextEditor*>(tempo->findChildWithID("standaloneBpm"))
                        : nullptr;
        auto* metroToggle =
            tempo != nullptr
                ? dynamic_cast<juce::TextButton*>(tempo->findChildWithID("metronomeToggle"))
                : nullptr;
        const auto* manual = editor.findChildWithID("manualButton");
        const auto* colors = editor.findChildWithID("colorsButton");
        const auto* settings = editor.findChildWithID("settingsButton");
        const auto* zoom = editor.findChildWithID("zoomButton");
        if (record == nullptr || metro == nullptr || tempo == nullptr || bpm == nullptr ||
            metroToggle == nullptr || manual == nullptr || colors == nullptr ||
            settings == nullptr || zoom == nullptr || tempo->isVisible() ||
            bpm->getText() != "120") {
            std::fprintf(stderr, "Standalone transport or header controls are missing\n");
            return 1;
        }
        for (const auto* button :
             {static_cast<const juce::Component*>(metro), manual, colors, settings, zoom})
            if (button->getWidth() != record->getWidth() ||
                button->getHeight() != record->getHeight() || button->getY() != record->getY()) {
                std::fprintf(stderr, "Standalone header button sizes are inconsistent\n");
                return 1;
            }
        if (record->getWidth() != 48 || record->getHeight() != 48) {
            std::fprintf(stderr, "Standalone header size is not 48x48\n");
            return 1;
        }
        if (argc > 1) {
            juce::PNGImageFormat png;
            const auto imageFile = juce::File(juce::String::fromUTF8(argv[1]));
            auto stream = imageFile.createOutputStream();
            if (stream == nullptr ||
                !png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),
                                        *stream))
                return 1;
            stream.reset();
            metro->onClick();
            if (!tempo->isVisible())
                return 1;
            auto tempoStream =
                imageFile.getSiblingFile(imageFile.getFileNameWithoutExtension() + "-tempo.png")
                    .createOutputStream();
            if (tempoStream == nullptr ||
                !png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),
                                        *tempoStream))
                return 1;
            tempoStream.reset();
            metro->onClick();
            if (auto* manual =
                    dynamic_cast<juce::TextButton*>(editor.findChildWithID("manualButton"))) {
                manual->onClick();
                auto manualStream =
                    imageFile
                        .getSiblingFile(imageFile.getFileNameWithoutExtension() + "-manual.png")
                        .createOutputStream();
                if (manualStream == nullptr ||
                    !png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),
                                            *manualStream))
                    return 1;
            }
        }
        metro->onClick();
        if (!tempo->isVisible())
            return 1;
        bpm->setText("156", false);
        if (bpm->onReturnKey)
            bpm->onReturnKey();
        if (processor.standaloneBpm() != 156)
            return 1;
        metroToggle->setToggleState(true, juce::sendNotification);
        if (!processor.metronomeEnabled() || !metro->getToggleState())
            return 1;
        metroToggle->setToggleState(false, juce::sendNotification);
        if (processor.metronomeEnabled() || metro->getToggleState())
            return 1;
        metro->onClick();
        if (tempo->isVisible())
            return 1;
    }
    processor.setStandaloneBpm(95);
    processor.setMetronomeEnabled(true);
    if (processor.standaloneBpm() != 95 || !processor.metronomeEnabled() ||
        !processor.startRecording())
        return 1;
    {
        WelleyzEditor reopened(processor);
        auto* panel = reopened.findChildWithID("tempoPanel");
        auto* bpm = panel != nullptr
                        ? dynamic_cast<juce::TextEditor*>(panel->findChildWithID("standaloneBpm"))
                        : nullptr;
        auto* met = dynamic_cast<juce::Button*>(reopened.findChildWithID("metronomeButton"));
        auto* rec = dynamic_cast<juce::TextButton*>(reopened.findChildWithID("recordButton"));
        if (bpm == nullptr || bpm->getText() != "95" || met == nullptr || !met->getToggleState() ||
            rec == nullptr || rec->getButtonText() != "STOP")
            return 1;
        auto& look = static_cast<WelleyzLookAndFeel&>(rec->getLookAndFeel());
        look.mRecordBlinkBright = true;
        const auto bright = rec->createComponentSnapshot(rec->getLocalBounds());
        look.mRecordBlinkBright = false;
        const auto dim = rec->createComponentSnapshot(rec->getLocalBounds());
        const auto brightColour = bright.getPixelAt(6, rec->getHeight() / 2);
        const auto dimColour = dim.getPixelAt(6, rec->getHeight() / 2);
        if (!rec->getToggleState() || brightColour.getRed() <= brightColour.getGreen() * 2 ||
            brightColour.getRed() <= dimColour.getRed() + 50) {
            std::fprintf(stderr, "REC does not pulse red while recording\n");
            return 1;
        }
        if (argc > 1) {
            juce::PNGImageFormat png;
            const auto imageFile = juce::File(juce::String::fromUTF8(argv[1]));
            for (const auto& state :
                 {std::pair{true, "record-bright"}, std::pair{false, "record-dim"}}) {
                look.mRecordBlinkBright = state.first;
                const auto file = imageFile.getSiblingFile(imageFile.getFileNameWithoutExtension() +
                                                           "-" + state.second + ".png");
                auto stream = file.createOutputStream();
                if (stream == nullptr ||
                    !png.writeImageToStream(
                        reopened.createComponentSnapshot(reopened.getLocalBounds()), *stream))
                    return 1;
            }
        }
        look.mRecordBlinkBright = true;
    }
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    processor.processBlock(audio, midi);
    midi.clear();
    for (int i = 0; i < 10; ++i)
        processor.processBlock(audio, midi);
    const auto wav = processor.stopRecording();
    juce::WavAudioFormat format;
    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        format.createReaderFor(wav.createInputStream().release(), true));
    const bool valid = !processor.recordingFailed() && reader != nullptr &&
                       reader->numChannels == 2 && reader->lengthInSamples == 11 * 256;
    reader.reset();
    wav.deleteFile();
    if (!valid) {
        std::fprintf(stderr, "Standalone WAV stream is incomplete\n");
        return 1;
    }
    std::printf("Standalone BPM, metronome and stereo WAV verified\n");
    return 0;
}
