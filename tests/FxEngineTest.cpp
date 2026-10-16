#include "FxEngine.h"
#include <cmath>
#include <cstdio>

int main() {
    constexpr double sr = 44100.0;
    constexpr int count = 44100;
    for (int effect = -1; effect < 4; ++effect) {
        WelleyzFx fx;
        fx.prepare(sr, 256);
        WelleyzFx::Settings settings;
        settings.mFilterOn = effect == 0;
        settings.mFilterMode = 2;
        settings.mFilterFreq = 500.0f;
        settings.mFilterMix = 0.9f;
        settings.mHazeOn = effect == 1;
        settings.mHazeMix = 0.8f;
        settings.mTapsOn = effect == 2;
        settings.mTapsTime = 0.18f;
        settings.mTapsMix = 0.8f;
        settings.mSpaceOn = effect == 3;
        settings.mSpaceMix = 0.8f;
        fx.configure(settings);
        double change = 0.0, rightEnergy = 0.0;
        for (int i = 0; i < count; ++i) {
            const float dry = i == 0 ? 0.4f : 0.0f;
            float l = dry, r = 0.0f;
            fx.process(l, r, settings);
            if (!std::isfinite(l) || !std::isfinite(r) || std::abs(l) > 4.0f ||
                std::abs(r) > 4.0f) {
                std::fprintf(stderr, "Non-finite or unstable output in FX %d\n", effect);
                return 1;
            }
            change += std::abs(l - dry) + std::abs(r);
            rightEnergy += std::abs(r);
        }
        if ((effect == -1 && change > 0.00001) || (effect >= 0 && change < 0.01) ||
            (effect >= 2 && rightEnergy < 0.001)) {
            std::fprintf(stderr, "FX %d failed: difference=%f, stereo=%f\n", effect, change,
                         rightEnergy);
            return 1;
        }
    }
    {
        WelleyzFx fx;
        fx.prepare(sr, 256);
        WelleyzFx::Settings settings;
        settings.mSpaceOn = true;
        settings.mSpaceMix = 1.0f;
        fx.configure(settings);
        for (int i = 0; i < 4096; ++i) {
            float l = i == 0 ? 0.5f : 0.0f, r = l;
            fx.process(l, r, settings);
        }
        fx.reset();
        fx.configure(settings);
        double residual = 0.0;
        for (int i = 0; i < 512; ++i) {
            float l = 0.0f, r = 0.0f;
            fx.process(l, r, settings);
            residual += std::abs(l) + std::abs(r);
        }
        if (residual > 0.0001) {
            std::fprintf(stderr, "FX state was not cleared: residual=%f\n", residual);
            return 1;
        }
    }
    std::printf("FX dry bypass and all four wet stages verified\n");
    return 0;
}
