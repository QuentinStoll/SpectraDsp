#pragma once

#include <JuceHeader.h>

#include <array>

#include "Components/CustomLookAndFeel.h"
#include "SignalChainLayout.h"
#include "Stylesheet.h"

class SignalChainBlock : public juce::Button {
   public:
    SignalChainBlock();
    SignalChainBlock(juce::Colour categoryColour, CustomLookAndFeel::RigIcon icon);
    ~SignalChainBlock() override = default;

    void setAppearance(juce::Colour categoryColour, CustomLookAndFeel::RigIcon icon);
    void setCaption(const juce::String& caption);

    enum class FlowMark { None,
                          Down,
                          Enter };

    void setLedOn(bool shouldBeOn);
    void setIoNode(bool isIoNode);
    void setShowsLed(bool shouldShowLed);
    void setSignalLevel(float level);
    void setFlowMark(FlowMark mark);
    bool isIoNode() const noexcept { return _isIoNode; }

    std::function<void()> onLedClicked;
    std::function<void()> onPickerRequested;
    std::function<void(const juce::MouseEvent&)> onDragBegin;
    std::function<void(const juce::MouseEvent&)> onDragMove;
    std::function<void(const juce::MouseEvent&)> onDragEnd;

   protected:
    void paint(juce::Graphics& g) override;
    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

   private:
    juce::Colour _categoryColour;
    CustomLookAndFeel::RigIcon _icon;
    juce::String _caption;
    bool _ledOn = true;
    bool _isIoNode = false;
    bool _showsLed = true;
    float _signalLevel = 0.0f;
    FlowMark _flowMark = FlowMark::None;

    juce::Rectangle<float> getLedBounds() const;
    bool isLedHit(juce::Point<int> position) const;
    void paintContents(juce::Graphics& g, bool isMouseOverButton);
    void paintFlowMark(juce::Graphics& g) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalChainBlock)
};

class SignalChainStrip : public juce::Component {
   public:
    enum class BlockId {
        InputGate = 0,
        AmpProfiler,
        CabinetIr,
        EqPostFx,
        PedalDrive,
        MasterVolume,
        Fx
    };

    SignalChainStrip();
    ~SignalChainStrip() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

    void setSelectedBlock(BlockId blockId);
    void selectSlot(int chainSlot);
    int getSelectedChainSlot() const noexcept { return _selectedChainSlot; }
    void setSlotLed(int chainSlot, bool isOn);
    BlockId getSelectedBlock() const noexcept { return _selected; }
    bool selectAdjacentBlock(int delta);

    bool keyPressed(const juce::KeyPress& key) override;

    void setBlockLed(BlockId blockId, bool isOn);
    void setIoMeterLevels(float inputDb, float outputDb);
    void setLayout(const SignalChain::Layout& layout);
    void setRoute(const SignalChain::ParallelRoute& route);
    void setForks(const SignalChain::ForkGraph& graph);
    SignalChain::Layout getLayout() const noexcept { return _layout; }
    SignalChain::ParallelRoute getRoute() const noexcept { return _route; }
    SignalChain::Stage selectedStage() const noexcept;
    bool isBlockOnChain(BlockId blockId) const noexcept;

    std::function<void(BlockId)> onBlockSelected;
    std::function<void(BlockId)> onBlockBypassToggled;
    std::function<void(const SignalChain::Layout&)> onLayoutChanged;
    std::function<void(int fromSlot, int toSlot)> onSlotsSwapped;
    std::function<void(int slot)> onBlockPickerRequested;
    std::function<void()> onClearAllBlocks;
    std::function<void(int movableIndex)> onBlockDraggedDown;
    std::function<void(int pathBIndex, int chainSlot)> onPathBReturned;
    std::function<void()> onEditRouteMix;
    std::function<void(SignalChain::BlockRef from, const SignalChain::DropPreview& preview)> onRouteDrop;
    std::function<void(int forkId, bool isSplit, int gap)> onForkEdgeMoved;

   private:
    class RouteNode : public juce::Button {
       public:
        RouteNode();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        int forkId = -1;
        bool split = true;
        std::function<void(const juce::MouseEvent&)> onNodeDrag;
        std::function<void()> onNodeDragEnd;

       private:
        bool _dragging = false;
    };

