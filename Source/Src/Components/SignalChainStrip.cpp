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
    if (event.mods.isPopupMenu()) {
        return;
    }

    juce::Button::mouseDown(event);
    if (_isIoNode || isLedHit(event.getPosition()) || onDragBegin == nullptr) {
        return;
    }

    onDragBegin(event);
}

void SignalChainBlock::mouseDrag(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu()) {
        return;
    }

    juce::Button::mouseDrag(event);
    if (onDragMove != nullptr) {
        onDragMove(event);
    }
}

void SignalChainBlock::mouseUp(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu()) {
        if (event.mouseWasClicked() && onPickerRequested) {
            onPickerRequested();
        }
        return;
    }

    if (onDragEnd != nullptr) {
        onDragEnd(event);
    }

    if (event.mouseWasClicked() && isLedHit(event.getPosition()) && onLedClicked) {
        onLedClicked();
        return;
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
            g.setColour(_categoryColour.withAlpha(_ledOn ? 0.95f : 0.45f));
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

void SignalChainStrip::RouteNode::mouseDrag(const juce::MouseEvent& event) {
    _dragging = true;
    if (onNodeDrag) {
        onNodeDrag(event);
    }
}

void SignalChainStrip::RouteNode::mouseUp(const juce::MouseEvent& event) {
    if (_dragging && onNodeDragEnd) {
        _dragging = false;
        onNodeDragEnd();
        return;
    }
    _dragging = false;
    juce::Button::mouseUp(event);
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

        g.setColour(ProfilerStyle::Colors::caption);
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
            _dragFrom = {};
            int visual = 0;
            for (const auto& fork : _forks.forks) {
                for (int column = fork.splitGap; column < fork.mergeGap; ++column) {
                    if (visual == index) {
                        _dragFrom.laneB = true;
                        _dragFrom.row = fork.row;
                        _dragFrom.column = column;
                        _dragFrom.forkId = fork.id;
                    }
                    ++visual;
                }
            }
            _dragStart = event.getEventRelativeTo(this).getPosition();
            _dragOrigin = _pathB[static_cast<size_t>(index)].getBounds();
        };
        block.onDragMove = [this](const juce::MouseEvent& event) { updateBlockDrag(event); };
        block.onDragEnd = [this](const juce::MouseEvent& event) { finishBlockDrag(event); };
    }

    for (int index = 0; index < kMaxForkHandles; ++index) {
        auto bind = [this, index](RouteNode& node, bool split) {
            addAndMakeVisible(node);
            node.setVisible(false);
            node.split = split;
            node.onClick = [this, index, split]() {
                if (onEditRouteMix) {
                    onEditRouteMix();
                }
                juce::ignoreUnused(index, split);
            };
            node.onNodeDrag = [this, index, split, &node](const juce::MouseEvent& event) {
                const auto id = split ? _splitNodes[static_cast<size_t>(index)].forkId
                                      : _mergeNodes[static_cast<size_t>(index)].forkId;
                if (id < 0) {
                    return;
                }
                const SignalChain::Bifurcation* fork = nullptr;
                for (const auto& candidate : _forks.forks) {
                    if (candidate.id == id) {
                        fork = &candidate;
                    }
                }
                if (fork == nullptr) {
                    return;
                }
                const auto gap = gapAtX(fork->row, event.getEventRelativeTo(this).getPosition().x);
                const auto clamped = split ? SignalChain::clampSplit(_forks, id, gap) : SignalChain::clampMerge(_forks, id, gap);
                const auto x = juce::roundToInt(gapCentreX(fork->row, clamped));
                auto bounds = node.getBounds();
                bounds.setCentre(x, bounds.getCentreY());
                node.setBounds(bounds);
            };
            node.onNodeDragEnd = [this, index, split]() {
                const auto id = split ? _splitNodes[static_cast<size_t>(index)].forkId
                                      : _mergeNodes[static_cast<size_t>(index)].forkId;
                if (id < 0 || onForkEdgeMoved == nullptr) {
                    return;
                }
                const SignalChain::Bifurcation* fork = nullptr;
                for (const auto& candidate : _forks.forks) {
                    if (candidate.id == id) {
                        fork = &candidate;
                    }
                }
                if (fork == nullptr) {
                    return;
                }
                const auto gap = split ? gapAtX(fork->row, _splitNodes[static_cast<size_t>(index)].getBounds().getCentreX())
                                       : gapAtX(fork->row, _mergeNodes[static_cast<size_t>(index)].getBounds().getCentreX());
                onForkEdgeMoved(id, split, gap);
            };
        };
        bind(_splitNodes[static_cast<size_t>(index)], true);
        bind(_mergeNodes[static_cast<size_t>(index)], false);
    }

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
    _effects[static_cast<size_t>(SignalChain::movableIndexForSlot(2))].setToggleState(true, juce::dontSendNotification);
    _busLayer.toBack();
    setOpaque(true);
    setWantsKeyboardFocus(true);
}

