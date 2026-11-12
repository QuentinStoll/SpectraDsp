#include "Components/BlockPickerPage.h"

#include <initializer_list>
#include <vector>

#include "Stylesheet.h"

namespace {
struct TileSpec {
    const char* title;
    CustomLookAndFeel::RigIcon icon;
    juce::Colour colour;
    SignalChain::Stage stage;
};

struct CategorySpec {
    const char* title;
    CustomLookAndFeel::RigIcon icon;
    juce::Colour colour;
    int firstItem;
    int itemCount;
};

struct Catalog {
    std::vector<TileSpec> items;
    std::vector<CategorySpec> categories;
};

const Catalog& blockCatalog() {
    static const Catalog catalog = [] {
        Catalog built;
        auto add = [&](const char* title,
                       CustomLookAndFeel::RigIcon icon,
                       juce::Colour colour,
                       std::initializer_list<TileSpec> specs) {
            const auto first = static_cast<int>(built.items.size());
            built.items.insert(built.items.end(), specs.begin(), specs.end());
            built.categories.push_back(CategorySpec{title, icon, colour, first, static_cast<int>(specs.size())});
        };

        using Icon = CustomLookAndFeel::RigIcon;
        const auto& colour = ProfilerStyle::Colors::rigAmp;
        add("Ampli", Icon::AmpHead, colour, {{"Amp", Icon::AmpHead, colour, SignalChain::Stage::Amp}});
        add("Cab",
            Icon::Cabinet,
            ProfilerStyle::Colors::rigCab,
            {{"Cab", Icon::Cabinet, ProfilerStyle::Colors::rigCab, SignalChain::Stage::Cab}});
        add("EQ",
            Icon::EqFaders,
            ProfilerStyle::Colors::rigEq,
            {{"EQ", Icon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::Eq},
             {"Parametric", Icon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::EqParametric},
             {"Tone", Icon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::EqTone},
             {"Dynamic", Icon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::EqDynamic}});
        add("Pedals",
            Icon::Pedal,
            ProfilerStyle::Colors::rigPedal,
            {{"Overdrive", Icon::Pedal, ProfilerStyle::Colors::rigPedal, SignalChain::Stage::Pedal}});
        add("Pitch",
            Icon::Pitch,
            ProfilerStyle::Colors::rigPitch,
            {{"Harmonizer", Icon::Pitch, ProfilerStyle::Colors::rigPitch, SignalChain::Stage::PitchHarmonizer},
             {"Drop", Icon::Pitch, ProfilerStyle::Colors::rigPitch, SignalChain::Stage::PitchDrop},
             {"Octaver", Icon::Pitch, ProfilerStyle::Colors::rigPitch, SignalChain::Stage::PitchOctaver}});
        add("Reverb",
            Icon::Reverb,
            ProfilerStyle::Colors::rigReverb,
            {{"Plate", Icon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbPlate},
             {"Hall", Icon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbHall},
             {"Shimmer", Icon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbShimmer},
             {"Spring", Icon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbSpring},
             {"Granular", Icon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbGranular}});
        add("Delay",
            Icon::Delay,
            ProfilerStyle::Colors::rigDelay,
            {{"Tape", Icon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayTape},
             {"Ping-Pong", Icon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayPingPong},
             {"Dark", Icon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayDark},
             {"Reverse", Icon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayReverse}});
        add("Modulation",
            Icon::Mod,
            ProfilerStyle::Colors::rigChorus,
            {{"Ensemble", Icon::Mod, ProfilerStyle::Colors::rigChorus, SignalChain::Stage::ChorusEnsemble},
             {"Mod Lead", Icon::Mod, ProfilerStyle::Colors::rigChorus, SignalChain::Stage::ChorusLead},
             {"Phaser 4", Icon::Mod, ProfilerStyle::Colors::rigPhaser, SignalChain::Stage::Phaser4},
             {"Phaser 8", Icon::Mod, ProfilerStyle::Colors::rigPhaser, SignalChain::Stage::Phaser8},
             {"Flanger", Icon::Mod, ProfilerStyle::Colors::rigFlanger, SignalChain::Stage::FlangerSubtle},
             {"Flanger Hard", Icon::Mod, ProfilerStyle::Colors::rigFlanger, SignalChain::Stage::FlangerHard}});
        add("Dynamics",
            Icon::Dynamics,
            ProfilerStyle::Colors::rigDynamics,
            {{"Tight", Icon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::CompBlack},
             {"Pump", Icon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::CompBrutal},
             {"Comp", Icon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::CompClear},
             {"Gate", Icon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::NoiseGate}});
        add("Tools",
            Icon::Tuner,
            ProfilerStyle::Colors::rigTuner,
            {{"Tuner", Icon::Tuner, ProfilerStyle::Colors::rigTuner, SignalChain::Stage::Tuner}});
        return built;
    }();
    return catalog;
}

bool categoryInUse(const CategorySpec& category, const Catalog& catalog, const SignalChain::Layout& layout) {
    for (int index = 0; index < category.itemCount; ++index) {
        if (layout.contains(catalog.items[static_cast<size_t>(category.firstItem + index)].stage)) {
            return true;
        }
    }
    return false;
}
}  // namespace

