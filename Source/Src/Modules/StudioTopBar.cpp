#include "Modules/StudioTopBar.h"

#include "ActionFeedback.h"
#include "Components/CustomLookAndFeel.h"
#include "PluginProcessor.h"
#include "Stylesheet.h"

StudioTopBar::IconButton::IconButton(const juce::String& name, Icon icon)
    : juce::Button(name),
      _icon(icon) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void StudioTopBar::IconButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) {
    auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
    const auto bounds = getLocalBounds().toFloat();

    if (laf != nullptr) {
        laf->paintFlatButtonBackground(g,
                                       bounds,
                                       juce::Colours::black,
                                       isMouseOverButton,
                                       isButtonDown,
                                       getToggleState());
        const auto iconColour = ProfilerStyle::Contrast::enabled
                                    ? (isButtonDown || getToggleState() ? juce::Colours::black : juce::Colours::white)
                                    : ProfilerStyle::Colors::text.withAlpha(isMouseOverButton ? 1.0f : 0.86f);
        if (_icon == Icon::Gear) {
            laf->drawGearIcon(g, bounds.reduced(7.0f), iconColour);
        } else {
            laf->drawLibraryIcon(g, bounds.reduced(7.0f), iconColour);
        }
        return;
    }

    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds, ProfilerStyle::Surfaces::controlCorner);
}

StudioTopBar::StudioTopBar(ProfilerAudioProcessor& processor)
    : _audioProcessor(processor),
      _apvts(processor._apvts) {
    _logoLabel.setText("SPECTRA", juce::dontSendNotification);
    _logoLabel.setFont(ProfilerStyle::Fonts::logo(18.0f));
    _logoLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _logoLabel.setJustificationType(juce::Justification::centredLeft);
    _logoLabel.setMinimumHorizontalScale(1.0f);
    _logoLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(_logoLabel);

    addAndMakeVisible(_presetMenu);
    addAndMakeVisible(_libraryButton);
    addAndMakeVisible(_muteButton);
    addAndMakeVisible(_settingsButton);

    _muteButton.setClickingTogglesState(true);
    _muteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(_apvts, "isMute", _muteButton);

    _audioProcessor.getPresetManager().addChangeListener(this);
    refreshProfileMenu();
    restoreLastUsedProfile();

    _presetMenu.onChange = [this]() {
        selectProfileFromMenu();
    };

    _libraryButton.onClick = [this]() {
        if (onLibraryClicked) {
            onLibraryClicked();
        }
    };

    _settingsButton.onClick = [this]() {
        if (onSettingsClicked) {
            onSettingsClicked();
        }
    };
}

StudioTopBar::~StudioTopBar() {
    _audioProcessor.getPresetManager().removeChangeListener(this);
    _presetMenu.setLookAndFeel(nullptr);
}

void StudioTopBar::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}

void StudioTopBar::resized() {
    auto area = getLocalBounds().reduced(12, 6);
    _logoLabel.setBounds(area.removeFromLeft(196));

    auto right = area.removeFromRight(94);
    _settingsButton.setBounds(right.removeFromRight(32).withSizeKeepingCentre(32, 32));
    right.removeFromRight(6);
    _muteButton.setBounds(right.removeFromRight(56).withSizeKeepingCentre(56, 26));

    area = area.withSizeKeepingCentre(juce::jmin(340, area.getWidth() - 40), area.getHeight());
    _libraryButton.setBounds(area.removeFromRight(32).withSizeKeepingCentre(32, 32));
    area.removeFromRight(6);
    _presetMenu.setBounds(area);
}

void StudioTopBar::refreshProfileMenu() {
    const juce::ScopedValueSetter<bool> updatingProfileMenu(_isUpdatingProfileMenu, true);
    auto& presets = _audioProcessor.getPresetManager();
    const auto names = presets.getNames();

    _presetMenu.clear(juce::dontSendNotification);
    _presetMenu.addItem("Default Chain", 1);

    for (int index = 0; index < names.size(); ++index) {
        const auto name = names[index].isNotEmpty() ? names[index] : "Preset " + juce::String(index + 1);
        _presetMenu.addItem(name, index + 2);
    }

    const auto currentIndex = presets.getCurrentIndex();
    _presetMenu.setSelectedId(currentIndex >= 0 ? currentIndex + 2 : 1, juce::dontSendNotification);
}

void StudioTopBar::restoreLastUsedProfile() {
    auto& presets = _audioProcessor.getPresetManager();
    const auto currentIndex = presets.getCurrentIndex();
    if (currentIndex < 0) {
        if (presets.getCurrentId().isNotEmpty()) {
            presets.clearCurrentPreset();
        }
        return;
    }

    juce::String errorMessage;
    if (_audioProcessor.applyPreset(currentIndex, &errorMessage)) {
        const juce::ScopedValueSetter<bool> updatingProfileMenu(_isUpdatingProfileMenu, true);
        _presetMenu.setSelectedId(currentIndex + 2, juce::dontSendNotification);
        if (errorMessage.isNotEmpty() && onStatus) {
            onStatus(errorMessage, false);
        }
    }
}

void StudioTopBar::selectProfileFromMenu() {
    if (_isUpdatingProfileMenu) {
        return;
    }

    auto& presets = _audioProcessor.getPresetManager();
    const auto selectedId = _presetMenu.getSelectedId();
    if (selectedId == 1) {
        resetParametersToDefaults();
        resetLoadedFiles();
        presets.clearCurrentPreset();
        if (onPresetApplied) {
            onPresetApplied();
        }
        ActionFeedback::post(*this, "Default chain selected.");
        return;
    }

    juce::String errorMessage;
    if (_audioProcessor.applyPreset(selectedId - 2, &errorMessage)) {
        if (errorMessage.isNotEmpty() && onStatus) {
            onStatus(errorMessage, false);
        }
        if (onPresetApplied) {
            onPresetApplied();
        }
        ActionFeedback::post(*this, errorMessage.isNotEmpty() ? errorMessage : "Preset loaded: " + _presetMenu.getText(), errorMessage.isNotEmpty());
    } else {
        if (onStatus && errorMessage.isNotEmpty()) {
            onStatus(errorMessage, false);
        }
        refreshProfileMenu();
        ActionFeedback::post(*this, errorMessage.isNotEmpty() ? errorMessage : "Could not load the selected preset.", true);
    }
}

juce::String StudioTopBar::restoreDefaults() {
    auto& presets = _audioProcessor.getPresetManager();
    const auto currentIndex = presets.getCurrentIndex();
    if (currentIndex >= 0) {
        juce::String errorMessage;
        if (_audioProcessor.applyPreset(currentIndex, &errorMessage)) {
            refreshProfileMenu();
            if (onPresetApplied) {
                onPresetApplied();
            }
            return errorMessage.isNotEmpty() ? errorMessage : "Current preset restored";
        }
    }

    resetParametersToDefaults();
    resetLoadedFiles();
    if (presets.getCurrentId().isNotEmpty()) {
        presets.clearCurrentPreset();
        refreshProfileMenu();
    }
    if (onPresetApplied) {
        onPresetApplied();
    }
    return "Default chain restored";
}

void StudioTopBar::resetParametersToDefaults() {
    for (auto* parameter : _audioProcessor.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
    }
    _audioProcessor.resetChainLayout();
}

void StudioTopBar::resetLoadedFiles() {
    _audioProcessor.unloadIRFile();
    _audioProcessor.unloadAmpFile();
    _audioProcessor.clearAppliedProfile();
}

void StudioTopBar::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &_audioProcessor.getPresetManager()) {
        refreshProfileMenu();
    }
}
