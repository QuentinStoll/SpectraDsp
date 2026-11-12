#include "Components/FileDropZone.h"

#include "ActionFeedback.h"
#include "Stylesheet.h"

namespace {
constexpr const char* kDropZoneHelp = "Left click / Drag & Drop to load • Right click to unload";
}

FileDropZone::FileDropZone(Options options)
    : _options(std::move(options)) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip(kDropZoneHelp);
    refresh();
}

void FileDropZone::refresh() {
    repaint();
}

bool FileDropZone::isFileLoaded() const {
    return _options.isLoaded && _options.isLoaded();
}

void FileDropZone::paint(juce::Graphics& g) {
    const auto detail = getDetailText();

    if (auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel())) {
        laf->drawDropZone(g,
                          getLocalBounds().toFloat(),
                          _options.title,
                          detail,
                          isFileLoaded(),
                          _isDragOver,
                          _options.icon,
                          _options.categoryColour.isTransparent() ? ProfilerStyle::Colors::accent
                                                                  : _options.categoryColour);
        return;
    }

    ProfilerStyle::Surfaces::fillPanel(g, getLocalBounds().toFloat());
    g.setColour(ProfilerStyle::Colors::text);
    g.drawFittedText(_options.title + "\n" + detail, getLocalBounds().reduced(12), juce::Justification::centred, 3);
}

void FileDropZone::mouseUp(const juce::MouseEvent& event) {
    if (!event.mouseWasClicked()) {
        return;
    }

    if (event.mods.isPopupMenu()) {
        unloadFile();
        return;
    }

    chooseFile();
}

bool FileDropZone::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size() == 1 && matchesPattern(juce::File(files[0]));
}

void FileDropZone::fileDragEnter(const juce::StringArray&, int, int) {
    _isDragOver = true;
    repaint();
}

void FileDropZone::fileDragExit(const juce::StringArray&) {
    _isDragOver = false;
    repaint();
}

void FileDropZone::filesDropped(const juce::StringArray& files, int, int) {
    _isDragOver = false;
    if (files.isEmpty()) {
        repaint();
        return;
    }

    tryLoadFile(juce::File(files[0]));
    repaint();
}

bool FileDropZone::matchesPattern(const juce::File& file) const {
    juce::StringArray wildcards;
    wildcards.addTokens(_options.filePattern, ";,", {});
    wildcards.trim();

    for (const auto& wildcard : wildcards) {
        if (wildcard.isNotEmpty() && file.getFileName().matchesWildcard(wildcard, true)) {
            return true;
        }
    }

    return false;
}

bool FileDropZone::tryLoadFile(const juce::File& file) {
    const auto loaded = file.existsAsFile() && _options.loadFile && _options.loadFile(file);
    refresh();

    if (onChanged) {
        onChanged();
    }

    if (onStatusMessage) {
        if (loaded && _options.successMessage.isNotEmpty()) {
            onStatusMessage(_options.successMessage, true);
        } else if (!loaded) {
            onStatusMessage(_options.failureMessage.isNotEmpty()
                                ? _options.failureMessage
                                : "Could not load file: " + file.getFullPathName(),
                            false);
        }
    }

    return loaded;
}

void FileDropZone::chooseFile() {
    ActionFeedback::post(*this, "Choose a file for " + _options.title + ".");
    _fileChooser = std::make_unique<juce::FileChooser>(_options.chooserTitle, juce::File{}, _options.filePattern);
    const juce::Component::SafePointer<FileDropZone> safeThis(this);

    _fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis](const juce::FileChooser& chooser) {
                                  if (safeThis == nullptr) {
                                      return;
                                  }

                                  const auto file = chooser.getResult();
                                  if (file.existsAsFile()) {
                                      safeThis->tryLoadFile(file);
                                  } else {
                                      ActionFeedback::post(*safeThis, safeThis->_options.title + " file selection cancelled.");
                                  }
                                  safeThis->_fileChooser.reset();
                              });
}

void FileDropZone::unloadFile() {
    if (!isFileLoaded() || !_options.unloadFile) {
        ActionFeedback::post(*this, "No file is loaded in " + _options.title + ".");
        return;
    }

    _options.unloadFile();
    ActionFeedback::post(*this, _options.title + " file unloaded.");
    refresh();
    if (onChanged) {
        onChanged();
    }
}

juce::String FileDropZone::getDetailText() const {
    if (_isDragOver) {
        return "Drop file to load";
    }

    if (isFileLoaded() && _options.getCurrentFile) {
        const auto file = _options.getCurrentFile();
        if (file.getFileName().isNotEmpty()) {
            return file.getFileName();
        }
    }

    return _options.emptyDetail;
}
