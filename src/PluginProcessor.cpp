#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ThemeManager.h"
#include <algorithm>
#include <cmath>
#include <limits>

WelleyzProcessor::WelleyzProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mApvts(*this, nullptr, "Welleyz", createLayout()) {
    mKeyboardState.addListener(this);
    for (auto& v : mVoices)
        v.mEnvelope.setSampleRate(44100.0);
}

WelleyzProcessor::~WelleyzProcessor() {
    stopRecording().deleteFile();
    mKeyboardState.removeListener(this);
}

juce::File WelleyzProcessor::recordingDirectory() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("welleyz recordings");
}

bool WelleyzProcessor::startRecording() {
    if (wrapperType != wrapperType_Standalone || isRecording() || mRecorder != nullptr ||
        getSampleRate() <= 0.0)
        return false;
    mRecordingTempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getNonexistentChildFile("welleyz-recording", ".wav", false);
    auto stream = mRecordingTempFile.createOutputStream();
    if (stream == nullptr)
        return false;
    juce::WavAudioFormat wav;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wav.createWriterFor(stream.release(), getSampleRate(), 2, 24, {}, 0));
    if (writer == nullptr) {
        mRecordingTempFile.deleteFile();
        return false;
    }
    mRecordingThread = std::make_unique<juce::TimeSliceThread>("welleyz WAV writer");
    mRecordingThread->startThread();
    mRecordingOverflow.store(false);
    mRecorder = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer.release(), *mRecordingThread, 1 << 19);
    mRecordingWriter.store(mRecorder.get());
    return true;
}

juce::File WelleyzProcessor::stopRecording() {
    mRecordingWriter.store(nullptr);
    while (mActiveRecordCallbacks.load() != 0)
        juce::Thread::sleep(1);
    mRecorder.reset();
    if (mRecordingThread != nullptr) {
        mRecordingThread->stopThread(5000);
        mRecordingThread.reset();
    }
    auto file = mRecordingTempFile;
    mRecordingTempFile = juce::File{};
    return file;
}

void WelleyzProcessor::handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) {
    if (mMirroringHostMidi)
        return;
    const juce::AbstractFifo::ScopedWrite write(mKeyboardInputFifo, 1);
    if (write.blockSize1 == 0)
        mKeyboardInputOverflow.store(true);
    else
        mKeyboardInput[(size_t)write.startIndex1] = {note, velocity, true};
}

void WelleyzProcessor::handleNoteOff(juce::MidiKeyboardState*, int, int note, float velocity) {
    if (mMirroringHostMidi)
        return;
    const juce::AbstractFifo::ScopedWrite write(mKeyboardInputFifo, 1);
    if (write.blockSize1 == 0)
        mKeyboardInputOverflow.store(true);
    else
        mKeyboardInput[(size_t)write.startIndex1] = {note, velocity, false};
}

void WelleyzProcessor::enqueueDisplay(int note, float velocity, bool on) {
    const juce::AbstractFifo::ScopedWrite write(mKeyboardDisplayFifo, 1);
    if (write.blockSize1 != 0)
        mKeyboardDisplay[(size_t)write.startIndex1] = {note, velocity, on};
    else
        mKeyboardDisplayOverflow.store(true);
}

void WelleyzProcessor::flushKeyboardDisplay() {
    mMirroringHostMidi = true;
    const int ready = mKeyboardDisplayFifo.getNumReady();
    {
        const juce::AbstractFifo::ScopedRead read(mKeyboardDisplayFifo, ready);
        for (int i = 0; i < read.blockSize1 + read.blockSize2; ++i) {
            const int index =
                i < read.blockSize1 ? read.startIndex1 + i : read.startIndex2 + i - read.blockSize1;
            const auto& event = mKeyboardDisplay[(size_t)index];
            if (event.mNote < 0)
                mKeyboardState.allNotesOff(0);
            else if (event.mOn)
                mKeyboardState.noteOn(1, event.mNote, event.mVelocity);
            else
                mKeyboardState.noteOff(1, event.mNote, event.mVelocity);
        }
    }
    if (mKeyboardDisplayOverflow.exchange(false))
        mKeyboardState.allNotesOff(0);
    mMirroringHostMidi = false;
    juce::MidiBuffer pending;
    mKeyboardState.processNextMidiBuffer(pending, 0, 1, false);
}

