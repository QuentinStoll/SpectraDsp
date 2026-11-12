#pragma once

#include <JuceHeader.h>

#include <vector>

class ProfileLibraryView : public juce::Component {
   public:
    static constexpr const char* importWavId = "import-wav-ir";

    struct Card {
        juce::String id;
        juce::String title;
        juce::String author;
        juce::String summary;
        juce::String badge;
        bool spectra = true;
        bool selected = false;
        bool importWav = false;
    };

    ProfileLibraryView();

    void setCards(std::vector<Card> cards);
    std::function<void(const juce::String& id)> onCardClicked;

    void resized() override;
    void paint(juce::Graphics& g) override;

   private:
    juce::Viewport _viewport;
    juce::Component _row;
    std::vector<std::unique_ptr<juce::Component>> _buttons;
    bool _empty = true;

    void layoutCards();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfileLibraryView)
};
