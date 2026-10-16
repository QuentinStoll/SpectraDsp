#include "PluginEditor.h"

#include "Fx/FxCatalog.h"
#include "ProfileManager.h"
#include "ProfilerConstantValues.h"
#include "Stylesheet.h"

#if JUCE_WINDOWS
#include <dwmapi.h>
#endif

namespace {
constexpr int kHostChromeRetryLimit = 10;

#if JUCE_WINDOWS
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif

void setImmersiveDarkMode(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }

    BOOL enabled = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enabled, sizeof(enabled));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &enabled, sizeof(enabled));
}

bool isDedicatedPluginFrame(HWND hwnd, int editorWidthPx, int editorHeightPx) {
    if (hwnd == nullptr) {
        return false;
    }

    RECT bounds{};
    if (!GetWindowRect(hwnd, &bounds)) {
        return false;
    }

    const auto width = bounds.right - bounds.left;
    const auto height = bounds.bottom - bounds.top;
    return width >= editorWidthPx - 16 && width <= editorWidthPx + 96 && height >= editorHeightPx - 16 && height <= editorHeightPx + 96;
}

void stripNativeFrame(HWND hwnd, int editorWidthPx, int editorHeightPx) {
    if (hwnd == nullptr) {
        return;
    }

    auto style = GetWindowLongPtr(hwnd, GWL_STYLE);
    auto exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    exStyle &= ~(WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE);
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    MARGINS margins{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    setImmersiveDarkMode(hwnd);

    RECT client{0, 0, editorWidthPx, editorHeightPx};
    AdjustWindowRectEx(&client, static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_STYLE)), FALSE,
                       static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_EXSTYLE)));
    SetWindowPos(hwnd, nullptr, 0, 0, client.right - client.left, client.bottom - client.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void restyleWindowsHostFrame(HWND pluginHwnd, int editorWidthPx, int editorHeightPx) {
    if (pluginHwnd == nullptr) {
        return;
    }

    setImmersiveDarkMode(pluginHwnd);

    auto* parent = GetParent(pluginHwnd);
    if (parent != nullptr) {
        setImmersiveDarkMode(parent);
    }

    auto* root = GetAncestor(pluginHwnd, GA_ROOT);
    if (root == nullptr || root == GetDesktopWindow()) {
        return;
    }

    setImmersiveDarkMode(root);
    if (isDedicatedPluginFrame(root, editorWidthPx, editorHeightPx) || isDedicatedPluginFrame(parent, editorWidthPx, editorHeightPx)) {
        stripNativeFrame(isDedicatedPluginFrame(root, editorWidthPx, editorHeightPx) ? root : parent,
                         editorWidthPx, editorHeightPx);
    }
}
#endif

juce::Point<int> standaloneMaxSize() {
    juce::Rectangle<int> area;
    for (const auto& display : juce::Desktop::getInstance().getDisplays().displays) {
        area = area.getUnion(display.totalArea);
    }

    if (area.isEmpty()) {
        return {8192, 8192};
    }

    return {juce::jmax(editorWidth, area.getWidth()), juce::jmax(editorHeight, area.getHeight())};
}
}  // namespace

