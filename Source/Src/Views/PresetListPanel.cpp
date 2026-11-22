#include "Views/PresetListPanel.h"

#include "PluginProcessor.h"
#include "Stylesheet.h"

namespace {
constexpr int kRowHeight = 32;
constexpr int kRowGap = 8;

#if JUCE_WINDOWS
constexpr int kContextMenuKeyCode = 0x5D;
#endif

enum MenuItem {
    renameItem = 1,
    saveItem,
    duplicateItem,
    resetItem,
    newPresetItem
};
}  // namespace

class PresetListPanel::Row : public juce::Component {
   public:
    Row(PresetListPanel& owner, juce::String presetId, juce::String name, bool active)
        : _owner(owner),
          _presetId(std::move(presetId)),
          _name(std::move(name)),
          _active(active) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(false);
    }

    const juce::String& presetId() const noexcept { return _presetId; }

    void setFocused(bool focused) {
        if (_focused != focused) {
            _focused = focused;
            repaint();
        }
    }

    juce::Rectangle<int> textBounds() const {
        return getLocalBounds().withTrimmedLeft(24).withTrimmedRight(8);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        const auto corner = ProfilerStyle::Surfaces::controlCorner;
        g.setColour(_active ? ProfilerStyle::Colors::accent.withAlpha(0.08f)
                            : (isMouseOver() ? ProfilerStyle::Colors::text.withAlpha(0.04f) : juce::Colours::black));
        g.fillRoundedRectangle(bounds, corner);

        auto border = ProfilerStyle::Colors::border;
        if (_active) {
            border = ProfilerStyle::Colors::accent;
        } else if (_focused) {
            border = ProfilerStyle::Colors::textMuted;
        } else if (isMouseOver()) {
            border = ProfilerStyle::Colors::lighterGrey;
        }
        g.setColour(border);
        g.drawRoundedRectangle(bounds, corner, _active || _focused ? 1.4f : 1.0f);

        const auto dot = juce::Rectangle<float>(6.0f, 6.0f).withCentre({12.0f, bounds.getCentreY()});
        g.setColour(_active ? ProfilerStyle::Colors::accent : ProfilerStyle::Colors::lighterGrey);
        g.fillEllipse(dot);

        g.setColour(ProfilerStyle::Colors::text);
        g.setFont(ProfilerStyle::Fonts::medium(12.0f));
        g.drawFittedText(_name, textBounds(), juce::Justification::centredLeft, 1, 0.8f);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseDown(const juce::MouseEvent& event) override {
        _owner.grabKeyboardFocus();
        _owner.setFocusedId(_presetId);
        if (event.mods.isPopupMenu()) {
            _owner.showContextMenu(_presetId, juce::Rectangle<int>(1, 1).withPosition(event.getScreenPosition()));
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        if (event.mouseWasClicked() && !event.mods.isPopupMenu() && event.mods.isLeftButtonDown()) {
            const auto presetId = _presetId;
            const juce::Component::SafePointer<PresetListPanel> owner(&_owner);
            juce::MessageManager::callAsync([owner, presetId]() {
                if (owner != nullptr) {
                    owner->loadPreset(presetId);
                }
            });
        }
    }

   private:
    PresetListPanel& _owner;
    juce::String _presetId;
    juce::String _name;
    bool _active = false;
    bool _focused = false;
};

PresetListPanel::PresetListPanel(ProfilerAudioProcessor& processor)
    : _processor(processor) {
    setWantsKeyboardFocus(true);

    _title.setText("PRESETS", juce::dontSendNotification);
    _title.setFont(ProfilerStyle::Fonts::moduleTitle());
    _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::caption);
    _title.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(_title);

    _countLabel.setFont(ProfilerStyle::Fonts::moduleTitle());
    _countLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::caption);
    _countLabel.setJustificationType(juce::Justification::centredRight);
    _countLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(_countLabel);

    _helpLabel.setText("Click: load preset and return to the rig.\nRight-click: rename, save, duplicate, reset.",
                       juce::dontSendNotification);
    _helpLabel.setFont(ProfilerStyle::Fonts::regular(10.0f));
    _helpLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    _helpLabel.setJustificationType(juce::Justification::bottomLeft);
    _helpLabel.setMinimumHorizontalScale(0.7f);
    _helpLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(_helpLabel);

    _viewport.setViewedComponent(&_rowHost, false);
    _viewport.setScrollBarsShown(true, false);
    _viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, ProfilerStyle::Colors::accent);
    _viewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(_viewport);
    _rowHost.addMouseListener(this, false);

    _renameEditor.setSelectAllWhenFocused(true);
    _renameEditor.setFont(ProfilerStyle::Fonts::medium(12.0f));
    _renameEditor.onReturnKey = [this]() { commitRename(); };
    _renameEditor.onEscapeKey = [this]() { cancelRename(); };
    _renameEditor.onFocusLost = [this]() { cancelRename(false); };
    _rowHost.addChildComponent(_renameEditor);

    _processor.getPresetManager().addChangeListener(this);
    _focusedId = _processor.getPresetManager().getCurrentId();
    rebuildRows();
}

