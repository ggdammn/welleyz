#pragma once
#include "DesignTokens.h"

// Shared user preferences are independent from host project state and sound presets.
class ThemeManager : public juce::ChangeBroadcaster {
  public:
    static ThemeManager& instance() {
        static ThemeManager manager;
        return manager;
    }

    juce::Colour accent() const { return mAccentColour; }
    juce::Colour primary() const { return mAccentColour; }
    juce::Colour onPrimary() const { return juce::Colour(0xff251b24); }
    juce::Colour primaryContainer() const {
        return welleyz::design::colours::kSurface.interpolatedWith(mAccentColour, 0.17f);
    }
    juce::Colour onPrimaryContainer() const { return welleyz::design::colours::kText; }
    juce::Colour secondary() const {
        return juce::Colour::fromHSV(std::fmod(mAccentColour.getHue() + 0.08f, 1.0f), 0.24f, 0.82f,
                                     1.0f);
    }
    juce::Colour secondaryContainer() const {
        return juce::Colour::fromHSV(std::fmod(mAccentColour.getHue() + 0.08f, 1.0f), 0.18f, 0.30f,
                                     1.0f);
    }
    juce::Colour tertiary() const {
        return juce::Colour::fromHSV(std::fmod(mAccentColour.getHue() + 0.17f, 1.0f), 0.28f, 0.84f,
                                     1.0f);
    }
    juce::Colour hover() const { return primary().brighter(0.10f); }
    juce::Colour pressed() const { return primaryContainer().darker(0.10f); }
    juce::Colour muted() const { return primary().withAlpha(0.30f); }
    juce::Colour background() const { return primaryContainer(); }
    bool hasSavedZoom() const { return mSettings->containsKey("uiZoom"); }
    float savedZoom() const {
        return juce::jlimit(0.5f, 2.0f, (float)mSettings->getDoubleValue("uiZoom", 1.0));
    }
    void saveZoom(float value) {
        mSettings->setValue("uiZoom", juce::jlimit(0.5f, 2.0f, value));
        mSettings->saveIfNeeded();
    }
    bool randomizePorta() const { return mRandomizePortaEnabled; }
    bool randomizeFx() const { return mRandomizeFxEnabled; }
    bool randomizeArp() const { return mRandomizeArpEnabled; }
    bool randomizeVoices() const { return mRandomizeVoicesEnabled; }
    bool randomizeNoise() const { return mRandomizeNoiseEnabled; }
    void setRandomizeGroups(bool porta, bool fx, bool arp, bool voices = true, bool noise = true) {
        mRandomizePortaEnabled = porta;
        mRandomizeFxEnabled = fx;
        mRandomizeArpEnabled = arp;
        mRandomizeVoicesEnabled = voices;
        mRandomizeNoiseEnabled = noise;
        mSettings->setValue("randomizePorta", porta);
        mSettings->setValue("randomizeFx", fx);
        mSettings->setValue("randomizeArp", arp);
        mSettings->setValue("randomizeVoices", voices);
        mSettings->setValue("randomizeNoise", noise);
        mSettings->saveIfNeeded();
        sendChangeMessage();
    }

    void setAccent(juce::Colour colour) {
        colour = colour.withAlpha(1.0f);
        if (colour == mAccentColour)
            return;
        mAccentColour = colour;
        mSettings->setValue("accent", colour.toString());
        mSettings->saveIfNeeded();
        sendChangeMessage();
    }

    bool isFavourite(const juce::String& id) const { return mFavourites.contains(id); }
    void toggleFavourite(const juce::String& id) {
        if (mFavourites.contains(id))
            mFavourites.removeString(id);
        else
            mFavourites.addIfNotAlreadyThere(id);
        mSettings->setValue("favourites", mFavourites.joinIntoString("\n"));
        mSettings->saveIfNeeded();
        sendChangeMessage();
    }

  private:
    ThemeManager() {
        juce::PropertiesFile::Options options;
        options.applicationName = "welleyz-ui";
        options.filenameSuffix = "settings";
        options.folderName = "shadowave";
        options.osxLibrarySubFolder = "Application Support";
        mSettings = std::make_unique<juce::PropertiesFile>(options);
        const auto stored = mSettings->getValue("accent");
        if (stored.isNotEmpty())
            mAccentColour = juce::Colour::fromString(stored);
        mFavourites.addLines(mSettings->getValue("favourites"));
        mFavourites.removeEmptyStrings();
        mRandomizePortaEnabled = mSettings->getBoolValue("randomizePorta", true);
        mRandomizeFxEnabled = mSettings->getBoolValue("randomizeFx", true);
        mRandomizeArpEnabled = mSettings->getBoolValue("randomizeArp", true);
        mRandomizeVoicesEnabled = mSettings->getBoolValue("randomizeVoices", true);
        mRandomizeNoiseEnabled = mSettings->getBoolValue("randomizeNoise", true);
    }
    std::unique_ptr<juce::PropertiesFile> mSettings;
    juce::Colour mAccentColour{welleyz::design::colours::kDefaultAccent};
    juce::StringArray mFavourites;
    bool mRandomizePortaEnabled = true, mRandomizeFxEnabled = true, mRandomizeArpEnabled = true,
         mRandomizeVoicesEnabled = true;
    bool mRandomizeNoiseEnabled = true;
};