juce::AudioProcessorValueTreeState::ParameterLayout WelleyzProcessor::createLayout() {
    using P = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray waves{"Sine", "Square", "Saw", "Triangle", "Pulse 12%", "Pulse 25%", "Noise"};

    for (int i = 1; i <= 3; ++i) {
        juce::String n = "oscillator" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            n + "Wave", "Osc " + juce::String(i) + " Wave", waves,
            // Пресет в духе чиптюна: 1=pulse25, 2=square, 3=triangle
            (i == 1 ? 5 : (i == 2 ? 1 : 3))));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            n + "Level", "Osc " + juce::String(i) + " Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), (i == 1 ? 0.8f : 0.6f)));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            n + "Octave", "Osc " + juce::String(i) + " Octave", -2, 2, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            n + "Semitone", "Osc " + juce::String(i) + " Semi", -12, 12, (i == 3 ? -12 : 0)));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            n + "FineTune", "Osc " + juce::String(i) + " Fine",
            juce::NormalisableRange<float>(-100.0f, 100.0f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            n + "Pan", "Osc " + juce::String(i) + " Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            n + "Voices", "Osc " + juce::String(i) + " Voices",
            juce::StringArray{"1", "2", "3", "4", "6", "8"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            n + "Detune", "Osc " + juce::String(i) + " Detune",
            juce::NormalisableRange<float>(0.0f, 50.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [](float v, int) { return juce::String(v, 1) + " ct"; })));
    }

    auto hz = [](float v, int) {
        return v < 1000.0f ? juce::String((int)v) + " Hz" : juce::String(v / 1000.0f, 1) + " kHz";
    };
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterCutoff", "Filter Cutoff",
        juce::NormalisableRange<float>(80.0f, 18000.0f, 0.0f, 0.35f), 12000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(hz)));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filterResonance", "Filter Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amplitudeAttack", "Amp Attack", juce::NormalisableRange<float>(0.001f, 2.0f, 0.0f, 0.4f),
        0.005f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amplitudeDecay", "Amp Decay", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.4f),
        0.12f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amplitudeSustain", "Amp Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amplitudeRelease", "Amp Release", juce::NormalisableRange<float>(0.01f, 4.0f, 0.0f, 0.4f),
        0.15f));

    // Ретро-секция: лампа + биткраш + вибрато
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tubeDrive", "Tube Drive", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tubeWarmth", "Tube Warmth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "tubeLampiness", "Lampiness", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lofiBitDepth", "Bit Depth", juce::NormalisableRange<float>(4.0f, 16.0f, 1.0f), 12.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return v >= 15.5f ? juce::String("16 (off)") : juce::String((int)v) + "-bit";
        })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lofiRateReduction", "Rate Crush", juce::NormalisableRange<float>(1.0f, 32.0f, 1.0f, 0.5f),
        1.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return v <= 1.5f ? juce::String("Off") : juce::String("1/") + juce::String((int)v);
        })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "vibratoRate", "Vibrato Rate", juce::NormalisableRange<float>(0.0f, 12.0f), 5.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "vibratoDepth", "Vibrato Depth", juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String((int)v) + " ct"; })));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "arpeggiatorMode", "Arp Mode", juce::StringArray{"Off", "Up", "Down", "Up-Down"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpeggiatorRate", "Arp Rate", juce::NormalisableRange<float>(1.0f, 20.0f), 8.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "portamentoTime", "Portamento Time",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.0f, 0.45f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String((int)(v * 1000.0f)) + " ms"; })));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "portamentoMode", "Portamento Mode", juce::StringArray{"Off", "Always", "Legato"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "masterLevel", "Master Volume", juce::NormalisableRange<float>(0.0f, 1.2f), 0.8f));

    // The serial FX rack starts bypassed so a fresh sound has no effect tail.
    for (const auto& entry :
         {std::pair<const char*, const char*>{"effectsFilterEnabled", "FX Filter On"},
          {"effectsHazeEnabled", "FX Haze On"},
          {"effectsTapsEnabled", "FX Taps On"},
          {"effectsSpaceEnabled", "FX Space On"}})
        params.push_back(
            std::make_unique<juce::AudioParameterBool>(entry.first, entry.second, false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "effectsFilterMode", "FX Filter Mode", juce::StringArray{"Lowpass", "Bandpass", "Highpass"},
        0));
    auto fxParam = [&](const char* id, const char* name, float min, float max, float initial) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            id, name, juce::NormalisableRange<float>(min, max), initial));
    };
    fxParam("effectsFilterFrequency", "FX Filter Frequency", 80.0f, 16000.0f, 6000.0f);
    fxParam("effectsFilterResonance", "FX Filter Resonance", 0.0f, 1.0f, 0.2f);
    fxParam("effectsFilterMix", "FX Filter Mix", 0.0f, 1.0f, 0.5f);
    fxParam("effectsHazeRate", "FX Haze Rate", 0.05f, 4.0f, 0.45f);
    fxParam("effectsHazeDepth", "FX Haze Depth", 0.0f, 1.0f, 0.5f);
    fxParam("effectsHazeMix", "FX Haze Mix", 0.0f, 1.0f, 0.35f);
    fxParam("effectsTapsTime", "FX Taps Time", 0.07f, 1.2f, 0.32f);
    fxParam("effectsTapsFeedback", "FX Taps Feedback", 0.0f, 0.82f, 0.38f);
    fxParam("effectsTapsMix", "FX Taps Mix", 0.0f, 1.0f, 0.3f);
    fxParam("effectsSpaceSize", "FX Space Size", 0.0f, 1.0f, 0.55f);
    fxParam("effectsSpaceDamping", "FX Space Damping", 0.0f, 1.0f, 0.45f);
    fxParam("effectsSpaceMix", "FX Space Mix", 0.0f, 1.0f, 0.35f);

    for (const auto& entry : {std::pair<const char*, const char*>{"oscillator1Enabled", "Osc 1 On"},
                              {"oscillator2Enabled", "Osc 2 On"},
                              {"oscillator3Enabled", "Osc 3 On"},
                              {"amplitudeEnabled", "Envelope On"},
                              {"filterEnabled", "Filter On"},
                              {"tubeEnabled", "Tube and Lo-Fi On"}})
        params.push_back(
            std::make_unique<juce::AudioParameterBool>(entry.first, entry.second, true));

    // Host-visible selection: FL Studio may restore the processor from its last
    // parameter snapshot when the VST window is closed without saving a project.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "randomSoundType", "Sound Type",
        juce::StringArray{"Pad", "Synth", "Lead", "Pluck", "Bell", "Bass", "Arp", "Keys", "Organ"},
        1));

    params.push_back(std::make_unique<juce::AudioParameterBool>("backgroundNoiseEnabled",
                                                                "Background Noise On", false));
    juce::StringArray noiseNames;
    for (auto* name : welleyz::backgroundNoise::kNames)
        noiseNames.add(name);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "backgroundNoiseType", "Background Noise Type", noiseNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "backgroundNoiseLevel", "Background Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    return {params.begin(), params.end()};
}

void WelleyzProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    mBeatPosition = 0.0;
    mBeatIndex = 0;
    mClickSamples = 0;
    mClickAmplitude = 0.0f;
    mHeldNotes.ensureStorageAllocated(128);
    mArpOrder.ensureStorageAllocated(256);
    resetVoiceState();
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    mProcessSpec.numChannels = 2;
    mFilter.prepare(mProcessSpec);
    mFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    mFilter.setCutoffFrequency(12000.0f);
    mFilter.setResonance(0.7f);
    mEffects.prepare(sampleRate, samplesPerBlock);

    for (auto& v : mVoices) {
        v.mEnvelope.setSampleRate(sampleRate);
        v.mEnvelope.reset();
    }
    mSmoothMaster.reset(sampleRate, 0.02);
    mSmoothMaster.setCurrentAndTargetValue(getParam("masterLevel"));
    mSmoothPitchBend.reset(sampleRate, 0.01);
    mSmoothModWheel.reset(sampleRate, 0.02);
    mSmoothPitchBend.setCurrentAndTargetValue(0.0f);
    mSmoothModWheel.setCurrentAndTargetValue(0.0f);
    mVibratoPhase = 0.0;
    mCrusher.reset();
    mNoiseLeft.reset(sampleRate, 0x4a3b2c1du);
    mNoiseRight.reset(sampleRate, 0x8e71bc65u);
    mCrushHoldLeft = mCrushHoldRight = 0.0f;
    mCrushCounter = 0;
    mArpPhase = 0.0;
    mArpIndex = 0;
    mArpCurrentNote = -1;
    mLastMidiNote = -1;
}

void WelleyzProcessor::releaseResources() {
    resetPlaybackState();
}

void WelleyzProcessor::resetPlaybackState() {
    resetVoiceState();
    mFilter.reset();
    mTubeLeft.reset();
    mTubeRight.reset();
    mCrusher.reset();
    mCrushHoldLeft = mCrushHoldRight = 0.0f;
    mCrushCounter = 0;
    mEffects.reset();
    mNoiseLeft.reset(mProcessSpec.sampleRate, 0x4a3b2c1du);
    mNoiseRight.reset(mProcessSpec.sampleRate, 0x8e71bc65u);
}

float WelleyzProcessor::getParam(juce::StringRef id) const {
    if (auto* p = mApvts.getRawParameterValue(id))
        return p->load();
    return 0.0f;
}

int WelleyzProcessor::getParamChoice(juce::StringRef id) const {
    if (auto* p = mApvts.getRawParameterValue(id))
        return (int)std::round(p->load());
    return 0;
}

WelleyzProcessor::Voice* WelleyzProcessor::findFreeVoice(int note) {
    // Если нота уже играет — перезапустить её (legato retrig)
    for (auto& v : mVoices)
        if (v.mActive && v.mNote == note)
            return &v;
    for (auto& v : mVoices)
        if (!v.mActive)
            return &v;
    // Voice stealing: самая старая
    Voice* oldest = &mVoices[0];
    for (auto& v : mVoices)
        if (v.mAge < oldest->mAge)
            oldest = &v;
    return oldest;
}

