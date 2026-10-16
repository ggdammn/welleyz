#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "WelleyzAssets.h"
#include "UpdateChecker.h"
#include <algorithm>
#include <map>

namespace {
void drawEmbeddedIcon(juce::Graphics& g, const char* resource, juce::Rectangle<float> bounds,
                      juce::Colour colour, bool mirror = false) {
    int bytes = 0;
    const auto* data = WelleyzAssets::getNamedResource(resource, bytes);
    if (data == nullptr || bytes <= 0)
        return;

    auto svg = juce::String::fromUTF8(data, bytes);
    const auto hex = colour.toString().substring(2).toLowerCase();
    svg = svg.replace("currentColor", "#" + hex);
    const auto key = juce::String(resource) + ":" + hex;
    static std::map<juce::String, std::unique_ptr<juce::Drawable>> cache;
    static std::vector<juce::String> insertionOrder;
    auto it = cache.find(key);
    if (it == cache.end()) {
        auto xml = juce::XmlDocument::parse(svg);
        if (xml == nullptr)
            return;
        auto drawable = juce::Drawable::createFromSVG(*xml);
        if (drawable == nullptr)
            return;
        if (cache.size() >= 44) {
            cache.erase(insertionOrder.front());
            insertionOrder.erase(insertionOrder.begin());
        }
        it = cache.emplace(key, std::move(drawable)).first;
        insertionOrder.push_back(key);
    }

    juce::Graphics::ScopedSaveState state(g);
    if (mirror) {
        g.addTransform(
            juce::AffineTransform::translation(-bounds.getCentreX(), -bounds.getCentreY())
                .scaled(-1.0f, 1.0f)
                .translated(bounds.getCentreX(), bounds.getCentreY()));
    }
    it->second->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
}

const juce::Image& blurredLogo() {
    static const auto image = [] {
        juce::Image source(juce::Image::ARGB, 192, 192, true);
        juce::Graphics drawing(source);
        drawEmbeddedIcon(drawing, "logo_svg", {32.0f, 32.0f, 128.0f, 128.0f}, juce::Colours::white);
        juce::Image blurred(juce::Image::ARGB, 192, 192, true);
        juce::ImageConvolutionKernel kernel(37);
        kernel.createGaussianBlur(16.0f);
        kernel.applyToImage(blurred, source, blurred.getBounds());
        return blurred;
    }();
    return image;
}
} // namespace

// ================= Signal-lab LookAndFeel =================
WelleyzLookAndFeel::WelleyzLookAndFeel() {
    setColour(juce::PopupMenu::textColourId, welleyz::design::colours::kText);
    setColour(juce::PopupMenu::headerTextColourId, welleyz::design::colours::kText);
}

void WelleyzLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float start, float end, juce::Slider& slider) {
    if (theme == nullptr)
        return;
    const auto& t = *theme;
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const auto id = slider.getComponentID();
    const bool primary = id == "filterCutoff" || id == "masterLevel" || id == "oscillator1Level" ||
                         id == "oscillator2Level" || id == "oscillator3Level" ||
                         id == "effectsFilterFrequency";
    const bool fine = id.endsWith("FineTune") || id.endsWith("Detune") || id.endsWith("Semitone") ||
                      id.endsWith("Octave");
    const float r = (juce::jmin(w, h) * 0.5f - 5.0f * uiScale) * (primary ? 0.82f
                                                                  : fine  ? 0.58f
                                                                          : 0.70f);
    if (r <= 4.0f)
        return;

    const auto ringColour = !slider.isEnabled() ? welleyz::design::colours::kMuted
                            : id.containsIgnoreCase("haze") || id.containsIgnoreCase("vib")
                                ? welleyz::design::colours::kViolet
                            : id.containsIgnoreCase("taps") || id.containsIgnoreCase("arp")
                                ? welleyz::design::colours::kModulation
                                : t.mAccent;
    if (slider.isEnabled() && (primary || slider.isMouseOverOrDragging()))
        welleyz::ui::drawHalo(g, {cx - r * 1.65f, cy - r * 1.65f, r * 3.3f, r * 3.3f},
                              ringColour.withAlpha(0.24f));
    g.setColour(welleyz::design::colours::kBackground);
    g.fillEllipse(cx - r - 4.0f * uiScale, cy - r - 4.0f * uiScale, (r + 4.0f * uiScale) * 2.0f,
                  (r + 4.0f * uiScale) * 2.0f);
    g.setGradientFill(juce::ColourGradient(
        welleyz::design::colours::kActive.brighter(0.25f), cx - r, cy - r,
        welleyz::design::colours::kSurface.darker(0.22f), cx + r, cy + r, false));
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(welleyz::design::colours::kBackground.withAlpha(0.45f));
    g.drawEllipse(cx - r + 3.0f * uiScale, cy - r + 3.0f * uiScale, 2.0f * (r - 3.0f * uiScale),
                  2.0f * (r - 3.0f * uiScale), 1.2f * uiScale);
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.drawEllipse(cx - r + uiScale, cy - r + uiScale, 2.0f * (r - uiScale), 2.0f * (r - uiScale),
                  uiScale);
    juce::Path track;
    track.addCentredArc(cx, cy, r + 5.0f * uiScale, r + 5.0f * uiScale, 0.0f, start, end, true);
    g.setColour(welleyz::design::colours::kBorderStrong.withAlpha(0.34f));
    g.strokePath(track, juce::PathStrokeType(2.5f * uiScale));
    juce::Path arc;
    arc.addCentredArc(cx, cy, r + 5.0f * uiScale, r + 5.0f * uiScale, 0.0f, start,
                      start + (end - start) * pos, true);
    welleyz::ui::drawAccentGlow(g, arc, ringColour, 2.0f, uiScale,
                                slider.isMouseOverOrDragging() ? 0.95f : 0.72f);
    const float a = start + (end - start) * pos;
    const float px = cx + std::cos(a - juce::MathConstants<float>::halfPi) * (r - 3.0f);
    const float py = cy + std::sin(a - juce::MathConstants<float>::halfPi) * (r - 3.0f);
    g.setColour(ringColour);
    g.drawLine(cx, cy, px, py, 2.0f * uiScale);
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.fillEllipse(cx - 2.0f * uiScale, cy - 2.0f * uiScale, 4.0f * uiScale, 4.0f * uiScale);
}

void WelleyzLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int,
                                      int, juce::ComboBox& combo) {
    if (theme == nullptr)
        return;
    const auto& t = *theme;
    auto r = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
    g.setGradientFill(juce::ColourGradient(welleyz::design::colours::kBackground, 0.0f, 0.0f,
                                           t.mComboBg, 0.0f, (float)height, false));
    g.fillRoundedRectangle(r, welleyz::design::radius::kSmall * uiScale);
    g.setColour(t.mComboEdge);
    g.drawRoundedRectangle(r.reduced(0.5f), welleyz::design::radius::kSmall * uiScale, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.drawLine(4.0f * uiScale, 1.5f * uiScale, width - 4.0f * uiScale, 1.5f * uiScale, uiScale);
    juce::Path p;
    const float ax = (float)width - 16.0f, ay = (float)height * 0.5f;
    p.addTriangle(ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour(combo.isEnabled() ? t.mComboText : welleyz::design::colours::kMuted);
    g.fillPath(p);
}

void WelleyzLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    if (theme == nullptr)
        return;
    const auto& t = *theme;
    g.setGradientFill(juce::ColourGradient(t.mPanelTop.brighter(0.10f), 0.0f, 0.0f, t.mPanelBottom,
                                           (float)width, (float)height, false));
    g.fillRect(0, 0, width, height);
    g.setColour(t.mPanelEdge);
    g.drawRect(0, 0, width, height, 1);
}

juce::Font WelleyzLookAndFeel::getPopupMenuFont() {
    return mono(14.0f * uiScale);
}

void WelleyzLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                              const juce::Colour&, bool hover, bool down) {
    if (theme == nullptr)
        return;
    const auto& t = *theme;
    const bool isRandom = b.getComponentID() == "randomSoundButton";
    if (isRandom) {
        const auto bounds = b.getLocalBounds().toFloat();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.46f;
        const auto centre = bounds.getCentre();
        const float x = centre.x, y = centre.y;
        welleyz::ui::drawHalo(
            g, {x - radius * 1.42f, y - radius * 1.42f, radius * 2.84f, radius * 2.84f},
            t.mAccent.withAlpha(hover ? 1.0f : 0.82f));
        g.setColour(welleyz::design::colours::kBackground);
        g.fillEllipse(x - radius, y - radius, radius * 2, radius * 2);
        g.setColour(welleyz::design::colours::kBorder.withAlpha(0.65f));
        g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, uiScale);
        const float innerR = radius * 0.79f;
        g.setGradientFill(juce::ColourGradient(
            down ? t.mPanelBottom : welleyz::design::colours::kActive.brighter(0.12f), x - innerR,
            y - innerR, down ? t.mPanelTop : welleyz::design::colours::kSecondary, x + innerR,
            y + innerR, false));
        g.fillEllipse(x - innerR, y - innerR, innerR * 2.0f, innerR * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.17f));
        g.drawEllipse(x - innerR, y - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f * uiScale);
        g.setColour(juce::Colours::white.withAlpha(0.13f));
        g.drawEllipse(x - innerR + 3.0f * uiScale, y - innerR + 3.0f * uiScale,
                      2.0f * (innerR - 3.0f * uiScale), 2.0f * (innerR - 3.0f * uiScale), uiScale);
        juce::Path bezel;
        const float bezelRadius = radius * 0.91f;
        bezel.addCentredArc(x, y, bezelRadius, bezelRadius, 0.0f,
                            -juce::MathConstants<float>::pi * 0.77f + randomizeRotation,
                            juce::MathConstants<float>::pi * 0.73f + randomizeRotation, true);
        welleyz::ui::drawAccentGlow(g, bezel, t.mAccent, 3.4f, uiScale,
                                    hover  ? 1.0f
                                    : down ? 0.78f
                                           : 0.97f);
        if (b.hasKeyboardFocus(false)) {
            g.setColour(t.mAccent);
            g.drawEllipse(x - radius - 2.0f * uiScale, y - radius - 2.0f * uiScale,
                          2.0f * (radius + 2.0f * uiScale), 2.0f * (radius + 2.0f * uiScale),
                          2.0f * uiScale);
        }
        return;
    }
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    if (b.getComponentID() == "recordButton" && b.getToggleState()) {
        const auto top = mRecordBlinkBright ? juce::Colour(0xfff44350) : juce::Colour(0xff713037);
        const auto bottom =
            mRecordBlinkBright ? juce::Colour(0xffaa1829) : juce::Colour(0xff421923);
        g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getRight(),
                                               r.getBottom(), false));
        g.fillRoundedRectangle(r, welleyz::design::radius::kMedium * uiScale);
        g.setColour(juce::Colour(0xffff737b));
        g.drawRoundedRectangle(r, welleyz::design::radius::kMedium * uiScale, 1.4f * uiScale);
        if (b.hasKeyboardFocus(false))
            g.drawRoundedRectangle(r.reduced(2.0f * uiScale),
                                   welleyz::design::radius::kMedium * uiScale, 2.0f * uiScale);
        return;
    }
    const bool headerIconButton =
        b.getComponentID() == "manualButton" || b.getComponentID() == "recordButton" ||
        b.getComponentID() == "metronomeButton" || b.getComponentID() == "settingsButton" ||
        b.getComponentID() == "colorsButton" || b.getComponentID() == "zoomButton";
    const float rad =
        (headerIconButton ? welleyz::design::radius::kMedium : welleyz::design::radius::kSmall) *
        uiScale;
    const bool isFxPower = b.getButtonText() == "ON" || b.getButtonText() == "OFF";
    const bool selected = b.getToggleState();
    g.setGradientFill(juce::ColourGradient(
        selected ? t.mBtnAccentTop.brighter(0.1f)
        : down   ? welleyz::design::colours::kSurface
        : hover  ? welleyz::design::colours::kActive
                 : t.mBtnTop.brighter(0.13f),
        r.getX(), r.getY(), selected ? t.mBtnAccentBottom : t.mBtnBottom.darker(0.12f),
        r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle(r, rad);
    g.setColour(juce::Colours::white.withAlpha(hover ? 0.13f : 0.07f));
    g.drawLine(r.getX() + 5.0f * uiScale, r.getY() + 1.5f * uiScale, r.getRight() - 5.0f * uiScale,
               r.getY() + 1.5f * uiScale, 0.8f * uiScale);
    if (selected || hover) {
        juce::Path outline;
        outline.addRoundedRectangle(r, rad);
        g.setColour(selected ? t.mAccent.withAlpha(0.72f) : t.mAccent.withAlpha(0.30f));
        g.strokePath(outline, juce::PathStrokeType(1.0f * uiScale));
    } else {
        g.setColour(t.mBtnEdge);
        g.drawRoundedRectangle(r, rad, 1.0f * uiScale);
    }
    if (isFxPower && selected) {
        const float ledX = r.getX() + 10.0f * uiScale, ledY = r.getCentreY();
        juce::Path led;
        led.addEllipse(ledX - 2.0f * uiScale, ledY - 2.0f * uiScale, 4.0f * uiScale,
                       4.0f * uiScale);
        welleyz::ui::drawAccentGlow(g, led, t.mAccent, 1.1f, uiScale, 0.9f);
    }
    if (b.hasKeyboardFocus(false)) {
        g.setColour(t.mAccent);
        g.drawRoundedRectangle(r.reduced(2.0f * uiScale), rad, 2.0f * uiScale);
    }
}

void WelleyzLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool hover,
                                        bool) {
    const auto id = button.getComponentID();
    const float s = uiScale;
    const bool tab = id == "randomTab" || id == "synthTab" || id == "fxRackTab";
    const bool utility = button.getButtonText() == "BACK" || button.getButtonText() == "RESET" ||
                         button.getButtonText() == "SAVE" || button.getButtonText() == "LOAD";
    const bool random = id == "randomSoundButton";
    g.setColour(
        button.isEnabled()
            ? (tab && button.getToggleState() ? theme->mAccent : welleyz::design::colours::kText)
            : welleyz::design::colours::kDisabled);
    if (random) {
        const auto center = button.getLocalBounds().toFloat().getCentre();
        if (button.isEnabled()) {
            const auto glowBounds = juce::Rectangle<float>(
                center.x - 107.0f * s, center.y - 107.0f * s, 214.0f * s, 214.0f * s);
            g.setColour(theme->mAccent.withAlpha(0.96f));
            g.drawImage(blurredLogo(), glowBounds, juce::RectanglePlacement::stretchToFit, true);
            g.setColour(theme->mAccent.withAlpha(0.58f));
            g.drawImage(blurredLogo(), glowBounds, juce::RectanglePlacement::stretchToFit, true);
        }
        drawEmbeddedIcon(g, "logo_svg",
                         {center.x - 68.0f * s, center.y - 68.0f * s, 136.0f * s, 136.0f * s},
                         button.isEnabled() ? theme->mAccent : welleyz::design::colours::kDisabled);
        return;
    }
    const char* iconResource = nullptr;
    if (id == "randomTab")
        iconResource = "random_icon_svg";
    else if (id == "synthTab")
        iconResource = "osc_icon_svg";
    else if (id == "fxRackTab")
        iconResource = "fx_icon_svg";
    else if (button.getButtonText() == "BACK")
        iconResource = "back_icon_svg";
    else if (button.getButtonText() == "RESET")
        iconResource = "reset_icon_svg";
    else if (button.getButtonText() == "SAVE")
        iconResource = "save__icon_svg";
    else if (button.getButtonText() == "LOAD")
        iconResource = "load_icon_svg";
    else if (id == "settingsButton")
        iconResource = "settings_icon_svg";
    else if (id == "manualButton")
        iconResource = "manual_icon_svg";
    else if (id == "recordButton")
        iconResource = "record_icon_svg";
    else if (id == "metronomeButton")
        iconResource = "metronome_icon_svg";
    else if (id == "colorsButton")
        iconResource = "colors_icon_svg";
    else if (id == "zoomButton")
        iconResource = "scale_icon_svg";

    const bool headerAction = id == "manualButton" || id == "recordButton" ||
                              id == "metronomeButton" || id == "settingsButton" ||
                              id == "colorsButton" || id == "zoomButton";
    if (iconResource != nullptr && (tab || utility || headerAction)) {
        const auto iconColour =
            id == "recordButton" && button.getToggleState() ? juce::Colours::white
            : (button.getToggleState() || hover)            ? theme->mAccent
                                                 : welleyz::design::colours::kSecondaryText;
        if (headerAction) {
            const float side = 24.0f * s;
            drawEmbeddedIcon(
                g, iconResource,
                {(button.getWidth() - side) * 0.5f, (button.getHeight() - side) * 0.5f, side, side},
                iconColour);
        } else {
            const float cx = button.getWidth() * 0.5f;
            const float iconSide = (tab ? 25.0f : 27.0f) * s;
            const float cy = (tab ? 25.0f : 29.0f) * s;
            drawEmbeddedIcon(g, iconResource,
                             {cx - iconSide * 0.5f, cy - iconSide * 0.5f, iconSide, iconSide},
                             iconColour);
            g.setColour(button.getToggleState() ? theme->mAccent : welleyz::design::colours::kText);
            g.setFont(mono((tab ? 14.0f : 13.0f) * s));
            g.drawText(button.getButtonText(), 0, (int)((tab ? 42.0f : 49.0f) * s),
                       button.getWidth(), (int)(20.0f * s), juce::Justification::centred);
        }
        return;
    }
    g.setColour(button.getToggleState() ? theme->mAccent
                : button.isEnabled()    ? welleyz::design::colours::kText
                                        : welleyz::design::colours::kDisabled);
    g.setFont(mono(14.0f * s));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
}

void WelleyzLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                          float position, float, float,
                                          const juce::Slider::SliderStyle style, juce::Slider&) {
    if (style == juce::Slider::LinearHorizontal) {
        juce::ColourGradient hue(juce::Colours::red, (float)x, (float)y, juce::Colours::red,
                                 (float)(x + w), (float)y, false);
        for (int i = 1; i < 6; ++i)
            hue.addColour((double)i / 6.0,
                          juce::Colour::fromHSV((float)i / 6.0f, 0.85f, 1.0f, 1.0f));
        g.setGradientFill(hue);
        g.fillRoundedRectangle((float)x, y + h * 0.35f, (float)w, h * 0.30f, h * 0.15f);
        g.setColour(theme->mAccent.withAlpha(0.18f));
        g.fillEllipse(position - 12.0f * uiScale, y + h * 0.5f - 12.0f * uiScale, 24.0f * uiScale,
                      24.0f * uiScale);
        g.setColour(theme->mAccent);
        g.drawEllipse(position - 8.0f * uiScale, y + h * 0.5f - 8.0f * uiScale, 16.0f * uiScale,
                      16.0f * uiScale, 2.0f * uiScale);
        return;
    }
    const float cx = x + w * 0.5f;
    const auto slot = juce::Rectangle<float>(cx - 8.0f * uiScale, y + 8.0f * uiScale,
                                             16.0f * uiScale, h - 16.0f * uiScale);
    g.setColour(welleyz::design::colours::kBackground);
    g.fillRoundedRectangle(slot, 3.0f * uiScale);
    g.setColour(welleyz::design::colours::kBorderStrong);
    g.drawRoundedRectangle(slot, 3.0f * uiScale, 1.0f * uiScale);
    const float bottom = slot.getBottom() - 2.0f * uiScale;
    g.setColour(theme->mAccent);
    g.fillRoundedRectangle(cx - 6.0f * uiScale, position, 12.0f * uiScale,
                           juce::jmax(2.0f * uiScale, bottom - position), 2.0f * uiScale);
}

juce::Font WelleyzLookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return mono(14.0f * uiScale);
}

juce::Font WelleyzLookAndFeel::getTextButtonFont(juce::TextButton& button, int) {
    if (button.getComponentID() == "randomSoundButton")
        return display(25.0f * uiScale);
    if (button.getComponentID() == "randomTab" || button.getComponentID() == "synthTab" ||
        button.getComponentID() == "fxRackTab")
        return display(18.0f * uiScale);
    return mono(14.0f * uiScale);
}

juce::Slider::SliderLayout WelleyzLookAndFeel::getSliderLayout(juce::Slider& slider) {
    auto layout = juce::LookAndFeel_V4::getSliderLayout(slider);
    if (slider.getTextBoxPosition() == juce::Slider::TextBoxBelow) {
        const int closeBy = juce::jmax(1, juce::roundToInt(3.0f * uiScale));
        layout.textBoxBounds.translate(0, -closeBy);
        layout.sliderBounds.setBottom(layout.textBoxBounds.getY());
    }
    return layout;
}

void PowerDot::paintButton(juce::Graphics& g, bool hover, bool down) {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    const auto colour =
        getToggleState() ? welleyz::design::colours::kOnLed : welleyz::design::colours::kOffLed;
    const float r = 4.2f * s;
    const auto centre = getLocalBounds().toFloat().getCentre();
    welleyz::ui::drawHalo(g, {centre.x - 12.0f * s, centre.y - 12.0f * s, 24.0f * s, 24.0f * s},
                          colour.withAlpha(hover ? 0.9f : 0.65f));
    g.setColour(welleyz::design::colours::kBackground);
    g.fillEllipse(centre.x - 7.0f * s, centre.y - 7.0f * s, 14.0f * s, 14.0f * s);
    g.setColour(colour.withAlpha(down ? 0.7f : 1.0f));
    g.fillEllipse(centre.x - r, centre.y - r, 2.0f * r, 2.0f * r);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.fillEllipse(centre.x - r * 0.52f, centre.y - r * 0.64f, r * 0.7f, r * 0.7f);
    if (hasKeyboardFocus(false)) {
        g.setColour(colour);
        g.drawEllipse(centre.x - 9.0f * s, centre.y - 9.0f * s, 18.0f * s, 18.0f * s, s);
    }
}

