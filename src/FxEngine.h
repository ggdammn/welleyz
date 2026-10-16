#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>

// Пост-синтезаторная стереоцепь. Все буферы выделяются в prepare(), в аудиопотоке нет аллокаций.
class WelleyzFx {
  public:
    struct Settings {
        bool mFilterOn = false, mHazeOn = false, mTapsOn = false, mSpaceOn = false;
        int mFilterMode = 0;
        float mFilterFreq = 6000.0f, mFilterReso = 0.2f, mFilterMix = 0.5f;
        float mHazeRate = 0.45f, mHazeDepth = 0.5f, mHazeMix = 0.35f;
        float mTapsTime = 0.32f, mTapsFeedback = 0.38f, mTapsMix = 0.3f;
        float mSpaceSize = 0.55f, mSpaceDamp = 0.45f, mSpaceMix = 0.35f;
    };

    void prepare(double sampleRate, int blockSize) {
        mSr = juce::jmax(8000.0, sampleRate);
        juce::dsp::ProcessSpec spec{mSr, (juce::uint32)juce::jmax(1, blockSize), 2};
        mFilter.prepare(spec);
        mFilter.reset();
        for (auto& line : mChorus)
            line.assign((size_t)(mSr * 0.06) + 4, 0.0f);
        for (auto& line : mDelay)
            line.assign((size_t)(mSr * 1.7) + 4, 0.0f);
        constexpr double lengths[4]{0.0311, 0.0377, 0.0419, 0.0533};
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4; ++i) {
                auto& c = mComb[(size_t)ch][(size_t)i];
                c.mSamples.assign((size_t)(mSr * (lengths[i] + ch * 0.0017)), 0.0f);
                c.mIndex = 0;
                c.mDamped = 0.0f;
            }
        mChorusIndex = mDelayIndex = 0;
        mPhase = 0.0;
        for (auto& mix : mSmoothMix) {
            mix.reset(mSr, 0.035);
            mix.setCurrentAndTargetValue(0.0f);
        }
    }

    void configure(const Settings& s) {
        mFilter.setType(s.mFilterMode == 1   ? juce::dsp::StateVariableTPTFilterType::bandpass
                        : s.mFilterMode == 2 ? juce::dsp::StateVariableTPTFilterType::highpass
                                             : juce::dsp::StateVariableTPTFilterType::lowpass);
        mFilter.setCutoffFrequency(juce::jlimit(60.0f, 18000.0f, s.mFilterFreq));
        mFilter.setResonance(juce::jlimit(0.5f, 5.0f, 0.5f + s.mFilterReso * 4.5f));
        for (int i = 0; i < 4; ++i) {
            const bool on = i == 0   ? s.mFilterOn
                            : i == 1 ? s.mHazeOn
                            : i == 2 ? s.mTapsOn
                                     : s.mSpaceOn;
            const float wet = i == 0   ? s.mFilterMix
                              : i == 1 ? s.mHazeMix
                              : i == 2 ? s.mTapsMix
                                       : s.mSpaceMix;
            mSmoothMix[(size_t)i].setTargetValue(on ? juce::jlimit(0.0f, 1.0f, wet) : 0.0f);
        }
    }

    void reset() {
        mFilter.reset();
        for (auto& line : mChorus)
            std::fill(line.begin(), line.end(), 0.0f);
        for (auto& line : mDelay)
            std::fill(line.begin(), line.end(), 0.0f);
        for (auto& channel : mComb)
            for (auto& c : channel) {
                std::fill(c.mSamples.begin(), c.mSamples.end(), 0.0f);
                c.mIndex = 0;
                c.mDamped = 0.0f;
            }
        mChorusIndex = mDelayIndex = 0;
        mPhase = 0.0;
        for (auto& mix : mSmoothMix)
            mix.setCurrentAndTargetValue(0.0f);
    }

    void process(float& l, float& r, const Settings& s) {
        const float f = mSmoothMix[0].getNextValue();
        const float h = mSmoothMix[1].getNextValue();
        const float d = mSmoothMix[2].getNextValue();
        const float v = mSmoothMix[3].getNextValue();
        if (f + h + d + v < 0.00001f)
            return;
        // FILTER: отдельная стереопара, частота/резонанс/тип задаются на блок.
        const float fl = mFilter.processSample(0, l);
        const float fr = mFilter.processSample(1, r);
        l += (fl - l) * f;
        r += (fr - r) * f;

        // HAZE: два расстроенных коротких delay с противоположными фазами модуляции.
        const float dryL = l, dryR = r;
        const float modulation = (float)std::sin(mPhase * juce::MathConstants<double>::twoPi);
        const float modulationR =
            (float)std::sin(mPhase * juce::MathConstants<double>::twoPi + 2.2);
        const float depth = juce::jlimit(0.0f, 1.0f, s.mHazeDepth);
        const float wetL =
            tap(mChorus[0], mChorusIndex, (float)mSr * (0.016f + 0.009f * depth * modulation));
        const float wetR =
            tap(mChorus[1], mChorusIndex, (float)mSr * (0.019f + 0.009f * depth * modulationR));
        mChorus[0][mChorusIndex] = dryL;
        mChorus[1][mChorusIndex] = dryR;
        mChorusIndex = (mChorusIndex + 1) % mChorus[0].size();
        mPhase += juce::jlimit(0.05f, 4.0f, s.mHazeRate) / mSr;
        if (mPhase >= 1.0)
            mPhase -= 1.0;
        l = dryL * (1.0f - h * 0.6f) + wetL * h * 0.6f;
        r = dryR * (1.0f - h * 0.6f) + wetR * h * 0.6f;

        // TAPS: ping-pong delay; буферы обновляются и при bypass, чтобы хвост не обрывался.
        const float seconds = juce::jlimit(0.07f, 1.2f, s.mTapsTime);
        const auto samples = (size_t)(seconds * (float)mSr);
        const size_t read = (mDelayIndex + mDelay[0].size() - samples) % mDelay[0].size();
        const float dl = mDelay[0][read], dr = mDelay[1][read];
        const float feedback = juce::jlimit(0.0f, 0.82f, s.mTapsFeedback);
        mDelay[0][mDelayIndex] = std::tanh(l + dr * feedback);
        mDelay[1][mDelayIndex] = std::tanh(r + dl * feedback);
        mDelayIndex = (mDelayIndex + 1) % mDelay[0].size();
        l += dl * d * 0.75f;
        r += dr * d * 0.75f;

        // SPACE: четыре параллельных стерео-comb резонатора с damping.
        const float in[2]{l * 0.86f + r * 0.14f, r * 0.86f + l * 0.14f};
        float out[2]{};
        const float decay = 0.53f + juce::jlimit(0.0f, 1.0f, s.mSpaceSize) * 0.37f;
        const float damp = 0.1f + juce::jlimit(0.0f, 1.0f, s.mSpaceDamp) * 0.82f;
        for (int ch = 0; ch < 2; ++ch)
            for (auto& c : mComb[(size_t)ch]) {
                const float sample = c.mSamples[c.mIndex];
                c.mDamped += (sample - c.mDamped) * (1.0f - damp);
                c.mSamples[c.mIndex] = std::tanh(in[ch] * 0.48f + c.mDamped * decay);
                out[ch] += sample * 0.25f;
                c.mIndex = (c.mIndex + 1) % c.mSamples.size();
            }
        l = l * (1.0f - 0.35f * v) + out[0] * v;
        r = r * (1.0f - 0.35f * v) + out[1] * v;
    }

  private:
    static float tap(const std::vector<float>& buffer, size_t write, float distance) {
        const float size = (float)buffer.size();
        float position = (float)write - distance;
        if (position < 0.0f)
            position += size;
        const auto a = (size_t)position;
        const auto b = (a + 1) % buffer.size();
        const float fraction = position - (float)a;
        return buffer[a] + (buffer[b] - buffer[a]) * fraction;
    }

    struct Comb {
        std::vector<float> mSamples;
        size_t mIndex = 0;
        float mDamped = 0.0f;
    };
    std::array<std::vector<float>, 2> mChorus, mDelay;
    std::array<std::array<Comb, 4>, 2> mComb;
    std::array<juce::SmoothedValue<float>, 4> mSmoothMix;
    juce::dsp::StateVariableTPTFilter<float> mFilter;
    size_t mChorusIndex = 0, mDelayIndex = 0;
    double mSr = 44100.0, mPhase = 0.0;
};