void WelleyzProcessor::noteOnInternal(int note, float vel, bool legato, int previousHeldNote) {
    int sourceNote = mLastMidiNote;
    if (getParamChoice("portamentoMode") == 2 && legato && previousHeldNote >= 0)
        sourceNote = previousHeldNote;
    double sourcePitch = (double)sourceNote;
    for (const auto& activeVoice : mVoices)
        if (activeVoice.mActive && activeVoice.mNote == sourceNote) {
            sourcePitch = activeVoice.mCurrentPitch;
            break;
        }

    // A piano-roll overlap in Legato mode slides the sounding voice itself:
    // retain oscillator phase and envelope instead of starting a second note.
    if (getParamChoice("portamentoMode") == 2 && legato && previousHeldNote >= 0)
        for (auto& voice : mVoices)
            if (voice.mActive && voice.mHeld && voice.mNote == previousHeldNote) {
                voice.mNote = note;
                voice.mVelocity = vel;
                const float time = getParam("portamentoTime");
                voice.mGlideSamples =
                    time > 0.0f ? juce::jmax(1, juce::roundToInt(time * (float)getSampleRate()))
                                : 0;
                voice.mPitchStep = voice.mGlideSamples > 0 ? ((double)note - voice.mCurrentPitch) /
                                                                 (double)voice.mGlideSamples
                                                           : 0.0;
                if (voice.mGlideSamples == 0)
                    voice.mCurrentPitch = (double)note;
                mLastMidiNote = note;
                return;
            }

    if (auto* v = findFreeVoice(note)) {
        const int mode = getParamChoice("portamentoMode");
        const float time = getParam("portamentoTime");
        const bool shouldGlide =
            time > 0.0f && sourceNote >= 0 && (mode == 1 || (mode == 2 && legato));
        v->mActive = true;
        v->mHeld = true;
        v->mNote = note;
        v->mVelocity = vel;
        v->mCurrentPitch = shouldGlide ? sourcePitch : (double)note;
        v->mGlideSamples =
            shouldGlide ? juce::jmax(1, juce::roundToInt(time * (float)getSampleRate())) : 0;
        v->mPitchStep = v->mGlideSamples > 0
                            ? ((double)note - v->mCurrentPitch) / (double)v->mGlideSamples
                            : 0.0;
        for (auto& osc : v->mOsc)
            for (auto& state : osc)
                state.reset();
        v->mEnvelope.setParams(getParam("amplitudeAttack"), getParam("amplitudeDecay"),
                               getParam("amplitudeSustain"), getParam("amplitudeRelease"));
        v->mEnvelope.noteOn();
        v->mAge = mVoiceClock++;
        mLastMidiNote = note;
    }
}

void WelleyzProcessor::noteOffInternal(int note) {
    for (auto& v : mVoices)
        if (v.mActive && v.mNote == note) {
            v.mHeld = false;
            v.mEnvelope.noteOff();
        }
}

void WelleyzProcessor::allNotesOff() {
    for (auto& v : mVoices)
        if (v.mActive) {
            v.mHeld = false;
            v.mEnvelope.noteOff();
        }
}

void WelleyzProcessor::resetVoiceState() {
    for (auto& v : mVoices) {
        v.mActive = false;
        v.mHeld = false;
        v.mNote = -1;
        v.mVelocity = 0.0f;
        v.mEnvelope.reset();
        v.mCurrentPitch = 60.0;
        v.mPitchStep = 0.0;
        v.mGlideSamples = 0;
        for (auto& osc : v.mOsc)
            for (auto& state : osc)
                state.reset();
    }
    mHeldNotes.clear();
    mArpOrder.clear();
    mArpIndex = 0;
    mArpPhase = 0.0;
    mArpCurrentNote = -1;
    mLastMidiNote = -1;
}

void WelleyzProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int N = buffer.getNumSamples();
    const double sr = getSampleRate();
    buffer.clear();

    float bendValue = mSmoothPitchBend.getTargetValue();
    float modulation = mSmoothModWheel.getTargetValue();
    if (const auto revision = mUiPitchRevision.load(); revision != mConsumedPitchRevision) {
        mConsumedPitchRevision = revision;
        bendValue = (mUiPitchValue.load() - 8192) / 8192.0f * 2.0f;
    }
    if (const auto revision = mUiModRevision.load(); revision != mConsumedModRevision) {
        mConsumedModRevision = revision;
        modulation = mUiModValue.load() / 127.0f;
    }

    // --- MIDI + арпеджиатор ---
    int arpMode = getParamChoice("arpeggiatorMode");
    float arpRate = getParam("arpeggiatorRate");

    auto rebuildArpeggio = [&] {
        mArpOrder.clearQuick();
        mArpOrder.addArray(mHeldNotes);
        std::sort(mArpOrder.begin(), mArpOrder.end());
        if (arpMode == 2)
            std::reverse(mArpOrder.begin(), mArpOrder.end());
        else if (arpMode == 3)
            for (int i = mHeldNotes.size() - 2; i >= 1; --i)
                mArpOrder.add(mArpOrder[i]);
    };

    if (mKeyboardInputOverflow.exchange(false)) {
        mHeldNotes.clearQuick();
        mArpOrder.clearQuick();
        allNotesOff();
    }
    const int inputReady = mKeyboardInputFifo.getNumReady();
    {
        const juce::AbstractFifo::ScopedRead read(mKeyboardInputFifo, inputReady);
        for (int i = 0; i < read.blockSize1 + read.blockSize2; ++i) {
            const int index =
                i < read.blockSize1 ? read.startIndex1 + i : read.startIndex2 + i - read.blockSize1;
            const auto& event = mKeyboardInput[(size_t)index];
            if (event.mOn) {
                const bool legato = !mHeldNotes.isEmpty();
                const int previous = legato ? mHeldNotes.getLast() : -1;
                if (!mHeldNotes.contains(event.mNote))
                    mHeldNotes.add(event.mNote);
                if (arpMode == 0)
                    noteOnInternal(event.mNote, event.mVelocity, legato, previous);
                else {
                    rebuildArpeggio();
                    if (mArpCurrentNote < 0 && !mArpOrder.isEmpty()) {
                        mArpIndex = 0;
                        mArpCurrentNote = mArpOrder[0];
                        noteOnInternal(mArpCurrentNote, event.mVelocity);
                        mArpPhase = 0.0;
                    }
                }
            } else {
                mHeldNotes.removeAllInstancesOf(event.mNote);
                if (arpMode == 0)
                    noteOffInternal(event.mNote);
                else {
                    rebuildArpeggio();
                    if (mHeldNotes.isEmpty()) {
                        if (mArpCurrentNote >= 0)
                            noteOffInternal(mArpCurrentNote);
                        mArpCurrentNote = -1;
                        mArpIndex = 0;
                    } else if (event.mNote == mArpCurrentNote) {
                        noteOffInternal(mArpCurrentNote);
                        mArpCurrentNote = -1;
                        mArpPhase = 0.0;
                    }
                }
            }
        }
    }

    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        if (msg.isPitchWheel())
            bendValue = (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
        else if (msg.isController() && msg.getControllerNumber() == 1)
            modulation = msg.getControllerValue() / 127.0f;
        else if (msg.isNoteOn()) {
            enqueueDisplay(msg.getNoteNumber(), msg.getFloatVelocity(), true);
            int n = msg.getNoteNumber();
            float vel = msg.getFloatVelocity();
            const bool legato = !mHeldNotes.isEmpty();
            const int previousHeldNote = legato ? mHeldNotes.getLast() : -1;
            if (!mHeldNotes.contains(n))
                mHeldNotes.add(n);
            if (arpMode == 0)
                noteOnInternal(n, vel, legato, previousHeldNote);
            else {
                // Перестроить порядок арпа
                rebuildArpeggio();
                if (mArpCurrentNote < 0 && mArpOrder.size() > 0) {
                    mArpIndex = 0;
                    mArpCurrentNote = mArpOrder[0];
                    noteOnInternal(mArpCurrentNote, vel);
                    mArpPhase = 0.0;
                }
            }
        } else if (msg.isNoteOff()) {
            enqueueDisplay(msg.getNoteNumber(), msg.getFloatVelocity(), false);
            int n = msg.getNoteNumber();
            mHeldNotes.removeAllInstancesOf(n);
            if (arpMode != 0)
                rebuildArpeggio();
            if (arpMode == 0)
                noteOffInternal(n);
            else {
                if (mHeldNotes.isEmpty()) {
                    if (mArpCurrentNote >= 0)
                        noteOffInternal(mArpCurrentNote);
                    mArpCurrentNote = -1;
                    mArpOrder.clear();
                    mArpIndex = 0;
                } else if (n == mArpCurrentNote) {
                    noteOffInternal(mArpCurrentNote);
                    mArpCurrentNote = -1; // следующий шаг подхватит из heldNotes
                    mArpPhase = 0.0;
                }
            }
        } else if (msg.isAllSoundOff()) {
            enqueueDisplay(-1, 0.0f, false);
            resetPlaybackState();
        } else if (msg.isAllNotesOff()) {
            enqueueDisplay(-1, 0.0f, false);
            mHeldNotes.clear();
            mArpOrder.clear();
            mArpCurrentNote = -1;
            allNotesOff();
        }
    }
    mSmoothPitchBend.setTargetValue(bendValue);
    mSmoothModWheel.setTargetValue(modulation);

    // --- Снэпшот параметров на блок ---
    struct OscP {
        int wave, oct, semi, mVoices;
        float fine, level, pan, detune;
    };
    OscP op[3];
    for (int i = 0; i < 3; ++i) {
        static constexpr const char* ids[3][8]{
            {"oscillator1Wave", "oscillator1Octave", "oscillator1Semitone", "oscillator1FineTune",
             "oscillator1Level", "oscillator1Pan", "oscillator1Voices", "oscillator1Detune"},
            {"oscillator2Wave", "oscillator2Octave", "oscillator2Semitone", "oscillator2FineTune",
             "oscillator2Level", "oscillator2Pan", "oscillator2Voices", "oscillator2Detune"},
            {"oscillator3Wave", "oscillator3Octave", "oscillator3Semitone", "oscillator3FineTune",
             "oscillator3Level", "oscillator3Pan", "oscillator3Voices", "oscillator3Detune"}};
        op[i].wave = getParamChoice(ids[i][0]);
        op[i].oct = (int)std::round(getParam(ids[i][1]));
        op[i].semi = (int)std::round(getParam(ids[i][2]));
        op[i].fine = getParam(ids[i][3]);
        op[i].level = getParam(ids[i][4]);
        op[i].pan = getParam(ids[i][5]);
        op[i].mVoices = getParamChoice(ids[i][6]);
        op[i].detune = getParam(ids[i][7]);
    }
    const bool oscEnabled[3]{getParam("oscillator1Enabled") > 0.5f,
                             getParam("oscillator2Enabled") > 0.5f,
                             getParam("oscillator3Enabled") > 0.5f};
    const bool envEnabled = getParam("amplitudeEnabled") > 0.5f;
    const bool filterEnabled = getParam("filterEnabled") > 0.5f;
    const bool tubeEnabled = getParam("tubeEnabled") > 0.5f;
    const bool noiseEnabled = getParam("backgroundNoiseEnabled") > 0.5f;
    const int noiseType = getParamChoice("backgroundNoiseType");
    const float noiseGain = 0.22f * std::pow(getParam("backgroundNoiseLevel"), 2.0f);
    float cutoff = getParam("filterCutoff");
    float reso = getParam("filterResonance");
    float drive = getParam("tubeDrive");
    float warmth = getParam("tubeWarmth");
    const float lampiness = getParam("tubeLampiness");
    float bits = getParam("lofiBitDepth");
    int crushDiv = juce::jmax(1, (int)std::round(getParam("lofiRateReduction")));
    float vibRate = getParam("vibratoRate");
    float vibDepthCt = tubeEnabled ? getParam("vibratoDepth") : 0.0f;
    WelleyzFx::Settings effect;
    effect.mFilterOn = getParam("effectsFilterEnabled") > 0.5f;
    effect.mHazeOn = getParam("effectsHazeEnabled") > 0.5f;
    effect.mTapsOn = getParam("effectsTapsEnabled") > 0.5f;
    effect.mSpaceOn = getParam("effectsSpaceEnabled") > 0.5f;
    effect.mFilterMode = getParamChoice("effectsFilterMode");
    effect.mFilterFreq = getParam("effectsFilterFrequency");
    effect.mFilterReso = getParam("effectsFilterResonance");
    effect.mFilterMix = getParam("effectsFilterMix");
    effect.mHazeRate = getParam("effectsHazeRate");
    effect.mHazeDepth = getParam("effectsHazeDepth");
    effect.mHazeMix = getParam("effectsHazeMix");
    effect.mTapsTime = getParam("effectsTapsTime");
    if (wrapperType == wrapperType_Standalone) {
        const auto beats = std::pow(2.0f, std::round(std::log2(effect.mTapsTime / 0.64f)));
        effect.mTapsTime = juce::jlimit(0.07f, 1.2f, beats * 60.0f / (float)mStandaloneBpm.load());
    }
    effect.mTapsFeedback = getParam("effectsTapsFeedback");
    effect.mTapsMix = getParam("effectsTapsMix");
    effect.mSpaceSize = getParam("effectsSpaceSize");
    effect.mSpaceDamp = getParam("effectsSpaceDamping");
    effect.mSpaceMix = getParam("effectsSpaceMix");
    mEffects.configure(effect);
    mSmoothMaster.setTargetValue(getParam("masterLevel"));

    if (filterEnabled) {
        mFilter.setCutoffFrequency(juce::jlimit(40.0f, 19000.0f, cutoff));
        mFilter.setResonance(juce::jlimit(0.5f, 8.0f, 0.5f + reso * 7.0f));
    } else
        mFilter.reset();
    if (!tubeEnabled) {
        mTubeLeft.reset();
        mTubeRight.reset();
        mCrusher.reset();
        mCrushHoldLeft = mCrushHoldRight = 0.0f;
        mCrushCounter = 0;
    }

    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    const double vibInc = vibRate / sr;
    const float vibDepthSemis = vibDepthCt / 100.0f;

    for (int s = 0; s < N; ++s) {
        // Арпеджиатор по времени
        if (arpMode != 0 && !mHeldNotes.isEmpty()) {
            double stepSamples = sr / juce::jmax(0.5, (double)arpRate);
            mArpPhase += 1.0;
            if (mArpPhase >= stepSamples) {
                mArpPhase = 0.0;
                if (!mArpOrder.isEmpty()) {
                    if (mArpCurrentNote >= 0)
                        noteOffInternal(mArpCurrentNote);
                    mArpIndex = (mArpIndex + 1) % mArpOrder.size();
                    mArpCurrentNote = mArpOrder[mArpIndex];
                    noteOnInternal(mArpCurrentNote, 0.9f);
                }
            }
        }

        // Вибрато: +-depth в полутонах
        mVibratoPhase += vibInc;
        if (mVibratoPhase >= 1.0)
            mVibratoPhase -= 1.0;
        const float wheelDepth = mSmoothModWheel.getNextValue() * 0.75f;
        float vibSemis = vibDepthSemis + wheelDepth > 0.0001f
                             ? (float)std::sin(2.0 * welleyz::dsp::kPi * mVibratoPhase) *
                                   (vibDepthSemis + wheelDepth)
                             : 0.0f;
        const float pitchBend = mSmoothPitchBend.getNextValue();

        float mixL = 0.0f, mixR = 0.0f;

        for (auto& v : mVoices) {
            if (!v.mActive)
                continue;
            if (v.mGlideSamples > 0) {
                v.mCurrentPitch += v.mPitchStep;
                if (--v.mGlideSamples == 0)
                    v.mCurrentPitch = (double)v.mNote;
            }
            float env = 1.0f;
            if (envEnabled) {
                v.mEnvelope.setParams(getParam("amplitudeAttack"), getParam("amplitudeDecay"),
                                      getParam("amplitudeSustain"), getParam("amplitudeRelease"));
                env = v.mEnvelope.tick();
                if (!v.mEnvelope.isActive()) {
                    v.mActive = false;
                    continue;
                }
            } else if (!v.mHeld) {
                v.mActive = false;
                continue;
            }

            float voiceSum = 0.0f;
            float panL = 0.0f, panR = 0.0f;
            bool hasPan = false;

            for (int i = 0; i < 3; ++i) {
                if (!oscEnabled[i])
                    continue;
                const int voiceCounts[]{1, 2, 3, 4, 6, 8};
                const int unisonCount = voiceCounts[juce::jlimit(0, 5, op[i].mVoices)];
                double unisonSum = 0.0;
                for (int u = 0; u < unisonCount; ++u) {
                    const double spread =
                        unisonCount > 1
                            ? ((double)u / (double)(unisonCount - 1) * 2.0 - 1.0) * op[i].detune
                            : 0.0;
                    const double semis = op[i].oct * 12.0 + op[i].semi +
                                         (op[i].fine + spread) / 100.0 + vibSemis + pitchBend;
                    const double frequency =
                        440.0 * std::pow(2.0, (v.mCurrentPitch - 69.0 + semis) / 12.0);
                    unisonSum +=
                        welleyz::dsp::renderOsc((welleyz::dsp::Wave)op[i].wave,
                                                v.mOsc[(size_t)i][(size_t)u], frequency, sr);
                }
                float s = (float)(unisonSum / (double)unisonCount);
                s *= op[i].level;
                voiceSum += s;
                // Панорама на осциллятор (constant power)
                float p = juce::jlimit(-1.0f, 1.0f, op[i].pan);
                float gl = std::cos((p + 1.0f) * 0.78539816f);
                float gr = std::sin((p + 1.0f) * 0.78539816f);
                panL += s * gl;
                panR += s * gr;
                hasPan = true;
            }
            voiceSum /= 3.0f; // нормируем микс 3 осц
            if (hasPan) {
                panL /= 3.0f;
                panR /= 3.0f;
            } else {
                panL = panR = voiceSum;
            }

            float g = env * v.mVelocity;
            mixL += panL * g;
            mixR += panR * g;
        }

        // Мягкий клип микса до фильтра
        mixL = std::tanh(mixL * 1.2f);
        mixR = std::tanh(mixR * 1.2f);

        // Фильтр (моно-обработка L/R через один SVF последовательно — дёшево и сердито;
        // для стерео-точности обрабатываем по очереди)
        if (filterEnabled)
            mixL = mFilter.processSample(0, mixL);
        // сбросим состояние второго канала эмуляцией: прогоним R через тот же фильтр
        // (TPT SVF в JUCE хранит состояние на канал, processSample(ch,...) корректен)
        float filteredR = filterEnabled ? mFilter.processSample(1, mixR) : mixR;

        // Лампа
        float tL = tubeEnabled
                       ? mTubeLeft.process(mixL * 1.5f, drive * lampiness, warmth * lampiness)
                       : mixL;
        float tR = tubeEnabled
                       ? mTubeRight.process(filteredR * 1.5f, drive * lampiness, warmth * lampiness)
                       : filteredR;

        // Биткраш (sample-hold + квантование)
        if (tubeEnabled && (crushDiv > 1 || bits < 15.5f)) {
            if (mCrushCounter % crushDiv == 0) {
                mCrushHoldLeft = welleyz::dsp::BitCrusher::quantize(tL, bits);
                mCrushHoldRight = welleyz::dsp::BitCrusher::quantize(tR, bits);
            }
            mCrushCounter++;
            tL = mCrushHoldLeft;
            tR = mCrushHoldRight;
        }

        mEffects.process(tL, tR, effect);
        if (noiseEnabled && noiseGain > 0.0f) {
            tL += noiseGain * mNoiseLeft.next(noiseType);
            tR += noiseGain * mNoiseRight.next(noiseType);
        }
        float m = mSmoothMaster.getNextValue();
        outL[s] = tL * m * 0.9f;
        if (outR)
            outR[s] = tR * m * 0.9f;
    }
    if (wrapperType == wrapperType_Standalone) {
        mActiveRecordCallbacks.fetch_add(1);
        if (auto* writer = mRecordingWriter.load())
            if (buffer.getNumChannels() < 2 || !writer->write(buffer.getArrayOfReadPointers(), N))
                mRecordingOverflow.store(true);
        mActiveRecordCallbacks.fetch_sub(1);

        const double beatLength = sr * 60.0 / (double)mStandaloneBpm.load();
        for (int s = 0; s < N; ++s) {
            if (mBeatPosition >= beatLength) {
                mBeatPosition -= beatLength;
                mClickSamples = 0;
                mClickAmplitude =
                    mMetronomeEnabled.load() ? (mBeatIndex == 0 ? 0.24f : 0.14f) : 0.0f;
                mBeatIndex = (mBeatIndex + 1) % 4;
            }
            mBeatPosition += 1.0;
            if (mClickAmplitude > 0.00001f) {
                const auto click = mClickAmplitude * std::sin(juce::MathConstants<double>::twoPi *
                                                              (mBeatIndex == 1 ? 1450.0 : 1000.0) *
                                                              (double)mClickSamples++ / sr);
                mClickAmplitude *= 0.994f;
                outL[s] += click;
                if (outR)
                    outR[s] += click;
            }
        }
    }
}