// ================= Knob / OscStrip / Section =================
void WelleyzEditor::StudioKeyboard::timerCallback(int) {
    bool changed = false;
    for (int n = 0; n < 128; ++n) {
        const float target = state.isNoteOnForChannels(0xffff, n) ? 1.0f : 0.0f;
        const float next = target > press[(size_t)n] ? juce::jmin(1.0f, press[(size_t)n] + 0.55f)
                                                     : juce::jmax(0.0f, press[(size_t)n] - 0.22f);
        if (next != press[(size_t)n]) {
            press[(size_t)n] = next;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void WelleyzEditor::StudioKeyboard::drawWhiteNote(int note, juce::Graphics& g,
                                                  juce::Rectangle<float> area, bool down, bool over,
                                                  juce::Colour line, juce::Colour) {
    const float travel = juce::jmax(press[(size_t)note], down ? 0.60f : 0.0f);
    const auto ivory = findColour(whiteNoteColourId);
    g.setColour(findColour(keySeparatorLineColourId));
    g.fillRect(area);
    auto face = area.reduced(0.8f, 0.0f);
    const auto keyColour =
        travel > 0.01f
            ? ivory.interpolatedWith(ThemeManager::instance().accent(), 0.35f + 0.48f * travel)
        : over ? ivory.brighter(0.09f)
               : ivory;
    g.setGradientFill(juce::ColourGradient(keyColour, face.getX(), face.getY(),
                                           keyColour.darker(0.035f), face.getX(), face.getBottom(),
                                           false));
    g.fillRoundedRectangle(face.reduced(0.0f, 0.5f), 3.0f);
    g.setColour(line.withAlpha(0.6f));
    g.drawRoundedRectangle(face.reduced(0.5f), 3.0f, 0.8f);
    if (note % 12 == 0) {
        g.setColour(juce::Colour(0xff1b2024));
        g.setFont(welleyz::fonts::mono(11.0f *
                                       static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale));
        g.drawText("C" + juce::String(note / 12 - 1), face.removeFromBottom(17.0f),
                   juce::Justification::centred, false);
    }
}

void WelleyzEditor::StudioKeyboard::drawBlackNote(int note, juce::Graphics& g,
                                                  juce::Rectangle<float> area, bool down, bool over,
                                                  juce::Colour fill) {
    const float travel = juce::jmax(press[(size_t)note], down ? 0.60f : 0.0f);
    if (travel > 0.01f) {
        g.setColour(ThemeManager::instance().accent().withAlpha(0.16f));
        g.fillRect(area.expanded(1.0f));
    }
    g.setColour(travel > 0.01f ? ThemeManager::instance().accent()
                : over         ? fill.brighter(0.15f)
                               : fill);
    g.fillRoundedRectangle(area.reduced(0.5f), 2.5f);
    g.setColour(welleyz::design::colours::kBorder);
    g.drawRoundedRectangle(area.reduced(0.5f), 2.5f, 1.0f);
}

void WelleyzEditor::Knob::paint(juce::Graphics& g) {
    auto& hl = static_cast<WelleyzLookAndFeel&>(getLookAndFeel());
    if (hl.theme == nullptr)
        return;
    welleyz::ui::drawEngraved(g, label, {0.0f, 0.0f, (float)getWidth(), 18.0f * hl.uiScale},
                              hl.mono(14.0f * hl.uiScale),
                              isEnabled() ? hl.theme->mLabelMain : welleyz::design::colours::kMuted,
                              hl.theme->mLabelShadow);
}

WelleyzEditor::OscStrip::OscStrip(const juce::String& t) : title(t) {
    addAndMakeVisible(power);
    oct = std::make_unique<Knob>("OCT");
    semi = std::make_unique<Knob>("SEMI");
    fine = std::make_unique<Knob>("FINE");
    level = std::make_unique<Knob>("LEVEL");
    pan = std::make_unique<Knob>("PAN");
    detune = std::make_unique<Knob>("DETUNE");
    voices.addItemList(juce::StringArray{"1", "2", "3", "4", "6", "8"}, 1);
    wave.addItemList(
        juce::StringArray{"Sine", "Square", "Saw", "Triangle", "Pulse 12%", "Pulse 25%", "Noise"},
        1);
    wave.onChange = [this] { repaint(); };
    addAndMakeVisible(wave);
    addAndMakeVisible(voices);
    addAndMakeVisible(*oct);
    addAndMakeVisible(*semi);
    addAndMakeVisible(*fine);
    addAndMakeVisible(*level);
    addAndMakeVisible(*pan);
    addAndMakeVisible(*detune);
}

void WelleyzEditor::OscStrip::paint(juce::Graphics& g) {
    auto& hl = static_cast<WelleyzLookAndFeel&>(getLookAndFeel());
    if (hl.theme == nullptr)
        return;
    const auto& t = *hl.theme;
    const float s = hl.uiScale;
    auto r = getLocalBounds().toFloat();
    welleyz::ui::fillPanel(g, r, t, welleyz::design::radius::kMedium * s);
    welleyz::ui::drawEngraved(g, title, {0.0f, 4.0f * s, r.getWidth(), 24.0f * s},
                              hl.display(17.0f * s),
                              power.getToggleState() ? t.mTitleMain : t.mLabelMain, t.mTitleShadow);
    welleyz::ui::drawEngraved(g, "WAVE", {12.0f * s, 55.0f * s, r.getWidth() * 0.64f, 14.0f * s},
                              hl.mono(13.0f * s), t.mLabelMain, t.mLabelShadow);
    welleyz::ui::drawEngraved(g, "VOICES",
                              {r.getWidth() * 0.70f, 55.0f * s, r.getWidth() * 0.25f, 14.0f * s},
                              hl.mono(13.0f * s), t.mLabelMain, t.mLabelShadow);
    const auto preview =
        juce::Rectangle<float>(r.getWidth() - 152.0f * s, 25.0f * s, 136.0f * s, 28.0f * s);
    juce::Path waveform;
    const int selected = wave.getSelectedId();
    for (int i = 0; i <= 64; ++i) {
        const float phase = (float)i / 64.0f * 2.0f;
        const float cycle = phase - std::floor(phase);
        float value = 0.0f;
        switch (selected) {
        case 2:
            value = cycle < 0.5f ? 1.0f : -1.0f;
            break;
        case 3:
            value = 1.0f - cycle * 2.0f;
            break;
        case 4:
            value = 1.0f - 4.0f * std::abs(cycle - 0.5f);
            break;
        case 5:
            value = cycle < 0.12f ? 1.0f : -1.0f;
            break;
        case 6:
            value = cycle < 0.25f ? 1.0f : -1.0f;
            break;
        case 7:
            value = std::sin(phase * 59.0f) * std::sin(phase * 107.0f);
            break;
        default:
            value = std::sin(phase * juce::MathConstants<float>::twoPi);
            break;
        }
        const float x = preview.getX() + preview.getWidth() * (float)i / 64.0f;
        const float y = preview.getCentreY() - value * preview.getHeight() * 0.36f;
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }
    welleyz::ui::drawAccentGlow(
        g, waveform, power.getToggleState() ? t.mAccent : welleyz::design::colours::kMuted, 1.3f, s,
        power.getToggleState() ? 0.85f : 0.4f);
}

void WelleyzEditor::OscStrip::resized() {
    auto& hl = static_cast<WelleyzLookAndFeel&>(getLookAndFeel());
    const float s = hl.uiScale;
    const float pad = 12.0f * s;
    power.setBounds(juce::roundToInt(8.0f * s), juce::roundToInt(8.0f * s),
                    juce::jmax(12, (int)(24.0f * s)), juce::jmax(12, (int)(24.0f * s)));
    wave.setBounds((int)pad, juce::roundToInt(70.0f * s), (int)(getWidth() * 0.64f),
                   juce::roundToInt(28.0f * s));
    voices.setBounds((int)(getWidth() * 0.70f), juce::roundToInt(70.0f * s),
                     (int)(getWidth() * 0.25f), juce::roundToInt(28.0f * s));
    const float top = 103.0f * s;
    const float gap = 8.0f * s;
    const float kw = (getWidth() - pad * 2.0f - gap * 5.0f) / 6.0f;
    const float kh = 119.0f * s;
    float x = pad;
    for (auto* k : {oct.get(), semi.get(), fine.get(), level.get(), pan.get(), detune.get()}) {
        k->setBounds((int)x, (int)top, (int)kw, (int)kh);
        x += kw + gap;
    }
}

void WelleyzEditor::OscStrip::updatePowerState() {
    const bool enabled = power.getToggleState();
    wave.setEnabled(enabled);
    voices.setEnabled(enabled);
    for (auto* knob : {oct.get(), semi.get(), fine.get(), level.get(), pan.get(), detune.get()})
        knob->setEnabled(enabled);
    repaint();
}

void WelleyzEditor::Section::paint(juce::Graphics& g) {
    auto& hl = static_cast<WelleyzLookAndFeel&>(getLookAndFeel());
    if (hl.theme == nullptr)
        return;
    const auto& t = *hl.theme;
    const float s = hl.uiScale;
    auto r = getLocalBounds().toFloat();
    welleyz::ui::fillPanel(g, r, t, welleyz::design::radius::kMedium * s);
    welleyz::ui::drawEngraved(
        g, title, {0.0f, 4.0f * s, r.getWidth(), 22.0f * s}, hl.display(17.0f * s),
        title == "MASTER + ARP" || power.getToggleState() ? t.mTitleMain : t.mLabelMain,
        t.mTitleShadow);
    if (title == "TUBE + LO-FI") {
        welleyz::ui::drawEngraved(g, "NOISE", {13.0f * s, 151.0f * s, 42.0f * s, 18.0f * s},
                                  hl.mono(12.0f * s), t.mLabelMain, t.mLabelShadow,
                                  juce::Justification::centredLeft);
        welleyz::ui::drawEngraved(g, "TEXTURE", {124.0f * s, 151.0f * s, 115.0f * s, 18.0f * s},
                                  hl.mono(12.0f * s), t.mLabelMain, t.mLabelShadow,
                                  juce::Justification::centredLeft);
    }
    if (graphControls[0] == nullptr)
        return;

    auto scope = juce::Rectangle<float>(14.0f * s, 157.0f * s, r.getWidth() - 28.0f * s, 53.0f * s);
    g.setColour(welleyz::design::colours::kBackground.withAlpha(0.55f));
    g.fillRoundedRectangle(scope, welleyz::design::radius::kSmall * s);
    g.setColour(t.mPanelEdge);
    g.drawRoundedRectangle(scope.reduced(0.5f * s), welleyz::design::radius::kSmall * s, 0.8f * s);
    g.setColour(t.mLabelMain.withAlpha(0.25f));
    g.drawHorizontalLine((int)scope.getCentreY(), scope.getX() + 5.0f * s,
                         scope.getRight() - 5.0f * s);
    auto norm = [](const juce::Slider* slider) {
        return (float)slider->getNormalisableRange().convertTo0to1(slider->getValue());
    };
    const auto graph = scope.reduced(8.0f * s, 7.0f * s);
    juce::Path curve;
    if (title == "ENVELOPE") {
        const float attack = 0.07f + 0.29f * norm(graphControls[0]);
        const float decay = 0.08f + 0.26f * norm(graphControls[1]);
        const float sustain = norm(graphControls[2]);
        const float release = 0.08f + 0.25f * norm(graphControls[3]);
        const float startX = graph.getX(), width = graph.getWidth();
        const float bottom = graph.getBottom(), height = graph.getHeight();
        curve.startNewSubPath(startX, bottom);
        curve.lineTo(startX + width * attack, bottom - height);
        curve.lineTo(startX + width * (attack + decay), bottom - height * sustain);
        curve.lineTo(startX + width * (1.0f - release), bottom - height * sustain);
        curve.lineTo(graph.getRight(), bottom);
    } else if (title == "FILTER") {
        const float cutoff = 0.06f + 0.88f * norm(graphControls[0]);
        const float resonance = norm(graphControls[1]);
        for (int i = 0; i <= 60; ++i) {
            const float u = (float)i / 60.0f;
            const float slope = 1.0f / (1.0f + std::exp((u - cutoff) * 32.0f));
            const float peak = resonance * 0.36f * std::exp(-std::pow((u - cutoff) * 30.0f, 2.0f));
            const float y = graph.getBottom() -
                            graph.getHeight() * juce::jlimit(0.0f, 1.0f, slope * 0.72f + peak);
            if (i == 0)
                curve.startNewSubPath(graph.getX(), y);
            else
                curve.lineTo(graph.getX() + graph.getWidth() * u, y);
        }
    }
    welleyz::ui::drawAccentGlow(
        g, curve, power.getToggleState() ? t.mAccent : welleyz::design::colours::kMuted, 1.3f, s,
        power.getToggleState() ? 0.85f : 0.4f);
}

void WelleyzEditor::Section::resized() {
    const float s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    if (title != "MASTER + ARP")
        power.setBounds(juce::roundToInt(8.0f * s), juce::roundToInt(8.0f * s),
                        juce::jmax(12, (int)(24.0f * s)), juce::jmax(12, (int)(24.0f * s)));
}

void WelleyzEditor::Section::updatePowerState() {
    const bool enabled = power.getToggleState();
    for (int i = 0; i < getNumChildComponents(); ++i)
        if (auto* child = getChildComponent(i); child != &power)
            child->setEnabled(enabled);
    repaint();
}

void WelleyzEditor::NoiseWave::paint(juce::Graphics& g) {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    const auto& t = *static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).theme;
    auto area = getLocalBounds().toFloat().reduced(0.5f * s);
    g.setColour(welleyz::design::colours::kBackground.withAlpha(0.65f));
    g.fillRoundedRectangle(area, welleyz::design::radius::kSmall * s);
    g.setColour(t.mPanelEdge);
    g.drawRoundedRectangle(area, welleyz::design::radius::kSmall * s, s);
    g.setColour(t.mLabelMain.withAlpha(0.25f));
    g.drawHorizontalLine(juce::roundToInt(area.getCentreY()), area.getX() + 5.0f * s,
                         area.getRight() - 5.0f * s);
    welleyz::backgroundNoise::Generator generator;
    generator.reset(44100.0, 0x4a3b2c1du);
    juce::Path waveform;
    const int typeIndex = juce::jmax(0, type.getSelectedId() - 1);
    // Deterministic preview of the selected texture, including its sparse clicks.
    for (int i = 0; i < 150; ++i) {
        float sample = 0.0f;
        for (int j = 0; j < 32; ++j)
            sample += generator.next(typeIndex);
        const float x = area.getX() + (6.0f + (area.getWidth() / s - 12.0f) * i / 149.0f) * s;
        const float y = area.getCentreY() -
                        juce::jlimit(-1.0f, 1.0f, sample * 0.23f) * area.getHeight() * 0.44f;
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }
    welleyz::ui::drawAccentGlow(
        g, waveform, power.getToggleState() ? t.mAccent : welleyz::design::colours::kMuted, 1.2f, s,
        power.getToggleState() ? 0.85f : 0.4f);
}

WelleyzEditor::FxModule::FxModule(juce::String t, juce::String description, juce::String a,
                                  juce::String b, juce::String c)
    : title(std::move(t)), subtitle(std::move(description)), first(a), second(b), third(c) {
    power.setClickingTogglesState(true);
    power.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    power.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    power.onStateChange = [this] {
        power.setButtonText(power.getToggleState() ? "ON" : "OFF");
        repaint();
    };
    for (auto* k : {&first, &second, &third}) {
        addAndMakeVisible(*k);
        k->slider.onValueChange = [this] { repaint(); };
    }
    addAndMakeVisible(power);
    if (title == "FILTER") {
        mode.addItemList(juce::StringArray{"Lowpass", "Bandpass", "Highpass"}, 1);
        mode.onChange = [this] { repaint(); };
        addAndMakeVisible(mode);
    }
}

void WelleyzEditor::FxModule::paint(juce::Graphics& g) {
    auto& hl = static_cast<WelleyzLookAndFeel&>(getLookAndFeel());
    if (hl.theme == nullptr)
        return;
    const auto& t = *hl.theme;
    const float s = hl.uiScale;
    const auto bounds = getLocalBounds().toFloat();
    welleyz::ui::fillPanel(g, bounds, t, welleyz::design::radius::kMedium * s);
    const auto stage = title == "FILTER" ? "01"
                       : title == "HAZE" ? "02"
                       : title == "TAPS" ? "03"
                                         : "04";
    welleyz::ui::drawEngraved(g, juce::String(stage) + " / " + title,
                              {22.0f * s, 8.0f * s, 260.0f * s, 30.0f * s}, hl.display(25.0f * s),
                              t.mTitleMain, t.mTitleShadow, juce::Justification::centredLeft);
    welleyz::ui::drawEngraved(g, subtitle, {24.0f * s, 37.0f * s, 230.0f * s, 17.0f * s},
                              hl.mono(12.0f * s), t.mLabelMain, t.mLabelShadow,
                              juce::Justification::centredLeft);

    auto scope = juce::Rectangle<float>(22.0f * s, 76.0f * s, 173.0f * s, 105.0f * s);
    g.setGradientFill(juce::ColourGradient(welleyz::design::colours::kBackground, scope.getX(),
                                           scope.getY(), t.mComboBg, scope.getRight(),
                                           scope.getBottom(), false));
    g.fillRoundedRectangle(scope, 4.0f * s);
    g.setColour(t.mComboEdge);
    g.drawRoundedRectangle(scope, 4.0f * s, 1.0f * s);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawLine(scope.getX() + 8.0f * s, scope.getY() + 1.0f * s, scope.getRight() - 8.0f * s,
               scope.getY() + 1.0f * s, s);
    for (int i = 1; i < 4; ++i) {
        g.setColour(t.mLabelMain.withAlpha(0.12f));
        const float y = scope.getY() + scope.getHeight() * (float)i / 4.0f;
        g.drawHorizontalLine((int)y, scope.getX() + 5.0f * s, scope.getRight() - 5.0f * s);
    }
    juce::Path waveform;
    const float amount =
        (float)first.slider.getNormalisableRange().convertTo0to1(first.slider.getValue());
    for (int i = 0; i <= 80; ++i) {
        const float u = (float)i / 80.0f;
        float v = 0.0f;
        if (title == "FILTER") {
            const float center = 0.15f + 0.74f * amount;
            const float sharpness =
                juce::jmax(0.015f, 0.04f + 0.13f * (1.0f - (float)second.slider.getValue()));
            const float low = 1.0f / (1.0f + std::exp((u - center) / sharpness));
            v = mode.getSelectedId() == 3   ? 1.0f - low
                : mode.getSelectedId() == 2 ? 4.0f * low * (1.0f - low)
                                            : low;
        } else if (title == "HAZE")
            v = 0.5f +
                0.28f * std::sin(u * (4.0f + 8.0f * amount) * juce::MathConstants<float>::pi);
        else if (title == "TAPS")
            v = 0.18f +
                std::exp(-u * 2.5f) * (std::pow(std::abs(std::sin(u * (8.0f + amount * 12.0f) *
                                                                  juce::MathConstants<float>::pi)),
                                                8.0f) *
                                       0.66f);
        else
            v = 0.48f + std::sin(u * 65.0f) * std::exp(-u * (2.0f + 4.0f * amount)) * 0.28f;
        const float x = scope.getX() + (9.0f + u * 155.0f) * s;
        const float y = scope.getBottom() - (12.0f + v * 79.0f) * s;
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }
    if (power.getToggleState())
        welleyz::ui::drawAccentGlow(g, waveform,
                                    title == "HAZE"   ? welleyz::design::colours::kViolet
                                    : title == "TAPS" ? welleyz::design::colours::kModulation
                                                      : t.mAccent,
                                    1.5f, s, 0.63f);
    else {
        g.setColour(t.mLabelMain.withAlpha(0.52f));
        g.strokePath(waveform, juce::PathStrokeType(1.5f * s));
    }
    if (title != "FILTER")
        welleyz::ui::drawEngraved(g, "RESPONSE  /  " + title,
                                  {22.0f * s, 191.0f * s, 180.0f * s, 22.0f * s},
                                  hl.mono(12.0f * s), t.mLabelMain, t.mLabelShadow);
}

void WelleyzEditor::FxModule::resized() {
    auto& hl = static_cast<WelleyzLookAndFeel&>(getLookAndFeel());
    const float s = hl.uiScale;
    auto S = [s](float x) { return (int)std::round(x * s); };
    power.setBounds(getWidth() - S(92.0f), S(12.0f), S(70.0f), S(29.0f));
    const int start = S(209.0f), gap = S(8.0f);
    const int kw = (getWidth() - start - S(14.0f) - gap * 2) / 3;
    int x = start;
    for (auto* k : {&first, &second, &third}) {
        k->setBounds(x, S(78.0f), kw, S(119.0f));
        x += kw + gap;
    }
    if (title == "FILTER")
        mode.setBounds(S(27.0f), S(192.0f), S(163.0f), S(30.0f));
}

// ================= Random page controls =================
WelleyzEditor::SoundTypePanel::Row::Row(juce::String text, int index)
    : juce::Button(text), name(std::move(text)), category(index) {
    setWantsKeyboardFocus(true);
}

void WelleyzEditor::SoundTypePanel::Row::paintButton(juce::Graphics& g, bool hover, bool down) {
    const auto& t = *static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).theme;
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    const bool selected = getToggleState();
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setGradientFill(juce::ColourGradient(selected ? welleyz::design::colours::kActive
                                           : down   ? welleyz::design::colours::kHover
                                           : hover
                                               ? welleyz::design::colours::kSurface.brighter(0.15f)
                                               : welleyz::design::colours::kSurface,
                                           r.getX(), r.getY(),
                                           selected ? ThemeManager::instance().primaryContainer()
                                                    : welleyz::design::colours::kSecondary,
                                           r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle(r, welleyz::design::radius::kSmall * s);
    g.setColour(juce::Colours::white.withAlpha(selected ? 0.10f : 0.05f));
    g.drawLine(r.getX() + 8.0f * s, r.getY() + 1.0f * s, r.getRight() - 8.0f * s,
               r.getY() + 1.0f * s, s);
    if (selected) {
        juce::Path outline;
        outline.addRoundedRectangle(r, welleyz::design::radius::kSmall * s);
        g.setColour(t.mAccent.withAlpha(0.8f));
        g.strokePath(outline, juce::PathStrokeType(1.0f * s));
        g.fillRect(r.getX(), r.getY() + 4.0f * s, 2.0f * s, r.getHeight() - 8.0f * s);
    } else {
        g.setColour(welleyz::design::colours::kBorder);
        g.drawRoundedRectangle(r, welleyz::design::radius::kSmall * s, 1.0f * s);
    }
    if (hasKeyboardFocus(false)) {
        g.setColour(t.mAccent);
        g.drawRoundedRectangle(r.reduced(2.0f * s), welleyz::design::radius::kSmall * s, 2.0f * s);
    }
    drawEmbeddedIcon(g, "soundtypes_icons_svg",
                     {15.0f * s, (r.getHeight() - 19.0f * s) * 0.5f, 19.0f * s, 19.0f * s},
                     selected ? t.mAccent : welleyz::design::colours::kSecondaryText);
    g.setFont(welleyz::fonts::display(21.0f * s));
    g.setColour(selected ? ThemeManager::instance().onPrimaryContainer()
                         : welleyz::design::colours::kText);
    g.drawText(name, juce::Rectangle<float>(46.0f * s, 0, r.getWidth() - 62.0f * s, r.getHeight()),
               juce::Justification::centredLeft, false);
}

WelleyzEditor::SoundTypePanel::SoundTypePanel() {
    setComponentID("soundTypePanel");
    for (const auto& item : {std::pair<const char*, int>{"Synth", 1},
                             {"Pad", 0},
                             {"Pluck", 3},
                             {"Lead", 2},
                             {"Bass", 5},
                             {"Keys", 7},
                             {"Organ", 8},
                             {"Bell", 4},
                             {"Arp", 6}}) {
        auto row = std::make_unique<Row>(item.first, item.second);
        row->setComponentID("randomSoundType" + juce::String(item.second));
        row->onClick = [this, category = item.second] { choose(category); };
        addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }
    choose(1);
}

void WelleyzEditor::SoundTypePanel::choose(int category) {
    selected = category;
    for (auto& row : rows)
        row->setToggleState(row->category == category, juce::dontSendNotification);
    if (onSelect)
        onSelect(category);
}

void WelleyzEditor::SoundTypePanel::paint(juce::Graphics& g) {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    welleyz::ui::fillPanel(g, getLocalBounds().toFloat(),
                           *static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).theme,
                           welleyz::design::radius::kMedium * s);
    g.setFont(welleyz::fonts::display(22.0f * s));
    g.setColour(welleyz::design::colours::kText);
    g.drawText("SOUND TYPE",
               juce::Rectangle<float>(18.0f * s, 12.0f * s, getWidth() - 36.0f * s, 28.0f * s),
               juce::Justification::centredLeft, false);
    g.setColour(welleyz::design::colours::kText);
    g.setFont(welleyz::fonts::mono(12.0f * s));
    g.drawText("01 / SOURCE",
               juce::Rectangle<float>(getWidth() - 116.0f * s, 16.0f * s, 98.0f * s, 20.0f * s),
               juce::Justification::centredRight);
}

void WelleyzEditor::SoundTypePanel::resized() {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    const int margin = juce::roundToInt(16.0f * s), gap = juce::jmax(1, juce::roundToInt(4.0f * s));
    const int top = juce::roundToInt(48.0f * s);
    const int height = (getHeight() - top - margin - gap * 8) / 9;
    for (int i = 0; i < 9; ++i)
        rows[(size_t)i]->setBounds(margin, top + i * (height + gap), getWidth() - margin * 2,
                                   height);
}

WelleyzEditor::PresetBrowser::PresetBrowser() {
    setComponentID("presetBrowser");
    search.setTextToShowWhenEmpty("Search presets...", welleyz::design::colours::kMuted);
    search.setComponentID("presetSearch");
    search.setName("Search presets");
    search.setColour(juce::TextEditor::backgroundColourId, welleyz::design::colours::kSurface);
    search.setColour(juce::TextEditor::textColourId, welleyz::design::colours::kText);
    search.setColour(juce::TextEditor::outlineColourId, welleyz::design::colours::kBorder);
    search.setColour(juce::TextEditor::focusedOutlineColourId, ThemeManager::instance().accent());
    search.setFont(welleyz::fonts::mono(15.0f));
    search.onTextChange = [this] { refreshList(); };
    addAndMakeVisible(search);
    viewport.setViewedComponent(&list, false);
    list.setComponentID("presetList");
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    for (const auto& name : {"All", "Likes", "Synth", "Pad", "Lead", "Bass", "Other"}) {
        auto chip = std::make_unique<juce::TextButton>(name);
        chip->setComponentID("presetFilter" + juce::String(name));
        chip->setClickingTogglesState(false);
        chip->onClick = [this, name] {
            categoryFilter = name;
            for (auto& button : chips)
                button->setToggleState(button->getButtonText() == name, juce::dontSendNotification);
            refreshList();
        };
        addAndMakeVisible(*chip);
        chips.push_back(std::move(chip));
    }
    chips.front()->setToggleState(true, juce::dontSendNotification);
}

void WelleyzEditor::PresetBrowser::setEntries(std::vector<Entry> values) {
    entries = std::move(values);
    refreshList();
}

void WelleyzEditor::PresetBrowser::refreshList() {
    visibleIndices.clear();
    for (int i = 0; i < (int)entries.size(); ++i) {
        const auto& e = entries[(size_t)i];
        if ((categoryFilter == "All" ||
             (categoryFilter == "Likes" && ThemeManager::instance().isFavourite(e.id)) ||
             e.category == categoryFilter) &&
            (search.getText().isEmpty() || e.name.containsIgnoreCase(search.getText())))
            visibleIndices.push_back(i);
    }
    list.focused = juce::jlimit(0, juce::jmax(0, (int)visibleIndices.size() - 1), list.focused);
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    list.setSize(
        juce::jmax(1, viewport.getWidth() - juce::roundToInt(9.0f * s)),
        juce::jmax(viewport.getHeight(), juce::roundToInt(37.0f * s) * (int)visibleIndices.size()));
    list.repaint();
}

void WelleyzEditor::PresetBrowser::List::paint(juce::Graphics& g) {
    const auto& t = *static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).theme;
    const float s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    const float rowH = 37.0f * s;
    g.setFont(welleyz::fonts::mono(16.0f * s));
    if (owner.visibleIndices.empty()) {
        g.setColour(welleyz::design::colours::kMuted);
        g.drawText(owner.categoryFilter == "Likes" ? "No liked presets yet" : "No presets found",
                   getLocalBounds(), juce::Justification::centredTop, false);
        if (hasKeyboardFocus(false)) {
            g.setColour(t.mAccent);
            g.drawRect(getLocalBounds().reduced(2), 2);
        }
    }
    for (int i = 0; i < (int)owner.visibleIndices.size(); ++i) {
        const auto& entry = owner.entries[(size_t)owner.visibleIndices[(size_t)i]];
        const float y = i * rowH;
        if (entry.id == owner.selectedId) {
            g.setColour(ThemeManager::instance().background());
            g.fillRoundedRectangle(1.0f * s, y, getWidth() - 2.0f * s, rowH - 2.0f * s, 4.0f * s);
            juce::Path selectedOutline;
            selectedOutline.addRoundedRectangle(1.0f * s, y, getWidth() - 2.0f * s, rowH - 2.0f * s,
                                                4.0f * s);
            g.setColour(t.mAccent.withAlpha(0.55f));
            g.strokePath(selectedOutline, juce::PathStrokeType(0.8f * s));
            g.setColour(t.mAccent);
            g.fillRect(0.0f, y, 2.0f * s, rowH - 2.0f * s);
        } else if (i == hovered) {
            g.setColour(welleyz::design::colours::kHover);
            g.fillRoundedRectangle(1.0f * s, y, getWidth() - 2.0f * s, rowH - 2.0f * s, 4.0f * s);
        }
        if (i == focused && hasKeyboardFocus(false)) {
            g.setColour(t.mAccent);
            g.drawRoundedRectangle(juce::Rectangle<float>(1.5f * s, y + 1.0f * s,
                                                          getWidth() - 3.0f * s, rowH - 3.0f * s),
                                   4.0f * s, 1.0f * s);
        }
        g.setColour(welleyz::design::colours::kMuted);
        g.drawText(juce::String(i + 1).paddedLeft('0', 3),
                   juce::Rectangle<float>(10.0f * s, y, 40.0f * s, rowH),
                   juce::Justification::centredLeft, false);
        g.setColour(entry.id == owner.selectedId ? welleyz::design::colours::kText
                                                 : welleyz::design::colours::kSecondaryText);
        g.drawText(entry.name, juce::Rectangle<float>(58.0f * s, y, getWidth() - 95.0f * s, rowH),
                   juce::Justification::centredLeft, true);
        const bool favourite = ThemeManager::instance().isFavourite(entry.id);
        const float heartSide = 18.0f * s;
        drawEmbeddedIcon(
            g, "like_preset_icon_svg",
            {getWidth() - 30.0f * s, y + (rowH - heartSide) * 0.5f, heartSide, heartSide},
            favourite ? t.mAccent : welleyz::design::colours::kSecondaryText);
        g.setColour(welleyz::design::colours::kBorder.withAlpha(0.65f));
        g.drawLine(8.0f * s, y + rowH, getWidth() - 8.0f * s, y + rowH, 1.0f * s);
    }
}

void WelleyzEditor::PresetBrowser::List::mouseUp(const juce::MouseEvent& e) {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    const int index = (int)(e.position.y / (37.0f * s));
    if (index < 0 || index >= (int)owner.visibleIndices.size())
        return;
    grabKeyboardFocus();
    focused = index;
    const auto& entry = owner.entries[(size_t)owner.visibleIndices[(size_t)index]];
    if (e.position.x > getWidth() - 43.0f * s)
        ThemeManager::instance().toggleFavourite(entry.id);
    else {
        owner.selectedId = entry.id;
        if (owner.onSelect)
            owner.onSelect(entry);
        repaint();
    }
}

bool WelleyzEditor::PresetBrowser::List::keyPressed(const juce::KeyPress& key) {
    if (owner.visibleIndices.empty())
        return false;
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::upKey) {
        focused = juce::jlimit(0, (int)owner.visibleIndices.size() - 1,
                               focused + (key == juce::KeyPress::downKey ? 1 : -1));
        const int y = juce::roundToInt(focused * 37.0f *
                                       static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale);
        owner.viewport.setViewPosition(
            owner.viewport.getViewPositionX(),
            juce::jlimit(0, juce::jmax(0, getHeight() - owner.viewport.getHeight()),
                         y - owner.viewport.getHeight() / 2));
        repaint();
        return true;
    }
    const auto& entry = owner.entries[(size_t)owner.visibleIndices[(size_t)focused]];
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) {
        owner.selectedId = entry.id;
        if (owner.onSelect)
            owner.onSelect(entry);
        repaint();
        return true;
    }
    if (key.getTextCharacter() == 'f') {
        ThemeManager::instance().toggleFavourite(entry.id);
        repaint();
        return true;
    }
    return false;
}

