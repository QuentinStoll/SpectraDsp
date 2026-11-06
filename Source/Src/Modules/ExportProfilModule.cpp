#include "Modules/ExportProfilModule.h"

#include "ActionFeedback.h"
#include "BinaryData.h"
#include "ProfileManager.h"
#include "Stylesheet.h"

namespace {
juce::String getExportFileName(const juce::String& profileName) {
    auto fileName = juce::File::createLegalFileName(profileName.trim());

    if (fileName.isEmpty()) {
        fileName = "Exported Profile";
    }

    return fileName + ProfileManager::profileFileExtension;
}
}  // namespace

ExportProfilModule::ExportProfilModule() {
    _musicIcon = juce::Drawable::createFromImageData(BinaryData::musicnote_svg,
                                                     BinaryData::musicnote_svgSize);
    if (_musicIcon != nullptr) {
        _musicIcon->replaceColour(juce::Colour(0xff000000), ProfilerStyle::Colors::white);
    }

    addAndMakeVisible(_profileNameEditor);
    addAndMakeVisible(_exportButton);
    addAndMakeVisible(_createButton);
    addAndMakeVisible(_cancelButton);
    addAndMakeVisible(_form);

    _exportButton.onClick = [this]() {
        exportProfil();
    };

    _createButton.onClick = [this]() {
        createProfil();
    };

    _cancelButton.onClick = [this]() {
        cancel();
    };

    _form.setLabelWidth(176);
    _form.setRowGap(12);

    _form.addSliderField("masterVolume", "Master Volume", 0.0, 100.0, 50.0, "%", 1.0);
    _form.addSliderField("gain", "Gain", -12.0, 12.0, 0.0, "dB", 0.1);
    _form.addSliderField("noiseGate", "Noise gate", 0.0, 60.0, 10.0, "dB", 0.1);
    _form.addSliderField("bass", "Bass", -24.0, 24.0, 0.0, "dB", 0.1);
    _form.addSliderField("middle", "Middle", -24.0, 24.0, 0.0, "dB", 0.1);
    _form.addSliderField("treble", "Treble", -24.0, 24.0, 0.0, "dB", 0.1);
    _form.addSliderField("presence", "Presence", -24.0, 24.0, 0.0, "dB", 0.1);
    _form.addSliderField("depth", "Depth", -24.0, 24.0, 0.0, "dB", 0.1);
    _form.addPathField("irPath", "IR Path", "*.wav;*.aiff;*.aif;*.flac");
    _form.addPathField("ampPath", "Amp Path", "*.nam;*.json;*.txt");
}

void ExportProfilModule::paint(juce::Graphics& g) {
    if (_musicIcon != nullptr) {
        _musicIcon->drawWithin(g,
                               juce::Rectangle<float>(10.0f, 14.0f, 28.0f, 28.0f),
                               juce::RectanglePlacement::centred,
                               1.0f);
    }
}

void ExportProfilModule::resized() {
    auto area = getLocalBounds();
    auto topRow = area.removeFromTop(58).reduced(0, 15);

    topRow.removeFromLeft(55);
    _profileNameEditor.setBounds(topRow.removeFromLeft(250));
    topRow.removeFromLeft(14);
    _exportButton.setBounds(topRow.removeFromLeft(76));
    topRow.removeFromLeft(5);
    _createButton.setBounds(topRow.removeFromLeft(76));
    topRow.removeFromLeft(5);
    _cancelButton.setBounds(topRow.removeFromLeft(76));

    auto formArea = getLocalBounds()
                        .withTrimmedTop(88)
                        .withTrimmedLeft(50)
                        .withTrimmedRight(44);

    _form.setBounds(formArea.removeFromTop(juce::jmin(formArea.getHeight(), _form.getContentHeight())));
}

int ExportProfilModule::getRequiredHeight(int /*width*/) const {
    return 88 + _form.getContentHeight() + 20;
}

void ExportProfilModule::setExportValues(const juce::NamedValueSet& values,
                                         const juce::File& defaultExportFile) {
    auto profileName = juce::String("Exported Profile");

    if (const auto* value = values.getVarPointer("profileName")) {
        const auto candidate = value->toString().trim();
        if (candidate.isNotEmpty()) {
            profileName = candidate;
        }
    }

    _profileNameEditor.setText(profileName, juce::dontSendNotification);
    _form.setValues(values);

    _defaultExportDirectory = defaultExportFile == juce::File{}
                                  ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                  : defaultExportFile.getParentDirectory();
}

void ExportProfilModule::exportProfil() {
    ActionFeedback::post(*this, "Choose where to export the profile.");
    const auto values = getEditedValues();

    const auto initialFile = getDefaultExportFileForName(_profileNameEditor.getText());
    _fileChooser = std::make_unique<juce::FileChooser>(
        "Export profile",
        initialFile,
        "*" + juce::String(ProfileManager::profileFileExtension));

    const juce::Component::SafePointer<ExportProfilModule> safeThis(this);
    _fileChooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                  juce::FileBrowserComponent::canSelectFiles |
                                  juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis, values](const juce::FileChooser& chooser) {
                                  if (safeThis == nullptr) {
                                      return;
                                  }

                                  const auto file = chooser.getResult();
                                  if (file.getFullPathName().isNotEmpty() && safeThis->onExportClicked) {
                                      safeThis->onExportClicked(values, file);
                                  } else if (file.getFullPathName().isEmpty()) {
                                      ActionFeedback::post(*safeThis, "Profile export cancelled.");
                                  }

                                  safeThis->_fileChooser.reset();
                              });
}

void ExportProfilModule::createProfil() {
    if (onCreateProfileClicked) {
        onCreateProfileClicked(getEditedValues());
    }
}

void ExportProfilModule::cancel() {
    if (onCancelClicked) {
        onCancelClicked();
    }
}

juce::NamedValueSet ExportProfilModule::getEditedValues() const {
    auto values = _form.getValues();
    values.set("profileName", _profileNameEditor.getText().trim());
    return values;
}

juce::File ExportProfilModule::getDefaultExportFileForName(const juce::String& profileName) const {
    const auto directory = _defaultExportDirectory == juce::File{}
                               ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               : _defaultExportDirectory;

    return directory.getChildFile(getExportFileName(profileName));
}
