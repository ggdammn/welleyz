#pragma once
#include <array>
#include <cmath>
#include <cstdint>

// Continuous, seedable texture generator. No allocations or system RNG on the audio thread.
namespace welleyz::backgroundNoise {
inline constexpr std::array<const char*, 9> kNames{
    "Vinyl", "Cassette", "Studio", "Wire", "Tape Hiss", "Dust", "Radio", "Rain", "Crackle"};

struct Generator {
    uint32_t mSeed = 0x4a3b2c1du;
    float mLow = 0.0f, mSlow = 0.0f, mCrackle = 0.0f;
    float mLowAlpha = 0.20f, mSlowAlpha = 0.02f;
    double mPhase = 0.0, mSampleRate = 44100.0;

    void reset(double rate, uint32_t initialSeed) {
        mSampleRate = rate > 0.0 ? rate : 44100.0;
        mSeed = initialSeed;
        mLow = mSlow = mCrackle = 0.0f;
        mPhase = 0.0;
        mLowAlpha = (float)(1.0 - std::exp(-2.0 * 3.141592653589793 * 1800.0 / mSampleRate));
        mSlowAlpha = (float)(1.0 - std::exp(-2.0 * 3.141592653589793 * 170.0 / mSampleRate));
    }

    float white() {
        mSeed ^= mSeed << 13;
        mSeed ^= mSeed >> 17;
        mSeed ^= mSeed << 5;
        return (float)(mSeed & 0x00ffffffu) * (2.0f / 16777215.0f) - 1.0f;
    }

    float next(int type) {
        const float raw = white();
        mLow += mLowAlpha * (raw - mLow);
        mSlow += mSlowAlpha * (raw - mSlow);
        const float hiss = raw - mLow;
        const float rustle = mLow - mSlow;
        const float hit = white() > 0.9995f ? (white() > 0.0f ? 1.0f : -1.0f) : 0.0f;
        mCrackle = mCrackle * 0.78f + hit;
        mPhase += 1.0 / mSampleRate;
        if (mPhase >= 1.0)
            mPhase -= 1.0;
        const float hum = (float)std::sin(2.0 * 3.141592653589793 * 50.0 * mPhase);
        switch (type) {
        case 0:
            return 0.30f * rustle + 0.12f * hiss + 0.70f * mCrackle; // Vinyl
        case 1:
            return 0.48f * hiss + 0.18f * rustle + 0.06f * hum; // Cassette
        case 2:
            return 0.40f * raw + 0.12f * hum; // Studio
        case 3:
            return 0.23f * hiss + 0.17f * hum + 0.20f * mCrackle; // Wire
        case 4:
            return 0.68f * hiss + 0.08f * hum; // Tape Hiss
        case 5:
            return 0.10f * rustle + 0.85f * mCrackle; // Dust
        case 6:
            return 0.42f * rustle + 0.30f * hiss + 0.13f * mCrackle; // Radio
        case 7:
            return 0.45f * mLow + 0.18f * raw + 0.35f * mCrackle; // Rain
        case 8:
            return 0.10f * hiss + 0.95f * mCrackle; // Crackle
        default:
            return 0.0f;
        }
    }
};
} // namespace welleyz::backgroundNoise