void WelleyzEditor::PresetBrowser::List::mouseMove(const juce::MouseEvent& e) {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    const int next = (int)(e.position.y / (37.0f * s));
    if (hovered != next) {
        hovered = next;
        repaint();
    }
}

void WelleyzEditor::PresetBrowser::paint(juce::Graphics& g) {
    const float s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    welleyz::ui::fillPanel(g, getLocalBounds().toFloat(),
                           *static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).theme,
                           welleyz::design::radius::kMedium * s);
    g.setColour(welleyz::design::colours::kText);
    g.setFont(welleyz::fonts::display(22.0f * s));
    g.drawText("PRESETS",
               juce::Rectangle<float>(18.0f * s, 12.0f * s, getWidth() - 36.0f * s, 28.0f * s),
               juce::Justification::centredLeft, false);
    g.setColour(welleyz::design::colours::kText);
    g.setFont(welleyz::fonts::mono(12.0f * s));
    g.drawText("03 / LIBRARY",
               juce::Rectangle<float>(getWidth() - 130.0f * s, 16.0f * s, 112.0f * s, 20.0f * s),
               juce::Justification::centredRight);
}

void WelleyzEditor::PresetBrowser::resized() {
    const float s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    auto S = [s](float v) { return juce::roundToInt(v * s); };
    search.setFont(welleyz::fonts::mono(15.0f * s));
    search.setBounds(S(16), S(52), getWidth() - S(32), S(37));
    const int gap = S(4),
              chipW = (getWidth() - S(30) - gap * ((int)chips.size() - 1)) / (int)chips.size();
    for (int i = 0; i < (int)chips.size(); ++i)
        chips[(size_t)i]->setBounds(S(15) + i * (chipW + gap), S(99), chipW, S(30));
    viewport.setBounds(S(15), S(139), getWidth() - S(30), getHeight() - S(155));
    refreshList();
}

void WelleyzEditor::PresetBrowser::mouseWheelMove(const juce::MouseEvent& e,
                                                  const juce::MouseWheelDetails& wheel) {
    viewport.mouseWheelMove(e, wheel);
}

namespace {
class AccentPicker final : public juce::Component {
  public:
    struct Swatch : juce::Button {
        juce::Colour colour;
        explicit Swatch(juce::Colour c) : juce::Button("Accent swatch"), colour(c) {
            setName("Accent " + colour.toDisplayString(false));
            setTooltip(getName());
        }
        void paintButton(juce::Graphics& g, bool, bool) override {
            auto r = getLocalBounds().toFloat().reduced(4.0f);
            g.setColour(colour.withAlpha(0.14f));
            g.fillEllipse(r.expanded(3.0f));
            g.setColour(colour);
            g.fillEllipse(r);
            if (ThemeManager::instance().accent() == colour) {
                g.setColour(welleyz::design::colours::kText);
                g.drawEllipse(r.expanded(2.0f), 1.5f);
            }
            if (hasKeyboardFocus(false)) {
                g.setColour(welleyz::design::colours::kText);
                g.drawEllipse(r.expanded(3.0f), 2.0f);
            }
        }
    };
    juce::Slider hue;
    std::vector<std::unique_ptr<Swatch>> swatches;
    AccentPicker() {
        hue.setName("Accent hue");
        hue.setSliderStyle(juce::Slider::LinearHorizontal);
        hue.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        hue.setRange(0.0, 1.0, 0.001);
        hue.setValue(ThemeManager::instance().accent().getHue(), juce::dontSendNotification);
        hue.onValueChange = [this] {
            ThemeManager::instance().setAccent(
                juce::Colour::fromHSV((float)hue.getValue(), 0.82f, 0.96f, 1.0f));
            repaint();
        };
        addAndMakeVisible(hue);
        for (auto colour :
             {juce::Colour(0xffffffff), welleyz::design::colours::kDefaultAccent,
              juce::Colour(0xfff542b9), juce::Colour(0xffba6bf0), juce::Colour(0xff8562f5),
              juce::Colour(0xff4996f5), juce::Colour(0xff4ad5e1)}) {
            auto swatch = std::make_unique<Swatch>(colour);
            swatch->onClick = [this, colour] {
                ThemeManager::instance().setAccent(colour);
                hue.setValue(colour.getHue(), juce::dontSendNotification);
                for (auto& button : swatches)
                    button->repaint();
            };
            addAndMakeVisible(*swatch);
            swatches.push_back(std::move(swatch));
        }
        setSize(295, 138);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(welleyz::design::colours::kSecondary);
        g.setColour(welleyz::design::colours::kText);
        g.setFont(welleyz::fonts::display(18.0f));
        g.drawText("THEME COLOR", 15, 11, getWidth() - 30, 22, juce::Justification::centredLeft);
    }
    void resized() override {
        hue.setBounds(14, 43, getWidth() - 28, 30);
        for (int i = 0; i < (int)swatches.size(); ++i)
            swatches[(size_t)i]->setBounds(11 + i * 40, 85, 37, 37);
    }
};

class RandomizeOptions final : public juce::Component {
  public:
    struct Option : juce::Button {
        Option(const juce::String& text) : juce::Button(text) { setClickingTogglesState(true); }
        void paintButton(juce::Graphics& g, bool hover, bool) override {
            const float s = 1.0f;
            const auto accent = ThemeManager::instance().accent();
            auto r = getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(hover ? welleyz::design::colours::kHover
                              : welleyz::design::colours::kSecondary);
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(welleyz::design::colours::kBorder);
            g.drawRoundedRectangle(r, 5.0f, 1.0f);
            auto box = juce::Rectangle<float>(12.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f);
            g.setColour(getToggleState() ? accent : welleyz::design::colours::kSurface);
            g.fillRoundedRectangle(box, 3.0f);
            g.setColour(getToggleState() ? accent : welleyz::design::colours::kBorderStrong);
            g.drawRoundedRectangle(box, 3.0f, 1.0f);
            if (getToggleState()) {
                g.setColour(welleyz::design::colours::kBackground);
                juce::Path check;
                check.startNewSubPath(15.0f, r.getCentreY());
                check.lineTo(18.0f, r.getCentreY() + 3.0f);
                check.lineTo(23.0f, r.getCentreY() - 3.0f);
                g.strokePath(check, juce::PathStrokeType(1.7f));
            }
            g.setColour(welleyz::design::colours::kText);
            g.setFont(welleyz::fonts::mono(15.0f));
            g.drawText(getButtonText(), 38, 0, getWidth() - 48, getHeight(),
                       juce::Justification::centredLeft);
            if (hasKeyboardFocus(false)) {
                g.setColour(accent);
                g.drawRoundedRectangle(r.reduced(2.0f), 5.0f, 2.0f);
            }
            juce::ignoreUnused(s);
        }
    } porta{"Porta"}, fx{"FX"}, arp{"Arp"}, voices{"Voices"}, noise{"Background noise"};

