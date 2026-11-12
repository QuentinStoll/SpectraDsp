#include "Components/SignalChainStrip.h"

#include <cmath>
#include <limits>

#include "Fx/FxAppearance.h"

namespace {
constexpr int kSignalChainRadioGroup = 0x50524F46;  // "PROF"
constexpr float kMeterFloorDb = -54.0f;
constexpr float kMeterCeilDb = -3.0f;
constexpr float kMeterAttackSeconds = 0.045f;
constexpr float kMeterReleaseSeconds = 0.22f;
constexpr int kDragThresholdPx = 6;
constexpr int kChangeBlockItem = 1;
constexpr int kClearAllBlocksItem = 2;
constexpr int kDragDownThresholdPx = 26;

float meterFromDb(float db) {
    const auto normalised = juce::jmap(juce::jlimit(kMeterFloorDb, kMeterCeilDb, db),
                                       kMeterFloorDb, kMeterCeilDb, 0.0f, 1.0f);
    return std::pow(normalised, 0.62f);
}

float smoothMeter(float current, float target, float deltaSeconds) {
    const auto tau = target > current ? kMeterAttackSeconds : kMeterReleaseSeconds;
    const auto coeff = 1.0f - std::exp(-deltaSeconds / tau);
    return current + (target - current) * coeff;
}

struct EffectAppearance {
    juce::Colour colour;
    CustomLookAndFeel::RigIcon icon;
};

EffectAppearance appearanceFor(SignalChain::Stage stage) {
    const auto appearance = Fx::appearanceFor(stage);
    return {appearance.colour, appearance.icon};
}
}  // namespace

SignalChainBlock::SignalChainBlock()
    : SignalChainBlock(juce::Colours::white, CustomLookAndFeel::RigIcon::AmpHead) {}

SignalChainBlock::SignalChainBlock(juce::Colour categoryColour, CustomLookAndFeel::RigIcon icon)
    : juce::Button({}),
      _categoryColour(categoryColour),
      _icon(icon) {
    setClickingTogglesState(true);
    setRadioGroupId(kSignalChainRadioGroup);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setOpaque(true);
}

void SignalChainBlock::setAppearance(juce::Colour categoryColour, CustomLookAndFeel::RigIcon icon) {
    if (_categoryColour == categoryColour && _icon == icon) {
        return;
    }

    _categoryColour = categoryColour;
    _icon = icon;
    repaint();
}

void SignalChainBlock::setCaption(const juce::String& caption) {
    if (_caption == caption) {
        return;
    }
    _caption = caption;
    repaint();
}

void SignalChainBlock::setLedOn(bool shouldBeOn) {
    if (_ledOn == shouldBeOn) {
        return;
    }

    _ledOn = shouldBeOn;
    repaint();
}