void SignalChainStrip::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
    auto drawEmptySlot = [&](juce::Rectangle<float> bounds) {
        if (laf != nullptr) {
            laf->drawEmptySignalSlot(g, bounds);
            return;
        }
        g.setColour(ProfilerStyle::Colors::border.withAlpha(0.55f));
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.5f;
        g.drawRoundedRectangle(bounds.withSizeKeepingCentre(side, side), 3.0f, 1.2f);
    };

    for (int slot = 0; slot < kSlotCount; ++slot) {
        if (isOccupiedSlot(slot) || _slotBounds[static_cast<size_t>(slot)].isEmpty()) {
            continue;
        }
        drawEmptySlot(_slotBounds[static_cast<size_t>(slot)].toFloat());
    }

    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        const auto bounds = _pathBBounds[static_cast<size_t>(index)];
        if (bounds.isEmpty()) {
            continue;
        }
        const auto& block = _pathB[static_cast<size_t>(index)];
        if (block.isVisible() && block.getBounds() == bounds) {
            continue;
        }
        drawEmptySlot(bounds.toFloat());
    }

    if (SignalChain::isMovableSlot(_dropSlot)) {
        auto dropBounds = _slotBounds[static_cast<size_t>(_dropSlot)].toFloat().reduced(4.0f);
        g.setColour(ProfilerStyle::Colors::text.withAlpha(0.72f));
        g.drawRoundedRectangle(dropBounds, 6.0f, 1.6f);
    }

    if (_previewFork) {
        const auto row = _hoverPreview.target.row;
        const auto column = _hoverPreview.target.column;
        const auto slot = SignalChain::chainSlotForMovableIndex(SignalChain::movableForFork(row, column));
        const auto columnBounds = _slotBounds[static_cast<size_t>(slot)];
        const auto laneTop = _laneTopForRow[row] >= 0 ? _laneTopForRow[row] : columnBounds.getBottom() + 8;
        auto ghost = columnBounds.withY(laneTop);
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.drawRoundedRectangle(ghost.toFloat().reduced(4.0f), 6.0f, 1.4f);
        const auto splitX = gapCentreX(row, column);
        const auto mergeX = gapCentreX(row, column + 1);
        const auto anchorY = static_cast<float>(columnBounds.getCentreY());
        const auto laneY = static_cast<float>(laneTop + columnBounds.getHeight() / 2);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawLine(splitX, anchorY, splitX, laneY, 1.3f);
        g.drawLine(splitX, laneY, mergeX, laneY, 1.3f);
        g.drawLine(mergeX, laneY, mergeX, anchorY, 1.3f);
    }

    for (const auto& fork : _forks.forks) {
        if (fork.columnCount() <= 0) {
            continue;
        }
        const auto firstSlot = SignalChain::chainSlotForMovableIndex(SignalChain::movableForFork(fork.row, fork.splitGap));
        const auto lastSlot = SignalChain::chainSlotForMovableIndex(SignalChain::movableForFork(fork.row, fork.mergeGap - 1));
        const auto anchorY = static_cast<float>(_slotBounds[static_cast<size_t>(firstSlot)].getCentreY());
        const auto laneTop = _laneTopForRow[fork.row] >= 0 ? _laneTopForRow[fork.row]
                                                           : _slotBounds[static_cast<size_t>(firstSlot)].getBottom() + 8;
        const auto laneY = static_cast<float>(laneTop + _slotBounds[static_cast<size_t>(firstSlot)].getHeight() / 2);
        const auto splitX = gapCentreX(fork.row, fork.splitGap);
        const auto mergeX = gapCentreX(fork.row, fork.mergeGap);
        g.setColour(juce::Colour(0xffF4F4F5).withAlpha(0.9f));
        g.drawLine(splitX, anchorY, splitX, laneY, 1.3f);
        g.drawLine(splitX, laneY, mergeX, laneY, 1.3f);
        g.drawLine(mergeX, static_cast<float>(_slotBounds[static_cast<size_t>(lastSlot)].getCentreY()), mergeX, laneY, 1.3f);
    }
}

int SignalChainStrip::parallelRow() const noexcept {
    if (_forks.forks.empty()) {
        return -1;
    }
    return _forks.forks.front().row;
}

