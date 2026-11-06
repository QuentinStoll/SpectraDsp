#include "Views/SettingsView.h"

#include "PluginProcessor.h"
#include "Stylesheet.h"
#include "UiSettings.h"
#include "ActionFeedback.h"

namespace {
enum BackgroundPresetId {
    defaultPreset = 1,
    bluePreset,
    purplePreset,
    greenPreset,
    redPreset
};

void repaintComponentTree(juce::Component& component) {
    component.repaint();

    for (int index = 0; index < component.getNumChildComponents(); ++index) {
        if (auto* child = component.getChildComponent(index)) {
            repaintComponentTree(*child);
        }
    }
}

void applyBackgroundColourPreset(int presetId) {
    switch (UiSettings::normaliseBackgroundColourPreset(presetId)) {
        case bluePreset:
            ProfilerStyle::Colors::appBackground = juce::Colour(0xff182a44);
            break;

        case purplePreset:
            ProfilerStyle::Colors::appBackground = juce::Colour(0xff30203f);
            break;

        case greenPreset:
            ProfilerStyle::Colors::appBackground = juce::Colour(0xff18352a);
            break;

        case redPreset:
            ProfilerStyle::Colors::appBackground = juce::Colour(0xff431f1f);
            break;

        case defaultPreset:
        default:
            ProfilerStyle::Colors::appBackground = ProfilerStyle::Colors::background;
            break;
    }
}
}  // namespace