BlockPickerPage::TileButton::TileButton()
    : juce::Button({}) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void BlockPickerPage::TileButton::setTile(const juce::String& title,
                                          CustomLookAndFeel::RigIcon icon,
                                          juce::Colour colour,
                                          bool inUse) {
    _title = title;
    _icon = icon;
    _colour = colour;
    _inUse = inUse;
    repaint();
}

void BlockPickerPage::TileButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
    const auto ink = _colour.isTransparent() ? ProfilerStyle::Colors::textMuted : _colour;
    const auto border = ink.withAlpha(isButtonDown ? 1.0f : (isMouseOverButton ? 0.95f : 0.85f));

    g.setColour(juce::Colour(0xff0C0C10));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.6f), 8.0f, isMouseOverButton || getToggleState() ? 2.0f : 1.4f);

    auto content = bounds.reduced(8.0f, 6.0f);
    const auto iconSide = juce::jmin(34.0f, content.getWidth() - 4.0f, juce::jmax(16.0f, content.getHeight() * 0.58f));
    auto iconArea = content.removeFromTop(iconSide);
    auto iconBounds = iconArea.withSizeKeepingCentre(iconSide, iconSide);
    if (laf != nullptr && iconBounds.getWidth() > 8.0f) {
        laf->drawRigIcon(g, iconBounds, _icon, ink);
    } else {
        g.setColour(ink);
        g.drawRoundedRectangle(iconBounds.reduced(2.0f), 3.0f, 1.6f);
    }

    g.setColour(ProfilerStyle::Colors::text);
    g.setFont(ProfilerStyle::Fonts::medium(13.0f));
    g.drawFittedText(_title, content.toNearestInt(), juce::Justification::centred, 1);

    if (_inUse) {
        g.setColour(_colour.withAlpha(0.9f));
        g.fillEllipse(bounds.getRight() - 14.0f, bounds.getY() + 8.0f, 6.0f, 6.0f);
    }
}

BlockPickerPage::BlockPickerPage() {
    _title.setText("CHOOSE A BLOCK", juce::dontSendNotification);
    _title.setFont(ProfilerStyle::Fonts::moduleTitle());
    _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _title.setJustificationType(juce::Justification::centredLeft);
    _title.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(_title);
    addAndMakeVisible(_bypassButton);
    addAndMakeVisible(_clearBlockButton);
    addAndMakeVisible(_clearAllButton);
    addAndMakeVisible(_backButton);
    addAndMakeVisible(_closeButton);

    for (auto& tile : _tiles) {
        addChildComponent(tile);
    }

    for (auto* button : {&_bypassButton, &_clearBlockButton, &_clearAllButton, &_backButton, &_closeButton}) {
        button->setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    }
    _bypassButton.setClickingTogglesState(true);

    _bypassButton.onClick = [this]() {
        if (onBypassChanged) {
            onBypassChanged(_targetSlot, _bypassButton.getToggleState());
        }
    };
    _clearBlockButton.onClick = [this]() {
        const auto slot = _targetSlot;
        if (onBlockRemoved) {
            onBlockRemoved(slot);
        }
        close();
    };
    _clearAllButton.onClick = [this]() {
        if (onClearAll) {
            onClearAll();
        }
        close();
    };
    _backButton.onClick = [this]() {
        showCategories();
    };
    _closeButton.onClick = [this]() {
        close();
    };

    setOpaque(true);
    setVisible(false);
}

void BlockPickerPage::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    g.setColour(ProfilerStyle::Colors::border);
    g.drawRect(getLocalBounds(), 1);
}