void SignalChainStrip::resized() {
    auto area = getLocalBounds().reduced(4, 8);
    // The two rows always sit with one lane between them, which is the spacing a split
    // uses. Creating a split then only draws into that lane.
    constexpr int bands = 3;
    const auto slotWidth = area.getWidth() / static_cast<float>(SignalChain::columnsPerRow);
    constexpr int kRowGap = 16;
    const auto squareByWidth = juce::jlimit(44, 78, juce::roundToInt(slotWidth) - 16);
    const auto squareByHeight = juce::jmax(34, (area.getHeight() - kRowGap * (bands - 1)) / bands);
    const auto square = juce::jmin(squareByWidth, squareByHeight);
    const auto contentHeight = square * bands + kRowGap * (bands - 1);
    const auto extra = juce::jmax(0, area.getHeight() - contentHeight);
    const auto laneRoom = square + kRowGap;
    const auto top = area.getY() + (extra >= laneRoom ? (extra - laneRoom) / 2 : extra / 2);
    const auto bandY = [top, square](int band) { return top + band * (square + kRowGap); };

    const int serialBand[2] = {0, 2};
    _laneTopForRow[0] = bandY(1);
    _laneTopForRow[1] = bandY(2) + square + kRowGap;
    _laneTop = _laneTopForRow[0];
    _laneSquare = square;

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
}

