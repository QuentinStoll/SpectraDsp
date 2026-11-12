#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

#include "Components/CustomLookAndFeel.h"
#include "SignalChainLayout.h"

class BlockPickerPage : public juce::Component {
   public:
    BlockPickerPage();
    ~BlockPickerPage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void open(int chainSlot, const SignalChain::Layout& layout, bool slotBypassed);
    void close();
    bool isOpen() const noexcept { return isVisible(); }
    int targetSlot() const noexcept { return _targetSlot; }
    void setSlotBypassed(bool bypassed);

    std::function<void(int slot, SignalChain::Stage stage)> onBlockChosen;
    std::function<void(int slot)> onBlockRemoved;
    std::function<void()> onClearAll;
    std::function<void(int slot, bool bypassed)> onBypassChanged;
    std::function<void()> onClosed;

   private:
    class TileButton : public juce::Button {
       public:
        TileButton();
        void setTile(const juce::String& title,
                     CustomLookAndFeel::RigIcon icon,
                     juce::Colour colour,
                     bool inUse);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

       private:
        juce::String _title;
        CustomLookAndFeel::RigIcon _icon = CustomLookAndFeel::RigIcon::AmpHead;
        juce::Colour _colour = juce::Colours::white;
        bool _inUse = false;
    };

    juce::Label _title;
    juce::TextButton _bypassButton{"Bypass"};
    juce::TextButton _clearBlockButton{"Clear block"};
    juce::TextButton _clearAllButton{"Clear all"};
    juce::TextButton _backButton{"Back"};
    juce::TextButton _closeButton{"Close"};
    std::array<TileButton, 10> _tiles;
    int _targetSlot = SignalChain::chainSlotForMovableIndex(0);
    SignalChain::Layout _layout{};
    bool _showingItems = false;
    int _category = 0;

    void showCategories();
    void showItems(int category);
    void chooseStage(SignalChain::Stage stage);
    void layoutTiles(juce::Rectangle<int> area, const std::vector<TileButton*>& tiles);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlockPickerPage)
};
