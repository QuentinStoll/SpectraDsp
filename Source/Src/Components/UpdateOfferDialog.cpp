#include "Components/UpdateOfferDialog.h"

#include "Stylesheet.h"

namespace {
constexpr float kScale = 1.3f;
constexpr float kRadius = 18.0f * kScale;

struct CardLayout {
    juce::Rectangle<float> card;
    juce::Rectangle<float> title;
    juce::Rectangle<float> version;
    juce::Rectangle<float> notes;
    juce::Rectangle<float> later;
    juce::Rectangle<float> install;
    float bulletHeight = 0.0f;
};

}  // namespace

UpdateOfferDialog::UpdateOfferDialog() {
    setVisible(false);
    setInterceptsMouseClicks(true, true);
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

UpdateOfferDialog::~UpdateOfferDialog() {
    stopTimer();
}

void UpdateOfferDialog::showOffer(const juce::String& version, const juce::StringArray& changes) {
    _version = version.startsWithIgnoreCase("v") ? version.substring(1) : version;
    _changes = changes;
    _phase = 0.0f;
    _sink = 0.0f;
    _cardHover = 0.0f;
    _hovered = Hit::None;
    _pressed = Hit::None;
    _lastTickMs = juce::Time::getMillisecondCounterHiRes();
    setVisible(true);
    toFront(false);
    startTimerHz(60);
    repaint();
}

void UpdateOfferDialog::dismiss() {
    stopTimer();
    setVisible(false);
    _pressed = Hit::None;
    _hovered = Hit::None;
}

CardLayout layoutFor(juce::Rectangle<int> bounds, int changeCount, float sink) {
    const auto bulletHeight = 36.0f * kScale;
    const auto padX = 28.0f * kScale;
    const auto padY = 24.0f * kScale;
    const auto titleHeight = 32.0f * kScale;
    const auto versionHeight = 36.0f * kScale;
    const auto buttonHeight = 40.0f * kScale;
    const auto notesHeight = static_cast<float>(changeCount) * bulletHeight;
    const auto contentHeight = titleHeight + 8.0f * kScale + versionHeight + 12.0f * kScale + notesHeight + 18.0f * kScale + buttonHeight;
    const auto width = juce::jmin(460.0f * kScale, static_cast<float>(bounds.getWidth()) - 48.0f);
    const auto height = juce::jmin(juce::jmax(248.0f * kScale, padY * 2.0f + contentHeight),
                                   static_cast<float>(bounds.getHeight()) - 48.0f);
    auto card = juce::Rectangle<float>(juce::jmax(280.0f * kScale, width), juce::jmax(200.0f * kScale, height));
    card.setCentre(bounds.toFloat().getCentre());
    const auto press = 1.0f - 0.028f * sink;
    card = card.withSizeKeepingCentre(card.getWidth() * press, card.getHeight() * press);
    card.translate(0.0f, 6.0f * kScale * sink);

    CardLayout placed;
    placed.card = card;
    placed.bulletHeight = bulletHeight * press;
    auto content = card.reduced(padX * press, padY * press);
    placed.title = content.removeFromTop(titleHeight * press);
    content.removeFromTop(8.0f * kScale * press);
    placed.version = content.removeFromTop(versionHeight * press);
    content.removeFromTop(12.0f * kScale * press);
    auto buttons = content.removeFromBottom(buttonHeight * press);
    content.removeFromBottom(18.0f * kScale * press);
    placed.notes = content;
    const auto gap = 12.0f * kScale * press;
    const auto buttonWidth = (buttons.getWidth() - gap) * 0.5f;
    placed.later = {buttons.getX(), buttons.getY(), buttonWidth, buttons.getHeight()};
    placed.install = {buttons.getRight() - buttonWidth, buttons.getY(), buttonWidth, buttons.getHeight()};
    return placed;
}

juce::Rectangle<float> UpdateOfferDialog::cardBounds() const {
    return layoutFor(getLocalBounds(), _changes.size(), _sink).card;
}

float UpdateOfferDialog::perimeterOf(juce::Rectangle<float> card, float radius) const {
    const auto straightW = juce::jmax(0.0f, card.getWidth() - 2.0f * radius);
    const auto straightH = juce::jmax(0.0f, card.getHeight() - 2.0f * radius);
    const auto arc = juce::MathConstants<float>::halfPi * radius;
    return 2.0f * (straightW + straightH) + 4.0f * arc;
}

juce::Point<float> UpdateOfferDialog::pointOnCard(juce::Rectangle<float> card, float radius, float distance) const {
    const auto straightW = juce::jmax(0.0f, card.getWidth() - 2.0f * radius);
    const auto straightH = juce::jmax(0.0f, card.getHeight() - 2.0f * radius);
    const auto arc = juce::MathConstants<float>::halfPi * radius;
    const auto perimeter = perimeterOf(card, radius);
    auto remaining = std::fmod(distance, perimeter);
    if (remaining < 0.0f) {
        remaining += perimeter;
    }

    const auto left = card.getX();
    const auto top = card.getY();
    const auto right = card.getRight();
    const auto bottom = card.getBottom();
    auto take = [&remaining](float length) {
        if (remaining <= length) {
            return true;
        }
        remaining -= length;
        return false;
    };

    if (take(straightW)) {
        return {left + radius + remaining, top};
    }
    if (take(arc)) {
        const auto angle = -juce::MathConstants<float>::halfPi + (radius > 0.0f ? remaining / radius : 0.0f);
        return {right - radius + std::cos(angle) * radius, top + radius + std::sin(angle) * radius};
    }
    if (take(straightH)) {
        return {right, top + radius + remaining};
    }
    if (take(arc)) {
        const auto angle = radius > 0.0f ? remaining / radius : 0.0f;
        return {right - radius + std::cos(angle) * radius, bottom - radius + std::sin(angle) * radius};
    }
    if (take(straightW)) {
        return {right - radius - remaining, bottom};
    }
    if (take(arc)) {
        const auto angle = juce::MathConstants<float>::halfPi + (radius > 0.0f ? remaining / radius : 0.0f);
        return {left + radius + std::cos(angle) * radius, bottom - radius + std::sin(angle) * radius};
    }
    if (take(straightH)) {
        return {left, bottom - radius - remaining};
    }

    const auto angle = juce::MathConstants<float>::pi + (radius > 0.0f ? remaining / radius : 0.0f);
    return {left + radius + std::cos(angle) * radius, top + radius + std::sin(angle) * radius};
}

UpdateOfferDialog::Hit UpdateOfferDialog::hitAt(juce::Point<int> position) const {
    const auto placed = layoutFor(getLocalBounds(), _changes.size(), _sink);
    if (placed.install.contains(position.toFloat())) {
        return Hit::Install;
    }
    if (placed.later.contains(position.toFloat())) {
        return Hit::Later;
    }
    return Hit::None;
}

void UpdateOfferDialog::timerCallback() {
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = static_cast<float>(juce::jlimit(0.001, 0.05, (now - _lastTickMs) * 0.001));
    _lastTickMs = now;

    const auto overCard = cardBounds().contains(getMouseXYRelative().toFloat());
    const auto hoverTarget = overCard ? 1.0f : 0.0f;
    _cardHover += (hoverTarget - _cardHover) * juce::jmin(1.0f, dt * 10.0f);
    const auto sinkTarget = overCard ? 1.0f : 0.0f;
    _sink += (sinkTarget - _sink) * juce::jmin(1.0f, dt * 12.0f);

    const auto turnsPerSecond = 0.28f + 0.72f * _cardHover;
    _phase = std::fmod(_phase + dt * turnsPerSecond, 1.0f);
    repaint();
}

void UpdateOfferDialog::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.62f));

    const auto placed = layoutFor(getLocalBounds(), _changes.size(), _sink);
    const auto card = placed.card;
    const auto radius = kRadius * (1.0f - 0.02f * _sink);
    const auto shadowOffset = 14.0f - 8.0f * _sink;
    for (int layer = 4; layer >= 1; --layer) {
        const auto spread = static_cast<float>(layer) * 5.0f;
        g.setColour(juce::Colours::black.withAlpha(0.16f * (1.0f - 0.35f * _sink)));
        g.fillRoundedRectangle(card.translated(0.0f, shadowOffset).expanded(spread * 0.35f), radius + spread);
    }

    juce::ColourGradient fill(juce::Colour(0xff14161C),
                              card.getTopLeft(),
                              juce::Colour(0xff090A0E),
                              card.getBottomLeft(),
                              false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(card, radius);

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(card.reduced(0.5f), radius, 1.0f);

    const auto perimeter = perimeterOf(card, radius);
    const auto head = _phase * perimeter;
    const auto span = perimeter * 0.30f;
    constexpr int steps = 56;
    for (int index = 0; index < steps; ++index) {
        const auto start = static_cast<float>(index) / static_cast<float>(steps);
        const auto end = static_cast<float>(index + 1) / static_cast<float>(steps);
        const auto from = pointOnCard(card, radius, head - span + span * start);
        const auto to = pointOnCard(card, radius, head - span + span * end);
        const auto glow = start * start;
        g.setColour(ProfilerStyle::Colors::accentCyan.withAlpha(0.22f * glow));
        g.drawLine(from.x, from.y, to.x, to.y, 7.0f);
        g.setColour(ProfilerStyle::Colors::accentCyan.withAlpha(0.15f + 0.85f * glow));
        g.drawLine(from.x, from.y, to.x, to.y, 1.8f);
    }
    const auto headPoint = pointOnCard(card, radius, head);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.fillEllipse(headPoint.x - 2.2f, headPoint.y - 2.2f, 4.4f, 4.4f);

    g.setColour(ProfilerStyle::Colors::text);
    g.setFont(ProfilerStyle::Fonts::medium(18.0f * kScale));
    g.drawText("Update available", placed.title, juce::Justification::centredLeft, true);

    g.setColour(ProfilerStyle::Colors::text);
    g.setFont(ProfilerStyle::Fonts::medium(22.0f * kScale));
    g.drawText(_version, placed.version, juce::Justification::centredLeft, true);

    g.setFont(ProfilerStyle::Fonts::regular(13.0f * kScale));
    auto noteTop = placed.notes.getY();
    const auto marker = 5.5f * kScale;
    for (const auto& change : _changes) {
        if (noteTop + placed.bulletHeight > placed.notes.getBottom() + 1.0f) {
            break;
        }
        auto row = juce::Rectangle<float>(placed.notes.getX(), noteTop, placed.notes.getWidth(), placed.bulletHeight);
        g.setColour(ProfilerStyle::Colors::accentCyan);
        g.fillEllipse(row.getX(), row.getCentreY() - marker * 0.5f, marker, marker);
        g.setColour(ProfilerStyle::Colors::textMuted);
        g.drawFittedText(change,
                         row.withTrimmedLeft(16.0f * kScale).toNearestInt(),
                         juce::Justification::centredLeft,
                         2,
                         1.0f);
        noteTop += placed.bulletHeight;
    }

    const auto later = placed.later;
    const auto install = placed.install;
    auto paintButton = [&](juce::Rectangle<float> bounds, const juce::String& label, bool primary, bool hot, bool down) {
        auto fill = primary ? juce::Colour(0xff07262B) : juce::Colour(0xff121318);
        if (down) {
            fill = fill.darker(0.18f);
        } else if (hot) {
            fill = fill.brighter(0.08f);
        }
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 8.0f * kScale);
        g.setColour(primary ? ProfilerStyle::Colors::accentCyan.withAlpha(hot ? 0.95f : 0.72f)
                            : juce::Colours::white.withAlpha(hot ? 0.28f : 0.12f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f * kScale, primary ? 1.4f : 1.0f);
        g.setColour(primary ? ProfilerStyle::Colors::accentCyan : ProfilerStyle::Colors::text);
        g.setFont(ProfilerStyle::Fonts::medium(13.0f * kScale));
        g.drawText(label, bounds, juce::Justification::centred, true);
    };
    paintButton(later, "Later", false, _hovered == Hit::Later, _pressed == Hit::Later);
    paintButton(install, "Install", true, _hovered == Hit::Install, _pressed == Hit::Install);
}

void UpdateOfferDialog::mouseMove(const juce::MouseEvent& event) {
    const auto next = hitAt(event.getPosition());
    if (next != _hovered) {
        _hovered = next;
        setMouseCursor(next == Hit::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void UpdateOfferDialog::mouseExit(const juce::MouseEvent&) {
    _hovered = Hit::None;
    _pressed = Hit::None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void UpdateOfferDialog::mouseDown(const juce::MouseEvent& event) {
    _pressed = hitAt(event.getPosition());
    repaint();
}

void UpdateOfferDialog::mouseUp(const juce::MouseEvent& event) {
    const auto released = hitAt(event.getPosition());
    const auto pressed = _pressed;
    _pressed = Hit::None;
    if (released != pressed) {
        repaint();
        return;
    }
    if (released == Hit::Install && onInstall) {
        onInstall();
        return;
    }
    if (released == Hit::Later && onLater) {
        onLater();
    }
}
