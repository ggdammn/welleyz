#pragma once
#include <JuceHeader.h>

namespace welleyz::design {
namespace colours {
const inline juce::Colour kBackground{0xff0c0f14};
const inline juce::Colour kSecondary{0xff15191f};
const inline juce::Colour kSurface{0xff222830};
const inline juce::Colour kHover{0xff303840};
const inline juce::Colour kActive{0xff3b454e};
const inline juce::Colour kBorder{0xff3c454d};
const inline juce::Colour kBorderStrong{0xff66757b};
const inline juce::Colour kText{0xfff4f6f5};
const inline juce::Colour kSecondaryText{0xffd2d9da};
const inline juce::Colour kMuted{0xffa3afb3};
const inline juce::Colour kDisabled{0xff778489};
const inline juce::Colour kDefaultAccent{0xff22dccb};
const inline juce::Colour kModulation{0xffc5eb42};
const inline juce::Colour kViolet{0xffa584ff};
const inline juce::Colour kOnLed{0xff5be6a6};
const inline juce::Colour kOffLed{0xffff697c};
const inline juce::Colour kKeyFace{0xfff4f2ed};
const inline juce::Colour kKeyShadow{0xff777a78};
} // namespace colours
namespace spacing {
constexpr int kXs = 4, kSm = 8, kMd = 12, kLg = 16, kXl = 24;
}
namespace radius {
constexpr float kSmall = 8.0f, kMedium = 11.0f, kLarge = 14.0f, kExtraLarge = 18.0f;
constexpr float kFull = 999.0f;
} // namespace radius
} // namespace welleyz::design
