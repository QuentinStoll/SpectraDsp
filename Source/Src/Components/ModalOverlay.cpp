#include "Components/ModalOverlay.h"

#include "ActionFeedback.h"

ModalOverlay::ModalOverlay() {
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);
    setVisible(false);
}

ModalOverlay::ModalOverlay(std::unique_ptr<juce::Component> content,
                           juce::Rectangle<int> contentBounds)
    : ModalOverlay() {
    setContent(std::move(content), contentBounds);
}

ModalOverlay::~ModalOverlay() {
    _content.reset();
}

void ModalOverlay::setContent(std::unique_ptr<juce::Component> content,
                              juce::Rectangle<int> contentBounds) {
    if (_content != nullptr)
        removeChildComponent(_content.get());

    _content = std::move(content);
    _contentBounds = contentBounds.withPosition(0, 0);

    if (_content != nullptr) {
        addAndMakeVisible(*_content);
        _content->setBounds(getCentredContentBounds());
    }

    resized();
}

void ModalOverlay::show() {
    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
    resized();
}

void ModalOverlay::show(std::unique_ptr<juce::Component> content,
                        juce::Rectangle<int> contentBounds) {
    setContent(std::move(content), contentBounds);
    show();
}

void ModalOverlay::dismiss() {
    if (!isVisible())
        return;

    setVisible(false);

    if (onDismissed)
        onDismissed();
}

void ModalOverlay::setDismissOnBackgroundClick(bool shouldDismiss) {
    _dismissOnBackgroundClick = shouldDismiss;
}

void ModalOverlay::setOverlayColour(juce::Colour colour) {
    _overlayColour = colour;
    repaint();
}

void ModalOverlay::setPanelCornerRadius(float radius) {
    _panelCornerRadius = radius;
    repaint();
}

void ModalOverlay::paint(juce::Graphics& g) {
    juce::Path path;
    path.addRoundedRectangle(getLocalBounds().toFloat(), 5.0f);

    g.setColour(_overlayColour);
    g.fillPath(path);

    const auto panelBounds = getCentredContentBounds().toFloat();

    g.setColour(ProfilerStyle::Colors::container);
    g.fillRoundedRectangle(panelBounds, _panelCornerRadius);
}

void ModalOverlay::resized() {
    if (_content != nullptr)
        _content->setBounds(getCentredContentBounds());
}

void ModalOverlay::mouseDown(const juce::MouseEvent& event) {
    if (_dismissOnBackgroundClick && !getCentredContentBounds().contains(event.getPosition())) {
        ActionFeedback::post(*this, "Dialog cancelled.");
        dismiss();
    }
}

bool ModalOverlay::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        ActionFeedback::post(*this, "Dialog cancelled.");
        dismiss();
        return true;
    }

    return false;
}

juce::Rectangle<int> ModalOverlay::getCentredContentBounds() const {
    const auto contentWidth = juce::jmin(_contentBounds.getWidth(), getWidth() - 40);
    const auto contentHeight = juce::jmin(_contentBounds.getHeight(), getHeight() - 40);

    return juce::Rectangle<int>(juce::jmax(1, contentWidth),
                                juce::jmax(1, contentHeight))
        .withCentre(getLocalBounds().getCentre());
}