void SignalChainStrip::layoutParallelLane() {
    _pathBBounds.fill({});
    if (_forks.forks.empty() || _laneSquare <= 0) {
        for (auto& node : _splitNodes) {
            node.setVisible(false);
        }
        for (auto& node : _mergeNodes) {
            node.setVisible(false);
        }
        for (auto& block : _pathB) {
            block.setVisible(false);
        }
        return;
    }

    const auto& primary = _forks.forks.front();
    const auto first = SignalChain::movableForFork(primary.row, primary.splitGap);
    const auto last = SignalChain::movableForFork(primary.row, primary.mergeGap - 1);
    auto centreOf = [this](int index) {
        return _slotBounds[static_cast<size_t>(SignalChain::chainSlotForMovableIndex(index))].getCentre();
    };
    const auto anchor = centreOf(first);
    const auto current = centreOf(last);
    juce::ignoreUnused(anchor, current);
    const auto split = juce::Point<int>(juce::roundToInt(gapCentreX(primary.row, primary.splitGap)), anchor.y);
    const auto merge = juce::Point<int>(juce::roundToInt(gapCentreX(primary.row, primary.mergeGap)), current.y);
    constexpr int node = 28;
    int handle = 0;
    for (const auto& fork : _forks.forks) {
        if (handle >= kMaxForkHandles) {
            break;
        }
        const auto splitPoint = juce::Point<int>(juce::roundToInt(gapCentreX(fork.row, fork.splitGap)),
                                                 _slotBounds[static_cast<size_t>(SignalChain::chainSlotForMovableIndex(
                                                                 SignalChain::movableForFork(fork.row, fork.splitGap)))]
                                                     .getCentreY());
        const auto mergePoint = juce::Point<int>(juce::roundToInt(gapCentreX(fork.row, fork.mergeGap)),
                                                 _slotBounds[static_cast<size_t>(SignalChain::chainSlotForMovableIndex(
                                                                 SignalChain::movableForFork(fork.row, fork.mergeGap - 1)))]
                                                     .getCentreY());
        auto& splitNode = _splitNodes[static_cast<size_t>(handle)];
        auto& mergeNode = _mergeNodes[static_cast<size_t>(handle)];
        splitNode.forkId = fork.id;
        mergeNode.forkId = fork.id;
        splitNode.setBounds(splitPoint.x - node / 2, splitPoint.y - node / 2, node, node);
        mergeNode.setBounds(mergePoint.x - node / 2, mergePoint.y - node / 2, node, node);
        splitNode.setVisible(true);
        mergeNode.setVisible(true);
        splitNode.toFront(false);
        mergeNode.toFront(false);
        ++handle;
    }
    juce::ignoreUnused(split, merge);

    int visual = 0;
    for (const auto& fork : _forks.forks) {
        for (int column = fork.splitGap; column < fork.mergeGap && visual < SignalChain::ParallelRoute::pathBCount; ++column) {
            auto& block = _pathB[static_cast<size_t>(visual)];
            const auto columnSlot = SignalChain::chainSlotForMovableIndex(SignalChain::movableForFork(fork.row, column));
            const auto columnBounds = _slotBounds[static_cast<size_t>(columnSlot)];
            const auto laneTop = _laneTopForRow[fork.row] >= 0 ? _laneTopForRow[fork.row] : columnBounds.getBottom() + 8;
            _pathBBounds[static_cast<size_t>(visual)] = {columnBounds.getX(), laneTop, columnBounds.getWidth(), columnBounds.getHeight()};
            const auto stage = fork.laneAt(column - fork.splitGap);
            if (stage == SignalChain::Stage::Empty || (_dragActive && _dragPathBIndex == visual)) {
                block.setVisible(stage != SignalChain::Stage::Empty);
                ++visual;
                continue;
            }
            const auto appearance = appearanceFor(stage);
            block.setAppearance(appearance.colour, appearance.icon);
            block.setCaption(Fx::captionFor(stage));
            block.setBounds(_pathBBounds[static_cast<size_t>(visual)]);
            block.setVisible(true);
            ++visual;
        }
    }
    for (int index = visual; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        _pathB[static_cast<size_t>(index)].setVisible(false);
        _pathBBounds[static_cast<size_t>(index)] = {};
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
    _forks = SignalChain::graphFromRoute(route);
    if (previousRow != parallelRow()) {
        resized();
    } else {
        placeBlocks();
        repaint();
    }
}

void SignalChainStrip::setForks(const SignalChain::ForkGraph& graph) {
    const auto previousRow = parallelRow();
    _forks = graph;
    _route = graph.forks.empty() ? SignalChain::ParallelRoute{} : SignalChain::routeFromFork(graph.forks.front());
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
    } else if (!isBlockOnChain(_selected)) {
        selectChainSlot(SignalChain::inputSlot);
    }
    placeBlocks();
    if (auto* selected = effectBlockAtSlot(_selectedChainSlot)) {
        selected->setToggleState(true, juce::dontSendNotification);
    }
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
    _selectedChainSlot = chainSlot;
    if (chainSlot == SignalChain::inputSlot) {
        _selected = BlockId::InputGate;
        _inputGate.setToggleState(true, juce::dontSendNotification);
    } else if (chainSlot == SignalChain::outputSlot) {
        _selected = BlockId::MasterVolume;
        _masterVolume.setToggleState(true, juce::dontSendNotification);
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

    handleBlockClick(_selected);
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

float SignalChainStrip::gapCentreX(int row, int gap) const noexcept {
    auto centreX = [this](int rowIndex, int column) {
        const auto slot = SignalChain::chainSlotForMovableIndex(SignalChain::movableForFork(rowIndex, column));
        return static_cast<float>(_slotBounds[static_cast<size_t>(slot)].getCentreX());
    };
    auto width = [this](int rowIndex) {
        const auto slot = SignalChain::chainSlotForMovableIndex(SignalChain::movableForFork(rowIndex, 0));
        return static_cast<float>(_slotBounds[static_cast<size_t>(slot)].getWidth());
    };
    if (gap <= 0) {
        return centreX(row, 0) - width(row) * 0.5f;
    }
    if (gap >= SignalChain::forkColumns) {
        return centreX(row, SignalChain::forkColumns - 1) + width(row) * 0.5f;
    }
    return (centreX(row, gap - 1) + centreX(row, gap)) * 0.5f;
}

int SignalChainStrip::gapAtX(int row, int x) const noexcept {
    int best = 0;
    auto bestDistance = std::numeric_limits<float>::max();
    for (int gap = 0; gap <= SignalChain::forkColumns; ++gap) {
        const auto distance = std::abs(gapCentreX(row, gap) - static_cast<float>(x));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = gap;
        }
    }
    return best;
}

int SignalChainStrip::laneYForRow(int row) const noexcept {
    if (row >= 0 && row < 2 && _laneTopForRow[row] >= 0) {
        return _laneTopForRow[row] + juce::jmax(0, _laneSquare) / 2;
    }
    const auto slot = SignalChain::chainSlotForMovableIndex(SignalChain::movableForFork(juce::jlimit(0, 1, row), 0));
    return _slotBounds[static_cast<size_t>(slot)].getBottom() + 8;
}

bool SignalChainStrip::hoverTarget(juce::Point<int> position, int& row, int& column, bool& laneB) const noexcept {
    int nearest = -1;
    auto best = std::numeric_limits<float>::max();
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        const auto bounds = _slotBounds[static_cast<size_t>(slot)];
        if (bounds.isEmpty()) {
            continue;
        }
        const auto distance = bounds.getCentre().toFloat().getDistanceFrom(position.toFloat());
        if (distance < best) {
            best = distance;
            nearest = index;
        }
    }
    if (nearest < 0) {
        return false;
    }
    row = SignalChain::forkRowForMovable(nearest);
    column = SignalChain::forkColumnForMovable(nearest);
    const auto slot = _slotBounds[static_cast<size_t>(SignalChain::chainSlotForMovableIndex(nearest))];
    const auto laneTop = _laneTopForRow[row] >= 0 ? _laneTopForRow[row] : slot.getBottom() + 4;
    const auto laneBounds = slot.withY(laneTop);
    if (laneBounds.expanded(10, 16).contains(position) && position.y > slot.getCentreY()) {
        laneB = true;
        return true;
    }
    for (int index = 0; index < SignalChain::ParallelRoute::pathBCount; ++index) {
        if (_pathBBounds[static_cast<size_t>(index)].expanded(8).contains(position)) {
            int visual = 0;
            for (const auto& fork : _forks.forks) {
                for (int forkColumn = fork.splitGap; forkColumn < fork.mergeGap; ++forkColumn) {
                    if (visual == index) {
                        row = fork.row;
                        column = forkColumn;
                        laneB = true;
                        return true;
                    }
                    ++visual;
                }
            }
        }
    }
    laneB = false;
    return slot.expanded(12).contains(position) || best < static_cast<float>(slot.getWidth());
}