void SignalChainBlock::setIoNode(bool isIoNode) {
    _isIoNode = isIoNode;
    setOpaque(!isIoNode);
    setMouseCursor(_isIoNode ? juce::MouseCursor::PointingHandCursor
                             : juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void SignalChainBlock::setShowsLed(bool shouldShowLed) {
    _showsLed = shouldShowLed;
    repaint();
}

void SignalChainBlock::setFlowMark(FlowMark mark) {
    _flowMark = mark;
    if (mark != FlowMark::None) {
        setRadioGroupId(0);
        setClickingTogglesState(false);
        setInterceptsMouseClicks(false, false);
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    repaint();
}

void SignalChainBlock::setSignalLevel(float level) {
    const auto clamped = juce::jlimit(0.0f, 1.0f, level);
    if (std::abs(_signalLevel - clamped) < 0.001f) {
        return;
    }

    _signalLevel = clamped;
    if (_isIoNode) {
        repaint();
    }
}

juce::Rectangle<float> SignalChainBlock::getLedBounds() const {
    const auto bounds = getLocalBounds().toFloat();
    return _isIoNode ? CustomLookAndFeel::getSignalIoLedBounds(bounds)
                     : CustomLookAndFeel::getSignalChainLedBounds(bounds);
}

bool SignalChainBlock::isLedHit(juce::Point<int> position) const {
    if (!_showsLed) {
        return false;
    }

    return getLedBounds().expanded(4.0f).contains(position.toFloat());
}

bool SignalChainBlock::hitTest(int x, int y) {
    if (isLedHit({x, y})) {
        return true;
    }

    if (!_isIoNode) {
        return juce::Button::hitTest(x, y);
    }

    const auto bounds = getLocalBounds().toFloat();
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.31f;
    return bounds.getCentre().getDistanceFrom({static_cast<float>(x), static_cast<float>(y)}) <= radius + 3.0f;
}

void SignalChainBlock::mouseDown(const juce::MouseEvent& event) {
    _ledPressActive = false;
    if (event.mods.isPopupMenu()) {
        return;
    }

    _ledPressActive = isLedHit(event.getPosition());
    if (_ledPressActive) {
        return;
    }

    juce::Button::mouseDown(event);
    if (_isIoNode || onDragBegin == nullptr) {
        return;
    }

    onDragBegin(event);
}

void SignalChainBlock::mouseDrag(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() || _ledPressActive) {
        return;
    }

    juce::Button::mouseDrag(event);
    if (onDragMove != nullptr) {
        onDragMove(event);
    }
}

void SignalChainBlock::mouseUp(const juce::MouseEvent& event) {
    if (_ledPressActive) {
        _ledPressActive = false;
        if (!event.mods.isPopupMenu() && event.mouseWasClicked() && isLedHit(event.getPosition()) && onLedClicked) {
            onLedClicked();
        }
        return;
    }

    if (event.mods.isPopupMenu()) {
        if (event.mouseWasClicked() && onPickerRequested) {
            onPickerRequested();
        }
        return;
    }

    if (onDragEnd != nullptr) {
        onDragEnd(event);
    }

    juce::Button::mouseUp(event);
}

void SignalChainBlock::paint(juce::Graphics& g) {
    paintContents(g, isMouseOverOrDragging());
}

void SignalChainBlock::paintButton(juce::Graphics&, bool, bool) {}

void SignalChainBlock::paintContents(juce::Graphics& g, bool isMouseOverButton) {
    if (!_isIoNode) {
        g.fillAll(juce::Colours::black);
    }

    if (auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel())) {
        if (_isIoNode) {
            laf->drawSignalIoNode(g, getLocalBounds().toFloat(), getToggleState(), isMouseOverButton, _ledOn, _showsLed, _signalLevel);
            paintFlowMark(g);
            return;
        }

        laf->drawSignalChainBlock(g,
                                  getLocalBounds().toFloat(),
                                  _categoryColour,
                                  _icon,
                                  getToggleState(),
                                  isMouseOverButton,
                                  _ledOn);
        if (_caption.isNotEmpty()) {
            g.setFont(ProfilerStyle::Fonts::micro());
            g.setColour(ProfilerStyle::Contrast::foreground(_categoryColour.withAlpha(_ledOn ? 0.95f : 0.45f)));
            g.drawFittedText(_caption, getLocalBounds().removeFromBottom(13).reduced(2, 0), juce::Justification::centred, 1);
        }
        return;
    }

    ProfilerStyle::Surfaces::fillPanel(g, getLocalBounds().toFloat());
}

void SignalChainBlock::paintFlowMark(juce::Graphics& g) const {
    if (_flowMark == FlowMark::None) {
        return;
    }

    const auto bounds = getLocalBounds().toFloat();
    const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto arm = side * 0.16f;
    const auto centre = bounds.getCentre();
    juce::Path path;
    if (_flowMark == FlowMark::Down) {
        path.startNewSubPath(centre.x - arm, centre.y - arm * 0.35f);
        path.lineTo(centre.x, centre.y + arm * 0.55f);
        path.lineTo(centre.x + arm, centre.y - arm * 0.35f);
    } else {
        path.startNewSubPath(centre.x - arm * 0.35f, centre.y - arm);
        path.lineTo(centre.x + arm * 0.55f, centre.y);
        path.lineTo(centre.x - arm * 0.35f, centre.y + arm);
    }

    g.setColour(juce::Colour(0xffF4F4F5).withAlpha(0.95f));
    g.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

SignalChainStrip::RouteNode::RouteNode() : juce::Button({}) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void SignalChainStrip::RouteNode::paintButton(juce::Graphics& g, bool isMouseOverButton, bool) {
    auto bounds = getLocalBounds().toFloat().reduced(7.0f);
    g.setColour(isMouseOverButton ? ProfilerStyle::Colors::accent : juce::Colour(0xffF4F4F5));
    g.fillRoundedRectangle(bounds, 2.0f);
}

void SignalChainStrip::SignalBusLayer::setRows(juce::Point<float> input,
                                               juce::Point<float> down,
                                               juce::Point<float> join,
                                               juce::Point<float> output) {
    _input = input;
    _down = down;
    _join = join;
    _output = output;
    _ready = true;
}

void SignalChainStrip::SignalBusLayer::paint(juce::Graphics& g) {
    if (!_ready || (_input.isOrigin() && _down.isOrigin())) {
        return;
    }

    auto draw = [this, &g](juce::Point<float> from, juce::Point<float> to) {
        if (auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel())) {
            laf->drawSignalBus(g, from, to);
            return;
        }

        g.setColour(ProfilerStyle::Contrast::foreground(ProfilerStyle::Colors::caption));
        g.drawLine(from.x, from.y, to.x, to.y, 1.2f);
    };
    draw(_input, _down);
    draw(_join, _output);
}

SignalChainStrip::SignalChainStrip() {
    addAndMakeVisible(_busLayer);
    _busLayer.setInterceptsMouseClicks(false, false);

    auto configure = [this](SignalChainBlock& block, BlockId id) {
        addAndMakeVisible(block);
        block.onClick = [this, id]() {
            selectChainSlot(id == BlockId::MasterVolume ? SignalChain::outputSlot : SignalChain::inputSlot);
        };
        block.onLedClicked = [this, id]() {
            setSelectedBlock(id);
            if (onBlockBypassToggled) {
                onBlockBypassToggled(id);
            }
        };
    };

    configure(_inputGate, BlockId::InputGate);
    configure(_masterVolume, BlockId::MasterVolume);

    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        auto& block = _effects[static_cast<size_t>(index)];
        addAndMakeVisible(block);
        block.onClick = [this, index]() {
            handleEffectClick(index);
        };
        block.onLedClicked = [this, index]() {
            handleEffectClick(index);
            if (onBlockBypassToggled) {
                onBlockBypassToggled(blockForStage(_layout.atSlot(SignalChain::chainSlotForMovableIndex(index))));
            }
        };
        block.onPickerRequested = [this, index]() {
            showBlockMenu(SignalChain::chainSlotForMovableIndex(index));
        };
        block.onDragBegin = [this, index](const juce::MouseEvent& event) {
            startEffectDrag(index, event);
        };
        block.onDragMove = [this](const juce::MouseEvent& event) {
            updateBlockDrag(event);
        };
        block.onDragEnd = [this](const juce::MouseEvent& event) {
            finishBlockDrag(event);
        };
    }

    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        auto& block = _pathB[static_cast<size_t>(index)];
        addAndMakeVisible(block);
        block.setVisible(false);
        block.onClick = [this, index]() { selectChainSlot(SignalChain::pathBSlotForIndex(index)); };
        block.onLedClicked = [this, index]() {
            selectChainSlot(SignalChain::pathBSlotForIndex(index));
            if (onBlockBypassToggled) {
                onBlockBypassToggled(blockForStage(_route.pathBAt(index)));
            }
        };
        block.onPickerRequested = [this, index]() {
            showBlockMenu(SignalChain::pathBSlotForIndex(index));
        };
        block.onDragBegin = [this, index](const juce::MouseEvent& event) {
            _dragPathBIndex = index;
            _dragMovableIndex = -1;
            _dragChainSlot = -1;
            _dragTracking = true;
            _dragActive = false;
            _dragToPathB = false;
            _dropSlot = -1;
            _dragStart = event.getEventRelativeTo(this).getPosition();
            _dragOrigin = _pathB[static_cast<size_t>(index)].getBounds();
        };
        block.onDragMove = [this](const juce::MouseEvent& event) { updateBlockDrag(event); };
        block.onDragEnd = [this](const juce::MouseEvent& event) { finishBlockDrag(event); };
    }

    addAndMakeVisible(_splitNode);
    addAndMakeVisible(_mergeNode);
    _splitNode.setVisible(false);
    _mergeNode.setVisible(false);
    _splitNode.onClick = [this]() {
        if (onEditRouteMix) {
            onEditRouteMix();
        }
    };
    _mergeNode.onClick = _splitNode.onClick;

    auto addFlowNode = [](SignalChainBlock& block, SignalChainBlock::FlowMark mark) {
        block.setIoNode(true);
        block.setShowsLed(false);
        block.setFlowMark(mark);
    };
    addAndMakeVisible(_pathDown);
    addAndMakeVisible(_pathReturn);
    addFlowNode(_pathDown, SignalChainBlock::FlowMark::Down);
    addFlowNode(_pathReturn, SignalChainBlock::FlowMark::Enter);

    _inputGate.setIoNode(true);
    _masterVolume.setIoNode(true);
    _masterVolume.setShowsLed(false);
    updateBlockSelection();
    _busLayer.toBack();
    setOpaque(true);
    setWantsKeyboardFocus(true);
}

