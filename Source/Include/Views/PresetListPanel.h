#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "Components/CustomTextEditor.h"

class ProfilerAudioProcessor;

class PresetListPanel : public juce::Component,
                        private juce::ChangeListener {
   public:
    explicit PresetListPanel(ProfilerAudioProcessor& processor);
    ~PresetListPanel() override;

    std::function<void(const juce::String& warning)> onPresetLoaded;
    std::function<void(const juce::String& message, bool success)> onStatus;
    std::function<void()> onPresetsChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

   private:
    class Row;

    ProfilerAudioProcessor& _processor;
    juce::Label _title;
    juce::Label _countLabel;
    juce::Label _helpLabel;
    juce::Viewport _viewport;
    juce::Component _rowHost;
    std::vector<std::unique_ptr<Row>> _rows;
    CustomTextEditor _renameEditor;
    juce::String _renamingId;
    juce::String _focusedId;
    bool _contextKeyDown = false;

    void rebuildRows();
    void layoutRows();
    Row* findRow(const juce::String& presetId) const;
    int focusedRowIndex() const;
    void setFocusedId(const juce::String& presetId);
    void moveFocus(int delta);

    void loadPreset(const juce::String& presetId);
    void showContextMenu(const juce::String& presetId, juce::Rectangle<int> screenTarget);
    void showEmptyAreaMenu(juce::Rectangle<int> screenTarget);
    void beginRename(const juce::String& presetId);
    void commitRename();
    void cancelRename(bool refocusList = true);
    void saveCurrentChainInto(const juce::String& presetId);
    void duplicatePreset(const juce::String& presetId);
    void resetPresetToEmpty(const juce::String& presetId);
    void saveCurrentChainAsNewPreset();
    void confirm(const juce::String& title,
                 const juce::String& message,
                 const juce::String& confirmLabel,
                 std::function<void()> onConfirmed);
    void report(const juce::String& message, bool success);
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetListPanel)
};