    explicit RandomizeOptions(WelleyzEditor& editor) {
        setSize(270, 253);
        addAndMakeVisible(porta);
        addAndMakeVisible(fx);
        addAndMakeVisible(arp);
        addAndMakeVisible(voices);
        addAndMakeVisible(noise);
        auto& prefs = ThemeManager::instance();
        porta.setToggleState(prefs.randomizePorta(), juce::dontSendNotification);
        fx.setToggleState(prefs.randomizeFx(), juce::dontSendNotification);
        arp.setToggleState(prefs.randomizeArp(), juce::dontSendNotification);
        voices.setToggleState(prefs.randomizeVoices(), juce::dontSendNotification);
        noise.setToggleState(prefs.randomizeNoise(), juce::dontSendNotification);
        auto update = [this] {
            ThemeManager::instance().setRandomizeGroups(
                porta.getToggleState(), fx.getToggleState(), arp.getToggleState(),
                voices.getToggleState(), noise.getToggleState());
        };
        porta.onClick = update;
        fx.onClick = update;
        arp.onClick = update;
        voices.onClick = update;
        noise.onClick = update;
        juce::ignoreUnused(editor);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(welleyz::design::colours::kSecondary);
        g.setColour(welleyz::design::colours::kText);
        g.setFont(welleyz::fonts::display(20.0f));
        g.drawText("RANDOMIZE OPTIONS", 14, 10, getWidth() - 28, 22,
                   juce::Justification::centredLeft);
        g.setColour(welleyz::design::colours::kMuted);
        g.setFont(welleyz::fonts::mono(12.0f));
        g.drawText("Selected groups may change on RANDOMIZE.", 14, 34, getWidth() - 28, 30,
                   juce::Justification::centredLeft, true);
        g.drawText("Host automation remains available for all parameters.", 14, 220,
                   getWidth() - 28, 24, juce::Justification::centredLeft, true);
    }
    void resized() override {
        porta.setBounds(12, 68, getWidth() - 24, 26);
        fx.setBounds(12, 99, getWidth() - 24, 26);
        arp.setBounds(12, 130, getWidth() - 24, 26);
        voices.setBounds(12, 161, getWidth() - 24, 26);
        noise.setBounds(12, 192, getWidth() - 24, 26);
    }
};
} // namespace

WelleyzEditor::TempoPanel::TempoPanel(WelleyzProcessor& processor) : mProcessor(processor) {
    setComponentID("tempoPanel");
    setName("Tempo and metronome");
    mBpmEditor.setComponentID("standaloneBpm");
    mBpmEditor.setName("Tempo in BPM");
    mBpmEditor.setJustification(juce::Justification::centred);
    mBpmEditor.setColour(juce::TextEditor::textColourId, welleyz::design::colours::kText);
    mBpmEditor.setColour(juce::TextEditor::backgroundColourId, welleyz::design::colours::kSurface);
    mBpmEditor.setColour(juce::TextEditor::outlineColourId,
                         welleyz::design::colours::kBorderStrong);
    mBpmEditor.setInputRestrictions(3, "0123456789");
    mBpmEditor.setText(juce::String(mProcessor.standaloneBpm()), false);
    mBpmEditor.onReturnKey = [this] {
        mProcessor.setStandaloneBpm(mBpmEditor.getText().getIntValue());
        mBpmEditor.setText(juce::String(mProcessor.standaloneBpm()), false);
        mBpmEditor.giveAwayKeyboardFocus();
    };
    mBpmEditor.onFocusLost = [this] {
        mProcessor.setStandaloneBpm(mBpmEditor.getText().getIntValue());
        mBpmEditor.setText(juce::String(mProcessor.standaloneBpm()), false);
    };
    addAndMakeVisible(mBpmEditor);
    mToggleButton.setComponentID("metronomeToggle");
    mToggleButton.setClickingTogglesState(true);
    mToggleButton.setToggleState(mProcessor.metronomeEnabled(), juce::dontSendNotification);
    mToggleButton.setButtonText(mProcessor.metronomeEnabled() ? "ON" : "OFF");
    mToggleButton.setName("Metronome on or off");
    mToggleButton.onClick = [this] {
        const bool enabled = mToggleButton.getToggleState();
        mProcessor.setMetronomeEnabled(enabled);
        mToggleButton.setButtonText(enabled ? "ON" : "OFF");
        if (onMetronomeChange)
            onMetronomeChange(enabled);
    };
    addAndMakeVisible(mToggleButton);
    mCloseButton.setName("Close tempo settings");
    mCloseButton.onClick = [this] { setVisible(false); };
    addAndMakeVisible(mCloseButton);
}

void WelleyzEditor::TempoPanel::paint(juce::Graphics& g) {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    welleyz::ui::fillPanel(g, getLocalBounds().toFloat(), makeWaveTheme(ThemeManager::instance()),
                           10.0f * s);
    g.setColour(ThemeManager::instance().accent());
    g.setFont(welleyz::fonts::display(19.0f * s));
    g.drawText("TEMPO / METRONOME", juce::roundToInt(17.0f * s), juce::roundToInt(12.0f * s),
               juce::roundToInt(170.0f * s), juce::roundToInt(28.0f * s),
               juce::Justification::centredLeft);
    g.setColour(welleyz::design::colours::kSecondaryText);
    g.setFont(welleyz::fonts::mono(11.0f * s));
    g.drawText("BPM", juce::roundToInt(20.0f * s), juce::roundToInt(47.0f * s),
               juce::roundToInt(90.0f * s), juce::roundToInt(18.0f * s),
               juce::Justification::centred);
    g.drawText("CLICK", juce::roundToInt(130.0f * s), juce::roundToInt(47.0f * s),
               juce::roundToInt(90.0f * s), juce::roundToInt(18.0f * s),
               juce::Justification::centred);
}

void WelleyzEditor::TempoPanel::resized() {
    const auto s = static_cast<WelleyzLookAndFeel&>(getLookAndFeel()).uiScale;
    auto S = [s](float v) { return juce::roundToInt(v * s); };
    mBpmEditor.setFont(welleyz::fonts::mono(17.0f * s));
    mBpmEditor.setBounds(S(20.0f), S(69.0f), S(90.0f), S(39.0f));
    mToggleButton.setBounds(S(130.0f), S(69.0f), S(90.0f), S(39.0f));
    mCloseButton.setBounds(S(197.0f), S(11.0f), S(27.0f), S(27.0f));
}

namespace {
struct ManualSheet : juce::Component {
    juce::TextEditor mText;
    juce::TextButton mClose{"CLOSE"};
    ManualSheet() {
        setName("Manual");
        setComponentID("manualPanel");
        setWantsKeyboardFocus(true);
        mText.setMultiLine(true);
        mText.setReadOnly(true);
        mText.setScrollbarsShown(true);
        mText.setColour(juce::TextEditor::backgroundColourId, welleyz::design::colours::kSurface);
        mText.setColour(juce::TextEditor::textColourId, welleyz::design::colours::kText);
        int size = 0;
        if (const auto* data = WelleyzAssets::getNamedResource("Manual_md", size)) {
            auto body = juce::String::fromUTF8(data, size);
            body = body.replace("\n## ", "\n\n").replace("\n# ", "\n");
            if (body.startsWith("# "))
                body = body.substring(2);
            mText.setText(body, false);
        }
        addAndMakeVisible(mText);
        addAndMakeVisible(mClose);
        mClose.onClick = [this] { close(); };
    }
    void close() {
        setVisible(false);
        if (auto* parent = getParentComponent())
            parent->grabKeyboardFocus();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey)
            close();
        return true;
    }
    void paint(juce::Graphics& g) override {
        const auto s = (float)getWidth() / kEditorDesignWidth;
        g.fillAll(juce::Colours::black.withAlpha(0.80f));
        welleyz::ui::fillPanel(g, {290.0f * s, 112.0f * s, 700.0f * s, 540.0f * s},
                               makeWaveTheme(ThemeManager::instance()), 12.0f * s);
        g.setColour(ThemeManager::instance().accent());
        g.setFont(welleyz::fonts::display(25.0f * s));
        g.drawText("MANUAL", juce::roundToInt(312.0f * s), juce::roundToInt(130.0f * s),
                   juce::roundToInt(400.0f * s), juce::roundToInt(45.0f * s),
                   juce::Justification::centredLeft);
    }
    void resized() override {
        const auto s = (float)getWidth() / kEditorDesignWidth;
        auto S = [s](float v) { return juce::roundToInt(v * s); };
        mText.applyFontToAllText(welleyz::fonts::mono(16.0f * s));
        mText.setBounds(S(312), S(183), S(656), S(445));
        mClose.setBounds(S(871), S(133), S(96), S(42));
    }
};
} // namespace

void WelleyzEditor::showAccentSettings() {
    juce::CallOutBox::launchAsynchronously(
        std::make_unique<RandomizeOptions>(*this),
        getLocalArea(&mSettingsButton, mSettingsButton.getLocalBounds()), this);
}

void WelleyzEditor::showColors() {
    const auto anchor = getLocalArea(&mColorsButton, mColorsButton.getLocalBounds());
    auto& callout =
        juce::CallOutBox::launchAsynchronously(std::make_unique<AccentPicker>(), anchor, this);
    // Keep the picker below its button so the bubble arrow points upward.
    callout.updatePosition(anchor, getLocalBounds().withTrimmedTop(anchor.getBottom()));
}

void WelleyzEditor::showManual() {
    if (mManualOverlay == nullptr) {
        mManualOverlay = std::make_unique<ManualSheet>();
        addChildComponent(mManualOverlay.get());
    }
    mManualOverlay->setBounds(getLocalBounds());
    mManualOverlay->setVisible(true);
    mManualOverlay->toFront(true);
}

void WelleyzEditor::finishRecording() {
    const auto temporary = mProcessor.stopRecording();
    mRecordButton.setButtonText("REC");
    mRecordButton.setToggleState(false, juce::dontSendNotification);
    mRecordButton.setTooltip("Record audio to WAV");
    mLookAndFeel.mRecordBlinkBright = true;
    mRecordButton.repaint();
    if (!temporary.existsAsFile())
        return;
    if (mProcessor.recordingFailed()) {
        temporary.deleteFile();
        juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                         juce::MessageBoxIconType::WarningIcon, "Recording stopped",
                                         "The audio buffer filled up. Try again.", "OK", this),
                                     nullptr);
        return;
    }
    auto safe = juce::Component::SafePointer<WelleyzEditor>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Recording complete")
            .withMessage("Save the recording as a WAV file?")
            .withButton("Save WAV")
            .withButton("Discard")
            .withAssociatedComponent(this),
        [safe, temporary](int result) {
            if (result != 1 || safe == nullptr) {
                temporary.deleteFile();
                return;
            }
            auto folder = WelleyzProcessor::recordingDirectory();
            folder.createDirectory();
            safe->mFileChooser = std::make_unique<juce::FileChooser>(
                "Save WAV recording", folder.getChildFile("welleyz recording.wav"), "*.wav");
            safe->mFileChooser->launchAsync(
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                    juce::FileBrowserComponent::warnAboutOverwriting,
                [temporary, safe](const juce::FileChooser& chooser) {
                    auto target = chooser.getResult();
                    bool saved = true;
                    if (target != juce::File()) {
                        if (target.getFileExtension().isEmpty())
                            target = target.withFileExtension(".wav");
                        saved = temporary.moveFileTo(target);
                    }
                    temporary.deleteFile();
                    if (!saved && safe != nullptr)
                        juce::AlertWindow::showAsync(
                            juce::MessageBoxOptions::makeOptionsOk(
                                juce::MessageBoxIconType::WarningIcon, "WAV not saved",
                                "Could not write " + target.getFileName() + ".", "OK",
                                safe.getComponent()),
                            nullptr);
                });
        });
}