void BlockPickerPage::resized() {
    auto area = getLocalBounds().reduced(12, 8);
    auto header = area.removeFromTop(28);
    auto place = [&header](juce::TextButton& button, int width) {
        button.setBounds(header.removeFromRight(width).withSizeKeepingCentre(width, 24));
        header.removeFromRight(8);
    };
    place(_closeButton, 72);
    place(_backButton, 72);
    place(_clearAllButton, 88);
    place(_clearBlockButton, 104);
    place(_bypassButton, 78);
    _title.setBounds(header);
    area.removeFromTop(10);

    std::vector<TileButton*> visible;
    for (auto& tile : _tiles) {
        if (tile.isVisible()) {
            visible.push_back(&tile);
        }
    }
    layoutTiles(area, visible);
}

void BlockPickerPage::open(int chainSlot, const SignalChain::Layout& layout, bool slotBypassed) {
    _targetSlot = chainSlot;
    _layout = layout;
    setSlotBypassed(slotBypassed);
    showCategories();
    setVisible(true);
    toFront(true);
}

void BlockPickerPage::setSlotBypassed(bool bypassed) {
    _bypassButton.setToggleState(bypassed, juce::dontSendNotification);
}

void BlockPickerPage::close() {
    setVisible(false);
    if (onClosed) {
        onClosed();
    }
}

void BlockPickerPage::showCategories() {
    _showingItems = false;
    _backButton.setEnabled(false);
    _title.setText("CHOOSE A BLOCK", juce::dontSendNotification);

    const auto& catalog = blockCatalog();
    const auto count = static_cast<int>(catalog.categories.size());
    for (int index = 0; index < static_cast<int>(_tiles.size()); ++index) {
        auto& tile = _tiles[static_cast<size_t>(index)];
        if (index >= count) {
            tile.setVisible(false);
            tile.onClick = nullptr;
            continue;
        }
        const auto& category = catalog.categories[static_cast<size_t>(index)];
        tile.setTile(category.title, category.icon, category.colour, categoryInUse(category, catalog, _layout));
        tile.setVisible(true);
        tile.onClick = [this, index]() {
            showItems(index);
        };
    }
    resized();
}

void BlockPickerPage::showItems(int category) {
    const auto& catalog = blockCatalog();
    if (category < 0 || category >= static_cast<int>(catalog.categories.size())) {
        return;
    }
    _category = category;
    _showingItems = true;
    _backButton.setEnabled(true);
    const auto& spec = catalog.categories[static_cast<size_t>(category)];
    _title.setText(juce::String(spec.title).toUpperCase(), juce::dontSendNotification);

    for (int index = 0; index < static_cast<int>(_tiles.size()); ++index) {
        auto& tile = _tiles[static_cast<size_t>(index)];
        if (index >= spec.itemCount) {
            tile.setVisible(false);
            tile.onClick = nullptr;
            continue;
        }
        const auto& item = catalog.items[static_cast<size_t>(spec.firstItem + index)];
        tile.setTile(item.title, item.icon, item.colour, _layout.contains(item.stage));
        tile.setVisible(true);
        const auto stage = item.stage;
        tile.onClick = [this, stage]() {
            chooseStage(stage);
        };
    }
    resized();
}

void BlockPickerPage::chooseStage(SignalChain::Stage stage) {
    const auto slot = _targetSlot;
    if (onBlockChosen) {
        onBlockChosen(slot, stage);
    }
    close();
}

void BlockPickerPage::layoutTiles(juce::Rectangle<int> area, const std::vector<TileButton*>& tiles) {
    if (tiles.empty() || area.isEmpty()) {
        return;
    }

    const auto count = static_cast<int>(tiles.size());
    const auto columns = count > 5 ? 5 : count;
    const auto rows = (count + columns - 1) / columns;
    const auto gap = 10;
    const auto tileW = juce::jlimit(1, 156, (area.getWidth() - gap * (columns - 1)) / columns);
    const auto tileH = juce::jlimit(1, 108, (area.getHeight() - gap * (rows - 1)) / juce::jmax(1, rows));
    const auto gridW = tileW * columns + gap * (columns - 1);
    const auto gridH = tileH * rows + gap * (rows - 1);
    const auto originX = area.getX() + juce::jmax(0, (area.getWidth() - gridW) / 2);
    const auto originY = area.getY() + juce::jmax(0, (area.getHeight() - gridH) / 2);
    for (int index = 0; index < count; ++index) {
        const auto column = index % columns;
        const auto row = index / columns;
        tiles[static_cast<size_t>(index)]->setBounds(originX + column * (tileW + gap),
                                                     originY + row * (tileH + gap),
                                                     tileW,
                                                     tileH);
        tiles[static_cast<size_t>(index)]->toFront(false);
    }
}
