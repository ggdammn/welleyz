#pragma once
#include <cmath>
#include <cstdint>
#include <random>

// ============================================================
// Oscillator and envelope DSP.
// 3 чиптюн-осциллятора (NES/GB-стилистика) + лампа + биткраш
// ============================================================

namespace welleyz::dsp {
constexpr double kPi = 3.14159265358979323846;

inline double rhMax(double a, double b) {
    return a > b ? a : b;
}

inline float midiToFreq(int midiNote, float pitchBendSemis = 0.0f) {
    return 440.0f * std::pow(2.0f, (float(midiNote) - 69.0f + pitchBendSemis) / 12.0f);
}

// Волны в духе 3xOSC + чиптюн-набор
enum Wave : int {
    sine = 0,
    square50, // классический квадрат 50%
    saw,      // пила
    triangle, // NES-треугольник
    pulse125, // GameBoy pulse 12.5%
    pulse25,  // GameBoy pulse 25%
    noiseLfsr // шум на LFSR как в NES (псевдо)
};

inline const char* waveName(int w) {
    switch (w) {
    case sine:
        return "Sine";
    case square50:
        return "Square";
    case saw:
        return "Saw";
    case triangle:
        return "Triangle";
    case pulse125:
        return "Pulse 12%";
    case pulse25:
        return "Pulse 25%";
    case noiseLfsr:
        return "Noise";
    default:
        return "?";
    }
}

// --- NES-подобный LFSR шум (15-бит) ---
struct LfsrNoise {
    uint16_t mReg = 0x4000;
    uint32_t mClockCount = 0;

    void reset() {
        mReg = 0x4000;
        mClockCount = 0;
    }

    // tick вызывается на "частоте шума" — для простоты каждый сэмпл
    float tick() {
        // Galois-ish: feedback bit0 ^ bit1 (короткий режим NES даёт металлический тон)
        uint16_t fb = ((mReg & 1) ^ ((mReg >> 1) & 1)) & 1;
        mReg = (mReg >> 1) | (fb << 14);
        return (mReg & 1) ? 0.9f : -0.9f;
    }
};

struct OscState {
    double mPhase = 0.0; // 0..1
    LfsrNoise mLfsr;
    float mNoiseHold = 0.0f;
    int mNoiseDecim = 0;

    void reset() {
        mPhase = 0.0;
        mLfsr.reset();
        mNoiseHold = 0.0f;
        mNoiseDecim = 0;
    }
};

inline float renderOsc(Wave w, OscState& st, double freqHz, double sampleRate,
                       float pulseWidthOverride = -1.0f) {
    if (freqHz <= 0.0)
        return 0.0f;
    double inc = freqHz / sampleRate;
    st.mPhase += inc;
    if (st.mPhase >= 1.0)
        st.mPhase -= 1.0;
    double p = st.mPhase;

    switch (w) {
    case sine:
        return (float)std::sin(2.0 * kPi * p);
    case square50:
        return (p < 0.5) ? 0.8f : -0.8f;
    case saw:
        // слегка смягчённая пила (меньше цифры, больше ретро)
        return (float)(2.0 * p - 1.0) * 0.9f;
    case triangle:
        // NES triangle: ступенчатый треугольник 4-бит
        {
            float tri = (float)(p < 0.5 ? (4.0 * p - 1.0) : (3.0 - 4.0 * p));
            // квантование до 16 ступеней — фирменный привкус
            tri = std::round(tri * 7.0f) / 7.0f;
            return tri * 0.9f;
        }
    case pulse125: {
        float pw = (pulseWidthOverride > 0.0f) ? pulseWidthOverride : 0.125f;
        return (p < pw) ? 0.8f : -0.8f;
    }
    case pulse25: {
        float pw = (pulseWidthOverride > 0.0f) ? pulseWidthOverride : 0.25f;
        return (p < pw) ? 0.8f : -0.8f;
    }
    case noiseLfsr: {
        // Децимация шума по частоте ноты для pitched-noise эффекта
        // freqHz задаёт скорость LFSR — как в трекерах
        double ticksPerSample = freqHz * 8.0 / sampleRate * 32.0;
        // минимум 1 тик на несколько сэмплов чтобы не было белого месива
        st.mNoiseDecim++;
        int holdLen = (int)rhMax(1.0, sampleRate / rhMax(1.0, freqHz * 16.0));
        // NOTE: holdLen пересчитывается ниже в голосе; здесь просто каждый сэмпл тикаем с
        // прореживанием
        (void)ticksPerSample;
        (void)holdLen;
        return st.mLfsr.tick();
    }
    default:
        return 0.0f;
    }
}

// helpers чтобы не тянуть juce в DSP.h
// --- Лампа (tube): tanh-драйв + 2-я гармоника (warmth) ---
struct TubeStage {
    float mDcBlockerX1 = 0.0f, mDcBlockerY1 = 0.0f;

