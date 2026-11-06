#include "Views/IrCapturePanel.h"

#include "ActionFeedback.h"
#include "PluginProcessor.h"
#include "Stylesheet.h"

namespace {
void wipeKey(std::array<std::uint8_t, 32>& key) {
    key.fill(0);
}
}  // namespace

IrCapturePanel::IrCapturePanel(ProfilerAudioProcessor& processor)
    : _processor(processor) {
    _title.setText("Clone IR", juce::dontSendNotification);
    _title.setFont(ProfilerStyle::Fonts::bold(20.0f));
    _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_title);

    _body.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _body.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    _body.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(_body);

    _nameLabel.setText("Name", juce::dontSendNotification);
    _nameLabel.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _nameLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    addAndMakeVisible(_nameLabel);

    _nameEditor.setInputRestrictions(80);
    addAndMakeVisible(_nameEditor);

    _cloneButton.onClick = [this]() { startCapture(); };
    addAndMakeVisible(_cloneButton);

    _status.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _status.setColour(juce::Label::textColourId, ProfilerStyle::Colors::accent);
    _status.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(_status);

    refreshAccountState();
}

IrCapturePanel::~IrCapturePanel() {
    stopTimer();
}

void IrCapturePanel::paint(juce::Graphics& g) {
    g.setColour(ProfilerStyle::Contrast::outline(ProfilerStyle::Colors::border.withAlpha(0.35f)));
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
}

void IrCapturePanel::resized() {
    auto area = getLocalBounds().reduced(0, 8);
    _title.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);
    _body.setBounds(area.removeFromTop(36));
    area.removeFromTop(10);
    auto nameRow = area.removeFromTop(32);
    _nameLabel.setBounds(nameRow.removeFromLeft(64));
    _nameEditor.setBounds(nameRow.removeFromLeft(juce::jmin(280, nameRow.getWidth())));
    area.removeFromTop(10);
    _cloneButton.setBounds(area.removeFromTop(34).removeFromLeft(160));
    area.removeFromTop(8);
    _status.setBounds(area.removeFromTop(juce::jmax(20, area.getHeight())));
}

void IrCapturePanel::refreshAccountState(bool updateStatus) {
    const auto standalone = _processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    Marketplace::Session session;
    const auto signedIn = Marketplace::loadSession(Marketplace::defaultSessionFile(), session) && session.accessToken.isNotEmpty();
    if (!standalone) {
        _body.setText("IR capture plays a sweep and records the return. Use the standalone, which owns the audio device.",
                      juce::dontSendNotification);
        _cloneButton.setEnabled(false);
        if (updateStatus) setStatus("Open Profiler standalone to clone an IR.");
        return;
    }

    _body.setText("The sweep is saved as a profile in this app. The sealed copy is also sent to My creations on profiler.audio.",
                  juce::dontSendNotification);
    _cloneButton.setEnabled(signedIn && !_busy);
    if (!updateStatus) {
        return;
    }
    if (!signedIn) {
        setStatus("Sign in on the Library page to clone an IR.");
    } else if (!_busy) {
        setStatus("Signed in as " + session.email + ".");
    }
}

void IrCapturePanel::startCapture() {
    if (_busy) {
        return;
    }

    refreshAccountState();
    if (!_cloneButton.isEnabled()) {
        return;
    }
    if (_nameEditor.getText().trim().isEmpty()) {
        setStatus("Name the IR before cloning it.");
        return;
    }

    juce::String error;
    if (!_processor.beginIrCapture(&error)) {
        setStatus(error);
        return;
    }

    _busy = true;
    _cloneButton.setEnabled(false);
    setStatus("Playing the sweep. Keep the return connected.");
    startTimerHz(15);
}

void IrCapturePanel::timerCallback() {
    if (!_processor.irCaptureFinished()) {
        return;
    }
    stopTimer();
    sealAndUpload();
}

void IrCapturePanel::sealAndUpload() {
    Marketplace::Session session;
    if (!Marketplace::loadSession(Marketplace::defaultSessionFile(), session) || session.accessToken.isEmpty()) {
        _busy = false;
        setStatus("Sign in on the Library page to clone an IR.");
        refreshAccountState(false);
        return;
    }

    if (!_processor.unlockSpectraSession(session.accessToken)) {
        _busy = false;
        setStatus("Could not open a capture session.");
        refreshAccountState(false);
        return;
    }

    juce::MemoryBlock sealed;
    std::array<std::uint8_t, 32> key{};
    juce::String error;
    const auto sealedOk = _processor.sealIrCapture(sealed, key, &error);
    juce::String localError;
    const auto storedLocally = sealedOk && _processor.storeCapturedIr(_nameEditor.getText().trim(), key, sealed, &localError);
    _processor.lockSpectraSession();
    if (!sealedOk) {
        wipeKey(key);
        sealed.fillWith(0);
        _busy = false;
        setStatus(error);
        refreshAccountState(false);
        return;
    }

    const auto title = _nameEditor.getText().trim();
    const auto token = session.accessToken;
    const auto localMessage = storedLocally ? "Profile \"" + title + "\" added. Open Library to use it. "
                                            : localError + " ";
    juce::Component::SafePointer<IrCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, title, token, sealed, key, localMessage]() mutable {
        Marketplace::Client client;
        const auto result = client.submitIrCapture(token, title, key, sealed);
        wipeKey(key);
        sealed.fillWith(0);
        juce::MessageManager::callAsync([safeThis, result, localMessage]() {
            if (safeThis == nullptr) {
                return;
            }
            safeThis->_busy = false;
            safeThis->setStatus(localMessage + result.message);
            safeThis->refreshAccountState(false);
        });
    });
    wipeKey(key);
    sealed.fillWith(0);
}

void IrCapturePanel::setStatus(const juce::String& message) {
    const bool changed = _status.getText() != message;
    _status.setText(message, juce::dontSendNotification);
    if (changed) ActionFeedback::post(*this, message);
}
