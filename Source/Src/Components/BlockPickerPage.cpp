#include "Components/BlockPickerPage.h"

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
    const TileSpec* items;
    int itemCount;
};

const TileSpec kAmp[] = {
    {"Amp", CustomLookAndFeel::RigIcon::AmpHead, ProfilerStyle::Colors::rigAmp, SignalChain::Stage::Amp}};
const TileSpec kCab[] = {
    {"Cab", CustomLookAndFeel::RigIcon::Cabinet, ProfilerStyle::Colors::rigCab, SignalChain::Stage::Cab}};
const TileSpec kEq[] = {
    {"EQ", CustomLookAndFeel::RigIcon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::Eq},
    {"Parametric", CustomLookAndFeel::RigIcon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::EqParametric},
    {"Tone", CustomLookAndFeel::RigIcon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::EqTone},
    {"Dynamic", CustomLookAndFeel::RigIcon::EqFaders, ProfilerStyle::Colors::rigEq, SignalChain::Stage::EqDynamic}};
const TileSpec kPedal[] = {
    {"Overdrive", CustomLookAndFeel::RigIcon::Pedal, ProfilerStyle::Colors::rigPedal, SignalChain::Stage::Pedal}};
const TileSpec kPitch[] = {
    {"Harmonizer", CustomLookAndFeel::RigIcon::Pitch, ProfilerStyle::Colors::rigPitch, SignalChain::Stage::PitchHarmonizer},
    {"Drop", CustomLookAndFeel::RigIcon::Pitch, ProfilerStyle::Colors::rigPitch, SignalChain::Stage::PitchDrop},
    {"Octaver", CustomLookAndFeel::RigIcon::Pitch, ProfilerStyle::Colors::rigPitch, SignalChain::Stage::PitchOctaver}};
const TileSpec kReverb[] = {
    {"Plate", CustomLookAndFeel::RigIcon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbPlate},
    {"Hall", CustomLookAndFeel::RigIcon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbHall},
    {"Shimmer", CustomLookAndFeel::RigIcon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbShimmer},
    {"Spring", CustomLookAndFeel::RigIcon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbSpring},
    {"Granular", CustomLookAndFeel::RigIcon::Reverb, ProfilerStyle::Colors::rigReverb, SignalChain::Stage::ReverbGranular}};
const TileSpec kDelay[] = {
    {"Tape", CustomLookAndFeel::RigIcon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayTape},
    {"Ping-Pong", CustomLookAndFeel::RigIcon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayPingPong},
    {"Dark", CustomLookAndFeel::RigIcon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayDark},
    {"Reverse", CustomLookAndFeel::RigIcon::Delay, ProfilerStyle::Colors::rigDelay, SignalChain::Stage::DelayReverse}};
const TileSpec kMod[] = {
    {"Ensemble", CustomLookAndFeel::RigIcon::Mod, ProfilerStyle::Colors::rigChorus, SignalChain::Stage::ChorusEnsemble},
    {"Mod Lead", CustomLookAndFeel::RigIcon::Mod, ProfilerStyle::Colors::rigChorus, SignalChain::Stage::ChorusLead},
    {"Phaser 4", CustomLookAndFeel::RigIcon::Mod, ProfilerStyle::Colors::rigPhaser, SignalChain::Stage::Phaser4},
    {"Phaser 8", CustomLookAndFeel::RigIcon::Mod, ProfilerStyle::Colors::rigPhaser, SignalChain::Stage::Phaser8},
    {"Flanger", CustomLookAndFeel::RigIcon::Mod, ProfilerStyle::Colors::rigFlanger, SignalChain::Stage::FlangerSubtle},
    {"Flanger Hard", CustomLookAndFeel::RigIcon::Mod, ProfilerStyle::Colors::rigFlanger, SignalChain::Stage::FlangerHard}};
const TileSpec kDynamics[] = {
    {"Tight", CustomLookAndFeel::RigIcon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::CompBlack},
    {"Pump", CustomLookAndFeel::RigIcon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::CompBrutal},
    {"Comp", CustomLookAndFeel::RigIcon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::CompClear},
    {"Gate", CustomLookAndFeel::RigIcon::Dynamics, ProfilerStyle::Colors::rigDynamics, SignalChain::Stage::NoiseGate}};
const TileSpec kTools[] = {
    {"Tuner", CustomLookAndFeel::RigIcon::Tuner, ProfilerStyle::Colors::rigTuner, SignalChain::Stage::Tuner}};