    float process(float x, float drive01, float warmth01) {
        // drive 0..1 -> k 1..9
        float k = 1.0f + drive01 * 8.0f;
        float shaped = std::tanh(k * x * 0.8f) / std::tanh(k * 0.8f + 1e-5f);
        // warmth: подмес чётной гармоники (ламповая асимметрия)
        float asym = x * x * warmth01 * 0.35f * ((x >= 0.0f) ? 1.0f : -0.6f);
        float y = shaped * (1.0f - warmth01 * 0.25f) + asym + x * warmth01 * 0.15f;
        // простейший DC-blocker
        float R = 0.995f;
        float out = y - mDcBlockerX1 + R * mDcBlockerY1;
        mDcBlockerX1 = y;
        mDcBlockerY1 = out;
        return out;
    }

    void reset() { mDcBlockerX1 = mDcBlockerY1 = 0.0f; }
};

// --- Биткраш: квантование + даунсэмплинг (sample-hold) ---
struct BitCrusher {
    float mHoldValueL = 0.0f, mHoldValueR = 0.0f;
    int mCounter = 0;

    static float quantize(float x, float bits) {
        if (bits >= 15.5f)
            return x;
        float levels = std::pow(2.0f, bits);
        float q = std::round(x * levels * 0.5f) / (levels * 0.5f);
        return q;
    }

    void reset() {
        mHoldValueL = mHoldValueR = 0.0f;
        mCounter = 0;
    }
};

// --- Простая ADSR (линейно-экспоненциальная) ---
struct Adsr {
    enum Stage { idle, attack, decay, sustainStage, release };
    Stage mStage = idle;
    float mLevel = 0.0f;
    float mAttackSec = 0.005f, mDecaySec = 0.1f, mSustain = 0.8f, mReleaseSec = 0.15f;
    double mSampleRate = 44100.0;
    float mReleaseStart = 0.0f;

    void setSampleRate(double sr) { mSampleRate = sr; }
    void setParams(float a, float d, float s, float r) {
        mAttackSec = rhMax(0.001, (double)a);
        mDecaySec = rhMax(0.005, (double)d);
        mSustain = s;
        mReleaseSec = rhMax(0.005, (double)r);
    }
    void noteOn() { mStage = attack; }
    void noteOff() {
        if (mStage != idle) {
            mStage = release;
            mReleaseStart = mLevel;
        }
    }
    bool isActive() const { return mStage != idle; }

    float tick() {
        float aInc = (float)(1.0 / (mAttackSec * mSampleRate));
        float dInc = (float)(1.0 / (mDecaySec * mSampleRate));
        float rInc = (mReleaseStart > 0.0f && mReleaseSec > 0.0f)
                         ? (float)(mReleaseStart / (mReleaseSec * mSampleRate))
                         : 0.01f;

        switch (mStage) {
        case attack:
            mLevel += aInc;
            if (mLevel >= 1.0f) {
                mLevel = 1.0f;
                mStage = decay;
            }
            break;
        case decay:
            mLevel -= dInc * (mLevel > mSustain ? 1.0f : 0.0f);
            // экспоненциальное приближение к sustain
            mLevel +=
                (mSustain - mLevel) * (float)(1.0 - std::exp(-1.0 / (mDecaySec * mSampleRate)));
            if (std::abs(mLevel - mSustain) < 0.001f) {
                mLevel = mSustain;
                mStage = sustainStage;
            }
            break;
        case sustainStage:
            mLevel = mSustain;
            break;
        case release:
            mLevel -=
                rInc + mLevel * (float)(1.0 - std::exp(-2.0 / (mReleaseSec * mSampleRate))) * 0.5f;
            if (mLevel <= 0.0008f) {
                mLevel = 0.0f;
                mStage = idle;
            }
            break;
        case idle:
            mLevel = 0.0f;
            break;
        }
        return mLevel;
    }

    void reset() {
        mStage = idle;
        mLevel = 0.0f;
        mReleaseStart = 0.0f;
    }
};

} // namespace welleyz::dsp