// ================= Editor =================
WelleyzEditor::WelleyzEditor(WelleyzProcessor& p)
    : AudioProcessorEditor(p), mProcessor(p), mKeyboard(p.getKeyboardState()) {
    setLookAndFeel(&mLookAndFeel);
    ThemeManager::instance().addChangeListener(this);

    mArpLabel.setText("ARP MODE", juce::dontSendNotification);
    mArpLabel.setJustificationType(juce::Justification::centred);
    mPortaLabel.setText("PORTA MODE", juce::dontSendNotification);
    mPortaLabel.setJustificationType(juce::Justification::centred);
    for (auto* tab : {&mRandomTab, &mOscTab, &mFxTab})
        tab->setClickingTogglesState(false);
    mRandomTab.setComponentID("randomTab");
    mOscTab.setComponentID("synthTab");
    mFxTab.setComponentID("fxRackTab");
    mRandomTab.onClick = [this] { showPage(0); };
    mOscTab.onClick = [this] { showPage(1); };
    mFxTab.onClick = [this] { showPage(2); };

    mArpMode.addItemList(juce::StringArray{"Off", "Up", "Down", "UpDown"}, 1);
    mPortaMode.addItemList(juce::StringArray{"Off", "Always", "Legato"}, 1);
    mBrowser.onSelect = [this](const PresetBrowser::Entry& entry) { loadBrowserPreset(entry); };
    mSoundTypes.onSelect = [this](int category) { mProcessor.selectSoundType(category); };
    mSoundTypes.choose(mProcessor.selectedSoundType());
    refreshPresets();

    mZoomButton.onClick = [this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&mLookAndFeel);
        menu.addItem(1, "50%", true, mProcessor.mUiZoom < 0.625f);
        menu.addItem(2, "75%", true, mProcessor.mUiZoom >= 0.625f && mProcessor.mUiZoom < 0.875f);
        menu.addItem(3, "100%", true, mProcessor.mUiZoom >= 0.875f && mProcessor.mUiZoom < 1.125f);
        menu.addItem(4, "125%", true, mProcessor.mUiZoom >= 1.125f && mProcessor.mUiZoom < 1.375f);
        menu.addItem(5, "150%", true, mProcessor.mUiZoom >= 1.375f && mProcessor.mUiZoom < 1.75f);
        menu.addItem(8, "200%", true, mProcessor.mUiZoom >= 1.75f);
        menu.addSeparator();
        menu.addItem(6, "Refresh presets");
        menu.addItem(7, "Check for updates");
        auto safe = juce::Component::SafePointer<WelleyzEditor>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&mZoomButton),
                           [safe](int id) {
                               if (safe == nullptr)
                                   return;
                               if ((id >= 1 && id <= 5) || id == 8) {
                                   safe->mProcessor.mUiZoomCustomized = true;
                                   const float selectedZoom = id == 1   ? 0.5f
                                                              : id == 2 ? 0.75f
                                                              : id == 3 ? 1.0f
                                                              : id == 4 ? 1.25f
                                                              : id == 5 ? 1.5f
                                                                        : 2.0f;
                                   ThemeManager::instance().saveZoom(selectedZoom);
                                   safe->setZoom(selectedZoom);
                               } else if (id == 6)
                                   safe->refreshPresets();
                               else if (id == 7)
                                   WelleyzUpdateChecker::check(safe.getComponent(), true);
                           });
    };
    mSettingsButton.onClick = [this] { showAccentSettings(); };
    mColorsButton.onClick = [this] { showColors(); };
    mSettingsButton.setComponentID("settingsButton");
    mColorsButton.setComponentID("colorsButton");
    mZoomButton.setComponentID("zoomButton");
    mManualButton.setComponentID("manualButton");
    mManualButton.setTooltip("Open manual");
    mManualButton.onClick = [this] { showManual(); };
    mRecordButton.setComponentID("recordButton");
    mRecordButton.setTooltip("Record audio to WAV");
    if (mProcessor.isRecording()) {
        mRecordButton.setButtonText("STOP");
        mRecordButton.setToggleState(true, juce::dontSendNotification);
        mRecordButton.setTooltip("Stop recording");
    }
    mRecordButton.onClick = [this] {
        if (mProcessor.isRecording())
            finishRecording();
        else if (mProcessor.startRecording()) {
            mRecordButton.setButtonText("STOP");
            mRecordButton.setToggleState(true, juce::dontSendNotification);
            mRecordButton.setTooltip("Stop recording");
        } else
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions::makeOptionsOk(
                    juce::MessageBoxIconType::WarningIcon, "Recording unavailable",
                    "Check the audio device and available disk space, then try again.", "OK", this),
                nullptr);
    };
    mMetronomeButton.setComponentID("metronomeButton");
    mMetronomeButton.setClickingTogglesState(false);
    mMetronomeButton.setToggleState(mProcessor.metronomeEnabled(), juce::dontSendNotification);
    mMetronomeButton.setTooltip("Tempo and metronome");
    mMetronomeButton.onClick = [this] {
        const bool opening = !mTempoPanel.isVisible();
        mTempoPanel.setVisible(opening);
        if (opening)
            mTempoPanel.toFront(true);
    };
    mTempoPanel.onMetronomeChange = [this](bool enabled) {
        mMetronomeButton.setToggleState(enabled, juce::dontSendNotification);
    };
    mSettingsButton.setName("Randomize options");
    mColorsButton.setName("Choose accent color");
    mZoomButton.setName("Zoom and updates");
    mSettingsButton.setTooltip(mSettingsButton.getName());
    mColorsButton.setTooltip(mColorsButton.getName());
    mZoomButton.setTooltip(mZoomButton.getName());

    // In hosts such as FL Studio, letting the button keep keyboard focus can
    // interrupt computer-keyboard MIDI until the editor is clicked again.
    mRandomButton.setMouseClickGrabsKeyboardFocus(false);
    mRandomButton.onClick = [this] {
        mRandomizeStartMs = juce::Time::getMillisecondCounterHiRes();
        mPendingFocusRestores = 2;
        mLookAndFeel.randomizeRotation = 0.0f;
        startTimerHz(60);
        mProcessor.randomizeParameters(mSoundTypes.selected);
        mProcessor.mUiPreset.clear();
        mBrowser.selectedId.clear();
        mBrowser.list.repaint();
        refreshBackButton();
        if (auto* peer = getPeer())
            peer->grabFocus();
        grabKeyboardFocus();
    };
    mRandomButton.setComponentID("randomSoundButton");
    mBackButton.onClick = [this] {
        if (mProcessor.restorePreviousSound()) {
            mSoundTypes.choose(mProcessor.selectedSoundType());
            refreshPresets();
            refreshBackButton();
        }
    };
    mBackButton.setComponentID("backPresetButton");
    mResetButton.onClick = [this] {
        mProcessor.resetToDefault();
        mProcessor.mUiPreset.clear();
        mBrowser.selectedId.clear();
        mBrowser.list.repaint();
        refreshBackButton();
    };
    mSaveButton.onClick = [&] {
        auto dir = WelleyzProcessor::presetDirectory();
        dir.createDirectory();
        mFileChooser = std::make_unique<juce::FileChooser>(
            "Save welleyz preset", dir.getChildFile("MyPreset.wlyz"), "*.wlyz");
        mFileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File()) {
                    if (f.getFileExtension().isEmpty())
                        f = f.withFileExtension(".wlyz");
                    if (!mProcessor.savePresetToFile(f)) {
                        juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                                         juce::MessageBoxIconType::WarningIcon,
                                                         "Preset not saved",
                                                         "Could not write " + f.getFileName() +
                                                             ". Check the folder permissions.",
                                                         "OK", this),
                                                     nullptr);
                        return;
                    }
                    refreshPresets();
                    for (int i = 0; i < mPresetFiles.size(); ++i)
                        if (mPresetFiles.getReference(i) == f) {
                            mProcessor.mUiPreset = "user:" + f.getFileName();
                            mBrowser.selectedId = mProcessor.mUiPreset;
                            mBrowser.list.repaint();
                        }
                }
            });
    };
    mLoadButton.onClick = [&] {
        auto dir = WelleyzProcessor::presetDirectory();
        mFileChooser = std::make_unique<juce::FileChooser>("Load welleyz preset", dir, "*.wlyz");
        mFileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f.existsAsFile()) {
                    if (mProcessor.loadPresetFromFile(f)) {
                        refreshBackButton();
                        refreshPresets();
                        for (int i = 0; i < mPresetFiles.size(); ++i)
                            if (mPresetFiles.getReference(i) == f) {
                                mProcessor.mUiPreset = "user:" + f.getFileName();
                                mBrowser.selectedId = mProcessor.mUiPreset;
                                mBrowser.list.repaint();
                            }
                        refreshAccent();
                        setZoom(mProcessor.mUiZoom);
                    } else {
                        juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                                         juce::MessageBoxIconType::WarningIcon,
                                                         "Preset not loaded",
                                                         "Could not read " + f.getFileName() +
                                                             ". Select a valid .wlyz file.",
                                                         "OK", this),
                                                     nullptr);
                    }
                }
            });
    };

    addAndMakeVisible(mRandomTab);
    addAndMakeVisible(mOscTab);
    addAndMakeVisible(mFxTab);
    addAndMakeVisible(mManualButton);
    if (mProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
        addAndMakeVisible(mRecordButton);
        addAndMakeVisible(mMetronomeButton);
        addChildComponent(mTempoPanel);
    }
    addAndMakeVisible(mRandomButton);
    addAndMakeVisible(mBackButton);
    addAndMakeVisible(mResetButton);
    addAndMakeVisible(mSaveButton);
    addAndMakeVisible(mLoadButton);
    addAndMakeVisible(mZoomButton);
    addAndMakeVisible(mSettingsButton);
    addAndMakeVisible(mColorsButton);
    addAndMakeVisible(mSoundTypes);
    addAndMakeVisible(mBrowser);
    for (auto* slider : {&mPitchSlider, &mModSlider}) {
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setRange(0.0, 1.0, 0.001);
        addAndMakeVisible(*slider);
    }
    mPitchSlider.setValue(0.5, juce::dontSendNotification);
    mPitchSlider.onValueChange = [this] {
        mProcessor.setUiPitchBend((int)std::round(mPitchSlider.getValue() * 16383.0));
    };
    mPitchSlider.onDragEnd = [this] { mPitchSlider.setValue(0.5); };
    mModSlider.onValueChange = [this] {
        mProcessor.setUiModWheel((int)std::round(mModSlider.getValue() * 127.0));
    };
    mMasterVolume.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mMasterVolume.setComponentID("masterLevel");
    mMasterVolume.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(mMasterVolume);
    addAndMakeVisible(mOscillator1);
    addAndMakeVisible(mOscillator2);
    addAndMakeVisible(mOscillator3);
    addAndMakeVisible(mEnvelopeSection);
    addAndMakeVisible(mFilterSection);
    addAndMakeVisible(mTubeSection);
    addAndMakeVisible(mMasterSection);
    addAndMakeVisible(mNoisePower);
    addAndMakeVisible(mNoiseAmount);
    addAndMakeVisible(mNoiseType);
    addAndMakeVisible(mNoiseWave);
    mNoisePower.setComponentID("noiseOnPower");
    mNoiseType.setComponentID("noiseTypeSelector");
    mNoiseAmount.setComponentID("noiseAmountKnob");
    juce::StringArray noiseNames;
    for (auto* name : welleyz::backgroundNoise::kNames)
        noiseNames.add(name);
    mNoiseType.addItemList(noiseNames, 1);
    mNoiseType.onChange = [this] { mNoiseWave.repaint(); };
    mNoisePower.onStateChange = [this] { mNoiseWave.repaint(); };
    for (auto* module : {&mFxFilter, &mFxHaze, &mFxTaps, &mFxSpace})
        addAndMakeVisible(*module);
    mEnvelopeSection.addAndMakeVisible(mAttackKnob);
    mEnvelopeSection.addAndMakeVisible(mDecayKnob);
    mEnvelopeSection.addAndMakeVisible(mSustainKnob);
    mEnvelopeSection.addAndMakeVisible(mReleaseKnob);
    mFilterSection.addAndMakeVisible(mCutoffKnob);
    mFilterSection.addAndMakeVisible(mResoKnob);
    mTubeSection.addAndMakeVisible(mLampinessKnob);
    mTubeSection.addAndMakeVisible(mBitsKnob);
    mTubeSection.addAndMakeVisible(mRateKnob);
    mTubeSection.addAndMakeVisible(mVibRateKnob);
    mTubeSection.addAndMakeVisible(mVibDepthKnob);
    mMasterSection.addAndMakeVisible(mMasterKnob);
    mMasterSection.addAndMakeVisible(mArpRateKnob);
    mMasterSection.addAndMakeVisible(mGlideKnob);
    mMasterSection.addAndMakeVisible(mArpMode);
    mMasterSection.addAndMakeVisible(mArpLabel);
    mMasterSection.addAndMakeVisible(mPortaMode);
    mMasterSection.addAndMakeVisible(mPortaLabel);
    addAndMakeVisible(mKeyboard);
    mKeyboard.setComponentID("pianoKeyboard");
    mKeyboard.setKeyPressBaseOctave(6); // A..; starts at C5 on a computer keyboard.
    mKeyboard.setOctaveForMiddleC(4);
    mKeyboard.setAvailableRange(36, 127);
    mKeyboard.setScrollButtonsVisible(true); // JUCE forces C2 when scrolling is disabled.

    attachAll();
    mMasterVolumeAttachment =
        std::make_unique<SliderA>(mProcessor.getApvts(), "masterLevel", mMasterVolume);
    refreshBackButton();
    refreshAccent();
    showPage(mProcessor.mUiPage);

    setWantsKeyboardFocus(true);
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double)kEditorDesignWidth / (double)kEditorDesignHeight);
    setResizeLimits(640, 380, 2560, 1520);
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForPoint(juce::Desktop::getMousePosition());
    const auto workArea =
        display != nullptr ? display->userArea : displays.getMainDisplay().userArea;
    const auto& prefs = ThemeManager::instance();
    const bool hasPreferredZoom = mProcessor.mUiZoomCustomized || prefs.hasSavedZoom();
    const float preferredZoom =
        mProcessor.mUiZoomCustomized ? mProcessor.mUiZoom : prefs.savedZoom();
    setZoom(initialZoomForArea(workArea, preferredZoom, hasPreferredZoom));
    WelleyzUpdateChecker::check(this, false);
    startTimerHz(30);
}

WelleyzEditor::~WelleyzEditor() {
    if (mManualOverlay != nullptr) {
        removeChildComponent(mManualOverlay.get());
        mManualOverlay.reset();
    }
    mTempoPanel.onMetronomeChange = nullptr;
    mTempoPanel.mBpmEditor.onReturnKey = nullptr;
    mTempoPanel.mBpmEditor.onFocusLost = nullptr;
    stopTimer();
    ThemeManager::instance().removeChangeListener(this);
    mMasterVolumeAttachment.reset();
    mButtonAttachments.clear();
    mComboAttachments.clear();
    mSliderAttachments.clear();
    setLookAndFeel(nullptr);
}

void WelleyzEditor::refreshPresets() {
    const auto selectedPreset = mProcessor.mUiPreset;
    mFactoryPresets.clear();
    const juce::StringArray bundledNames{"1bass",    "1bell",    "1lead",       "1pad",
                                         "2pad",     "arpsynth", "detunedkeys", "foggypad",
                                         "noisypad", "organ"};
    for (int i = 0; i < WelleyzAssets::namedResourceListSize; ++i) {
        const auto name =
            juce::File(juce::String::fromUTF8(WelleyzAssets::originalFilenames[i])).getFileName();
        if (bundledNames.contains(juce::File(name).getFileNameWithoutExtension()) &&
            name.endsWith(".wlyz"))
            mFactoryPresets.push_back(
                {name, juce::String::fromUTF8(WelleyzAssets::namedResourceList[i])});
    }
    std::sort(mFactoryPresets.begin(), mFactoryPresets.end(),
              [](const auto& a, const auto& b) { return a.name.compareIgnoreCase(b.name) < 0; });
    mPresetFiles.clear();
    auto dir = WelleyzProcessor::presetDirectory();
    dir.findChildFiles(mPresetFiles, juce::File::findFiles, false, "*.wlyz");
    mPresetFiles.sort();
    std::vector<PresetBrowser::Entry> entries;
    auto categoryFor = [](juce::String name) {
        name = name.toLowerCase();
        if (name.contains("bass"))
            return juce::String("Bass");
        if (name.contains("pad"))
            return juce::String("Pad");
        if (name.contains("lead"))
            return juce::String("Lead");
        if (name.contains("synth") || name.contains("arp"))
            return juce::String("Synth");
        return juce::String("Other");
    };
    for (const auto& entry : mFactoryPresets) {
        const juce::StringArray actualNames{"1bass",    "1bell",    "1lead",       "1pad",
                                            "2pad",     "arpsynth", "detunedkeys", "foggypad",
                                            "noisypad", "organ"};
        const juce::StringArray displayNames{"01 Bass",   "01 Bell",   "01 Lead",      "01 Pad",
                                             "02 Pad",    "Arp Synth", "Detuned Keys", "Foggy Pad",
                                             "Noise Pad", "Organ"};
        const auto baseName = juce::File(entry.name).getFileNameWithoutExtension();
        const int index = actualNames.indexOf(baseName);
        entries.push_back({"factory:" + baseName,
                           index >= 0 ? displayNames[index] : baseName,
                           entry.resource,
                           categoryFor(baseName),
                           {}});
    }
    for (int i = 0; i < mPresetFiles.size(); ++i) {
        const auto& file = mPresetFiles.getReference(i);
        entries.push_back({"user:" + file.getFileName(),
                           file.getFileNameWithoutExtension(),
                           {},
                           categoryFor(file.getFileNameWithoutExtension()),
                           file});
    }
    mBrowser.setEntries(std::move(entries));
    mBrowser.selectedId = selectedPreset;
    mBrowser.list.repaint();
}

void WelleyzEditor::loadBrowserPreset(const PresetBrowser::Entry& entry) {
    bool loaded = false;
    if (entry.file.existsAsFile())
        loaded = mProcessor.loadPresetFromFile(entry.file);
    else {
        int size = 0;
        if (const auto* data = WelleyzAssets::getNamedResource(entry.resource.toRawUTF8(), size))
            loaded = mProcessor.loadPresetFromData(data, (size_t)size);
    }
    if (loaded) {
        mProcessor.mUiPreset = entry.id;
        refreshBackButton();
        mBrowser.selectedId = entry.id;
        mBrowser.list.repaint();
    }
}

void WelleyzEditor::refreshBackButton() {
    mBackButton.setEnabled(mProcessor.canRestorePreviousSound());
}

float WelleyzEditor::initialZoomForArea(juce::Rectangle<int> workArea, float savedZoom,
                                        bool customized) {
    const float preferred = workArea.getWidth() < 1500 || workArea.getHeight() < 850 ? 0.75f : 1.0f;
    const float desired = customized ? savedZoom : preferred;
    const float fit = juce::jmin((workArea.getWidth() - 80.0f) / (float)kEditorDesignWidth,
                                 (workArea.getHeight() - 80.0f) / (float)kEditorDesignHeight);
    return juce::jlimit(0.5f, 2.0f, juce::jmin(desired, fit));
}

