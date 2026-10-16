#pragma once

#include <JuceHeader.h>

class WelleyzUpdateChecker {
  public:
    static bool isConfigured();
    static void check(juce::Component::SafePointer<juce::Component> owner, bool userInitiated);
};
