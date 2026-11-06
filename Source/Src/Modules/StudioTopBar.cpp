#include "Modules/StudioTopBar.h"

#include "Components/CustomLookAndFeel.h"
#include "PluginProcessor.h"
#include "Stylesheet.h"
#include "ActionFeedback.h"

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
    _logoLabel.setText("PROFILER", juce::dontSendNotification);
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

    _audioProcessor.getProfileManager().addChangeListener(this);
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
    _audioProcessor.getProfileManager().removeChangeListener(this);
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
    auto& profileManager = _audioProcessor.getProfileManager();
    const auto profileNames = profileManager.getProfileNames();

    _presetMenu.clear(juce::dontSendNotification);
    _presetMenu.addItem("Default Chain", 1);

    for (int index = 0; index < profileNames.size(); ++index) {
        const auto profileName = profileNames[index].isNotEmpty()
                                     ? profileNames[index]
                                     : "Profil " + juce::String(index + 1);
        _presetMenu.addItem(profileName, index + 2);
    }

    const auto currentProfileIndex = profileManager.getCurrentProfileIndex();
    const auto selectedId = currentProfileIndex >= 0 ? currentProfileIndex + 2 : 1;
    _presetMenu.setSelectedId(selectedId, juce::dontSendNotification);
}

void StudioTopBar::restoreLastUsedProfile() {
    auto& profileManager = _audioProcessor.getProfileManager();

    if (profileManager.getCurrentProfileId().isEmpty()) {
        return;
    }

    const auto currentProfileIndex = profileManager.getCurrentProfileIndex();
    if (currentProfileIndex < 0) {
        resetParametersToDefaults();
        resetLoadedFiles();
        profileManager.clearCurrentProfile();
        refreshProfileMenu();
        return;
    }

    if (_audioProcessor.getAppliedProfileId() == profileManager.getCurrentProfileId()) {
        const juce::ScopedValueSetter<bool> updatingProfileMenu(_isUpdatingProfileMenu, true);
        _presetMenu.setSelectedId(currentProfileIndex + 2, juce::dontSendNotification);
        return;
    }

    juce::String errorMessage;
    if (_audioProcessor.applyProfile(currentProfileIndex, &errorMessage)) {
        const juce::ScopedValueSetter<bool> updatingProfileMenu(_isUpdatingProfileMenu, true);
        _presetMenu.setSelectedId(currentProfileIndex + 2, juce::dontSendNotification);
    } else {
        resetParametersToDefaults();
        resetLoadedFiles();
        profileManager.clearCurrentProfile();
        refreshProfileMenu();
    }
}

void StudioTopBar::selectProfileFromMenu() {
    if (_isUpdatingProfileMenu) {
        return;
    }

    auto& profileManager = _audioProcessor.getProfileManager();
    const auto selectedId = _presetMenu.getSelectedId();
    if (selectedId == 1) {
        resetParametersToDefaults();
        resetLoadedFiles();
        profileManager.clearCurrentProfile();
        ActionFeedback::post(*this, "Default chain selected.");
        return;
    }

    const auto profileIndex = selectedId - 2;
    juce::String errorMessage;
    if (_audioProcessor.applyProfile(profileIndex, &errorMessage)) {
        if (const auto* profile = profileManager.getProfile(profileIndex)) {
            profileManager.setCurrentProfileId(profile->id);
            ActionFeedback::post(*this, "Profile loaded: " + profile->name);
        }
    } else {
        refreshProfileMenu();
        ActionFeedback::post(*this, errorMessage.isNotEmpty() ? errorMessage : "Could not load the selected profile.", true);
    }
}

juce::String StudioTopBar::restoreDefaults() {
    auto& profileManager = _audioProcessor.getProfileManager();
    const auto currentProfileIndex = profileManager.getCurrentProfileIndex();

    if (currentProfileIndex >= 0) {
        juce::String errorMessage;
        if (_audioProcessor.applyProfile(currentProfileIndex, &errorMessage)) {
            refreshProfileMenu();
            return "Current profile restored";
        }
    }

    resetParametersToDefaults();
    resetLoadedFiles();

    if (profileManager.getCurrentProfileId().isNotEmpty()) {
        profileManager.clearCurrentProfile();
        refreshProfileMenu();
    }

    return "Default settings restored";
}

void StudioTopBar::resetParametersToDefaults() {
    auto resetParam = [this](const juce::String& paramID) {
        if (auto* param = _apvts.getParameter(paramID)) {
            param->setValueNotifyingHost(param->getDefaultValue());
        }
    };

    resetParam("master");
    resetParam("gain");
    resetParam("noise");
    resetParam("input");
    resetParam("output");
    for (const auto& band : EqBands::specs) {
        resetParam(band.gainId);
        resetParam(band.freqId);
    }
    resetParam("isMute");
    resetParam("isEqEnabled");
    resetParam("isGateEnabled");
    resetParam("isAmpEnabled");
    resetParam("isCabEnabled");
    resetParam("cabLowCut");
    resetParam("isPedalEnabled");
    resetParam("pedalDrive");
    resetParam("pedalTone");
    resetParam("pedalLevel");
    _audioProcessor.resetChainLayout();
}

void StudioTopBar::resetLoadedFiles() {
    _audioProcessor.unloadIRFile();
    _audioProcessor.unloadAmpFile();
    _audioProcessor.clearAppliedProfile();
}

void StudioTopBar::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &_audioProcessor.getProfileManager()) {
        refreshProfileMenu();
    }
}