PresetListPanel::~PresetListPanel() {
    _processor.getPresetManager().removeChangeListener(this);
    _rowHost.removeMouseListener(this);
}

void PresetListPanel::paint(juce::Graphics& g) {
    if (!_rows.empty()) {
        return;
    }
    g.setColour(ProfilerStyle::Colors::textMuted);
    g.setFont(ProfilerStyle::Fonts::regular(12.0f));
    g.drawFittedText("No saved presets yet.\nRight-click here to save the current chain.",
                     _viewport.getBounds().removeFromTop(64).reduced(4),
                     juce::Justification::centredLeft,
                     3);
}

void PresetListPanel::resized() {
    auto area = getLocalBounds().reduced(16);
    auto header = area.removeFromTop(22);
    _countLabel.setBounds(header.removeFromRight(40));
    _title.setBounds(header);
    area.removeFromTop(10);
    _helpLabel.setBounds(area.removeFromBottom(32));
    area.removeFromBottom(10);
    _viewport.setBounds(area);
    layoutRows();
}

void PresetListPanel::mouseDown(const juce::MouseEvent& event) {
    if (event.eventComponent != this && event.eventComponent != &_rowHost) {
        return;
    }
    grabKeyboardFocus();
    if (event.mods.isPopupMenu()) {
        showEmptyAreaMenu(juce::Rectangle<int>(1, 1).withPosition(event.getScreenPosition()));
    }
}

bool PresetListPanel::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::upKey) {
        moveFocus(-1);
        return true;
    }
    if (key == juce::KeyPress::downKey) {
        moveFocus(1);
        return true;
    }
    if (key == juce::KeyPress::returnKey) {
        if (_focusedId.isNotEmpty()) {
            loadPreset(_focusedId);
        }
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::F10Key && key.getModifiers().isShiftDown()) {
        if (auto* row = findRow(_focusedId)) {
            showContextMenu(_focusedId, row->getScreenBounds());
        } else {
            showEmptyAreaMenu(_viewport.getScreenBounds().withHeight(kRowHeight));
        }
        return true;
    }
    return false;
}

bool PresetListPanel::keyStateChanged(bool isKeyDown) {
#if JUCE_WINDOWS
    // Windows reports the context-menu key as a modifier, so it never reaches keyPressed().
    const auto down = isKeyDown && juce::KeyPress::isKeyCurrentlyDown(kContextMenuKeyCode);
    if (down && !_contextKeyDown) {
        _contextKeyDown = true;
        if (auto* row = findRow(_focusedId)) {
            showContextMenu(_focusedId, row->getScreenBounds());
        } else {
            showEmptyAreaMenu(_viewport.getScreenBounds().withHeight(kRowHeight));
        }
        return true;
    }
    _contextKeyDown = down;
#else
    juce::ignoreUnused(isKeyDown);
#endif
    return false;
}

void PresetListPanel::rebuildRows() {
    auto& presets = _processor.getPresetManager();
    const auto activeId = presets.getCurrentId();

    for (auto& row : _rows) {
        _rowHost.removeChildComponent(row.get());
    }
    _rows.clear();

    for (int index = 0; index < presets.getCount(); ++index) {
        const auto* preset = presets.getPreset(index);
        auto row = std::make_unique<Row>(*this, preset->id, preset->name, preset->id == activeId);
        _rowHost.addAndMakeVisible(*row);
        _rows.push_back(std::move(row));
    }

    if (findRow(_focusedId) == nullptr) {
        _focusedId = findRow(activeId) != nullptr ? activeId : (_rows.empty() ? juce::String{} : _rows.front()->presetId());
    }
    for (auto& row : _rows) {
        row->setFocused(row->presetId() == _focusedId);
    }
    if (_renamingId.isNotEmpty() && findRow(_renamingId) == nullptr) {
        cancelRename(false);
    }

    _countLabel.setText(juce::String(presets.getCount()), juce::dontSendNotification);
    _renameEditor.toFront(false);
    layoutRows();
    repaint();
    if (onPresetsChanged) {
        onPresetsChanged();
    }
}