// --- RANDOM: музыкально осмысленная рандомизация ---
void WelleyzProcessor::randomizeParameters(int category, bool factoryPreset) {
    const auto& randomizeOptions = ThemeManager::instance();
    rememberCurrentSound();
    juce::Random rng(factoryPreset ? (juce::int64)(74021 + juce::jlimit(0, 8, category) * 11939)
                                   : juce::Random::getSystemRandom().nextInt64());
    if (mApvts.undoManager != nullptr)
        mApvts.undoManager->beginNewTransaction(factoryPreset ? "Load factory preset"
                                                              : "Randomize welleyz");
    // JUCE8 Random has no nextFloatInRange — local helper instead
    auto frange = [&](float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };

    auto setChoice = [&](const juce::String& id, int v) {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(mApvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    auto setF = [&](const juce::String& id, float v) {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(mApvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    auto setI = [&](const juce::String& id, int v) {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(mApvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1((float)v));
    };
    auto setB = [&](const juce::String& id, bool v) {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*>(mApvts.getParameter(id)))
            p->setValueNotifyingHost(v ? 1.0f : 0.0f);
    };

    category = juce::jlimit(0, 8, category);
    const int variation = rng.nextInt(4);
    auto wave = [&](std::initializer_list<int> choices) {
        auto it = choices.begin();
        std::advance(it, rng.nextInt((int)choices.size()));
        return *it;
    };
    for (int i = 1; i <= 3; ++i) {
        const juce::String n = "oscillator" + juce::String(i);
        int w = 2, oct = 0, semi = 0;
        float level = i == 1 ? frange(0.60f, 0.78f) : frange(0.12f, 0.42f);
        float fine = i == 1 ? 0.0f : frange(-15.0f, 15.0f);
        int unison = 0;
        float detune = 0.0f;
        switch (category) {
        case 0: // Pad: unison / glass fifths / airy noise / low string bed
            w = variation == 0
                    ? 2
                    : (variation == 1 ? wave({0, 3})
                                      : (variation == 2 && i == 3 ? 6 : wave({0, 2, 3})));
            if (variation == 1 && i == 3)
                semi = 7;
            if (variation == 2 && i == 2)
                semi = rng.nextBool() ? 4 : 5;
            if (variation == 3 && i == 3)
                oct = -1;
            if (w == 6)
                level = frange(0.03f, 0.09f);
            else
                level *= variation == 0 ? 0.70f : 0.8f;
            fine *= variation == 0 ? 2.8f : 1.2f;
            break;
        case 1: // Synth: organ, wide saw, pulse stack, gritty digital
            w = variation == 0
                    ? 2
                    : (variation == 1 ? wave({0, 3})
                                      : (variation == 2 ? wave({4, 5}) : (i == 3 ? 6 : 1)));
            if (variation == 1 && i == 2)
                oct = 1;
            if (variation == 1 && i == 3)
                semi = 7;
            if (variation == 0 && i == 3 && rng.nextBool())
                oct = -1;
            if (w == 6)
                level = frange(0.03f, 0.12f);
            break;
        case 2: // Lead: unison / saw octave / pulse reed / square fifth
            w = variation == 0 ? 5 : (variation == 1 ? 2 : (variation == 2 ? wave({1, 3}) : 1));
            if (i == 3)
                oct = variation == 1 ? -1 : 0;
            if (variation == 3 && i == 2)
                semi = 7;
            if (variation == 2 && i == 2 && rng.nextFloat() < 0.4f)
                semi = -12;
            if (variation == 2 && i == 3)
                level *= 0.55f;
            break;
        case 3: // Pluck: rounded, wooden, chiptune, metallic
            w = variation == 0
                    ? wave({0, 3})
                    : (variation == 1 ? 2 : (variation == 2 ? wave({4, 5}) : (i == 3 ? 6 : 3)));
            if (i == 3)
                oct = variation == 1 ? -1 : 1;
            if (variation == 0 && i == 2 && rng.nextFloat() < 0.5f)
                semi = 7;
            if (w == 6)
                level = 0.08f;
            level *= 0.80f;
            break;
        case 4: // Bell: octaves, fifths, soft chime, noisy strike
            w = variation == 3 && i == 3 ? 6 : (variation == 2 ? 3 : 0);
            if (i == 2)
                oct = 1;
            if (i == 3) {
                oct = variation == 1 ? 0 : 1;
                semi = variation == 1 ? 7 : 0;
            }
            if (variation == 2 && i == 2)
                semi = rng.nextBool() ? 4 : 7;
            if (w == 6)
                level = 0.06f;
            level *= 0.72f;
            break;
        case 5: // Bass: round sub, detuned fuzz, pulse, hollow triangle
            w = variation == 0
                    ? (i == 1 ? 0 : 3)
                    : (variation == 1 ? wave({1, 2}) : (variation == 2 ? wave({4, 5}) : 3));
            oct = -1;
            if (i == 3)
                oct = variation == 1 ? -1 : -2;
            fine *= 0.25f;
            break;
        case 6: // Arp: broken chords / octave pulses / retro bleeps
            w = variation == 0 ? wave({1, 5})
                               : (variation == 1 ? 2 : (variation == 2 ? wave({0, 3}) : 4));
            if (i == 2)
                semi = variation == 2 ? 0 : 7;
            if (i == 3)
                oct = variation == 3 ? -1 : 1;
            break;
        case 7: // Keys: electric piano / organ / glass / lo-fi EP
            w = variation == 0 ? wave({0, 3})
                               : (variation == 1 ? 3 : (variation == 2 ? 0 : (i == 3 ? 5 : 3)));
            if (i == 3)
                oct = variation == 1 ? 1 : -1;
            if (variation == 2 && i == 2)
                semi = 7;
            if (variation == 0 && i == 2 && rng.nextFloat() < 0.5f)
                semi = rng.nextBool() ? 3 : 4;
            level *= 0.85f;
            break;
        case 8: // Organ: harmonic drawbars, restrained detune, no forced saw stack
            w = wave({0, 1, 3, 4, 5});
            level = i == 1   ? frange(0.68f, 0.86f)
                    : i == 2 ? frange(0.34f, 0.62f)
                             : frange(0.18f, 0.42f);
            oct = i == 2 ? (rng.nextBool() ? 0 : 1) : i == 3 ? (rng.nextBool() ? 0 : -1) : 0;
            if (i == 3 && rng.nextFloat() < 0.3f)
                semi = 7;
            fine = i == 1 ? 0.0f : frange(-4.0f, 4.0f);
            break;
        }

        if (randomizeOptions.randomizeVoices()) {
            if (category == 0)
                unison = 2 + rng.nextInt(4);
            else if (category == 1 || category == 3 || category == 6)
                unison = rng.nextInt(6);
            else if (category == 2 || category == 7)
                unison = rng.nextInt(4);
            else if (category == 5)
                unison = rng.nextInt(3);
            else if (category == 8)
                unison = rng.nextInt(4);
            else
                unison = rng.nextInt(3);
            detune = unison == 0 ? 0.0f
                                 : frange(category == 8 ? 0.5f : 2.0f,
                                          category == 0 || category == 1 ? 28.0f : 18.0f);
        }

        // Не закрепляем форму волны за общими variation-ветками: каждый голос
        // получает независимый выбор из характерного для категории набора.
        switch (category) {
        case 0:
            w = wave({0, 1, 2, 3, 4, 5});
            break;
        case 1:
            w = wave({0, 1, 2, 3, 4, 5, 6});
            break;
        case 2:
            w = wave({0, 1, 2, 3, 4, 5});
            break;
        case 3:
            w = wave({0, 1, 2, 3, 4, 5, 6});
            break;
        case 4:
            w = wave({0, 3, 4, 5, 6});
            break;
        case 5:
            w = wave({0, 1, 2, 3, 4, 5});
            break;
        case 6:
            w = wave({0, 1, 2, 3, 4, 5, 6});
            break;
        case 7:
            w = wave({0, 1, 3, 4, 5});
            break;
        case 8:
            w = wave({0, 1, 3, 4, 5});
            break;
        }
        if (w == 6)
            level = juce::jmin(level, 0.10f);
        setChoice(n + "Wave", w);
        setF(n + "Level", level);
        setI(n + "Octave", oct);
        setI(n + "Semitone", semi);
        setF(n + "FineTune", fine);
        setF(n + "Pan", i == 1 ? 0.0f : frange(-0.65f, 0.65f));
        if (randomizeOptions.randomizeVoices()) {
            setChoice(n + "Voices", unison);
            setF(n + "Detune", detune);
        }
    }

    // Каждый профиль задаёт свою огибающую, фильтр и обработку.
    float attack = 0.005f, decay = 0.25f, sustain = 0.6f, release = 0.2f;
    float cutoff = 9000.0f, reso = 0.15f, drive = 0.3f, warmth = 0.4f;
    float bits = 16.0f, rate = 1.0f, vib = 0.0f, glide = 0.0f;
    int arp = 0, portaMode = 0;
    float arpRate = 8.0f;
    switch (category) {
    case 0: // pad
        attack = variation == 2 ? frange(0.6f, 1.5f) : frange(0.12f, 0.8f);
        decay = frange(0.4f, 1.2f);
        sustain = frange(0.65f, 0.95f);
        release = frange(1.0f, 3.2f);
        cutoff = variation == 1 ? frange(6500.0f, 12000.0f) : frange(1300.0f, 5500.0f);
        warmth = frange(0.35f, 0.8f);
        drive = frange(0.08f, 0.32f);
        vib = frange(3.0f, 12.0f);
        break;
    case 1: // synth
        attack = variation == 1 ? frange(0.025f, 0.11f) : frange(0.002f, 0.03f);
        decay = frange(0.12f, 0.75f);
        sustain = frange(0.45f, 0.9f);
        cutoff = variation == 3 ? frange(800.0f, 3500.0f) : frange(2800.0f, 15000.0f);
        reso = frange(0.1f, 0.55f);
        drive = frange(0.15f, 0.75f);
        bits = variation >= 2 ? (float)rng.nextInt(juce::Range<int>(6, 13)) : 16.0f;
        rate = variation == 3 ? (float)rng.nextInt(juce::Range<int>(3, 8)) : 1.0f;
        break;
    case 2: // lead
        attack = frange(0.002f, 0.025f);
        sustain = frange(0.7f, 0.95f);
        release = frange(0.12f, 0.45f);
        cutoff = frange(4500.0f, 14000.0f);
        drive = frange(0.2f, 0.72f);
        vib = variation == 2 ? frange(25.0f, 52.0f) : frange(0.0f, 24.0f);
        glide = variation == 3 ? frange(0.10f, 0.65f) : frange(0.0f, 0.07f);
        portaMode = variation == 3 ? 1 + rng.nextInt(2) : 0;
        break;
    case 3: // pluck
        attack = 0.002f;
        decay = frange(0.04f, 0.6f);
        sustain = frange(0.0f, 0.18f);
        release = frange(0.04f, 0.5f);
        cutoff = variation == 0 ? frange(1300.0f, 4500.0f) : frange(4500.0f, 15000.0f);
        bits = variation == 2 ? frange(8.0f, 11.0f) : 16.0f;
        break;
    case 4: // bell
        attack = 0.002f;
        decay = frange(0.6f, 1.5f);
        sustain = 0.0f;
        release = frange(0.7f, 2.5f);
        cutoff = frange(6000.0f, 16000.0f);
        drive = frange(0.02f, 0.16f);
        warmth = frange(0.05f, 0.3f);
        if (variation == 3) {
            decay = frange(0.08f, 0.28f);
            release = frange(0.2f, 0.65f);
        }
        break;
    case 5: // bass
        attack = frange(0.002f, 0.015f);
        decay = frange(0.1f, 0.4f);
        sustain = frange(0.55f, 0.85f);
        release = frange(0.08f, 0.28f);
        cutoff = frange(350.0f, 2600.0f);
        reso = frange(0.05f, 0.38f);
        drive = variation == 0 ? frange(0.1f, 0.27f) : frange(0.32f, 0.8f);
        bits = variation == 2 ? (float)rng.nextInt(juce::Range<int>(6, 10)) : 16.0f;
        rate = variation == 2 ? frange(2.0f, 5.0f) : 1.0f;
        break;
    case 6: // arp
        attack = 0.002f;
        decay = frange(0.06f, 0.22f);
        sustain = frange(0.08f, 0.3f);
        release = frange(0.06f, 0.2f);
        cutoff = frange(3500.0f, 12000.0f);
        arp = 1 + rng.nextInt(3);
        arpRate = frange(4.0f, 19.0f);
        bits = variation == 3 ? frange(7.0f, 11.0f) : 16.0f;
        break;
    case 7: // keys
        attack = frange(0.004f, 0.03f);
        decay = frange(0.35f, 1.0f);
        sustain = frange(0.25f, 0.55f);
        release = frange(0.4f, 1.2f);
        cutoff = variation == 2 ? frange(6500.0f, 12000.0f) : frange(1800.0f, 8500.0f);
        warmth = frange(0.22f, 0.7f);
        if (variation == 1) {
            sustain = frange(0.65f, 0.9f);
            release = frange(0.1f, 0.4f);
        }
        if (variation == 3) {
            bits = frange(9.0f, 13.0f);
            rate = frange(1.0f, 3.0f);
        }
        break;
    case 8: // organ
        attack = frange(0.004f, 0.045f);
        decay = frange(0.05f, 0.28f);
        sustain = frange(0.72f, 1.0f);
        release = frange(0.08f, 0.65f);
        cutoff = frange(2200.0f, 13500.0f);
        reso = frange(0.0f, 0.24f);
        drive = frange(0.02f, 0.26f);
        warmth = frange(0.18f, 0.62f);
        vib = variation == 3 ? frange(3.0f, 12.0f) : frange(0.0f, 2.0f);
        break;
    }
    setF("amplitudeAttack", attack);
    setF("amplitudeDecay", decay);
    setF("amplitudeSustain", sustain);
    setF("amplitudeRelease", release);
    setF("filterCutoff", cutoff);
    setF("filterResonance", reso);
    setF("tubeDrive", drive);
    setF("tubeWarmth", warmth);
    setF("lofiBitDepth", bits);
    setF("lofiRateReduction", rate);
    setF("vibratoRate", frange(4.0f, 7.0f));
    setF("vibratoDepth", vib);
    if (randomizeOptions.randomizeArp()) {
        setChoice("arpeggiatorMode", arp);
        setF("arpeggiatorRate", arpRate);
    }
    if (randomizeOptions.randomizePorta()) {
        setF("portamentoTime", glide);
        setChoice("portamentoMode", portaMode);
    }
    setF("masterLevel", 0.8f);

    // Кластеры FX: различные маршруты обработки дают характерно разные пресеты
    // даже при одинаковой категории осцилляторов. Все значения перезаписываются.
    if (randomizeOptions.randomizeFx()) {
        const float pick = rng.nextFloat();
        const bool space = category == 0 || category == 4 ? pick < 0.82f
                           : category == 5                ? pick < 0.12f
                                                          : pick < 0.42f;
        const bool haze = category == 0 || category == 7 || category == 8 ? rng.nextFloat() < 0.68f
                          : category == 5                                 ? rng.nextFloat() < 0.12f
                                                                          : rng.nextFloat() < 0.38f;
        const bool taps = category == 3 || category == 6 ? rng.nextFloat() < 0.77f
                          : category == 5                ? rng.nextFloat() < 0.12f
                                                         : rng.nextFloat() < 0.36f;
        const bool filter = category == 5 || category == 1 || category == 8
                                ? rng.nextFloat() < 0.72f
                                : rng.nextFloat() < 0.36f;
        setB("effectsFilterEnabled", filter);
        setChoice("effectsFilterMode", category == 5 ? 0 : rng.nextInt(3));
        setF("effectsFilterFrequency",
             category == 5 ? frange(350.0f, 4000.0f) : frange(600.0f, 12000.0f));
        setF("effectsFilterResonance", frange(0.08f, 0.65f));
        setF("effectsFilterMix", frange(0.38f, 0.88f));
        setB("effectsHazeEnabled", haze);
        setF("effectsHazeRate", frange(0.08f, category == 6 ? 3.2f : 1.8f));
        setF("effectsHazeDepth", frange(0.20f, 0.95f));
        setF("effectsHazeMix", frange(0.28f, 0.82f));
        setB("effectsTapsEnabled", taps);
        setF("effectsTapsTime", category == 6 ? frange(0.09f, 0.35f) : frange(0.15f, 0.85f));
        setF("effectsTapsFeedback", frange(0.19f, 0.71f));
        setF("effectsTapsMix", frange(0.25f, 0.67f));
        setB("effectsSpaceEnabled", space);
        setF("effectsSpaceSize", frange(0.35f, 0.95f));
        setF("effectsSpaceDamping", frange(0.18f, 0.79f));
        setF("effectsSpaceMix", category == 5 ? frange(0.15f, 0.35f) : frange(0.30f, 0.8f));
    }
    if (randomizeOptions.randomizeNoise()) {
        setB("backgroundNoiseEnabled",
             rng.nextFloat() < (category == 0 || category == 8 ? 0.65f : 0.28f));
        setChoice("backgroundNoiseType", rng.nextInt((int)welleyz::backgroundNoise::kNames.size()));
        setF("backgroundNoiseLevel", frange(0.12f, 0.48f));
    }
}

void WelleyzProcessor::rememberCurrentSound() {
    if (mSoundHistory.size() == 10)
        mSoundHistory.erase(mSoundHistory.begin());
    mSoundHistory.push_back({mApvts.copyState(), mUiPreset, selectedSoundType()});
}

void WelleyzProcessor::selectSoundType(int category) {
    mUiSoundType = juce::jlimit(0, 8, category);
    if (auto* parameter = mApvts.getParameter("randomSoundType"))
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float)mUiSoundType));
}