SettingsView::SettingsView(ProfilerAudioProcessor& processor)
    : _capturePanel(processor), _ampPanel(processor) {
    _generalTab.onClick = [this]() {
        _page = Page::General;
        resized();
        ActionFeedback::post(*this, "General settings opened.");
    };
    _captureTab.onClick = [this]() {
        _page = Page::CloneIr;
        resized();
        ActionFeedback::post(*this, "IR capture settings opened.");
    };
    _ampTab.onClick = [this]() {
        _page = Page::CloneAmp;
        resized();
        ActionFeedback::post(*this, "Amp capture settings opened.");
    };
    addAndMakeVisible(_generalTab);
    addAndMakeVisible(_captureTab);
    addAndMakeVisible(_ampTab);
    addChildComponent(_capturePanel);
    addChildComponent(_ampPanel);

    _titleLabel.setText("Settings", juce::dontSendNotification);
    _titleLabel.setJustificationType(juce::Justification::centred);
    _titleLabel.setFont(ProfilerStyle::Fonts::bold(24.0f));
    _titleLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);

    addAndMakeVisible(_titleLabel);

    _backgroundLabel.setText("Background colour", juce::dontSendNotification);
    _backgroundLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);

    addAndMakeVisible(_backgroundLabel);

    _backgroundMenu.addItem("Default", defaultPreset);
    _backgroundMenu.addItem("Blue", bluePreset);
    _backgroundMenu.addItem("Purple", purplePreset);
    _backgroundMenu.addItem("Green", greenPreset);
    _backgroundMenu.addItem("Red", redPreset);

    _backgroundMenu.setSelectedId(UiSettings::loadBackgroundColourPreset(),
                                  juce::dontSendNotification);
    applyBackgroundColour();

    _backgroundMenu.onChange = [this]() {
        applyBackgroundColour();
        ActionFeedback::post(*this, "Background colour: " + _backgroundMenu.getText());
    };

    addAndMakeVisible(_backgroundMenu);

    _interfaceScaleLabel.setText("Interface scale", juce::dontSendNotification);
    _interfaceScaleLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_interfaceScaleLabel);
    _interfaceScaleMenu.setTitle("Interface scale");
    _interfaceScaleMenu.setTooltip("Enlarge text and controls. Scroll to reach content that does not fit the window.");
    for (int percent = 100; percent <= 200; percent += 25) {
        _interfaceScaleMenu.addItem(juce::String(percent) + "%", percent);
    }
    _interfaceScaleMenu.setSelectedId(UiSettings::loadInterfaceScalePercent(), juce::dontSendNotification);
    _interfaceScaleMenu.onChange = [this]() {
        const int percent = _interfaceScaleMenu.getSelectedId();
        UiSettings::saveInterfaceScalePercent(percent);
        if (onInterfaceScaleChanged) {
            onInterfaceScaleChanged(percent);
        }
        ActionFeedback::post(*this, "Interface scale: " + juce::String(percent) + "%");
    };
    addAndMakeVisible(_interfaceScaleMenu);

    _contrastLabel.setText("High contrast", juce::dontSendNotification);
    addAndMakeVisible(_contrastLabel);
    _contrastMenu.setTitle("High contrast");
    _contrastMenu.setTooltip("Use bright text and outlines on black backgrounds.");
    _contrastMenu.addItem("Off", 1);
    _contrastMenu.addItem("On", 2);
    _contrastMenu.setSelectedId(UiSettings::loadHighContrastEnabled() ? 2 : 1, juce::dontSendNotification);
    _contrastMenu.onChange = [this]() {
        const bool enabled = _contrastMenu.getSelectedId() == 2;
        UiSettings::saveHighContrastEnabled(enabled);
        ProfilerStyle::Contrast::setEnabled(enabled);
        refreshContrastSetting();
        refreshColours();
        ActionFeedback::post(*this, enabled ? "High contrast enabled." : "High contrast disabled.");
    };
    addAndMakeVisible(_contrastMenu);

    _actionStatusLabel.setText("Action status messages", juce::dontSendNotification);
    addAndMakeVisible(_actionStatusLabel);
    _actionStatusMenu.setTitle("Action status messages");
    _actionStatusMenu.setTooltip("Keep readable feedback for your latest action at the bottom of the window.");
    _actionStatusMenu.addItem("Off", 1);
    _actionStatusMenu.addItem("On", 2);
    _actionStatusMenu.setSelectedId(UiSettings::loadActionStatusEnabled() ? 2 : 1, juce::dontSendNotification);
    _actionStatusMenu.onChange = [this]() {
        const bool enabled = _actionStatusMenu.getSelectedId() == 2;
        UiSettings::saveActionStatusEnabled(enabled);
        if (onActionStatusChanged) onActionStatusChanged(enabled);
    };
    addAndMakeVisible(_actionStatusMenu);

    _troubleshootingLabel.setText("Allow troubleshooting logs", juce::dontSendNotification);
    _troubleshootingLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_troubleshootingLabel);

    _troubleshootingMenu.addItem("Don't allow", 1);
    _troubleshootingMenu.addItem("Allow", 2);
    _troubleshootingMenu.setSelectedId(UiSettings::loadTroubleshootingSetting(),
                                       juce::dontSendNotification);
    _troubleshootingMenu.onChange = [this]() {
        UiSettings::saveTroubleshootingSetting(_troubleshootingMenu.getSelectedId());
        ActionFeedback::post(*this, "Troubleshooting logs: " + _troubleshootingMenu.getText());
    };
    addAndMakeVisible(_troubleshootingMenu);

    _hardwareInfoLabel.setText("Hardware info in logs", juce::dontSendNotification);
    _hardwareInfoLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_hardwareInfoLabel);

    _hardwareInfoMenu.addItem("Don't allow", 1);
    _hardwareInfoMenu.addItem("Allow", 2);
    _hardwareInfoMenu.setSelectedId(UiSettings::loadHardwareInfoSetting(),
                                    juce::dontSendNotification);
    _hardwareInfoMenu.onChange = [this]() {
        UiSettings::saveHardwareInfoSetting(_hardwareInfoMenu.getSelectedId());
        ActionFeedback::post(*this, "Hardware info in logs: " + _hardwareInfoMenu.getText());
    };
    addAndMakeVisible(_hardwareInfoMenu);
}

void SettingsView::applySavedBackgroundColour() {
    applyBackgroundColourPreset(UiSettings::loadBackgroundColourPreset());
}

void SettingsView::paint(juce::Graphics& g) {
    ProfilerStyle::Surfaces::fillPanel(g, getLocalBounds().toFloat());
}

