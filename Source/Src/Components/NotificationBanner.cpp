#include "Components/NotificationBanner.h"

#include "BinaryData.h"

namespace {
int getTextWidth(const juce::Font& font, const juce::String& text) {
    if (text.isEmpty())
        return 0;

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, text, 0.0f, 0.0f);

    return juce::roundToInt(glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth());
}
}  // namespace

NotificationBanner::NotificationBanner() {
    _dismissIcon = juce::Drawable::createFromImageData(BinaryData::crossicon_svg,
                                                       BinaryData::crossicon_svgSize);

    if (_dismissIcon != nullptr)
        _dismissIcon->replaceColour(juce::Colour(0xff000000), ProfilerStyle::Colors::white);

    setVisible(false);
    setInterceptsMouseClicks(true, true);
}

NotificationBanner::~NotificationBanner() {
    stopTimer();
}

void NotificationBanner::setMessage(const juce::String& message) {
    _message = message;
    repaint();
}

void NotificationBanner::showMessage(const juce::String& message,
                                     Type type,
                                     int timeoutMs) {
    _message = message;
    _type = type;

    if (timeoutMs > 0)
        startTimer(timeoutMs);
    else
        stopTimer();

    setVisible(true);
    toFront(false);
    resized();
    repaint();
}

void NotificationBanner::dismiss() {
    if (!isVisible())
        return;

    stopTimer();
    setVisible(false);

    if (onDismissed)
        onDismissed();
}

void NotificationBanner::setType(Type type) {
    _type = type;
    repaint();
}

void NotificationBanner::setAction(const juce::String& label, std::function<void()> action) {
    _actionLabel = label;
    _action = std::move(action);
    resized();
    repaint();
}

void NotificationBanner::clearAction() {
    _actionLabel.clear();
    _action = nullptr;
    resized();
    repaint();
}

int NotificationBanner::getIdealWidth() const {
    const auto messageFont = ProfilerStyle::Fonts::regular(14.0f);
    const auto actionFont = ProfilerStyle::Fonts::regular(13.0f);
    auto width = 64 + getTextWidth(messageFont, _message);

    if (_actionLabel.isNotEmpty())
        width += 20 + getTextWidth(actionFont, _actionLabel) + 18;

    return juce::jlimit(220, 640, width);
}

int NotificationBanner::getIdealHeight() const {
    return bannerHeight;
}

void NotificationBanner::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const auto accent = getAccentColour();

    g.setColour(ProfilerStyle::Colors::elevated);
    g.fillRoundedRectangle(bounds, ProfilerStyle::Surfaces::controlCorner);

    g.setColour(accent);
    g.fillRoundedRectangle(bounds.withWidth(3.0f), 1.5f);

    g.setColour(ProfilerStyle::Contrast::outline(accent.withAlpha(0.28f)));
    g.drawRoundedRectangle(bounds.reduced(0.5f), ProfilerStyle::Surfaces::controlCorner, 1.0f);

    auto dotBounds = juce::Rectangle<float>(0.0f, 0.0f, 8.0f, 8.0f)
                         .withCentre(_messageBounds.withWidth(10).getCentre().toFloat());
    g.setColour(accent);
    g.fillEllipse(dotBounds);

    g.setFont(ProfilerStyle::Fonts::regular(14.0f));
    g.setColour(ProfilerStyle::Contrast::foreground(ProfilerStyle::Colors::text));
    g.drawFittedText(_message, _messageBounds.withTrimmedLeft(18),
                     juce::Justification::centredLeft, 1);

    const auto mousePosition = getMouseXYRelative();

    if (_actionLabel.isNotEmpty()) {
        const auto isActionHovered = _actionBounds.contains(mousePosition);
        g.setColour(accent.withAlpha(isActionHovered ? 0.28f : 0.16f));
        g.fillRoundedRectangle(_actionBounds.toFloat(), ProfilerStyle::Surfaces::controlCorner);

        g.setFont(ProfilerStyle::Fonts::regular(13.0f));
        g.setColour(ProfilerStyle::Contrast::foreground(ProfilerStyle::Colors::text.withAlpha(isActionHovered ? 1.0f : 0.86f)));
        g.drawFittedText(_actionLabel, _actionBounds,
                         juce::Justification::centred, 1);
    }

    const auto isDismissHovered = _dismissBounds.contains(mousePosition);
    if (_dismissIcon != nullptr) {
        _dismissIcon->drawWithin(g,
                                 _dismissBounds.toFloat().reduced(isDismissHovered ? 6.0f : 7.0f),
                                 juce::RectanglePlacement::centred,
                                 isDismissHovered ? 1.0f : 0.62f);
    }
}

void NotificationBanner::resized() {
    auto area = getLocalBounds().reduced(12, 6);

    _dismissBounds = area.removeFromRight(24);
    area.removeFromRight(8);

    if (_actionLabel.isNotEmpty()) {
        const auto actionWidth = getTextWidth(ProfilerStyle::Fonts::regular(13.0f), _actionLabel) + 18;
        _actionBounds = area.removeFromRight(juce::jlimit(58, 160, actionWidth)).reduced(0, 3);
        area.removeFromRight(8);
    } else {
        _actionBounds = {};
    }

    _messageBounds = area;
}

void NotificationBanner::mouseMove(const juce::MouseEvent& event) {
    updateMouseCursor(event.getPosition());
    repaint();
}

void NotificationBanner::mouseExit(const juce::MouseEvent& /*event*/) {
    setMouseCursor(juce::MouseCursor(juce::MouseCursor::NormalCursor));
    repaint();
}

void NotificationBanner::mouseUp(const juce::MouseEvent& event) {
    const auto position = event.getPosition();

    if (_dismissBounds.contains(position)) {
        dismiss();
        return;
    }

    if (_actionLabel.isNotEmpty() && _actionBounds.contains(position) && _action) {
        _action();
    }
}

void NotificationBanner::timerCallback() {
    dismiss();
}

void NotificationBanner::updateMouseCursor(juce::Point<int> position) {
    const auto isInteractive = _dismissBounds.contains(position) || (_actionLabel.isNotEmpty() && _actionBounds.contains(position));

    setMouseCursor(juce::MouseCursor(isInteractive
                                         ? juce::MouseCursor::PointingHandCursor
                                         : juce::MouseCursor::NormalCursor));
}

juce::Colour NotificationBanner::getAccentColour() const {
    switch (_type) {
        case Type::Success:
            return juce::Colour(0xff38d17a);
        case Type::Warning:
            return juce::Colour(0xffffb020);
        case Type::Error:
            return juce::Colour(0xffff4d4f);
        case Type::Info:
        default:
            return ProfilerStyle::Colors::orange;
    }
}