void SignalChainStrip::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());

    for (int slot = 0; slot < kSlotCount; ++slot) {
        if (isOccupiedSlot(slot) || _slotBounds[static_cast<size_t>(slot)].isEmpty()) {
            continue;
        }

        if (laf != nullptr) {
            laf->drawEmptySignalSlot(g, _slotBounds[static_cast<size_t>(slot)].toFloat());
        } else {
            g.setColour(ProfilerStyle::Contrast::outline(ProfilerStyle::Colors::border.withAlpha(0.55f)));
            auto outline = _slotBounds[static_cast<size_t>(slot)].toFloat();
            const auto side = juce::jmin(outline.getWidth(), outline.getHeight()) / 2.5f;
            g.drawRoundedRectangle(outline.withSizeKeepingCentre(side, side), 3.0f, 1.2f);
        }
    }

    if (SignalChain::isMovableSlot(_dropSlot)) {
        auto dropBounds = _slotBounds[static_cast<size_t>(_dropSlot)].toFloat().reduced(4.0f);
        g.setColour(ProfilerStyle::Contrast::foreground(ProfilerStyle::Colors::text.withAlpha(0.72f)));
        g.drawRoundedRectangle(dropBounds, 6.0f, 1.6f);
    }

    if (!_route.active() || !_splitNode.isVisible()) {
        return;
    }

    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        if (_route.pathBAt(index) != SignalChain::Stage::Empty) {
            continue;
        }
        const auto bounds = _pathBBounds[static_cast<size_t>(index)];
        if (bounds.isEmpty()) {
            continue;
        }
        if (laf != nullptr) {
            laf->drawEmptySignalSlot(g, bounds.toFloat());
        }
    }

    const auto split = _splitNode.getBounds().toFloat().getCentre();
    const auto merge = _mergeNode.getBounds().toFloat().getCentre();
    const auto laneY = static_cast<float>(_laneTop) + static_cast<float>(_laneSquare) * 0.5f;
    g.setColour(juce::Colour(0xffF4F4F5).withAlpha(0.9f));
    g.drawLine(split.x, split.y, split.x, laneY, 1.3f);
    g.drawLine(split.x, laneY, merge.x, laneY, 1.3f);
    g.drawLine(merge.x, laneY, merge.x, merge.y, 1.3f);
}

int SignalChainStrip::parallelRow() const noexcept {
    if (!_route.active()) {
        return -1;
    }
    const auto first = juce::jmax(0, _route.splitAfter + 1);
    return SignalChain::visualRow(SignalChain::chainSlotForMovableIndex(first));
}