ProfilerAudioProcessorEditor::ProfilerAudioProcessorEditor(ProfilerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      _audioProcessor(p),
      _topBar(p),
      _inputGatePanel(p._apvts),
      _ampPanel(p),
      _cabinetPanel(p),
      _eqPanel(p._apvts),
      _pedalPanel(p._apvts),
      _masterPanel(p),
      _fxPanel(p),
      _libraryView(p),
      _settingsView(p) {
    SettingsView::applySavedBackgroundColour();
    setLookAndFeel(&_customLookAndFeel);
    setOpaque(true);
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::black);

    setResizable(true, true);
    if (_audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
        const auto maxSize = standaloneMaxSize();
        setResizeLimits(editorWidth, editorHeight, maxSize.x, maxSize.y);
        setSize(editorWidth, editorHeight);
    } else {
        setResizeLimits(editorMinWidth, editorMinHeight, editorWidth, editorHeight);
        getConstrainer()->setFixedAspectRatio(editorWidth / static_cast<double>(editorHeight));
        setSize(editorWidth, editorHeight);
    }
    setBroughtToFrontOnMouseClick(true);

    addAndMakeVisible(_content);
    _content.addAndMakeVisible(_topBar);
    _content.addAndMakeVisible(_signalChain);
    _content.addAndMakeVisible(_editHost);
    _editHost.setInterceptsMouseClicks(false, true);
    _editHost.addAndMakeVisible(_inputGatePanel);
    _editHost.addAndMakeVisible(_ampPanel);
    _editHost.addAndMakeVisible(_cabinetPanel);
    _editHost.addAndMakeVisible(_eqPanel);
    _editHost.addAndMakeVisible(_pedalPanel);
    _editHost.addAndMakeVisible(_masterPanel);
    _editHost.addAndMakeVisible(_fxPanel);
    _editHost.addChildComponent(_blockPicker);

    _settingsHost.addAndMakeVisible(_settingsView);
    _settingsHost.addAndMakeVisible(_resetButton);
    _settingsHost.addAndMakeVisible(_exportButton);
    _content.addChildComponent(_settingsHost);
    _content.addChildComponent(_libraryView);

    _exportViewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, ProfilerStyle::Colors::accent);
    _exportViewport.setViewedComponent(&_exportModule, false);
    _exportViewport.setScrollBarsShown(true, false);
    _content.addChildComponent(_exportViewport);
    _content.addChildComponent(_closeOverlayButton);
    _content.addChildComponent(_notificationBanner);

    _topBar.onSettingsClicked = [this]() {
        showOverlay(_overlayMode == OverlayMode::Settings ? OverlayMode::None : OverlayMode::Settings);
    };
    _topBar.onLibraryClicked = [this]() {
        showOverlay(_overlayMode == OverlayMode::Library ? OverlayMode::None : OverlayMode::Library);
    };
    _closeOverlayButton.onClick = [this]() {
        showStudio();
    };
    _resetButton.onClick = [this]() {
        showStatus(_topBar.restoreDefaults(), true);
        _ampPanel.refreshAssets();
        _cabinetPanel.refreshAssets();
        _eqPanel.refreshBypassState();
        _inputGatePanel.refreshBypassState();
        _pedalPanel.refreshBypassState();
        closeBlockPicker();
        _signalChain.setLayout(_audioProcessor.getChainLayout());
        updateChainStatus();
    };
    _exportButton.onClick = [this]() {
        showOverlay(OverlayMode::Export);
    };

    _ampPanel.onStatusMessage = [this](const juce::String& message, bool success) {
        showStatus(message, success);
    };
    _ampPanel.onAssetsChanged = [this]() {
        updateChainStatus();
    };
    _cabinetPanel.onStatusMessage = [this](const juce::String& message, bool success) {
        showStatus(message, success);
    };
    _cabinetPanel.onAssetsChanged = [this]() {
        updateChainStatus();
    };

    _signalChain.onBlockSelected = [this](SignalChainStrip::BlockId blockId) {
        closeBlockPicker();
        showEditPanel(blockId);
    };
    _signalChain.onBlockBypassToggled = [this](SignalChainStrip::BlockId blockId) {
        toggleBlockBypass(blockId);
    };
    _signalChain.onSlotsSwapped = [this](int fromSlot, int toSlot) {
        _audioProcessor.swapChainSlots(fromSlot, toSlot);
    };
    _signalChain.onBlockPickerRequested = [this](int slot) {
        openBlockPicker(slot);
    };
    _blockPicker.onBlockChosen = [this](int slot, SignalChain::Stage stage) {
        placeChosenBlock(slot, stage);
    };
    _blockPicker.onBlockRemoved = [this](int slot) {
        _audioProcessor.clearChainSlot(slot);
        _signalChain.setLayout(_audioProcessor.getChainLayout());
        updateChainStatus();
    };
    _audioProcessor._apvts.state.addListener(this);
    _signalChain.setLayout(_audioProcessor.getChainLayout());

    _exportModule.onExportClicked = [this](const juce::NamedValueSet& values, const juce::File& destinationFile) {
        exportProfil(values, destinationFile);
    };
    _exportModule.onCreateProfileClicked = [this](const juce::NamedValueSet& values) {
        createProfil(values);
    };
    _exportModule.onCancelClicked = [this]() {
        showStudio();
    };

    _audioProcessor.getProfileManager().addChangeListener(this);
    _audioProcessor._apvts.addParameterListener("isEqEnabled", this);
    _audioProcessor._apvts.addParameterListener("isGateEnabled", this);
    _audioProcessor._apvts.addParameterListener("isAmpEnabled", this);
    _audioProcessor._apvts.addParameterListener("isCabEnabled", this);
    _audioProcessor._apvts.addParameterListener("isPedalEnabled", this);
    _audioProcessor._apvts.addParameterListener("noise", this);

    showEditPanel(SignalChainStrip::BlockId::AmpProfiler);
    updateChainStatus();
    setWantsKeyboardFocus(true);
    startTimerHz(30);
    resized();
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<ProfilerAudioProcessorEditor>(this)]() {
        if (safeThis != nullptr) {
            safeThis->grabKeyboardFocus();
        }
    });
}

