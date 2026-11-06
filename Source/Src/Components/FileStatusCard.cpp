#include "Components/FileStatusCard.h"

FileStatusCard::FileStatusCard(Options options)
    : _options(std::move(options)),
      _loadButton(_options.loadButtonText, _options.loadButtonTheme),
      _unloadButton(_options.unloadButtonText, _options.unloadButtonTheme) {
    configureLabel(_titleLabel,
                   _options.title,
                   16.0f,
                   ProfilerStyle::Colors::white,
                   juce::Justification::centredLeft,
                   true);
    configureLabel(_statusLabel,
                   {},
                   13.0f,
                   ProfilerStyle::Colors::white.withAlpha(0.58f),
                   juce::Justification::centredRight,
                   true);
    configureLabel(_fileLabel,
                   {},
                   16.0f,
                   ProfilerStyle::Colors::white,
                   juce::Justification::centredLeft,
                   true);
    configureLabel(_pathLabel,
                   {},
                   13.0f,
                   ProfilerStyle::Colors::white.withAlpha(0.62f));

    addAndMakeVisible(_titleLabel);
    addAndMakeVisible(_statusLabel);
    addAndMakeVisible(_fileLabel);
    addAndMakeVisible(_pathLabel);
    addAndMakeVisible(_loadButton);
    addAndMakeVisible(_unloadButton);

    _loadButton.onClick = [this]() {
        if (onLoadClicked) {
            onLoadClicked();
        }
    };
    _unloadButton.onClick = [this]() {
        if (onUnloadClicked) {
            onUnloadClicked();
        }
    };

    updateLabels();
}

void FileStatusCard::setFileState(bool isLoaded, const juce::File& file) {
    setFileState(isLoaded ? Status::Loaded : Status::Empty, file);
}

void FileStatusCard::setFileState(Status status, const juce::File& file) {
    _status = status;
    _file = file;
    updateLabels();
    resized();
    repaint();
}

void FileStatusCard::paint(juce::Graphics& g) {
    const auto card = getLocalBounds().toFloat();
    const auto accent = getStatusColour(_status);

    g.setColour(ProfilerStyle::Colors::elevated);
    g.fillRoundedRectangle(card, ProfilerStyle::Surfaces::panelCorner);

    g.setColour(ProfilerStyle::Contrast::accent(accent.withAlpha(_status == Status::Empty ? 0.28f : 0.85f)));
    g.fillRoundedRectangle(card.withWidth(3.0f), 1.5f);

    g.setColour(ProfilerStyle::Contrast::outline(accent.withAlpha(_status == Status::Empty ? 0.22f : 0.4f)));
    g.drawRoundedRectangle(card.reduced(0.5f), ProfilerStyle::Surfaces::panelCorner, 1.0f);
}

void FileStatusCard::resized() {
    auto content = getLocalBounds().reduced(16);
    auto titleRow = content.removeFromTop(24);

    _statusLabel.setBounds(titleRow.removeFromRight(96));
    _titleLabel.setBounds(titleRow);

    content.removeFromTop(14);
    _fileLabel.setBounds(content.removeFromTop(28));

    content.removeFromTop(4);
    _pathLabel.setBounds(content.removeFromTop(24));

    auto buttonArea = content.removeFromBottom(38);
    const auto loadButtonWidth = juce::jmin(148, buttonArea.getWidth());

    if (canUnload()) {
        constexpr auto gap = 10;
        const auto unloadButtonWidth = juce::jmin(112, juce::jmax(0, buttonArea.getWidth() - loadButtonWidth - gap));
        _loadButton.setBounds(buttonArea.removeFromLeft(loadButtonWidth));
        buttonArea.removeFromLeft(gap);
        _unloadButton.setBounds(buttonArea.removeFromLeft(unloadButtonWidth));
    } else {
        _loadButton.setBounds(buttonArea.withWidth(loadButtonWidth));
        _unloadButton.setBounds({});
    }
}

void FileStatusCard::configureLabel(juce::Label& label,
                                    const juce::String& text,
                                    float fontSize,
                                    juce::Colour textColour,
                                    juce::Justification justification,
                                    bool isBold) {
    auto font = isBold ? ProfilerStyle::Fonts::bold(fontSize) : ProfilerStyle::Fonts::regular(fontSize);

    label.setText(text, juce::dontSendNotification);
    label.setFont(font);
    label.setJustificationType(justification);
    label.setColour(juce::Label::textColourId, textColour);
    label.setMinimumHorizontalScale(0.7f);
    label.setInterceptsMouseClicks(false, false);
}

void FileStatusCard::updateLabels() {
    _statusLabel.setText(getStatusText(), juce::dontSendNotification);
    _statusLabel.setColour(juce::Label::textColourId, getStatusColour(_status));

    _fileLabel.setText(getFileNameOrFallback(_file, _status, _options.emptyFileText),
                       juce::dontSendNotification);
    _pathLabel.setText(getPathOrFallback(_file, _status, _options.emptyPathText),
                       juce::dontSendNotification);

    const auto tooltip = _status == Status::Empty ? juce::String{} : _file.getFullPathName();
    _fileLabel.setTooltip(tooltip);
    _pathLabel.setTooltip(tooltip);

    _unloadButton.setEnabled(canUnload());
    _unloadButton.setVisible(canUnload());
}

bool FileStatusCard::isLoaded() const noexcept {
    return _status == Status::Loaded;
}

bool FileStatusCard::canUnload() const noexcept {
    return _status != Status::Empty;
}

juce::String FileStatusCard::getStatusText() const {
    switch (_status) {
        case Status::Loaded:
            return _options.loadedStatusText;
        case Status::Warning:
            return _options.warningStatusText;
        case Status::Error:
            return _options.errorStatusText;
        case Status::Empty:
        default:
            return _options.emptyStatusText;
    }
}

juce::Colour FileStatusCard::getStatusColour(Status status) {
    switch (status) {
        case Status::Loaded:
            return juce::Colour(0xff38d17a);
        case Status::Warning:
            return juce::Colour(0xffffb020);
        case Status::Error:
            return juce::Colour(0xffff4d4f);
        case Status::Empty:
        default:
            return ProfilerStyle::Colors::white.withAlpha(0.58f);
    }
}

juce::String FileStatusCard::getFileNameOrFallback(const juce::File& file,
                                                   Status status,
                                                   const juce::String& fallback) {
    if (status == Status::Empty) {
        return fallback;
    }

    const auto fileName = file.getFileName();
    return fileName.isNotEmpty() ? fileName : fallback;
}

juce::String FileStatusCard::getPathOrFallback(const juce::File& file,
                                               Status status,
                                               const juce::String& fallback) {
    if (status == Status::Empty) {
        return fallback;
    }

    const auto path = file.getFullPathName();
    return path.isNotEmpty() ? path : fallback;
}