void PresetListPanel::layoutRows() {
    const auto scrollbar = _viewport.getScrollBarThickness() + 2;
    const auto width = juce::jmax(0, _viewport.getWidth() - scrollbar);
    int y = 0;
    for (auto& row : _rows) {
        row->setBounds(0, y, width, kRowHeight);
        y += kRowHeight + kRowGap;
    }
    _rowHost.setSize(width, juce::jmax(_viewport.getHeight(), y));

    if (auto* row = findRow(_renamingId)) {
        _renameEditor.setBounds(row->getBounds().withTrimmedLeft(18).reduced(2, 3));
    }
}

PresetListPanel::Row* PresetListPanel::findRow(const juce::String& presetId) const {
    if (presetId.isEmpty()) {
        return nullptr;
    }
    for (const auto& row : _rows) {
        if (row->presetId() == presetId) {
            return row.get();
        }
    }
    return nullptr;
}

int PresetListPanel::focusedRowIndex() const {
    for (int index = 0; index < static_cast<int>(_rows.size()); ++index) {
        if (_rows[static_cast<size_t>(index)]->presetId() == _focusedId) {
            return index;
        }
    }
    return -1;
}

void PresetListPanel::setFocusedId(const juce::String& presetId) {
    _focusedId = presetId;
    for (auto& row : _rows) {
        row->setFocused(row->presetId() == _focusedId);
    }
    if (auto* row = findRow(_focusedId)) {
        const auto bounds = row->getBounds();
        auto view = _viewport.getViewPosition();
        if (bounds.getY() < view.y) {
            view.y = bounds.getY();
        } else if (bounds.getBottom() > view.y + _viewport.getViewHeight()) {
            view.y = bounds.getBottom() - _viewport.getViewHeight();
        }
        _viewport.setViewPosition(view);
    }
}

void PresetListPanel::moveFocus(int delta) {
    if (_rows.empty()) {
        return;
    }
    const auto current = focusedRowIndex();
    const auto next = current < 0 ? 0 : juce::jlimit(0, static_cast<int>(_rows.size()) - 1, current + delta);
    setFocusedId(_rows[static_cast<size_t>(next)]->presetId());
}

void PresetListPanel::loadPreset(const juce::String& presetId) {
    const auto index = _processor.getPresetManager().indexOfId(presetId);
    if (index < 0) {
        report("Preset not found.", false);
        return;
    }

    juce::String warning;
    if (!_processor.applyPreset(index, &warning)) {
        report(warning.isNotEmpty() ? warning : "Could not load the preset.", false);
        return;
    }
    if (onPresetLoaded) {
        onPresetLoaded(warning);
    }
}

void PresetListPanel::showContextMenu(const juce::String& presetId, juce::Rectangle<int> screenTarget) {
    cancelRename();
    juce::PopupMenu menu;
    menu.addItem(renameItem, "Rename");
    menu.addItem(saveItem, "Save current preset");
    menu.addItem(duplicateItem, "Duplicate");
    menu.addSeparator();
    menu.addItem(juce::PopupMenu::Item("Reset to empty").setID(resetItem).setColour(ProfilerStyle::Colors::accent));

    const juce::Component::SafePointer<PresetListPanel> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenTarget),
                       [safeThis, presetId](int result) {
                           if (safeThis == nullptr) {
                               return;
                           }
                           switch (result) {
                               case renameItem:
                                   safeThis->beginRename(presetId);
                                   break;
                               case saveItem:
                                   safeThis->saveCurrentChainInto(presetId);
                                   break;
                               case duplicateItem:
                                   safeThis->duplicatePreset(presetId);
                                   break;
                               case resetItem:
                                   safeThis->resetPresetToEmpty(presetId);
                                   break;
                               default:
                                   safeThis->grabKeyboardFocus();
                                   break;
                           }
                       });
}

void PresetListPanel::showEmptyAreaMenu(juce::Rectangle<int> screenTarget) {
    cancelRename();
    juce::PopupMenu menu;
    menu.addItem(newPresetItem, "Save current chain as new preset");
    const juce::Component::SafePointer<PresetListPanel> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenTarget), [safeThis](int result) {
        if (safeThis != nullptr && result == newPresetItem) {
            safeThis->saveCurrentChainAsNewPreset();
        }
    });
}

void PresetListPanel::beginRename(const juce::String& presetId) {
    const auto* preset = _processor.getPresetManager().getPreset(_processor.getPresetManager().indexOfId(presetId));
    if (preset == nullptr) {
        return;
    }
    _renamingId = presetId;
    _renameEditor.setText(preset->name, juce::dontSendNotification);
    layoutRows();
    _renameEditor.setVisible(true);
    _renameEditor.toFront(false);
    _renameEditor.grabKeyboardFocus();
    _renameEditor.selectAll();
}

