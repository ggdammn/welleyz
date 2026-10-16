#pragma once
#include "ThemeManager.h"

// Studio surfaces and semantic colours shared by the native JUCE controls.
struct WaveTheme {
    juce::Colour mChassisTop, mChassisBottom, mPanelTop, mPanelBottom, mPanelEdge, mHeaderStrip;
    juce::Colour mLabelMain, mLabelShadow, mTitleMain, mTitleShadow, mValueMain, mValueShadow;
    juce::Colour mAccent, mAccentDim;
    juce::Colour mKnobRing, mKnobCapTop, mKnobCapBottom, mKnobPointer;
    juce::Colour mComboBg, mComboText, mComboEdge;
    juce::Colour mBtnTop, mBtnBottom, mBtnEdge, mBtnText;
    juce::Colour mBtnAccentTop, mBtnAccentBottom, mBtnAccentText;
    juce::Colour mKeyWhite, mKeyBlack, mKeySep, mLogoMain, mLogoShadow;
};

inline WaveTheme makeWaveTheme(const ThemeManager& manager) {
    using namespace welleyz::design::colours;
    return {kBackground,
            kSecondary,
            kSurface,
            kSecondary,
            kBorder.withAlpha(0.75f),
            kSecondary,
            kMuted,
            kBackground,
            kText,
            kBackground,
            kSecondaryText,
            kBackground,
            manager.primary(),
            manager.primary().withAlpha(0.30f),
            kBorder,
            kActive,
            kSurface,
            kText,
            kSecondary,
            kText,
            kBorder,
            kSurface,
            kSecondary,
            kBorder,
            kText,
            manager.primaryContainer(),
            manager.pressed(),
            manager.onPrimaryContainer(),
            kKeyFace,
            kBackground,
            kKeyShadow,
            kText,
            kBackground};
}

namespace welleyz::ui {
// Offscreen frost and noise masks are created once on the UI thread. The resulting
// images are reused by every panel and never touch the audio callback.
inline const juce::Image& noise() {
    static const auto image = [] {
        juce::Image result(juce::Image::ARGB, 64, 64, true);
        juce::uint32 state = 0x73bd1ea5;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                const auto alpha = (juce::uint8)(state % 16);
                result.setPixelAt(x, y, juce::Colours::white.withAlpha(alpha));
            }
        return result;
    }();
    return image;
}

inline const juce::Image& blurredHalo() {
    static const auto image = [] {
        juce::Image source(juce::Image::ARGB, 96, 96, true);
        juce::Graphics glow(source);
        glow.setColour(juce::Colours::white);
        glow.fillEllipse(30.0f, 30.0f, 36.0f, 36.0f);
        juce::Image blurred(juce::Image::ARGB, 96, 96, true);
        juce::ImageConvolutionKernel kernel(37);
        kernel.createGaussianBlur(16.0f);
        kernel.applyToImage(blurred, source, blurred.getBounds());
        return blurred;
    }();
    return image;
}

inline void drawHalo(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour) {
    g.setColour(colour);
    g.drawImage(blurredHalo(), area, juce::RectanglePlacement::stretchToFit, true);
}

inline void drawAccentGlow(juce::Graphics& g, juce::Path path, juce::Colour accent, float width,
                           float scale = 1.0f, float coreAlpha = 1.0f) {
    g.setColour(accent.withAlpha(0.19f * coreAlpha));
    g.strokePath(path, juce::PathStrokeType(width * 3.0f * scale, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(accent.withAlpha(0.38f * coreAlpha));
    g.strokePath(path, juce::PathStrokeType(width * 1.7f * scale, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(accent.withAlpha(coreAlpha));
    g.strokePath(path, juce::PathStrokeType(width * scale, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

inline void fillPanel(juce::Graphics& g, juce::Rectangle<float> r, const WaveTheme& t,
                      float radius) {
    r = r.reduced(2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.36f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), radius);
    drawHalo(g,
             {r.getX() - r.getWidth() * 0.18f, r.getY() - r.getHeight() * 0.9f,
              r.getWidth() * 1.35f, r.getHeight() * 1.5f},
             t.mAccent.withAlpha(0.19f));
    g.setGradientFill(juce::ColourGradient(t.mPanelTop.brighter(0.08f).withAlpha(0.94f), r.getX(),
                                           r.getY(), t.mPanelBottom.darker(0.14f).withAlpha(0.90f),
                                           r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    g.setTiledImageFill(noise(), (int)r.getX(), (int)r.getY(), 0.55f);
    g.fillRoundedRectangle(r, radius);
    g.setColour(t.mPanelEdge.brighter(0.07f).withAlpha(0.78f));
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.1f);
    auto inner = r.reduced(2.5f);
    g.setColour(juce::Colours::white.withAlpha(0.055f));
    g.drawRoundedRectangle(inner, juce::jmax(1.0f, radius - 2.0f), 0.8f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.28f), r.getX(),
                                           r.getY(), t.mAccent.withAlpha(0.03f), r.getRight(),
                                           r.getY(), false));
    g.drawLine(r.getX() + radius, r.getY() + 1.3f, r.getRight() - radius, r.getY() + 1.3f, 1.1f);
}

inline void drawEngraved(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                         const juce::Font& font, juce::Colour main, juce::Colour,
                         juce::Justification align = juce::Justification::centred) {
    g.setFont(font);
    g.setColour(main);
    g.drawText(text, r, align, false);
}

inline void drawLed(juce::Graphics& g, juce::Point<float> c, float radius, const WaveTheme& t,
                    bool on = true) {
    g.setColour(on ? t.mAccent : t.mAccentDim);
    g.fillEllipse(c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
}
} // namespace welleyz::ui
