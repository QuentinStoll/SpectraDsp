#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "MarketplaceClient.h"
#include "Views/MarketplaceAccountPanel.h"

class MarketplaceGridPanel : public juce::Component {
   public:
    struct Entry {
        Marketplace::LibraryItem item;
        bool downloaded = false;
    };

    MarketplaceGridPanel();
    ~MarketplaceGridPanel() override;

    void setContent(MarketplaceAccountPanel::LibraryState state,
                    std::vector<Entry> entries,
                    const juce::String& message,
                    bool busy);

    std::function<void(const juce::String& id)> onDownloadClicked;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    class Card;

    juce::Label _title;
    juce::Label _countLabel;
    juce::Label _messageLabel;
    juce::Viewport _viewport;
    juce::Component _cardHost;
    std::vector<std::unique_ptr<Card>> _cards;
    MarketplaceAccountPanel::LibraryState _state = MarketplaceAccountPanel::LibraryState::SignedOut;
    juce::String _message;

    juce::String emptyText() const;
    void layoutCards();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarketplaceGridPanel)
};