ProfilerAudioProcessorEditor::~ProfilerAudioProcessorEditor() {
    stopTimer();
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>()) {
        window->setLookAndFeel(nullptr);
    }
    _audioProcessor.getProfileManager().removeChangeListener(this);
    _audioProcessor._apvts.removeParameterListener("isEqEnabled", this);
    _audioProcessor._apvts.removeParameterListener("isGateEnabled", this);
    _audioProcessor._apvts.removeParameterListener("isAmpEnabled", this);
    _audioProcessor._apvts.removeParameterListener("isCabEnabled", this);
    _audioProcessor._apvts.removeParameterListener("isPedalEnabled", this);
    _audioProcessor._apvts.removeParameterListener("noise", this);
    _audioProcessor._apvts.state.removeListener(this);
    setLookAndFeel(nullptr);
}

void ProfilerAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}

bool ProfilerAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    if (_blockPicker.isOpen() && key == juce::KeyPress::escapeKey) {
        closeBlockPicker();
        return true;
    }

    if (_overlayMode != OverlayMode::None) {
        return false;
    }

    if (auto* focused = juce::Component::getCurrentlyFocusedComponent()) {
        if (dynamic_cast<juce::TextEditor*>(focused) != nullptr || dynamic_cast<juce::ComboBox*>(focused) != nullptr) {
            return false;
        }
    }

    return _signalChain.keyPressed(key);
}

void ProfilerAudioProcessorEditor::parentHierarchyChanged() {
    applyHostWindowChrome();
}

void ProfilerAudioProcessorEditor::visibilityChanged() {
    if (isVisible()) {
        applyHostWindowChrome();
    }
}

void ProfilerAudioProcessorEditor::broughtToFront() {
    applyHostWindowChrome();
}

void ProfilerAudioProcessorEditor::mouseDown(const juce::MouseEvent& event) {
    applyHostWindowChrome();
    AudioProcessorEditor::mouseDown(event);
}

void ProfilerAudioProcessorEditor::applyHostWindowChrome() {
    applyStandaloneWindowChrome();

#if JUCE_WINDOWS
    if (_audioProcessor.wrapperType != juce::AudioProcessor::wrapperType_Standalone) {
        if (auto* peer = getPeer()) {
            restyleWindowsHostFrame(static_cast<HWND>(peer->getNativeHandle()), getWidth(), getHeight());
        }
    }
#else
    juce::ignoreUnused(juce::Desktop::getInstance().getDisplays());
#endif
}

void ProfilerAudioProcessorEditor::applyStandaloneWindowChrome() {
    auto* window = findParentComponentOfClass<juce::DocumentWindow>();
    if (window == nullptr) {
        return;
    }

    window->setLookAndFeel(&_customLookAndFeel);
    if (window->isUsingNativeTitleBar()) {
        window->setUsingNativeTitleBar(false);
    }
    window->setBackgroundColour(juce::Colours::black);
    window->setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::black);
    window->setColour(juce::DocumentWindow::backgroundColourId, juce::Colours::black);
    window->setTitleBarHeight(32);
    const auto buttons = _audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone
                             ? juce::DocumentWindow::minimiseButton | juce::DocumentWindow::maximiseButton | juce::DocumentWindow::closeButton
                             : juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton;
    window->setTitleBarButtonsRequired(buttons, false);
    window->repaint();

    if (_audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
        requestStandaloneFullscreen();
    }
}

void ProfilerAudioProcessorEditor::requestStandaloneFullscreen() {
    if (_standaloneFullscreenRequested) {
        return;
    }

    _standaloneFullscreenRequested = true;
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<ProfilerAudioProcessorEditor>(this)]() {
        if (safeThis != nullptr) {
            safeThis->openStandaloneFullscreen();
        }
    });
}

