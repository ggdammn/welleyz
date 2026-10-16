#include <juce_core/juce_core.h>
#include "WelleyzAssets.h"
#include <cstdio>

int main() {
    const juce::StringArray expected{"1bass",    "1bell",       "1lead",    "1pad",     "2pad",
                                     "arpsynth", "detunedkeys", "foggypad", "noisypad", "organ"};
    for (const auto& name : expected) {
        bool found = false;
        for (int i = 0; i < WelleyzAssets::namedResourceListSize; ++i) {
            if (name + ".wlyz" !=
                juce::File(juce::String::fromUTF8(WelleyzAssets::originalFilenames[i]))
                    .getFileName())
                continue;
            found = true;
            int size = 0;
            const auto* data =
                WelleyzAssets::getNamedResource(WelleyzAssets::namedResourceList[i], size);
            const auto document = data == nullptr
                                      ? juce::var()
                                      : juce::JSON::parse(juce::String::fromUTF8(data, size));
            auto* object = document.getDynamicObject();
            if (object == nullptr || object->getProperty("format") != "welleyz" ||
                (int)object->getProperty("version") != 1 || object->getProperty("name") != name ||
                object->getProperty("parameters").getDynamicObject() == nullptr ||
                object->getProperty("parameters").getDynamicObject()->getProperties().size() < 30) {
                std::fprintf(stderr, "Invalid factory preset: %s\n", name.toRawUTF8());
                return 1;
            }
        }
        if (!found) {
            std::fprintf(stderr, "Missing factory preset: %s\n", name.toRawUTF8());
            return 1;
        }
    }
    std::printf("Verified %d JSON factory presets\n", expected.size());
    return 0;
}