void SignalChainStrip::resized() {
    auto area = getLocalBounds().reduced(4, 8);
    const auto laneRow = parallelRow();
    const auto bands = laneRow >= 0 ? 3 : SignalChain::rowCount;
    const auto slotWidth = area.getWidth() / static_cast<float>(SignalChain::columnsPerRow);
    constexpr int kRowGap = 16;
    const auto squareByWidth = juce::jlimit(44, 78, juce::roundToInt(slotWidth) - 16);
    const auto squareByHeight = juce::jmax(34, (area.getHeight() - kRowGap * (bands - 1)) / bands);
    const auto square = juce::jmin(squareByWidth, squareByHeight);
    const auto contentHeight = square * bands + kRowGap * (bands - 1);
    const auto top = area.getY() + juce::jmax(0, (area.getHeight() - contentHeight) / 2);
    const auto bandY = [top, square](int band) { return top + band * (square + kRowGap); };

    int serialBand[2] = {0, 1};
    if (laneRow == 0) {
        serialBand[1] = 2;
        _laneTop = bandY(1);
    } else if (laneRow == 1) {
        serialBand[1] = 1;
        _laneTop = bandY(2);
    } else {
        _laneTop = 0;
    }
    _laneSquare = laneRow >= 0 ? square : 0;

    for (int chainSlot = 0; chainSlot < SignalChain::slotCount; ++chainSlot) {
        const auto row = SignalChain::visualRow(chainSlot);
        const auto column = SignalChain::visualColumn(chainSlot);
        const auto centreX = juce::roundToInt(static_cast<float>(area.getX()) + slotWidth * (static_cast<float>(column) + 0.5f));
        const auto y = bandY(serialBand[row]);
        _slotBounds[static_cast<size_t>(chainSlot)] = {centreX - square / 2, y, square, square};
    }

    placeBlocks();
    updateBusLayer();
    _busLayer.toBack();
}

void SignalChainStrip::mouseUp(const juce::MouseEvent& event) {
    if (!event.mods.isPopupMenu() || !event.mouseWasClicked()) {
        return;
    }

    const auto slot = slotAtPosition(event.getPosition());
    if (SignalChain::isMovableSlot(slot) || SignalChain::isPathBSlot(slot)) {
        if (SignalChain::isPathBSlot(slot) && _route.pathBAt(SignalChain::pathBIndexForSlot(slot)) != SignalChain::Stage::Empty) {
            showBlockMenu(slot);
            return;
        }
        requestPicker(slot);
    }
}

void SignalChainStrip::placeBlocks() {
    const auto square = _slotBounds[static_cast<size_t>(SignalChain::inputSlot)].getWidth();
    const auto ioHit = juce::jlimit(26, 34, juce::roundToInt(static_cast<float>(juce::jmax(1, square)) * 0.38f));
    auto placeIo = [&](SignalChainBlock& block, int slot) {
        const auto bounds = _slotBounds[static_cast<size_t>(slot)];
        block.setVisible(!bounds.isEmpty());
        if (!bounds.isEmpty()) {
            block.setBounds(bounds.withSizeKeepingCentre(ioHit, ioHit));
        }
    };

    placeIo(_inputGate, SignalChain::inputSlot);
    placeIo(_pathDown, SignalChain::downSlot);
    placeIo(_pathReturn, SignalChain::returnSlot);
    placeIo(_masterVolume, SignalChain::outputSlot);

    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        const auto stage = _layout.atSlot(slot);
        auto& block = _effects[static_cast<size_t>(index)];
        const auto onChain = stage != SignalChain::Stage::Empty;
        block.setVisible(onChain);
        if (!onChain || (_dragActive && slot == _dragChainSlot)) {
            continue;
        }

        const auto appearance = appearanceFor(stage);
        block.setAppearance(appearance.colour, appearance.icon);
        block.setCaption(Fx::captionFor(stage));
        block.setBounds(_slotBounds[static_cast<size_t>(slot)]);
    }
    layoutParallelLane();
    updateBlockSelection();
}