void ProfilerAudioProcessorEditor::openStandaloneFullscreen() {
    auto* window = findParentComponentOfClass<juce::DocumentWindow>();
    if (window == nullptr || window->getPeer() == nullptr || !window->isShowing()) {
        _standaloneFullscreenRequested = false;
        return;
    }

    const auto maxSize = standaloneMaxSize();
    setResizeLimits(editorWidth, editorHeight, maxSize.x, maxSize.y);
    window->setResizable(true, false);
    window->setFullScreen(true);
}

void ProfilerAudioProcessorEditor::resized() {
    _content.setTransform({});
    _content.setBounds(getLocalBounds());

    auto area = _content.getLocalBounds();
    _topBar.setBounds(area.removeFromTop(48));

    if (_overlayMode == OverlayMode::None) {
        constexpr int editHeight = 228;
        _editHost.setBounds(area.removeFromBottom(juce::jmin(editHeight, juce::jmax(0, area.getHeight() - 160))));
        _signalChain.setBounds(area);

        auto panelBounds = _editHost.getLocalBounds();
        _inputGatePanel.setBounds(panelBounds);
        _ampPanel.setBounds(panelBounds);
        _cabinetPanel.setBounds(panelBounds);
        _eqPanel.setBounds(panelBounds);
        _pedalPanel.setBounds(panelBounds);
        _masterPanel.setBounds(panelBounds);
        _fxPanel.setBounds(panelBounds);
        _blockPicker.setBounds(panelBounds);
        _closeOverlayButton.setBounds({});
    } else {
        area = area.reduced(12, 8);
        auto header = area.removeFromTop(36);
        _closeOverlayButton.setBounds(header.removeFromRight(88));
        area.removeFromTop(8);

        _settingsHost.setBounds(area);
        _libraryView.setBounds(area);
        _exportViewport.setBounds(area);

        auto settingsArea = _settingsHost.getLocalBounds().reduced(12);
        auto actions = settingsArea.removeFromBottom(40);
        _exportButton.setBounds(actions.removeFromRight(140));
        actions.removeFromRight(10);
        _resetButton.setBounds(actions.removeFromRight(140));
        settingsArea.removeFromBottom(12);
        _settingsView.setBounds(settingsArea);

        const auto exportHeight = juce::jmax(area.getHeight(), _exportModule.getRequiredHeight(area.getWidth()));
        _exportModule.setBounds(0, 0, area.getWidth(), exportHeight);
    }

    const auto bannerWidth = juce::jmin(_notificationBanner.getIdealWidth(), juce::jmax(220, _content.getWidth() - 50));
    _notificationBanner.setBounds(_content.getLocalBounds()
                                      .withSizeKeepingCentre(bannerWidth, _notificationBanner.getIdealHeight())
                                      .withRightX(_content.getWidth() - 22)
                                      .withY(18));
}

void ProfilerAudioProcessorEditor::showStudio() {
    closeBlockPicker();
    _overlayMode = OverlayMode::None;
    _signalChain.setVisible(true);
    _editHost.setVisible(true);
    _settingsHost.setVisible(false);
    _libraryView.setVisible(false);
    _exportViewport.setVisible(false);
    _closeOverlayButton.setVisible(false);
    grabKeyboardFocus();
    resized();
    repaint();
}

void ProfilerAudioProcessorEditor::showOverlay(OverlayMode mode) {
    if (mode == OverlayMode::None) {
        showStudio();
        return;
    }

    if (mode == OverlayMode::Export) {
        const auto exportName = getDefaultExportName();
        auto values = _audioProcessor.getProfileManager().getCurrentProfileValues(exportName);
        values.set("irPath", _audioProcessor.isIRLoaded()
                                 ? _audioProcessor.getCurrentIRFile().getFullPathName()
                                 : juce::String{});
        values.set("ampPath", _audioProcessor.isAmpFileLoaded()
                                  ? _audioProcessor.getCurrentAmpFile().getFullPathName()
                                  : juce::String{});
        _exportModule.setExportValues(values, getDefaultExportFile(exportName));
        _exportViewport.setViewPosition(0, 0);
    }

    _overlayMode = mode;
    closeBlockPicker();
    _signalChain.setVisible(false);
    _editHost.setVisible(false);
    _settingsHost.setVisible(mode == OverlayMode::Settings);
    _libraryView.setVisible(mode == OverlayMode::Library);
    _exportViewport.setVisible(mode == OverlayMode::Export);
    _closeOverlayButton.setVisible(true);
    resized();
    repaint();
}