int WelleyzProcessor::selectedSoundType() const {
    return juce::jlimit(0, 8, getParamChoice("randomSoundType"));
}

bool WelleyzProcessor::restorePreviousSound() {
    if (mSoundHistory.empty())
        return false;

    auto previous = std::move(mSoundHistory.back());
    mSoundHistory.pop_back();
    resetPlaybackState();
    mApvts.replaceState(previous.mState);
    mUiPreset = std::move(previous.mPreset);
    mUiSoundType = selectedSoundType();
    return true;
}

void WelleyzProcessor::resetToDefault() {
    rememberCurrentSound();
    resetPlaybackState();
    for (auto* p : getParameters()) {
        if (auto* fp = dynamic_cast<juce::RangedAudioParameter*>(p))
            fp->setValueNotifyingHost(fp->getDefaultValue());
    }
}

juce::File WelleyzProcessor::presetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("welleyz presets");
}

bool WelleyzProcessor::savePresetToFile(const juce::File& file) {
    auto parameters = std::make_unique<juce::DynamicObject>();
    auto state = mApvts.copyState();
    for (int i = 0; i < state.getNumChildren(); ++i) {
        const auto child = state.getChild(i);
        if (child.hasProperty("id") && child.hasProperty("value"))
            parameters->setProperty(child.getProperty("id").toString(), child.getProperty("value"));
    }

    auto preset = std::make_unique<juce::DynamicObject>();
    preset->setProperty("format", "welleyz");
    preset->setProperty("version", 1);
    preset->setProperty("name", file.getFileNameWithoutExtension());
    preset->setProperty("parameters", juce::var(parameters.release()));
    return file.replaceWithText(juce::JSON::toString(juce::var(preset.release()), true));
}