void SignalChainStrip::layoutParallelLane() {
    _pathBBounds.fill({});
    if (!_route.active() || _laneSquare <= 0) {
        _splitNode.setVisible(false);
        _mergeNode.setVisible(false);
        for (auto& block : _pathB) {
            block.setVisible(false);
        }
        return;
    }

    const auto first = juce::jmax(0, _route.splitAfter + 1);
    auto last = _route.mergeAfter;
    const auto row = SignalChain::visualRow(SignalChain::chainSlotForMovableIndex(first));
    while (last > first && SignalChain::visualRow(SignalChain::chainSlotForMovableIndex(last)) != row) {
        --last;
    }
    auto centreOf = [this](int index) {
        return _slotBounds[static_cast<size_t>(SignalChain::chainSlotForMovableIndex(index))].getCentre();
    };
    const auto anchor = centreOf(first);
    const auto current = centreOf(last);
    const auto before = _route.splitAfter >= 0 &&
                                SignalChain::visualRow(SignalChain::chainSlotForMovableIndex(_route.splitAfter)) == row
                            ? centreOf(_route.splitAfter)
                            : _slotBounds[static_cast<size_t>(row == 0 ? SignalChain::inputSlot : SignalChain::returnSlot)]
                                  .getCentre();
    const auto after = last + 1 < SignalChain::movableSlotCount &&
                               SignalChain::visualRow(SignalChain::chainSlotForMovableIndex(last + 1)) == row
                           ? centreOf(last + 1)
                           : _slotBounds[static_cast<size_t>(row == 0 ? SignalChain::downSlot : SignalChain::outputSlot)]
                                 .getCentre();
    const auto split = juce::Point<int>((before.x + anchor.x) / 2, anchor.y);
    const auto merge = juce::Point<int>((current.x + after.x) / 2, current.y);
    constexpr int node = 28;
    _splitNode.setBounds(split.x - node / 2, split.y - node / 2, node, node);
    _mergeNode.setBounds(merge.x - node / 2, merge.y - node / 2, node, node);
    _splitNode.setVisible(true);
    _mergeNode.setVisible(true);
    _splitNode.toFront(false);
    _mergeNode.toFront(false);

    int shown = _route.occupiedPathB();
    if (shown < SignalChain::ParallelRoute::pathBCount) {
        ++shown;
    }
    shown = juce::jmax(1, shown);
    const auto span = juce::jmax(1, merge.x - split.x);
    auto blockSize = juce::jmin(_laneSquare, (span / shown) - 8);
    if (blockSize < 30 && shown > _route.occupiedPathB()) {
        shown = juce::jmax(1, _route.occupiedPathB());
        blockSize = juce::jmin(_laneSquare, (span / shown) - 8);
    }
    blockSize = juce::jmax(28, blockSize);
    const auto step = span / shown;
    int visual = 0;
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        auto& block = _pathB[static_cast<size_t>(index)];
        const auto stage = _route.pathBAt(index);
        if (stage == SignalChain::Stage::Empty) {
            block.setVisible(false);
            continue;
        }
        const auto centreX = split.x + step * visual + step / 2;
        _pathBBounds[static_cast<size_t>(index)] = {centreX - blockSize / 2, _laneTop, blockSize, blockSize};
        ++visual;
        if (_dragActive && _dragPathBIndex == index) {
            block.setVisible(true);
            continue;
        }
        const auto appearance = appearanceFor(stage);
        block.setAppearance(appearance.colour, appearance.icon);
        block.setCaption(Fx::captionFor(stage));
        block.setBounds(_pathBBounds[static_cast<size_t>(index)]);
        block.setVisible(true);
    }
    if (visual < shown) {
        const auto centreX = split.x + step * visual + step / 2;
        for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
            if (_route.pathBAt(index) == SignalChain::Stage::Empty) {
                _pathBBounds[static_cast<size_t>(index)] = {centreX - blockSize / 2, _laneTop, blockSize, blockSize};
                break;
            }
        }
    }
}

void SignalChainStrip::updateBusLayer() {
    _busLayer.setBounds(getLocalBounds());
    auto centre = [this](int slot) {
        return _slotBounds[static_cast<size_t>(slot)].toFloat().getCentre();
    };

    const auto input = centre(SignalChain::inputSlot);
    const auto down = centre(SignalChain::downSlot);
    const auto join = centre(SignalChain::returnSlot);
    const auto output = centre(SignalChain::outputSlot);
    if (input.isOrigin() && down.isOrigin()) {
        return;
    }

    _busLayer.setRows(input, down, join, output);
}

void SignalChainStrip::selectSlot(int chainSlot) {
    selectChainSlot(chainSlot);
}

void SignalChainStrip::setSlotLed(int chainSlot, bool isOn) {
    if (auto* block = effectBlockAtSlot(chainSlot)) {
        block->setLedOn(isOn);
    }
}

void SignalChainStrip::setSelectedBlock(BlockId blockId) {
    if (!isBlockOnChain(blockId)) {
        return;
    }

    selectChainSlot(slotForBlock(blockId));
}

bool SignalChainStrip::selectAdjacentBlock(int delta) {
    if (delta == 0) {
        return false;
    }

    std::array<int, 16> order{};
    const auto blockCount = fillChainOrder(order);
    if (blockCount <= 0) {
        return false;
    }

    int currentIndex = 0;
    for (int index = 0; index < blockCount; ++index) {
        if (order[static_cast<size_t>(index)] == _selectedChainSlot) {
            currentIndex = index;
            break;
        }
    }

    const auto next = (currentIndex + delta % blockCount + blockCount) % blockCount;
    selectChainSlot(order[static_cast<size_t>(next)]);
    return true;
}

bool SignalChainStrip::selectBlockInDirection(int columnDelta, int rowDelta) {
    if (columnDelta == 0 && rowDelta == 0) {
        return false;
    }

    std::array<int, 16> order{};
    const auto blockCount = fillChainOrder(order);
    const auto originSlot = _selectedChainSlot;
    const auto originRow = SignalChain::visualRow(originSlot);
    const auto originColumn = SignalChain::visualColumn(originSlot);

    int bestIndex = -1;
    auto bestScore = std::numeric_limits<int>::max();
    for (int index = 0; index < blockCount; ++index) {
        const auto slot = order[static_cast<size_t>(index)];
        if (slot == originSlot) {
            continue;
        }

        const auto dColumn = SignalChain::visualColumn(slot) - originColumn;
        const auto dRow = SignalChain::visualRow(slot) - originRow;
        if (columnDelta != 0) {
            if (dRow != 0 || dColumn * columnDelta <= 0) {
                continue;
            }
            const auto score = std::abs(dColumn);
            if (score < bestScore) {
                bestScore = score;
                bestIndex = index;
            }
            continue;
        }

        if (dRow * rowDelta <= 0) {
            continue;
        }
        const auto score = std::abs(dRow) * 16 + std::abs(dColumn);
        if (score < bestScore) {
            bestScore = score;
            bestIndex = index;
        }
    }

    if (bestIndex < 0) {
        return false;
    }

    selectChainSlot(order[static_cast<size_t>(bestIndex)]);
    return true;
}