void ProfilerAudioProcessorEditor::showEditPanel(SignalChainStrip::BlockId blockId) {
    _inputGatePanel.setVisible(blockId == SignalChainStrip::BlockId::InputGate);
    _ampPanel.setVisible(blockId == SignalChainStrip::BlockId::AmpProfiler);
    _cabinetPanel.setVisible(blockId == SignalChainStrip::BlockId::CabinetIr);
    _eqPanel.setVisible(blockId == SignalChainStrip::BlockId::EqPostFx);
    _pedalPanel.setVisible(blockId == SignalChainStrip::BlockId::PedalDrive);
    _masterPanel.setVisible(blockId == SignalChainStrip::BlockId::MasterVolume);
    const bool fx = blockId == SignalChainStrip::BlockId::Fx;
    _fxPanel.setVisible(fx);
    if (fx) {
        _fxPanel.setStage(_signalChain.getLayout().atSlot(_signalChain.getSelectedChainSlot()),
                          _signalChain.getSelectedChainSlot());
    }
}

void ProfilerAudioProcessorEditor::openBlockPicker(int slot) {
    if (_overlayMode != OverlayMode::None) {
        return;
    }

    _blockPicker.open(slot, _audioProcessor.getChainLayout());
}

void ProfilerAudioProcessorEditor::closeBlockPicker() {
    if (_blockPicker.isVisible()) {
        _blockPicker.setVisible(false);
    }
}

void ProfilerAudioProcessorEditor::placeChosenBlock(int slot, SignalChain::Stage stage) {
    _audioProcessor.placeChainStage(stage, slot);
    _signalChain.setLayout(_audioProcessor.getChainLayout());
    _signalChain.selectSlot(slot);
    updateChainStatus();
}

void ProfilerAudioProcessorEditor::updateChainStatus() {
    const auto ampLoaded = _audioProcessor.isAmpFileLoaded();
    const auto irLoaded = _audioProcessor.isIRLoaded();
    auto isEnabled = [this](const juce::String& parameterId, float fallback = 1.0f) {
        const auto* value = _audioProcessor._apvts.getRawParameterValue(parameterId);
        return (value != nullptr ? value->load() : fallback) > 0.5f;
    };
    const auto noise = _audioProcessor._apvts.getRawParameterValue("noise");
    const auto gateOn = isEnabled("isGateEnabled") && (noise == nullptr || noise->load() > 0.0f);
    const auto ampOn = ampLoaded && isEnabled("isAmpEnabled");
    const auto cabOn = irLoaded && isEnabled("isCabEnabled");
    const auto eqOn = isEnabled("isEqEnabled");
    const auto pedalOn = isEnabled("isPedalEnabled");

    _signalChain.setBlockLed(SignalChainStrip::BlockId::InputGate, gateOn);
    _signalChain.setBlockLed(SignalChainStrip::BlockId::MasterVolume, true);

    const auto layout = _audioProcessor.getChainLayout();
    for (int index = 0; index < SignalChain::movableSlotCount; ++index) {
        const auto slot = SignalChain::chainSlotForMovableIndex(index);
        const auto stage = layout.atSlot(slot);
        if (stage == SignalChain::Stage::Empty) {
            continue;
        }
        bool typeOn = true;
        switch (stage) {
            case SignalChain::Stage::Amp:
                typeOn = ampOn;
                break;
            case SignalChain::Stage::Cab:
                typeOn = cabOn;
                break;
            case SignalChain::Stage::Eq:
                typeOn = eqOn;
                break;
            case SignalChain::Stage::Pedal:
                typeOn = pedalOn;
                break;
            default:
                break;
        }
        _signalChain.setSlotLed(slot, typeOn && !_audioProcessor.isChainSlotBypassed(slot));
    }
    _fxPanel.refreshBypass();
}

void ProfilerAudioProcessorEditor::toggleBlockBypass(SignalChainStrip::BlockId blockId) {
    auto toggle = [this](const juce::String& parameterId) {
        auto* parameter = _audioProcessor._apvts.getParameter(parameterId);
        if (parameter == nullptr) {
            return;
        }

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->getValue() < 0.5f ? 1.0f : 0.0f);
        parameter->endChangeGesture();
    };

    switch (blockId) {
        case SignalChainStrip::BlockId::InputGate:
            toggle("isGateEnabled");
            _inputGatePanel.refreshBypassState();
            break;
        case SignalChainStrip::BlockId::AmpProfiler:
        case SignalChainStrip::BlockId::CabinetIr:
        case SignalChainStrip::BlockId::EqPostFx:
        case SignalChainStrip::BlockId::PedalDrive:
        case SignalChainStrip::BlockId::Fx:
            _audioProcessor.toggleChainSlotBypass(_signalChain.getSelectedChainSlot());
            _fxPanel.refreshBypass();
            break;
        case SignalChainStrip::BlockId::MasterVolume:
            break;
    }

    updateChainStatus();
}