bool WelleyzEditor::keyPressed(const juce::KeyPress& key) {
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*>(focused) != nullptr)
            return false;
    if (key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown() ||
        key.getModifiers().isCommandDown())
        return false;
    return mKeyboard.handlesComputerKey(key);
}

bool WelleyzEditor::keyStateChanged(bool isKeyDown) {
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*>(focused) != nullptr)
            return false;
    return mKeyboard.handleComputerKeyState(isKeyDown);
}

void WelleyzEditor::focusOfChildComponentChanged(FocusChangeType) {
    if (!hasKeyboardFocus(true) ||
        dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        mKeyboard.releaseComputerKeys();
}

void WelleyzEditor::focusLost(FocusChangeType) {
    if (!hasKeyboardFocus(true))
        mKeyboard.releaseComputerKeys();
}

void WelleyzEditor::attachAll() {
    auto& a = mProcessor.getApvts();
    auto reg = [&](Knob& k, const juce::String& id) {
        mSliderAttachments.push_back(std::make_unique<SliderA>(a, id, k.slider));
        k.slider.setComponentID(id);
        if (auto* parameter = a.getParameter(id))
            k.slider.setDoubleClickReturnValue(
                true, parameter->convertFrom0to1(parameter->getDefaultValue()));
        const bool standalone =
            mProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        k.slider.textFromValueFunction = [id, standalone](double value) {
            if (standalone && id == "effectsTapsTime") {
                const auto division = std::pow(2.0, std::round(std::log2(value / 0.64)));
                return division >= 1.0 ? juce::String(juce::roundToInt(division)) +
                                             (division == 1.0 ? " beat" : " beats")
                                       : "1/" + juce::String(juce::roundToInt(1.0 / division));
            }
            if (id == "filterCutoff" || id == "effectsFilterFrequency")
                return value >= 1000.0 ? juce::String(value / 1000.0, 1) + " kHz"
                                       : juce::String(juce::roundToInt(value)) + " Hz";
            if (id == "amplitudeAttack" || id == "amplitudeDecay" || id == "amplitudeRelease" ||
                id == "portamentoTime")
                return juce::String(value, 3) + " s";
            if (id == "vibratoRate" || id == "arpeggiatorRate" || id == "effectsHazeRate")
                return juce::String(value, 1) + " Hz";
            if (id == "lofiBitDepth")
                return juce::String(juce::roundToInt(value)) + "-bit";
            if (id == "lofiRateReduction")
                return value <= 1.5 ? juce::String("Off")
                                    : "1/" + juce::String(juce::roundToInt(value));
            if (id == "vibratoDepth" || id.endsWith("Detune") || id.endsWith("FineTune"))
                return juce::String(value, 1) + " ct";
            if (id.endsWith("Octave") || id.endsWith("Semitone"))
                return juce::String(juce::roundToInt(value));
            return juce::String(value, 2);
        };
        k.slider.updateText();
        mSliders.push_back(&k.slider);
    };
    OscStrip* strips[3] = {&mOscillator1, &mOscillator2, &mOscillator3};
    for (int i = 0; i < 3; ++i) {
        juce::String n = "osc" + juce::String(i + 1);
        strips[i]->setComponentID(n + "Strip");
        strips[i]->wave.setComponentID(n + "WaveSelector");
        strips[i]->oct->setComponentID(n + "OctKnob");
        strips[i]->power.setComponentID(n + "Power");
        strips[i]->power.setName(n.toUpperCase() + " power");
        strips[i]->power.onStateChange = [strip = strips[i]] { strip->updatePowerState(); };
        const auto param = "oscillator" + juce::String(i + 1);
        mButtonAttachments.push_back(
            std::make_unique<ButtonA>(a, param + "Enabled", strips[i]->power));
        mComboAttachments.push_back(std::make_unique<ComboA>(a, param + "Wave", strips[i]->wave));
        mComboAttachments.push_back(
            std::make_unique<ComboA>(a, param + "Voices", strips[i]->voices));
        reg(*strips[i]->oct, param + "Octave");
        reg(*strips[i]->semi, param + "Semitone");
        reg(*strips[i]->fine, param + "FineTune");
        reg(*strips[i]->level, param + "Level");
        reg(*strips[i]->pan, param + "Pan");
        reg(*strips[i]->detune, param + "Detune");
    }
    reg(mAttackKnob, "amplitudeAttack");
    reg(mDecayKnob, "amplitudeDecay");
    reg(mSustainKnob, "amplitudeSustain");
    reg(mReleaseKnob, "amplitudeRelease");
    reg(mCutoffKnob, "filterCutoff");
    reg(mResoKnob, "filterResonance");
    mEnvelopeSection.graphControls = {&mAttackKnob.slider, &mDecayKnob.slider, &mSustainKnob.slider,
                                      &mReleaseKnob.slider};
    mFilterSection.graphControls = {&mCutoffKnob.slider, &mResoKnob.slider, nullptr, nullptr};
    for (auto* slider : mEnvelopeSection.graphControls)
        slider->onValueChange = [this] { mEnvelopeSection.repaint(); };
    for (int i = 0; i < 2; ++i)
        mFilterSection.graphControls[(size_t)i]->onValueChange = [this] {
            mFilterSection.repaint();
        };
    reg(mLampinessKnob, "tubeLampiness");
    reg(mBitsKnob, "lofiBitDepth");
    reg(mRateKnob, "lofiRateReduction");
    reg(mVibRateKnob, "vibratoRate");
    reg(mVibDepthKnob, "vibratoDepth");
    reg(mNoiseAmount, "backgroundNoiseLevel");
    mButtonAttachments.push_back(
        std::make_unique<ButtonA>(a, "backgroundNoiseEnabled", mNoisePower));
    mComboAttachments.push_back(std::make_unique<ComboA>(a, "backgroundNoiseType", mNoiseType));
    reg(mMasterKnob, "masterLevel");
    reg(mArpRateKnob, "arpeggiatorRate");
    reg(mGlideKnob, "portamentoTime");
    mMasterSection.setComponentID("masterArpSection");
    mArpMode.setComponentID("arpModeSelector");
    mPortaMode.setComponentID("portaModeSelector");
    for (auto entry : {std::pair<Section*, const char*>{&mEnvelopeSection, "amplitudeEnabled"},
                       {&mFilterSection, "filterEnabled"},
                       {&mTubeSection, "tubeEnabled"}}) {
        entry.first->power.setComponentID(juce::String(entry.second) + "Power");
        entry.first->power.setName(entry.first->title + " power");
        entry.first->power.onStateChange = [section = entry.first] { section->updatePowerState(); };
        mButtonAttachments.push_back(
            std::make_unique<ButtonA>(a, entry.second, entry.first->power));
    }
    for (int i = 0; i < 3; ++i) {
        const auto id = "oscillator" + juce::String(i + 1) + "Enabled";
        strips[i]->power.setToggleState(a.getRawParameterValue(id)->load() > 0.5f,
                                        juce::dontSendNotification);
        strips[i]->updatePowerState();
    }
    for (auto entry : {std::pair<Section*, const char*>{&mEnvelopeSection, "amplitudeEnabled"},
                       {&mFilterSection, "filterEnabled"},
                       {&mTubeSection, "tubeEnabled"}}) {
        entry.first->power.setToggleState(a.getRawParameterValue(entry.second)->load() > 0.5f,
                                          juce::dontSendNotification);
        entry.first->updatePowerState();
    }
    mComboAttachments.push_back(std::make_unique<ComboA>(a, "arpeggiatorMode", mArpMode));
    mComboAttachments.push_back(std::make_unique<ComboA>(a, "portamentoMode", mPortaMode));
    for (auto entry : {std::pair<FxModule*, const char*>{&mFxFilter, "effectsFilterEnabled"},
                       {&mFxHaze, "effectsHazeEnabled"},
                       {&mFxTaps, "effectsTapsEnabled"},
                       {&mFxSpace, "effectsSpaceEnabled"}})
        mButtonAttachments.push_back(
            std::make_unique<ButtonA>(a, entry.second, entry.first->power));
    mComboAttachments.push_back(std::make_unique<ComboA>(a, "effectsFilterMode", mFxFilter.mode));
    auto fxKnobs = [&](FxModule& module, const char* first, const char* second, const char* third) {
        reg(module.first, first);
        reg(module.second, second);
        reg(module.third, third);
    };
    fxKnobs(mFxFilter, "effectsFilterFrequency", "effectsFilterResonance", "effectsFilterMix");
    fxKnobs(mFxHaze, "effectsHazeRate", "effectsHazeDepth", "effectsHazeMix");
    fxKnobs(mFxTaps, "effectsTapsTime", "effectsTapsFeedback", "effectsTapsMix");
    fxKnobs(mFxSpace, "effectsSpaceSize", "effectsSpaceDamping", "effectsSpaceMix");
}

void WelleyzEditor::showPage(int page) {
    page = juce::jlimit(0, 2, page);
    mProcessor.mUiPage = page;
    mRandomTab.setToggleState(page == 0, juce::dontSendNotification);
    mOscTab.setToggleState(page == 1, juce::dontSendNotification);
    mFxTab.setToggleState(page == 2, juce::dontSendNotification);
    juce::Component* homeParts[]{&mRandomButton, &mBackButton, &mResetButton, &mSaveButton,
                                 &mLoadButton,   &mSoundTypes, &mBrowser};
    for (auto* c : homeParts)
        c->setVisible(page == 0);
    juce::Component* synthParts[]{&mOscillator1,   &mOscillator2, &mOscillator3,  &mEnvelopeSection,
                                  &mFilterSection, &mTubeSection, &mMasterSection};
    for (auto* c : synthParts)
        c->setVisible(page == 1);
    juce::Component* noiseParts[]{&mNoisePower, &mNoiseAmount, &mNoiseType, &mNoiseWave};
    for (auto* c : noiseParts)
        c->setVisible(page == 1);
    for (auto* c : {&mFxFilter, &mFxHaze, &mFxTaps, &mFxSpace})
        c->setVisible(page == 2);
    repaint();
}

void WelleyzEditor::refreshValueFonts() {
    const float h = juce::jmax(10.0f, 13.0f * mLookAndFeel.uiScale);
    const juce::Font f = mLookAndFeel.mono(h);
    for (auto* s : mSliders)
        for (int i = 0; i < s->getNumChildComponents(); ++i)
            if (auto* l = dynamic_cast<juce::Label*>(s->getChildComponent(i)))
                l->setFont(f);
}

void WelleyzEditor::applyTheme(int idx) {
    juce::ignoreUnused(idx);
    refreshAccent();
}

void WelleyzEditor::changeListenerCallback(juce::ChangeBroadcaster*) {
    refreshAccent();
}

void WelleyzEditor::timerCallback() {
    mProcessor.flushKeyboardDisplay();
    const bool blinkBright =
        !mProcessor.isRecording() || (juce::Time::getMillisecondCounter() / 350u) % 2u == 0u;
    if (mLookAndFeel.mRecordBlinkBright != blinkBright) {
        mLookAndFeel.mRecordBlinkBright = blinkBright;
        mRecordButton.repaint();
    }
    if (mRandomizeStartMs == 0.0)
        return;
    constexpr double durationMs = 650.0;
    const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - mRandomizeStartMs;
    const float progress = (float)juce::jlimit(0.0, 1.0, elapsedMs / durationMs);
    // Hosts may process the batch of automated parameters after mouse-up, and
    // reclaim the native focus after the first message-loop iteration. Restore
    // it after the host notifications have settled, not just in onClick.
    if (isShowing() && ((mPendingFocusRestores == 2 && elapsedMs >= 110.0) ||
                        (mPendingFocusRestores == 1 && elapsedMs >= 280.0))) {
        if (auto* peer = getPeer())
            peer->grabFocus();
        grabKeyboardFocus();
        --mPendingFocusRestores;
    }
    if (progress >= 1.0f) {
        startTimerHz(30);
        mRandomizeStartMs = 0.0;
        mPendingFocusRestores = 0;
        mLookAndFeel.randomizeRotation = 0.0f;
    } else {
        const float eased = progress * progress * (3.0f - 2.0f * progress);
        mLookAndFeel.randomizeRotation = eased * juce::MathConstants<float>::twoPi;
    }
    mRandomButton.repaint();
}

void WelleyzEditor::refreshAccent() {
    mCurrentTheme = makeWaveTheme(ThemeManager::instance());
    mLookAndFeel.setTheme(&mCurrentTheme);
    const auto& t = mCurrentTheme;

    for (auto* b :
         {&mSaveButton, &mLoadButton, &mResetButton, &mBackButton, &mZoomButton, &mSettingsButton,
          &mColorsButton, &mManualButton, &mRecordButton, &mMetronomeButton,
          &mTempoPanel.mToggleButton, &mTempoPanel.mCloseButton, &mRandomTab, &mOscTab, &mFxTab,
          &mFxFilter.power, &mFxHaze.power, &mFxTaps.power, &mFxSpace.power}) {
        b->setColour(juce::TextButton::textColourOffId, t.mBtnText);
        b->setColour(juce::TextButton::textColourOnId, t.mBtnText);
    }
    for (auto* power : {&mFxFilter.power, &mFxHaze.power, &mFxTaps.power, &mFxSpace.power}) {
        power->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        power->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    mRandomButton.setColour(juce::TextButton::textColourOffId, t.mBtnAccentText);
    mRandomButton.setColour(juce::TextButton::textColourOnId, t.mBtnAccentText);

    for (auto* c :
         {&mOscillator1.wave, &mOscillator2.wave, &mOscillator3.wave, &mOscillator1.voices,
          &mOscillator2.voices, &mOscillator3.voices, &mArpMode, &mPortaMode, &mFxFilter.mode}) {
        c->setColour(juce::ComboBox::textColourId, t.mComboText);
        c->setColour(juce::ComboBox::backgroundColourId, t.mComboBg);
        c->setColour(juce::ComboBox::focusedOutlineColourId, juce::Colours::transparentBlack);
        c->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    }
    mLookAndFeel.setColour(juce::PopupMenu::backgroundColourId, t.mPanelTop);
    mLookAndFeel.setColour(juce::PopupMenu::textColourId, t.mLabelMain);
    mLookAndFeel.setColour(juce::PopupMenu::headerTextColourId, t.mTitleMain);
    mLookAndFeel.setColour(juce::PopupMenu::highlightedBackgroundColourId, t.mBtnBottom);
    mLookAndFeel.setColour(juce::PopupMenu::highlightedTextColourId, t.mTitleMain);

    for (auto* s : mSliders) {
        s->setColour(juce::Slider::textBoxTextColourId, t.mValueMain);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour(juce::Slider::textBoxHighlightColourId, t.mAccent.withAlpha(0.35f));
    }
    mArpLabel.setColour(juce::Label::textColourId, t.mLabelMain);
    mPortaLabel.setColour(juce::Label::textColourId, t.mLabelMain);

    mKeyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, t.mKeyWhite);
    mKeyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, t.mKeyBlack);
    mKeyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, t.mKeySep);
    mKeyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        t.mAccent.withAlpha(0.30f));
    mKeyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        t.mAccent.withAlpha(0.55f));
    mKeyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId, juce::Colour(0xff1b2024));
    mKeyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, t.mBtnBottom);
    mKeyboard.setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId, t.mAccent);
    mKeyboard.repaint();
    mBrowser.search.setColour(juce::TextEditor::focusedOutlineColourId, t.mAccent);
    mBrowser.viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, t.mAccent);
    mBrowser.viewport.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId,
                                                       welleyz::design::colours::kSecondary);
    mBrowser.refreshList(); // Favourite changes can add/remove rows in the Likes view.
    mBrowser.list.repaint();
    mBrowser.repaint();
    mSoundTypes.repaint();
    for (auto& row : mSoundTypes.rows)
        row->repaint();
    for (auto& chip : mBrowser.chips)
        chip->repaint();
    for (int i = 0; i < getNumChildComponents(); ++i)
        getChildComponent(i)->repaint();
    for (auto* slider : mSliders)
        slider->repaint();
    mMasterVolume.repaint();
    mPitchSlider.repaint();
    mModSlider.repaint();

    repaint();
}