bool SignalChainStrip::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        if ((!SignalChain::isMovableSlot(_selectedChainSlot) && SignalChain::pathBIndexForSlot(_selectedChainSlot) < 0) || selectedStage() == SignalChain::Stage::Empty || !onBlockRemoved) {
            return false;
        }

        onBlockRemoved(_selectedChainSlot);
        return true;
    }

    if (key == juce::KeyPress::leftKey) {
        return selectBlockInDirection(-1, 0);
    }
    if (key == juce::KeyPress::rightKey) {
        return selectBlockInDirection(1, 0);
    }
    if (key == juce::KeyPress::upKey) {
        return selectBlockInDirection(0, -1);
    }
    if (key == juce::KeyPress::downKey) {
        return selectBlockInDirection(0, 1);
    }

    return false;
}

void SignalChainStrip::setBlockLed(BlockId blockId, bool isOn) {
    if (blockId == BlockId::InputGate) {
        _inputGate.setLedOn(isOn);
        return;
    }
    if (blockId == BlockId::MasterVolume) {
        _masterVolume.setLedOn(isOn);
        return;
    }

    const auto stage = stageForBlock(blockId);
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        if (_layout.atSlot(SignalChain::chainSlotForMovableIndex(index)) == stage) {
            _effects[static_cast<size_t>(index)].setLedOn(isOn);
        }
    }
}

void SignalChainStrip::setIoMeterLevels(float inputDb, float outputDb) {
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto deltaSeconds = _lastMeterMs > 0.0
                                  ? static_cast<float>(juce::jlimit(0.001, 0.05, (nowMs - _lastMeterMs) * 0.001))
                                  : (1.0f / 60.0f);
    _lastMeterMs = nowMs;

    _inputMeter = smoothMeter(_inputMeter, meterFromDb(inputDb), deltaSeconds);
    _outputMeter = smoothMeter(_outputMeter, meterFromDb(outputDb), deltaSeconds);
    _inputGate.setSignalLevel(_inputMeter);
    _masterVolume.setSignalLevel(_outputMeter);
}

void SignalChainStrip::setRoute(const SignalChain::ParallelRoute& route) {
    const auto previousRow = parallelRow();
    _route = route;
    if (previousRow != parallelRow()) {
        resized();
    } else {
        placeBlocks();
        repaint();
    }
}

SignalChain::Stage SignalChainStrip::selectedStage() const noexcept {
    const auto pathB = SignalChain::pathBIndexForSlot(_selectedChainSlot);
    if (pathB >= 0) {
        return _route.pathBAt(pathB);
    }
    return _layout.atSlot(_selectedChainSlot);
}

void SignalChainStrip::setLayout(const SignalChain::Layout& layout) {
    _layout = layout.isValid() ? layout : SignalChain::Layout{};
    if (SignalChain::isMovableSlot(_selectedChainSlot) && _layout.atSlot(_selectedChainSlot) == SignalChain::Stage::Empty) {
        const auto fallback = _layout.slotFor(stageForBlock(_selected));
        if (fallback >= 0) {
            _selectedChainSlot = fallback;
        } else {
            selectChainSlot(SignalChain::inputSlot);
        }
    } else if (SignalChain::pathBIndexForSlot(_selectedChainSlot) < 0 && !isBlockOnChain(_selected)) {
        selectChainSlot(SignalChain::inputSlot);
    }
    placeBlocks();
    updateBusLayer();
    repaint();
}

bool SignalChainStrip::isBlockOnChain(BlockId blockId) const noexcept {
    return slotForBlock(blockId) >= 0;
}

SignalChainBlock* SignalChainStrip::effectBlockAtSlot(int chainSlot) noexcept {
    const auto pathB = SignalChain::pathBIndexForSlot(chainSlot);
    if (pathB >= 0) {
        return &_pathB[static_cast<size_t>(pathB)];
    }
    const int index = SignalChain::movableIndexForSlot(chainSlot);
    if (index < 0) {
        return nullptr;
    }
    return &_effects[static_cast<size_t>(index)];
}

void SignalChainStrip::selectChainSlot(int chainSlot) {
    const auto pathBIndex = SignalChain::pathBIndexForSlot(chainSlot);
    if (chainSlot != SignalChain::inputSlot && chainSlot != SignalChain::outputSlot &&
        (pathBIndex >= 0 ? _route.pathBAt(pathBIndex) == SignalChain::Stage::Empty
                         : !SignalChain::isMovableSlot(chainSlot) || _layout.atSlot(chainSlot) == SignalChain::Stage::Empty)) {
        return;
    }

    _selectedChainSlot = chainSlot;
    if (chainSlot == SignalChain::inputSlot) {
        _selected = BlockId::InputGate;
    } else if (chainSlot == SignalChain::outputSlot) {
        _selected = BlockId::MasterVolume;
    } else {
        const auto pathB = SignalChain::pathBIndexForSlot(chainSlot);
        const auto stage = pathB >= 0 ? _route.pathBAt(pathB) : _layout.atSlot(chainSlot);
        if (stage == SignalChain::Stage::Empty) {
            return;
        }
        _selected = blockForStage(stage);
        if (auto* block = effectBlockAtSlot(chainSlot)) {
            block->setToggleState(true, juce::dontSendNotification);
        }
    }

    updateBlockSelection();
    handleBlockClick(_selected);
}