void PresetListPanel::commitRename() {
    if (_renamingId.isEmpty()) {
        return;
    }
    auto& presets = _processor.getPresetManager();
    const auto index = presets.indexOfId(_renamingId);
    const auto* preset = presets.getPreset(index);
    const auto newName = _renameEditor.getText().trim();
    if (preset != nullptr && newName == preset->name) {
        cancelRename();
        return;
    }

    juce::String error;
    if (!presets.renamePreset(index, newName, &error)) {
        report(error.isNotEmpty() ? error : "Could not rename the preset.", false);
        _renameEditor.grabKeyboardFocus();
        return;
    }
    cancelRename();
    report("Preset renamed", true);
}

void PresetListPanel::cancelRename(bool refocusList) {
    if (_renamingId.isEmpty()) {
        return;
    }
    _renamingId.clear();
    _renameEditor.setVisible(false);
    if (refocusList && isShowing()) {
        grabKeyboardFocus();
    }
}

void PresetListPanel::saveCurrentChainInto(const juce::String& presetId) {
    const auto index = _processor.getPresetManager().indexOfId(presetId);
    const auto* preset = _processor.getPresetManager().getPreset(index);
    if (preset == nullptr) {
        report("Preset not found.", false);
        return;
    }

    auto save = [safeThis = juce::Component::SafePointer<PresetListPanel>(this), presetId]() {
        if (safeThis == nullptr) {
            return;
        }
        juce::String error;
        const auto target = safeThis->_processor.getPresetManager().indexOfId(presetId);
        if (safeThis->_processor.overwritePresetWithCurrentChain(target, &error)) {
            safeThis->report("Preset saved", true);
        } else {
            safeThis->report(error.isNotEmpty() ? error : "Could not save the preset.", false);
        }
    };

    if (_processor.presetMatchesCurrentChain(index)) {
        save();
        return;
    }
    confirm("Save current preset",
            "Replace \"" + preset->name + "\" with the current chain? Its saved settings will be overwritten.",
            "Save",
            std::move(save));
}

void PresetListPanel::duplicatePreset(const juce::String& presetId) {
    auto& presets = _processor.getPresetManager();
    juce::String newId;
    juce::String error;
    if (!presets.duplicatePreset(presets.indexOfId(presetId), &newId, &error)) {
        report(error.isNotEmpty() ? error : "Could not duplicate the preset.", false);
        return;
    }
    _focusedId = newId;
    report("Preset duplicated", true);
}

void PresetListPanel::resetPresetToEmpty(const juce::String& presetId) {
    const auto* preset = _processor.getPresetManager().getPreset(_processor.getPresetManager().indexOfId(presetId));
    if (preset == nullptr) {
        report("Preset not found.", false);
        return;
    }

    confirm("Reset to empty",
            "Reset \"" + preset->name + "\" to an empty chain? Its blocks and settings will be lost.",
            "Reset",
            [safeThis = juce::Component::SafePointer<PresetListPanel>(this), presetId]() {
                if (safeThis == nullptr) {
                    return;
                }
                juce::String error;
                const auto target = safeThis->_processor.getPresetManager().indexOfId(presetId);
                if (safeThis->_processor.resetPresetToEmpty(target, &error)) {
                    safeThis->report("Preset reset to an empty chain", true);
                } else {
                    safeThis->report(error.isNotEmpty() ? error : "Could not reset the preset.", false);
                }
            });
}

void PresetListPanel::saveCurrentChainAsNewPreset() {
    auto& presets = _processor.getPresetManager();
    auto number = presets.getCount() + 1;
    while (presets.isNameTaken("Preset " + juce::String(number))) {
        ++number;
    }

    juce::String error;
    if (_processor.saveCurrentPreset("Preset " + juce::String(number), &error)) {
        _focusedId = presets.getCurrentId();
        report("Preset saved", true);
    } else {
        report(error.isNotEmpty() ? error : "Could not save the preset.", false);
    }
}

void PresetListPanel::confirm(const juce::String& title,
                              const juce::String& message,
                              const juce::String& confirmLabel,
                              std::function<void()> onConfirmed) {
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::WarningIcon)
                                     .withTitle(title)
                                     .withMessage(message)
                                     .withButton(confirmLabel)
                                     .withButton("Cancel")
                                     .withParentComponent(getTopLevelComponent()),
                                 [safeThis = juce::Component::SafePointer<PresetListPanel>(this),
                                  onConfirmed = std::move(onConfirmed)](int result) {
                                     if (result == 1 && onConfirmed) {
                                         onConfirmed();
                                     }
                                     if (safeThis != nullptr && safeThis->isShowing()) {
                                         safeThis->grabKeyboardFocus();
                                     }
                                 });
}

void PresetListPanel::report(const juce::String& message, bool success) {
    if (onStatus) {
        onStatus(message, success);
    }
}

void PresetListPanel::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &_processor.getPresetManager()) {
        rebuildRows();
    }
}
