#include "UpdateChecker.h"

#ifndef WELLEYZ_VERSION
#define WELLEYZ_VERSION "0.4.0"
#endif

#ifndef WELLEYZ_UPDATE_URL
#define WELLEYZ_UPDATE_URL ""
#endif

namespace {
const juce::String updateEndpoint = WELLEYZ_UPDATE_URL;
const juce::String currentVersion = WELLEYZ_VERSION;
std::atomic<bool> automaticCheckStarted{false};
juce::ThreadPool updateWorkers{1};

bool isVersionNewer(juce::String candidate, juce::String current) {
    candidate =
        candidate.trim().trimCharactersAtStart("vV").upToFirstOccurrenceOf("-", false, false);
    current = current.trim().trimCharactersAtStart("vV").upToFirstOccurrenceOf("-", false, false);
    juce::StringArray candidateParts, currentParts;
    candidateParts.addTokens(candidate, ".", "");
    currentParts.addTokens(current, ".", "");

    for (int i = 0; i < 3; ++i) {
        const int candidatePart = i < candidateParts.size() ? candidateParts[i].getIntValue() : 0;
        const int currentPart = i < currentParts.size() ? currentParts[i].getIntValue() : 0;
        if (candidatePart != currentPart)
            return candidatePart > currentPart;
    }
    return false;
}

void showUpdateResult(juce::Component::SafePointer<juce::Component> owner,
                      const juce::String& title, const juce::String& message) {
    if (owner == nullptr)
        return;

    const auto options = juce::MessageBoxOptions::makeOptionsOk(
        juce::MessageBoxIconType::InfoIcon, title, message, "OK", owner.getComponent());
    juce::AlertWindow::showAsync(options, nullptr);
}
} // namespace

bool WelleyzUpdateChecker::isConfigured() {
    return updateEndpoint.startsWithIgnoreCase("https://");
}

void WelleyzUpdateChecker::check(juce::Component::SafePointer<juce::Component> owner,
                                 bool userInitiated) {
    if (!userInitiated) {
        if (!isConfigured() || automaticCheckStarted.exchange(true))
            return;
    }

    if (!isConfigured()) {
        if (userInitiated)
            showUpdateResult(
                owner, "Update checks are not configured",
                "Set WELLEYZ_UPDATE_URL to "
                "https://api.github.com/repos/OWNER/REPO/releases/latest and rebuild.");
        return;
    }

    updateWorkers.addJob([owner, userInitiated] {
        juce::String title, message;
        int statusCode = 0;
        const juce::URL url(updateEndpoint);
        const auto options =
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(5000)
                .withNumRedirectsToFollow(4)
                .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: welleyz")
                .withStatusCode(&statusCode);
        auto stream = url.createInputStream(options);

        if (stream == nullptr || statusCode < 200 || statusCode >= 300) {
            if (userInitiated) {
                title = "Update check failed";
                message = "Could not reach the configured GitHub Releases endpoint.";
            }
        } else {
            const auto response = juce::JSON::parse(stream->readEntireStreamAsString());
            const auto latest = response.getProperty("tag_name", juce::String()).toString();
            const auto releaseUrl = response.getProperty("html_url", juce::String()).toString();
            const auto notes = response.getProperty("body", juce::String()).toString().trim();

            if (latest.isEmpty()) {
                if (userInitiated) {
                    title = "Update check failed";
                    message = "The release response did not contain a version tag.";
                }
            } else if (isVersionNewer(latest, currentVersion)) {
                title = "welleyz update available";
                message = "Installed: " + currentVersion + "\nLatest: " + latest;
                if (notes.isNotEmpty())
                    message += "\n\n" + notes.substring(0, 700);
                if (releaseUrl.isNotEmpty())
                    message += "\n\nDownload: " + releaseUrl;
            } else if (userInitiated) {
                title = "welleyz is up to date";
                message = "Installed version: " + currentVersion;
            }
        }

        if (title.isNotEmpty())
            juce::MessageManager::callAsync(
                [owner, title, message] { showUpdateResult(owner, title, message); });
    });
}