void SignalChainStrip::updateBlockSelection() {
    _inputGate.setToggleState(_selectedChainSlot == SignalChain::inputSlot, juce::dontSendNotification);
    _masterVolume.setToggleState(_selectedChainSlot == SignalChain::outputSlot, juce::dontSendNotification);
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        _effects[static_cast<size_t>(index)].setToggleState(
            slot == _selectedChainSlot && _layout.atSlot(slot) != SignalChain::Stage::Empty,
            juce::dontSendNotification);
    }
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        _pathB[static_cast<size_t>(index)].setToggleState(
            SignalChain::pathBSlotForIndex(index) == _selectedChainSlot && _route.pathBAt(index) != SignalChain::Stage::Empty,
            juce::dontSendNotification);
    }
}

void SignalChainStrip::handleEffectClick(int movableIndex) {
    selectChainSlot(SignalChain::chainSlotForMovableIndex(movableIndex));
}

void SignalChainStrip::handleBlockClick(BlockId blockId) {
    _selected = blockId;
    if (onBlockSelected) {
        onBlockSelected(blockId);
    }
}

bool SignalChainStrip::isOccupiedSlot(int slot) const {
    if (_dragActive && slot == _dragChainSlot) {
        return false;
    }

    return _layout.occupies(slot);
}

int SignalChainStrip::slotForBlock(BlockId blockId) const noexcept {
    switch (blockId) {
        case BlockId::InputGate:
            return SignalChain::inputSlot;
        case BlockId::MasterVolume:
            return SignalChain::outputSlot;
        case BlockId::AmpProfiler:
            return _layout.slotFor(SignalChain::Stage::Amp);
        case BlockId::CabinetIr:
            return _layout.slotFor(SignalChain::Stage::Cab);
        case BlockId::EqPostFx:
            return _layout.slotFor(SignalChain::Stage::Eq);
        case BlockId::PedalDrive:
            return _layout.slotFor(SignalChain::Stage::Pedal);
        default:
            return -1;
    }
}

SignalChain::Stage SignalChainStrip::stageForBlock(BlockId blockId) noexcept {
    switch (blockId) {
        case BlockId::CabinetIr:
            return SignalChain::Stage::Cab;
        case BlockId::EqPostFx:
            return SignalChain::Stage::Eq;
        case BlockId::PedalDrive:
            return SignalChain::Stage::Pedal;
        case BlockId::AmpProfiler:
            return SignalChain::Stage::Amp;
        case BlockId::Fx:
        case BlockId::InputGate:
        case BlockId::MasterVolume:
        default:
            return SignalChain::Stage::Empty;
    }
}

SignalChainStrip::BlockId SignalChainStrip::blockForStage(SignalChain::Stage stage) noexcept {
    switch (stage) {
        case SignalChain::Stage::Cab:
            return BlockId::CabinetIr;
        case SignalChain::Stage::Eq:
            return BlockId::EqPostFx;
        case SignalChain::Stage::Pedal:
            return BlockId::PedalDrive;
        case SignalChain::Stage::Amp:
            return BlockId::AmpProfiler;
        case SignalChain::Stage::Empty:
            return BlockId::AmpProfiler;
        default:
            return BlockId::Fx;
    }
}

int SignalChainStrip::nearestMovableSlot(juce::Point<int> position) const noexcept {
    int nearest = SignalChain::chainSlotForMovableIndex(0);
    auto bestDistance = std::numeric_limits<float>::max();
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        const auto centre = _slotBounds[static_cast<size_t>(slot)].getCentre().toFloat();
        const auto distance = centre.getDistanceFrom(position.toFloat());
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = slot;
        }
    }
    return nearest;
}

int SignalChainStrip::slotAtPosition(juce::Point<int> position) const noexcept {
    if (_route.active()) {
        for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
            if (_pathBBounds[static_cast<size_t>(index)].expanded(8).contains(position)) {
                return SignalChain::pathBSlotForIndex(index);
            }
        }
    }

    int found = -1;
    auto bestDistance = std::numeric_limits<float>::max();
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        const auto bounds = _slotBounds[static_cast<size_t>(slot)];
        if (!bounds.expanded(8).contains(position)) {
            continue;
        }
        const auto distance = bounds.getCentre().toFloat().getDistanceFrom(position.toFloat());
        if (distance < bestDistance) {
            bestDistance = distance;
            found = slot;
        }
    }
    return found;
}

juce::Rectangle<int> SignalChainStrip::movableDragArea() const {
    juce::Rectangle<int> area;
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = _slotBounds[static_cast<size_t>(SignalChain::chainSlotForMovableIndex(index))];
        area = area.isEmpty() ? slot : area.getUnion(slot);
    }
    return area.expanded(12);
}

void SignalChainStrip::startEffectDrag(int movableIndex, const juce::MouseEvent& event) {
    _dragMovableIndex = movableIndex;
    _dragPathBIndex = -1;
    _dragChainSlot = SignalChain::chainSlotForMovableIndex(movableIndex);
    _dragTracking = true;
    _dragActive = false;
    _dragToPathB = false;
    _dropSlot = -1;
    _dragStart = event.getEventRelativeTo(this).getPosition();
    _dragOrigin = _effects[static_cast<size_t>(movableIndex)].getBounds();
}

