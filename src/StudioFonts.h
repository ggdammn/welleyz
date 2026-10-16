#pragma once
#include <JuceHeader.h>
#include "WelleyzAssets.h"

namespace welleyz::fonts {
inline juce::Font fromResource(bool isDisplay, float height) {
    // Typeface data is owned by BinaryData for the lifetime of the plugin.
    // Each font is decoded once, never from the audio callback or paint().
    static const auto displayFace = [] {
        int bytes = 0;
        const auto* data = WelleyzAssets::getNamedResource("Synevariable_ttf", bytes);
        return data != nullptr ? juce::Typeface::createSystemTypefaceFor(data, (size_t)bytes)
                               : juce::Typeface::Ptr{};
    }();
    static const auto bodyFace = [] {
        int bytes = 0;
        const auto* data = WelleyzAssets::getNamedResource("Outfitvariable_ttf", bytes);
        return data != nullptr ? juce::Typeface::createSystemTypefaceFor(data, (size_t)bytes)
                               : juce::Typeface::Ptr{};
    }();
    const auto& face = isDisplay ? displayFace : bodyFace;
    return face != nullptr
               ? (isDisplay ? juce::Font(face).boldened() : juce::Font(face)).withHeight(height)
               : juce::Font(juce::FontOptions(height));
}

inline juce::Font display(float height) {
    return fromResource(true, height);
}
inline juce::Font mono(float height) {
    return fromResource(false, height);
}

inline juce::Font brand(float height) {
    static const auto face = [] {
        int bytes = 0;
        const auto* data = WelleyzAssets::getNamedResource("SarinaRegular_ttf", bytes);
        return data != nullptr ? juce::Typeface::createSystemTypefaceFor(data, (size_t)bytes)
                               : juce::Typeface::Ptr{};
    }();
    return face != nullptr ? juce::Font(face).withHeight(height)
                           : juce::Font(juce::FontOptions(height));
}
} // namespace welleyz::fonts
