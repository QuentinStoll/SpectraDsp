#include "Components/UpdateOfferDialog.h"

#include "Stylesheet.h"

namespace {
constexpr float kRadius = 18.0f;

juce::Rectangle<float> buttonRect(juce::Rectangle<float> row, bool install) {
    const auto gap = 12.0f;
    const auto width = (row.getWidth() - gap) * 0.5f;
    if (install) {
        return {row.getRight() - width, row.getY(), width, row.getHeight()};
    }
    return {row.getX(), row.getY(), width, row.getHeight()};
}
}  // namespace

UpdateOfferDialog::UpdateOfferDialog() {
    setVisible(false);
    setInterceptsMouseClicks(true, true);
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

UpdateOfferDialog::~UpdateOfferDialog() {
    stopTimer();
}

void UpdateOfferDialog::showOffer(const juce::String& message) {
    _message = message;
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

juce::Rectangle<float> UpdateOfferDialog::cardBounds() const {
    const auto width = juce::jmin(460.0f, static_cast<float>(getWidth()) - 48.0f);
    const auto height = juce::jmin(248.0f, static_cast<float>(getHeight()) - 48.0f);
    auto card = juce::Rectangle<float>(juce::jmax(280.0f, width), juce::jmax(200.0f, height));
    card.setCentre(getLocalBounds().toFloat().getCentre());
    const auto scale = 1.0f - 0.028f * _sink;
    card = card.withSizeKeepingCentre(card.getWidth() * scale, card.getHeight() * scale);
    card.translate(0.0f, 6.0f * _sink);
    return card;
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
    auto card = cardBounds().reduced(28.0f, 24.0f);
    auto row = card.removeFromBottom(40.0f);
    const auto install = buttonRect(row, true);
    const auto later = buttonRect(row, false);
    if (install.contains(position.toFloat())) {
        return Hit::Install;
    }
    if (later.contains(position.toFloat())) {
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

    const auto card = cardBounds();
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

    auto content = card.reduced(28.0f, 24.0f);
    auto titleArea = content.removeFromTop(32.0f);
    g.setColour(ProfilerStyle::Colors::text);
    g.setFont(ProfilerStyle::Fonts::medium(18.0f));
    g.drawText("Update available", titleArea, juce::Justification::centredLeft, true);

    content.removeFromTop(8.0f);
    auto messageArea = content.removeFromBottom(40.0f);
    content.removeFromBottom(18.0f);
    g.setColour(ProfilerStyle::Colors::textMuted);
    g.setFont(ProfilerStyle::Fonts::regular(13.0f));
    g.drawFittedText(_message, content.toNearestInt(), juce::Justification::topLeft, 4, 1.0f);

    const auto later = buttonRect(messageArea, false);
    const auto install = buttonRect(messageArea, true);
    auto paintButton = [&](juce::Rectangle<float> bounds, const juce::String& label, bool primary, bool hot, bool down) {
        auto fill = primary ? juce::Colour(0xff07262B) : juce::Colour(0xff121318);
        if (down) {
            fill = fill.darker(0.18f);
        } else if (hot) {
            fill = fill.brighter(0.08f);
        }
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(primary ? ProfilerStyle::Colors::accentCyan.withAlpha(hot ? 0.95f : 0.72f)
                            : juce::Colours::white.withAlpha(hot ? 0.28f : 0.12f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, primary ? 1.4f : 1.0f);
        g.setColour(primary ? ProfilerStyle::Colors::accentCyan : ProfilerStyle::Colors::text);
        g.setFont(ProfilerStyle::Fonts::medium(13.0f));
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