bool WelleyzProcessor::loadPresetFromFile(const juce::File& file) {
    juce::MemoryBlock data;
    return file.loadFileAsData(data) && loadPresetFromData(data.getData(), data.getSize());
}

bool WelleyzProcessor::loadPresetFromData(const void* data, size_t size) {
    if (data == nullptr || size == 0 || size > (size_t)std::numeric_limits<int>::max())
        return false;
    const auto document =
        juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(data), (int)size));
    auto* preset = document.getDynamicObject();
    if (preset == nullptr || preset->getProperty("format").toString() != "welleyz" ||
        (int)preset->getProperty("version") != 1 || !preset->hasProperty("name"))
        return false;
    auto* parameters = preset->getProperty("parameters").getDynamicObject();
    if (parameters == nullptr || parameters->getProperties().isEmpty())
        return false;

    std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
    for (const auto& entry : parameters->getProperties()) {
        auto* parameter = mApvts.getParameter(entry.name.toString());
        if (parameter == nullptr || !(entry.value.isDouble() || entry.value.isInt() ||
                                      entry.value.isInt64() || entry.value.isBool()))
            return false;
        const auto value = (float)(double)entry.value;
        const auto range = parameter->getNormalisableRange().getRange();
        if (!std::isfinite(value) || value < range.getStart() || value > range.getEnd())
            return false;
        values.emplace_back(parameter, value);
    }

    rememberCurrentSound();
    resetPlaybackState();
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
    for (const auto& [parameter, value] : values)
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    mUiSoundType = selectedSoundType();
    return true;
}