const CategorySpec kCategories[] = {
    {"Ampli", CustomLookAndFeel::RigIcon::AmpHead, ProfilerStyle::Colors::rigAmp, kAmp, 1},
    {"Cab", CustomLookAndFeel::RigIcon::Cabinet, ProfilerStyle::Colors::rigCab, kCab, 1},
    {"EQ", CustomLookAndFeel::RigIcon::EqFaders, ProfilerStyle::Colors::rigEq, kEq, 4},
    {"Pedals", CustomLookAndFeel::RigIcon::Pedal, ProfilerStyle::Colors::rigPedal, kPedal, 1},
    {"Pitch", CustomLookAndFeel::RigIcon::Pitch, ProfilerStyle::Colors::rigPitch, kPitch, 3},
    {"Reverb", CustomLookAndFeel::RigIcon::Reverb, ProfilerStyle::Colors::rigReverb, kReverb, 5},
    {"Delay", CustomLookAndFeel::RigIcon::Delay, ProfilerStyle::Colors::rigDelay, kDelay, 4},
    {"Modulation", CustomLookAndFeel::RigIcon::Mod, ProfilerStyle::Colors::rigChorus, kMod, 6},
    {"Dynamics", CustomLookAndFeel::RigIcon::Dynamics, ProfilerStyle::Colors::rigDynamics, kDynamics, 4},
    {"Tools", CustomLookAndFeel::RigIcon::Tuner, ProfilerStyle::Colors::rigTuner, kTools, 1}};

bool categoryInUse(const CategorySpec& category, const SignalChain::Layout& layout) {
    for (int index = 0; index < category.itemCount; ++index) {
        if (layout.contains(category.items[index].stage)) {
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
    const auto fill = juce::Colours::black;
    const auto border = _colour.withAlpha(isButtonDown ? 1.0f : (isMouseOverButton ? 0.95f : 0.72f));

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, isMouseOverButton || getToggleState() ? 2.0f : 1.0f);

    auto content = bounds.reduced(10.0f, 8.0f);
    const auto iconSize = juce::jmin(42.0f, content.getHeight() * 0.46f);
    auto iconBounds = content.removeFromTop(iconSize + 6.0f).withSizeKeepingCentre(iconSize, iconSize);
    if (laf != nullptr) {
        laf->drawRigIcon(g, iconBounds, _icon, _colour);
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
    addAndMakeVisible(_backButton);
    addChildComponent(_removeButton);
    addAndMakeVisible(_closeButton);

    for (auto& tile : _tiles) {
        addChildComponent(tile);
    }

    _backButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    _removeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    _closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);

    _backButton.onClick = [this]() {
        showCategories();
    };
    _closeButton.onClick = [this]() {
        close();
    };
    _removeButton.onClick = [this]() {
        const auto slot = _targetSlot;
        if (onBlockRemoved) {
            onBlockRemoved(slot);
        }
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
    _closeButton.setBounds(header.removeFromRight(72).withSizeKeepingCentre(72, 24));
    header.removeFromRight(8);
    if (_removeButton.isVisible()) {
        _removeButton.setBounds(header.removeFromRight(88).withSizeKeepingCentre(88, 24));
        header.removeFromRight(8);
    } else {
        _removeButton.setBounds({});
    }
    _backButton.setBounds(header.removeFromRight(72).withSizeKeepingCentre(72, 24));
    header.removeFromRight(10);
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

void BlockPickerPage::open(int chainSlot, const SignalChain::Layout& layout) {
    _targetSlot = chainSlot;
    _layout = layout;
    showCategories();
    setVisible(true);
    toFront(true);
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
    _removeButton.setVisible(_layout.atSlot(_targetSlot) != SignalChain::Stage::Empty);
    _title.setText("CHOOSE A BLOCK", juce::dontSendNotification);

    const auto count = static_cast<int>(std::size(kCategories));
    for (int index = 0; index < static_cast<int>(_tiles.size()); ++index) {
        auto& tile = _tiles[static_cast<size_t>(index)];
        if (index >= count) {
            tile.setVisible(false);
            tile.onClick = nullptr;
            continue;
        }
        const auto& category = kCategories[index];
        tile.setTile(category.title, category.icon, category.colour, categoryInUse(category, _layout));
        tile.setVisible(true);
        tile.onClick = [this, index]() {
            showItems(index);
        };
    }
    resized();
}

void BlockPickerPage::showItems(int category) {
    if (category < 0 || category >= static_cast<int>(std::size(kCategories))) {
        return;
    }
    _category = category;
    _showingItems = true;
    _backButton.setEnabled(true);
    const auto& spec = kCategories[category];
    _title.setText(juce::String(spec.title).toUpperCase(), juce::dontSendNotification);

    for (int index = 0; index < static_cast<int>(_tiles.size()); ++index) {
        auto& tile = _tiles[static_cast<size_t>(index)];
        if (index >= spec.itemCount) {
            tile.setVisible(false);
            tile.onClick = nullptr;
            continue;
        }
        const auto& item = spec.items[index];
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
    const auto tileW = juce::jlimit(88, 150, (area.getWidth() - gap * (columns - 1)) / columns);
    const auto tileH = juce::jlimit(72, 110, (area.getHeight() - gap * (rows - 1)) / juce::jmax(1, rows));
    const auto gridW = tileW * columns + gap * (columns - 1);
    const auto gridH = tileH * rows + gap * (rows - 1);
    const auto origin = area.getCentre() - juce::Point<int>(gridW / 2, gridH / 2);
    for (int index = 0; index < count; ++index) {
        const auto column = index % columns;
        const auto row = index / columns;
        tiles[static_cast<size_t>(index)]->setBounds(origin.x + column * (tileW + gap),
                                                     origin.y + row * (tileH + gap),
                                                     tileW,
                                                     tileH);
    }
}