void SignalChainStrip::updateHover(juce::Point<int> position) {
    int row = 0;
    int column = 0;
    bool laneB = false;
    _previewFork = false;
    _previewForbidden = false;
    _hoverPreview = {};
    if (_swapPreviewSlot >= 0) {
        const auto restore = SignalChain::chainSlotForMovableIndex(_swapPreviewSlot);
        if (SignalChain::isMovableSlot(restore)) {
            _effects[static_cast<size_t>(_swapPreviewSlot)].setBounds(_slotBounds[static_cast<size_t>(restore)]);
        }
        _swapPreviewSlot = -1;
    }
    if (!hoverTarget(position, row, column, laneB)) {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }
    _hoverPreview = SignalChain::previewDrop(_layout, _forks, row, column, laneB);
    _previewFork = _hoverPreview.kind == SignalChain::DropKind::NewFork;
    _previewForbidden = laneB && _hoverPreview.kind == SignalChain::DropKind::Forbidden;
    setMouseCursor(_previewForbidden ? juce::MouseCursor::NoCursor : juce::MouseCursor::DraggingHandCursor);
    if (_hoverPreview.kind == SignalChain::DropKind::Slot && _hoverPreview.swap && !_hoverPreview.target.laneB) {
        const auto index = SignalChain::movableForFork(_hoverPreview.target.row, _hoverPreview.target.column);
        if (index != _dragMovableIndex && index >= 0 && index < SignalChain::movableSlotCount) {
            _effects[static_cast<size_t>(index)].setBounds(_dragOrigin);
            _swapPreviewSlot = index;
        }
    }
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
    auto& dragged = _dragPathBIndex >= 0 ? _pathB[static_cast<size_t>(_dragPathBIndex)]
                                         : _effects[static_cast<size_t>(_dragMovableIndex)];
    if (!_dragActive) {
        _dragActive = true;
        dragged.toFront(false);
        if (_dragChainSlot >= 0) {
            selectChainSlot(_dragChainSlot);
        }
        if (_dragMovableIndex >= 0) {
            _dragFrom.laneB = false;
            _dragFrom.row = SignalChain::forkRowForMovable(_dragMovableIndex);
            _dragFrom.column = SignalChain::forkColumnForMovable(_dragMovableIndex);
        }
    }
    dragged.setBounds(_dragOrigin.translated(delta.x, delta.y));
    updateHover(position);
    repaint();
}

void SignalChainStrip::finishBlockDrag(const juce::MouseEvent&) {
    if (!_dragTracking) {
        return;
    }

    const auto preview = _hoverPreview;
    const auto from = _dragFrom;
    const auto wasDragging = _dragActive;
    _dragTracking = false;
    _dragActive = false;
    _dragToPathB = false;
    _previewFork = false;
    _previewForbidden = false;
    _dropSlot = -1;
    _dragMovableIndex = -1;
    _dragChainSlot = -1;
    _dragPathBIndex = -1;
    _swapPreviewSlot = -1;

    if (wasDragging && preview.kind != SignalChain::DropKind::Forbidden && onRouteDrop) {
        onRouteDrop(from, preview);
    }

    placeBlocks();
    updateBusLayer();
    _busLayer.toBack();
    setMouseCursor(juce::MouseCursor::NormalCursor);
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