void WelleyzProcessor::applyStateTree(const juce::ValueTree& tree) {
    mSoundHistory.clear();
    resetPlaybackState();
    mUiPreset.clear();
    mUiSoundType = 1;
    mUiZoomCustomized = false;
    mUiPage = 0;
    if (auto ui = tree.getChildWithName("ui"); ui.isValid()) {
        mUiZoom = juce::jlimit(0.5f, 2.0f, (float)ui.getProperty("zoom", 1.0f));
        mUiZoomCustomized = (bool)ui.getProperty("zoomCustomized", false);
        mUiPage = juce::jlimit(0, 2, (int)ui.getProperty("page", 0));
        mUiPreset = ui.getProperty("preset", juce::String()).toString();
    }
    mApvts.replaceState(tree);
    mUiSoundType = selectedSoundType();
}

void WelleyzProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = mApvts.copyState();
    auto ui = state.getOrCreateChildWithName("ui", nullptr);
    ui.setProperty("zoom", mUiZoom, nullptr);
    ui.setProperty("zoomCustomized", mUiZoomCustomized, nullptr);
    ui.setProperty("page", mUiPage, nullptr);
    ui.setProperty("preset", mUiPreset, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void WelleyzProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName("Welleyz"))
        applyStateTree(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new WelleyzProcessor();
}