void WelleyzEditor::setZoom(float z) {
    z = juce::jlimit(0.5f, 2.0f, z);
    mProcessor.mUiZoom = z;
    mZoomButton.setButtonText(juce::String(juce::roundToInt(z * 100.0f)) + "%");
    setSize((int)(kEditorDesignWidth * z), (int)(kEditorDesignHeight * z));
}

void WelleyzEditor::paint(juce::Graphics& g) {
    const auto s = mLookAndFeel.uiScale;
    g.setGradientFill(juce::ColourGradient(welleyz::design::colours::kSecondary, 0.0f, 0.0f,
                                           welleyz::design::colours::kBackground, (float)getWidth(),
                                           (float)getHeight(), false));
    g.fillAll();
    welleyz::ui::drawHalo(g, {-180.0f * s, 0.0f, 850.0f * s, 700.0f * s},
                          mCurrentTheme.mAccent.withAlpha(0.14f));
    welleyz::ui::drawHalo(g, {840.0f * s, 230.0f * s, 650.0f * s, 640.0f * s},
                          welleyz::design::colours::kViolet.withAlpha(0.095f));
    if (mProcessor.mUiPage == 0) {
        welleyz::ui::drawHalo(g, {315.0f * s, 20.0f * s, 680.0f * s, 580.0f * s},
                              mCurrentTheme.mAccent.withAlpha(0.68f));
    }
    g.setTiledImageFill(welleyz::ui::noise(), 0, 0, 0.42f);
    g.fillAll();
    welleyz::ui::fillPanel(g, {6.0f * s, 4.0f * s, 1268.0f * s, 94.0f * s}, mCurrentTheme,
                           welleyz::design::radius::kMedium * s);
    welleyz::ui::fillPanel(g, {14.0f * s, 580.0f * s, 1252.0f * s, 166.0f * s}, mCurrentTheme,
                           welleyz::design::radius::kMedium * s);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawVerticalLine(875 * s, 17.0f * s, 87.0f * s);
    welleyz::ui::drawHalo(g, {9.0f * s, 8.0f * s, 78.0f * s, 73.0f * s},
                          mCurrentTheme.mAccent.withAlpha(0.55f));
    drawEmbeddedIcon(g, "logo_svg", {25.0f * s, 24.0f * s, 44.0f * s, 44.0f * s},
                     mCurrentTheme.mAccent);
    juce::GlyphArrangement wordmarkGlyphs;
    wordmarkGlyphs.addLineOfText(welleyz::fonts::brand(43.0f * s), "welleyz", 0.0f, 0.0f);
    juce::Path wordmark;
    wordmarkGlyphs.createPath(wordmark);
    if (!wordmark.isEmpty()) {
        wordmark.applyTransform(
            juce::RectanglePlacement(juce::RectanglePlacement::xLeft |
                                     juce::RectanglePlacement::yMid)
                .getTransformToFit(wordmark.getBounds(),
                                   {80.0f * s, 17.0f * s, 280.0f * s, 48.0f * s}));
        juce::DropShadow(juce::Colours::black.withAlpha(0.78f), juce::jmax(1, (int)(5.0f * s)),
                         {0, juce::roundToInt(2.0f * s)})
            .drawForPath(g, wordmark);
        g.setColour(mCurrentTheme.mAccent.withAlpha(0.18f));
        g.strokePath(wordmark, juce::PathStrokeType(3.2f * s));
        const auto bounds = wordmark.getBounds();
        juce::ColourGradient chrome(juce::Colour(0xfff9fdff), bounds.getX(), bounds.getY(),
                                    juce::Colour(0xff8797a2), bounds.getX(), bounds.getBottom(),
                                    false);
        chrome.addColour(0.19, juce::Colour(0xff71828f));
        chrome.addColour(0.38, juce::Colour(0xfff5fafc));
        chrome.addColour(0.49, juce::Colour(0xff40525e));
        chrome.addColour(0.59, juce::Colour(0xffe5f0f4));
        chrome.addColour(0.82, juce::Colour(0xff607480));
        g.setGradientFill(chrome);
        g.fillPath(wordmark);
        g.setColour(juce::Colours::white.withAlpha(0.42f));
        g.strokePath(wordmark, juce::PathStrokeType(0.65f * s));
    }
    g.setColour(welleyz::design::colours::kMuted);
    g.setFont(mLookAndFeel.mono(13.0f * s));
    g.drawText("by shadowave", (int)(83.0f * s), (int)(66.0f * s), (int)(220.0f * s),
               (int)(18.0f * s), juce::Justification::centredLeft);
    if (mProcessor.mUiPage == 0) {
        g.setColour(welleyz::design::colours::kBorderStrong.withAlpha(0.34f));
        g.drawVerticalLine(426 * s, 117.0f * s, 564.0f * s);
        g.drawVerticalLine(854 * s, 117.0f * s, 564.0f * s);
        g.setColour(welleyz::design::colours::kMuted);
        g.setFont(mLookAndFeel.mono(12.0f * s));
        g.drawText("SELECT A TYPE  /  GENERATE A SOUND", (int)(447.0f * s), (int)(422.0f * s),
                   (int)(385.0f * s), (int)(24.0f * s), juce::Justification::centred);
        g.setColour(welleyz::design::colours::kText);
        g.setFont(mLookAndFeel.mono(12.0f * s));
        g.drawText("02 / GENERATOR", (int)(495.0f * s), (int)(121.0f * s), (int)(290.0f * s),
                   (int)(20.0f * s), juce::Justification::centred);
    }
    g.setFont(mLookAndFeel.mono(12.0f * s));
    g.setColour(welleyz::design::colours::kSecondaryText);
    g.drawText("PITCH", (int)(24.0f * s), (int)(717.0f * s), (int)(50.0f * s), (int)(20.0f * s),
               juce::Justification::centred);
    g.drawText("MOD", (int)(77.0f * s), (int)(717.0f * s), (int)(50.0f * s), (int)(20.0f * s),
               juce::Justification::centred);
    g.drawText("VOLUME", (int)(1140.0f * s), (int)(597.0f * s), (int)(106.0f * s), (int)(20.0f * s),
               juce::Justification::centred);
}

void WelleyzEditor::resized() {
    const float s = (float)getWidth() / (float)kEditorDesignWidth;
    mLookAndFeel.setUiScale(s);
    auto S = [&](float v) { return (int)std::round(v * s); };
    const int M = S(16.0f);

    const int navW = S(145), navGap = S(3), navX = (getWidth() - (navW * 3 + navGap * 2)) / 2;
    mRandomTab.setBounds(navX, S(17), navW, S(70));
    mOscTab.setBounds(navX + navW + navGap, S(17), navW, S(70));
    mFxTab.setBounds(navX + (navW + navGap) * 2, S(17), navW, S(70));
    const bool standalone = mProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    const int headerButtonSize = S(48.0f), headerButtonGap = S(8.0f);
    const int headerButtonsWidth =
        headerButtonSize * (standalone ? 6 : 4) + headerButtonGap * (standalone ? 5 : 3);
    const int headerButtonsX = getWidth() - S(16) - headerButtonsWidth;
    const int headerButtonsY = (S(103) - headerButtonSize) / 2;
    int bx = headerButtonsX;
    if (standalone) {
        mRecordButton.setBounds(bx, headerButtonsY, headerButtonSize, headerButtonSize);
        bx += headerButtonSize + headerButtonGap;
        mMetronomeButton.setBounds(bx, headerButtonsY, headerButtonSize, headerButtonSize);
        bx += headerButtonSize + headerButtonGap;
        mTempoPanel.setBounds(mMetronomeButton.getX(), S(102.0f), S(240.0f), S(132.0f));
    }
    mManualButton.setBounds(bx, headerButtonsY, headerButtonSize, headerButtonSize);
    bx += headerButtonSize + headerButtonGap;
    mColorsButton.setBounds(bx, headerButtonsY, headerButtonSize, headerButtonSize);
    bx += headerButtonSize + headerButtonGap;
    mSettingsButton.setBounds(bx, headerButtonsY, headerButtonSize, headerButtonSize);
    bx += headerButtonSize + headerButtonGap;
    mZoomButton.setBounds(bx, headerButtonsY, headerButtonSize, headerButtonSize);
    if (mManualOverlay != nullptr)
        mManualOverlay->setBounds(getLocalBounds());
    mSoundTypes.setBounds(S(15), S(115), S(397), S(450));
    mBrowser.setBounds(S(869), S(115), S(396), S(450));
    mRandomButton.setBounds(S(510), S(149), S(260), S(260));
    const int actionY = S(471), actionH = S(78);
    mBackButton.setBounds(S(451), actionY, S(86), actionH);
    mResetButton.setBounds(S(548), actionY, S(86), actionH);
    mSaveButton.setBounds(S(645), actionY, S(86), actionH);
    mLoadButton.setBounds(S(742), actionY, S(86), actionH);

    // Ряд осцилляторов
    const int row1y = S(115.0f), row1h = S(234.0f), gapO = S(10.0f);
    const int sw = S(409.0f);
    const int x1 = M, x2 = M + sw + gapO, x3 = M + (sw + gapO) * 2;
    mOscillator1.setBounds(x1, row1y, sw, row1h);
    mOscillator2.setBounds(x2, row1y, sw, row1h);
    mOscillator3.setBounds(x3, row1y, S((float)kEditorDesignWidth) - M - x3, row1h);

    // Нижний ряд: одинаковые отступы, единый стиль секций
    const int row2y = S(356.0f), row2h = S(222.0f), gap2 = S(8.0f);
    const int envW = S(320.0f), filtW = S(170.0f), tubeW = S(460.0f);
    mEnvelopeSection.setBounds(M, row2y, envW, row2h);
    mFilterSection.setBounds(M + envW + gap2, row2y, filtW, row2h);
    mTubeSection.setBounds(M + envW + gap2 + filtW + gap2, row2y, tubeW, row2h);
    const int mastX = M + envW + gap2 + filtW + gap2 + tubeW + gap2;
    mMasterSection.setBounds(mastX, row2y, S((float)kEditorDesignWidth) - M - mastX, row2h);
    const int fxGap = S(10.0f);
    const int fxW = (getWidth() - M * 2 - fxGap) / 2;
    mFxFilter.setBounds(M, S(115.0f), fxW, S(230.0f));
    mFxHaze.setBounds(M + fxW + fxGap, S(115.0f), fxW, S(230.0f));
    mFxTaps.setBounds(M, S(350.0f), fxW, S(228.0f));
    mFxSpace.setBounds(M + fxW + fxGap, S(350.0f), fxW, S(228.0f));

    auto rowOf = [&](juce::Component& box, std::vector<Knob*> knobs, float top, float h) {
        auto rr = box.getLocalBounds().reduced(S(12.0f), 0);
        rr.removeFromTop(S(top));
        rr.removeFromBottom(S(10.0f));
        const int n = (int)knobs.size();
        const int gap = S(6.0f);
        const int kw = (rr.getWidth() - gap * (n - 1)) / n;
        for (auto* k : knobs) {
            k->setBounds(rr.removeFromLeft(kw).withHeight(S(h)));
            rr.removeFromLeft(gap);
        }
    };
    rowOf(mEnvelopeSection, {&mAttackKnob, &mDecayKnob, &mSustainKnob, &mReleaseKnob}, 30.0f,
          119.0f);
    rowOf(mFilterSection, {&mCutoffKnob, &mResoKnob}, 30.0f, 119.0f);
    rowOf(mTubeSection, {&mLampinessKnob, &mBitsKnob, &mRateKnob, &mVibRateKnob, &mVibDepthKnob},
          30.0f, 112.0f);
    mNoisePower.setBounds(mTubeSection.getX() + S(13), mTubeSection.getY() + S(176), S(27), S(27));
    mNoiseAmount.setBounds(mTubeSection.getX() + S(53), mTubeSection.getY() + S(151), S(65), S(68));
    mNoiseType.setBounds(mTubeSection.getX() + S(124), mTubeSection.getY() + S(177), S(118), S(29));
    mNoiseWave.setBounds(mTubeSection.getX() + S(250), mTubeSection.getY() + S(157),
                         mTubeSection.getWidth() - S(265), S(54));

    // Master: ручки + ARP / portamento selectors
    {
        auto rr = mMasterSection.getLocalBounds().reduced(S(12.0f), 0);
        rr.removeFromTop(S(30.0f));
        const int gap = S(8.0f);
        const int kw = (rr.getWidth() - gap * 2) / 3;
        mMasterKnob.setBounds(rr.removeFromLeft(kw).withHeight(S(112.0f)));
        rr.removeFromLeft(gap);
        mArpRateKnob.setBounds(rr.removeFromLeft(kw).withHeight(S(112.0f)));
        rr.removeFromLeft(gap);
        mGlideKnob.setBounds(rr.withHeight(S(112.0f)));
        auto bott = mMasterSection.getLocalBounds()
                        .reduced(S(12.0f), 0)
                        .withTrimmedTop(S(151.0f))
                        .withHeight(S(52.0f));
        const int selectorGap = S(8.0f);
        const int selectorWidth = (bott.getWidth() - selectorGap) / 2;
        auto arpArea = bott.removeFromLeft(selectorWidth);
        bott.removeFromLeft(selectorGap);
        auto portaArea = bott;
        mArpLabel.setBounds(arpArea.removeFromTop(S(14.0f)));
        mPortaLabel.setBounds(portaArea.removeFromTop(S(14.0f)));
        mArpLabel.setFont(mLookAndFeel.mono(12.0f * s));
        mPortaLabel.setFont(mLookAndFeel.mono(12.0f * s));
        mArpMode.setBounds(arpArea.reduced(0, S(2.0f)));
        mPortaMode.setBounds(portaArea.reduced(0, S(2.0f)));
    }

    // Клавиатура
    mPitchSlider.setBounds(S(24), S(611), S(46), S(104));
    mModSlider.setBounds(S(78), S(611), S(46), S(104));
    const int visibleKey = mKeyboardInitialPositionPending ? 72 : mKeyboard.getLowestVisibleKey();
    mKeyboard.setBounds(S(149), S(596), S(971), S(137));
    mKeyboard.setKeyWidth(36.0f * s);
    // JUCE can move the viewport when bounds/key width change; restore the
    // selected octave after both layout operations.
    mKeyboard.setLowestVisibleKey(visibleKey);
    mKeyboardInitialPositionPending = false;
    mMasterVolume.setBounds(S(1150), S(621), S(85), S(85));

    refreshValueFonts();
    repaint();
}

// createEditor — реализация здесь чтобы не тащить Editor.h в Processor.cpp
juce::AudioProcessorEditor* WelleyzProcessor::createEditor() {
    return new WelleyzEditor(*this);
}