void SignalChainStrip::updateBlockDrag(const juce::MouseEvent& event) {
    if (!_dragTracking || (_dragMovableIndex < 0 && _dragPathBIndex < 0)) {
        return;
    }

    const auto position = event.getEventRelativeTo(this).getPosition();
    if (!_dragActive && position.getDistanceFrom(_dragStart) < kDragThresholdPx) {
        return;
    }

    const auto delta = position - _dragStart;
    if (_dragPathBIndex >= 0) {
        auto& branch = _pathB[static_cast<size_t>(_dragPathBIndex)];
        if (!_dragActive) {
            _dragActive = true;
            branch.toFront(false);
        }
        branch.setBounds(_dragOrigin.translated(delta.x, delta.y));
        const bool upward = delta.y < -kDragDownThresholdPx && std::abs(delta.y) > std::abs(delta.x);
        _dropSlot = upward ? nearestMovableSlot(position) : -1;
        repaint();
        return;
    }

    auto& block = _effects[static_cast<size_t>(_dragMovableIndex)];
    if (!_dragActive) {
        _dragActive = true;
        block.toFront(false);
        selectChainSlot(_dragChainSlot);
    }
    _dragToPathB = delta.y > kDragDownThresholdPx && delta.y > std::abs(delta.x);
    if (_dragToPathB) {
        auto preview = _dragOrigin;
        const auto laneY = _laneSquare > 0 ? _laneTop + _laneSquare / 2 : _dragOrigin.getBottom() + _dragOrigin.getHeight() / 2 + 10;
        preview.setCentre(_dragOrigin.getCentreX(), laneY);
        block.setBounds(preview);
        _dropSlot = -1;
        repaint();
        return;
    }

    auto bounds = _dragOrigin.translated(delta.x, 0);
    const auto area = movableDragArea();
    if (!area.isEmpty()) {
        bounds.setCentre(area.getConstrainedPoint(bounds.getCentre()).x, _dragOrigin.getCentreY());
    }
    block.setBounds(bounds);
    _dropSlot = nearestMovableSlot(bounds.getCentre());
    repaint();
}

void SignalChainStrip::finishBlockDrag(const juce::MouseEvent&) {
    if (!_dragTracking) {
        return;
    }

    const auto wasDragging = _dragActive;
    const auto dropSlot = _dropSlot;
    const auto fromSlot = _dragChainSlot;
    const auto movableIndex = _dragMovableIndex;
    const auto pathBIndex = _dragPathBIndex;
    const auto toPathB = _dragToPathB;
    _dragTracking = false;
    _dragActive = false;
    _dragToPathB = false;
    _dropSlot = -1;
    _dragMovableIndex = -1;
    _dragChainSlot = -1;
    _dragPathBIndex = -1;

    if (wasDragging && pathBIndex >= 0 && SignalChain::isMovableSlot(dropSlot) && onPathBReturned) {
        onPathBReturned(pathBIndex, dropSlot);
    } else if (wasDragging && toPathB && movableIndex >= 0 && onBlockDraggedDown) {
        onBlockDraggedDown(movableIndex);
    } else if (wasDragging && SignalChain::isMovableSlot(dropSlot)) {
        commitDrop(fromSlot, dropSlot);
    }

    placeBlocks();
    updateBusLayer();
    _busLayer.toBack();
    if (movableIndex >= 0 && _effects[static_cast<size_t>(movableIndex)].isVisible()) {
        _effects[static_cast<size_t>(movableIndex)].toFront(false);
    }
    repaint();
}

void SignalChainStrip::commitDrop(int fromSlot, int toSlot) {
    _layout.moveSlot(fromSlot, toSlot);
    if (fromSlot == _selectedChainSlot) {
        _selectedChainSlot = toSlot;
    } else if (toSlot == _selectedChainSlot) {
        _selectedChainSlot = fromSlot;
    }
    if (onSlotsSwapped) {
        onSlotsSwapped(fromSlot, toSlot);
    } else if (onLayoutChanged) {
        onLayoutChanged(_layout);
    }
}

void SignalChainStrip::requestPicker(int slot) {
    if (!SignalChain::isMovableSlot(slot) && !SignalChain::isPathBSlot(slot)) {
        return;
    }

    if (onBlockPickerRequested) {
        onBlockPickerRequested(slot);
    }
}

void SignalChainStrip::showBlockMenu(int slot) {
    const auto pathB = SignalChain::pathBIndexForSlot(slot);
    const auto movable = SignalChain::movableIndexForSlot(slot);
    if (pathB < 0 && (movable < 0 || _layout.atSlot(slot) == SignalChain::Stage::Empty)) {
        return;
    }
    if (pathB >= 0 && _route.pathBAt(pathB) == SignalChain::Stage::Empty) {
        return;
    }

    juce::PopupMenu menu;
    menu.addItem(kChangeBlockItem, "Change block");
    menu.addSeparator();
    menu.addItem(kClearAllBlocksItem, "Clear all blocks", _layout.occupiedCount() > 0 || _route.occupiedPathB() > 0);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
                       [safe = juce::Component::SafePointer<SignalChainStrip>(this), slot](int result) {
                           if (safe == nullptr) {
                               return;
                           }
                           if (result == kChangeBlockItem) {
                               safe->requestPicker(slot);
                           } else if (result == kClearAllBlocksItem && safe->onClearAllBlocks) {
                               safe->onClearAllBlocks();
                           }
                       });
}

int SignalChainStrip::fillChainOrder(std::array<int, 16>& order) const noexcept {
    int count = 0;
    order[static_cast<size_t>(count++)] = SignalChain::inputSlot;
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        if (_layout.atSlot(slot) != SignalChain::Stage::Empty) {
            order[static_cast<size_t>(count++)] = slot;
        }
    }
    order[static_cast<size_t>(count++)] = SignalChain::outputSlot;
    return count;
}