void ProfilerAudioProcessorEditor::showStatus(const juce::String& message, bool success) {
    _notificationBanner.clearAction();
    _notificationBanner.showMessage(message,
                                    success ? NotificationBanner::Type::Success : NotificationBanner::Type::Error,
                                    5000);
    resized();
}

void ProfilerAudioProcessorEditor::exportProfil(const juce::NamedValueSet& values, const juce::File& destinationFile) {
    juce::String errorMessage;
    if (_audioProcessor.getProfileManager().exportProfile(values, destinationFile, &errorMessage)) {
        showStudio();
        showStatus("Profile exported successfully", true);
    } else {
        showStatus(errorMessage, false);
    }
}

void ProfilerAudioProcessorEditor::createProfil(const juce::NamedValueSet& values) {
    juce::String errorMessage;
    if (_audioProcessor.getProfileManager().createProfile(values, &errorMessage)) {
        showStudio();
        showStatus("Profile created successfully", true);
    } else {
        showStatus(errorMessage, false);
    }
}

juce::String ProfilerAudioProcessorEditor::getDefaultExportName() const {
    const auto& profileManager = _audioProcessor.getProfileManager();
    if (const auto* profile = profileManager.getProfile(profileManager.getCurrentProfileIndex())) {
        if (profile->name.trim().isNotEmpty()) {
            return profile->name;
        }
    }

    return "Exported Profile";
}

juce::File ProfilerAudioProcessorEditor::getDefaultExportFile(const juce::String& profileName) const {
    auto fileName = juce::File::createLegalFileName(profileName.trim());
    if (fileName.isEmpty()) {
        fileName = "Exported Profile";
    }

    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile(fileName + ProfileManager::profileFileExtension);
}

void ProfilerAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &_audioProcessor.getProfileManager()) {
        _ampPanel.refreshAssets();
        _cabinetPanel.refreshAssets();
        _eqPanel.refreshBypassState();
        _inputGatePanel.refreshBypassState();
        _pedalPanel.refreshBypassState();
        updateChainStatus();
    }
}

void ProfilerAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float /*newValue*/) {
    if (parameterID == "isEqEnabled" || parameterID == "isGateEnabled" || parameterID == "isAmpEnabled" || parameterID == "isCabEnabled" || parameterID == "isPedalEnabled" || parameterID == "noise") {
        auto refresh = [safeThis = juce::Component::SafePointer<ProfilerAudioProcessorEditor>(this)]() {
            if (safeThis == nullptr) {
                return;
            }

            safeThis->_eqPanel.refreshBypassState();
            safeThis->_inputGatePanel.refreshBypassState();
            safeThis->_ampPanel.refreshAssets();
            safeThis->_cabinetPanel.refreshAssets();
            safeThis->_pedalPanel.refreshBypassState();
            safeThis->updateChainStatus();
        };

        if (juce::MessageManager::existsAndIsCurrentThread()) {
            refresh();
        } else {
            juce::MessageManager::callAsync(std::move(refresh));
        }
    }
}

void ProfilerAudioProcessorEditor::timerCallback() {
    _signalChain.setIoMeterLevels(_audioProcessor.getRmsLevelInput(), _audioProcessor.getRmsLevelOutput());
    updateChainStatus();
    if (_hostChromePasses < kHostChromeRetryLimit) {
        applyHostWindowChrome();
        ++_hostChromePasses;
    }
}

void ProfilerAudioProcessorEditor::valueTreePropertyChanged(juce::ValueTree&,
                                                            const juce::Identifier& property) {
    if (property != juce::Identifier("chainLayout") && property != juce::Identifier("chainLayoutHigh")) {
        return;
    }

    _signalChain.setLayout(_audioProcessor.getChainLayout());
}

void ProfilerAudioProcessorEditor::valueTreeRedirected(juce::ValueTree&) {
    _signalChain.setLayout(_audioProcessor.getChainLayout());
}
