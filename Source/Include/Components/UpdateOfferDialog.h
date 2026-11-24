#pragma once

#include <JuceHeader.h>

class UpdateOfferDialog : public juce::Component,
                          private juce::Timer {
   public:
    UpdateOfferDialog();
    ~UpdateOfferDialog() override;

    void showOffer(const juce::String& version, const juce::StringArray& changes);
    void dismiss();

    std::function<void()> onInstall;
    std::function<void()> onLater;

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

   private:
    enum class Hit {
        None,
        Install,
        Later
    };

    juce::String _version;
    juce::StringArray _changes;
    float _phase = 0.0f;
    float _sink = 0.0f;
    float _cardHover = 0.0f;
    double _lastTickMs = 0.0;
    Hit _hovered = Hit::None;
    Hit _pressed = Hit::None;

    juce::Rectangle<float> cardBounds() const;
    float perimeterOf(juce::Rectangle<float> card, float radius) const;
    juce::Point<float> pointOnCard(juce::Rectangle<float> card, float radius, float distance) const;
    Hit hitAt(juce::Point<int> position) const;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateOfferDialog)
};