    class SignalBusLayer : public juce::Component {
       public:
        void setRows(juce::Point<float> input,
                     juce::Point<float> down,
                     juce::Point<float> join,
                     juce::Point<float> output);
        void paint(juce::Graphics& g) override;

       private:
        juce::Point<float> _input;
        juce::Point<float> _down;
        juce::Point<float> _join;
        juce::Point<float> _output;
        bool _ready = false;
    };

    static constexpr int kSlotCount = SignalChain::slotCount;

    SignalBusLayer _busLayer;
    SignalChainBlock _inputGate{ProfilerStyle::Colors::rigInput, CustomLookAndFeel::RigIcon::InputJack};
    std::array<SignalChainBlock, SignalChain::movableSlotCount> _effects{};
    SignalChainBlock _masterVolume{ProfilerStyle::Colors::rigMaster, CustomLookAndFeel::RigIcon::Speaker};
    SignalChainBlock _pathDown{juce::Colour(0xffF4F4F5), CustomLookAndFeel::RigIcon::InputJack};
    SignalChainBlock _pathReturn{juce::Colour(0xffF4F4F5), CustomLookAndFeel::RigIcon::InputJack};
    std::array<SignalChainBlock, SignalChain::ParallelRoute::pathBCount> _pathB{};
    static constexpr int kMaxForkHandles = 6;
    std::array<RouteNode, kMaxForkHandles> _splitNodes{};
    std::array<RouteNode, kMaxForkHandles> _mergeNodes{};
    std::array<int, 2> _laneTopForRow{-1, -1};
    std::array<juce::Rectangle<int>, kSlotCount> _slotBounds{};
    std::array<juce::Rectangle<int>, SignalChain::ParallelRoute::pathBCount> _pathBBounds{};
    int _laneTop = 0;
    int _laneSquare = 0;
    SignalChain::Layout _layout{};
    SignalChain::ParallelRoute _route{};
    SignalChain::ForkGraph _forks;
    BlockId _selected = BlockId::AmpProfiler;
    int _selectedChainSlot = 2;
    int _dragMovableIndex = -1;
    int _dragChainSlot = -1;
    int _dragPathBIndex = -1;
    bool _dragTracking = false;
    bool _dragActive = false;
    bool _dragToPathB = false;
    int _dropSlot = -1;
    bool _previewFork = false;
    bool _previewForbidden = false;
    SignalChain::DropPreview _hoverPreview{};
    SignalChain::BlockRef _dragFrom{};
    int _swapPreviewSlot = -1;
    juce::Point<int> _dragStart{};
    juce::Rectangle<int> _dragOrigin{};
    float _inputMeter = 0.0f;
    float _outputMeter = 0.0f;
    double _lastMeterMs = 0.0;

    SignalChainBlock* effectBlockAtSlot(int chainSlot) noexcept;
    bool selectBlockInDirection(int columnDelta, int rowDelta);
    void selectChainSlot(int chainSlot);
    void handleBlockClick(BlockId blockId);
    void handleEffectClick(int movableIndex);
    bool isOccupiedSlot(int slot) const;
    void updateBusLayer();
    void placeBlocks();
    int slotForBlock(BlockId blockId) const noexcept;
    static SignalChain::Stage stageForBlock(BlockId blockId) noexcept;
    static BlockId blockForStage(SignalChain::Stage stage) noexcept;
    int nearestMovableSlot(juce::Point<int> position) const noexcept;
    int slotAtPosition(juce::Point<int> position) const noexcept;
    juce::Rectangle<int> movableDragArea() const;
    void startEffectDrag(int movableIndex, const juce::MouseEvent& event);
    void updateBlockDrag(const juce::MouseEvent& event);
    void finishBlockDrag(const juce::MouseEvent& event);
    void commitDrop(int fromSlot, int toSlot);
    void requestPicker(int slot);
    void showBlockMenu(int slot);
    int parallelRow() const noexcept;
    int laneYForRow(int row) const noexcept;
    float gapCentreX(int row, int gap) const noexcept;
    int gapAtX(int row, int x) const noexcept;
    bool hoverTarget(juce::Point<int> position, int& row, int& column, bool& laneB) const noexcept;
    void updateHover(juce::Point<int> position);
    void layoutParallelLane();
    int fillChainOrder(std::array<int, 16>& order) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalChainStrip)
};