void SettingsView::refreshContrastSetting() {
    _contrastMenu.setSelectedId(ProfilerStyle::Contrast::enabled ? 2 : 1, juce::dontSendNotification);
    _backgroundMenu.setEnabled(!ProfilerStyle::Contrast::enabled);
}

void SettingsView::resized() {
    auto area = getLocalBounds().reduced(24, 16);
    auto tabs = area.removeFromTop(32);
    _generalTab.setBounds(tabs.removeFromLeft(120));
    tabs.removeFromLeft(8);
    _captureTab.setBounds(tabs.removeFromLeft(120));
    tabs.removeFromLeft(8);
    _ampTab.setBounds(tabs.removeFromLeft(120));
    area.removeFromTop(12);

    const auto showGeneral = _page == Page::General;
    _titleLabel.setVisible(showGeneral);
    _backgroundLabel.setVisible(showGeneral);
    _backgroundMenu.setVisible(showGeneral);
    _interfaceScaleLabel.setVisible(showGeneral);
    _interfaceScaleMenu.setVisible(showGeneral);
    _contrastLabel.setVisible(showGeneral);
    _contrastMenu.setVisible(showGeneral);
    _actionStatusLabel.setVisible(showGeneral);
    _actionStatusMenu.setVisible(showGeneral);
    _troubleshootingLabel.setVisible(showGeneral);
    _troubleshootingMenu.setVisible(showGeneral);
    _hardwareInfoLabel.setVisible(showGeneral);
    _hardwareInfoMenu.setVisible(showGeneral);
    for (auto [panel, page] : {std::pair<juce::Component*, Page>{&_capturePanel, Page::CloneIr},
                               std::pair<juce::Component*, Page>{&_ampPanel, Page::CloneAmp}}) {
        panel->setVisible(_page == page);
        panel->setBounds(area);
        if (_page == page) {
            panel->toFront(false);
        }
    }

    _titleLabel.setBounds(area.removeFromTop(30));

    area.removeFromTop(8);

    auto settingLine = area.removeFromTop(32);

    _backgroundLabel.setBounds(settingLine.removeFromLeft(180));

    settingLine.removeFromLeft(20);

    _backgroundMenu.setBounds(settingLine.removeFromLeft(220));

    area.removeFromTop(8);

    settingLine = area.removeFromTop(32);
    _interfaceScaleLabel.setBounds(settingLine.removeFromLeft(180));
    settingLine.removeFromLeft(20);
    _interfaceScaleMenu.setBounds(settingLine.removeFromLeft(220));

    area.removeFromTop(8);

    settingLine = area.removeFromTop(32);
    _contrastLabel.setBounds(settingLine.removeFromLeft(180));
    settingLine.removeFromLeft(20);
    _contrastMenu.setBounds(settingLine.removeFromLeft(220));
    area.removeFromTop(8);

    settingLine = area.removeFromTop(32);
    _actionStatusLabel.setBounds(settingLine.removeFromLeft(180));
    settingLine.removeFromLeft(20);
    _actionStatusMenu.setBounds(settingLine.removeFromLeft(220));
    area.removeFromTop(8);

    settingLine = area.removeFromTop(32);

    _troubleshootingLabel.setBounds(settingLine.removeFromLeft(180));

    settingLine.removeFromLeft(20);

    _troubleshootingMenu.setBounds(settingLine.removeFromLeft(220));

    area.removeFromTop(8);

    settingLine = area.removeFromTop(32);

    _hardwareInfoLabel.setBounds(settingLine.removeFromLeft(180));

    settingLine.removeFromLeft(20);

    _hardwareInfoMenu.setBounds(settingLine.removeFromLeft(220));
}

void SettingsView::applyBackgroundColour() {
    applyBackgroundColourPreset(_backgroundMenu.getSelectedId());
    UiSettings::saveBackgroundColourPreset(_backgroundMenu.getSelectedId());
    refreshColours();
}

void SettingsView::refreshColours() {
    if (auto* editor = getTopLevelComponent()) {
        repaintComponentTree(*editor);
    }
}
