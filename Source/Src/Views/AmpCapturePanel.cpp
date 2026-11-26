#include "Views/AmpCapturePanel.h"

#include <cstdlib>

#include "MarketplaceClient.h"
#include "PluginProcessor.h"
#include "SettingsPath.h"
#include "Stylesheet.h"

namespace {
juce::File capturesFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Spectra")
        .getChildFile("Captures");
}

juce::File defaultReampFile() {
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Downloads")
        .getChildFile("NAM")
        .getChildFile("input.wav");
}

juce::String decibels(float gain) {
    return juce::String(juce::Decibels::gainToDecibels(gain, -120.0f), 1) + " dBFS";
}

bool isPedal(const juce::String& device) {
    return device == "pedal";
}

juce::String writeCapture(const juce::File& folder,
                          const juce::File& reampFile,
                          const std::vector<float>& recorded,
                          const AmpCapture::Analysis& analysis);

juce::String signInBeforeClone(const juce::String& device) {
    return "Sign in on the Library page before cloning a " + device + ".";
}

juce::String keepFailedTake(const juce::File& reampFile,
                            const std::vector<float>& recorded,
                            const AmpCapture::Analysis& analysis,
                            const juce::String& name) {
    const auto stem = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ").trim();
    const auto folder = capturesFolder().getChildFile(
        (stem.isEmpty() ? juce::String("Amp") : stem) + " " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S"));
    const auto error = writeCapture(folder, reampFile, recorded, analysis);
    if (error.isEmpty()) {
        return " This take is saved in " + folder.getFullPathName() + ".";
    }
    return " The audio was not kept on this machine.";
}

juce::String returnLevelKnob(const juce::String& device) {
    if (isPedal(device)) {
        return "Turn the pedal Level (not Drive), or the Helix input that receives the pedal.";
    }
    return "Turn the Torpedo output, or the Helix input that receives the Torpedo. Leave the amp Gain, Volume and "
           "channel alone, and leave the Helix send alone.";
}

juce::String describe(const AmpCapture::Analysis& analysis, const std::optional<int>& previous, const juce::String& device) {
    juce::ignoreUnused(device);
    if (analysis.returnPeak < 1.0e-4f || !analysis.detected) {
        return {};
    }

    const auto milliseconds = analysis.latency * 1000.0 / AmpCapture::kSampleRate;
    juce::StringArray lines;
    lines.add("Delay " + juce::String(analysis.latency) + " samples (" + juce::String(milliseconds, 1) +
              " ms). Clicks " + juce::String(analysis.blipLatency[0]) + " / " + juce::String(analysis.blipLatency[1]) +
              ". Peak " + decibels(analysis.returnPeak) + ", hiss " + decibels(analysis.noiseRms) + ".");
    if (previous.has_value() && analysis.latency != *previous) {
        lines.add("Previous delay: " + juce::String(*previous) + " samples.");
    }
    if (analysis.replicateChecked) {
        lines.add("Start vs end ESR " + juce::String(analysis.replicateEsr, 3) + ". Aligned " +
                  juce::String(analysis.alignedEsr, 3) + ", level-matched " + juce::String(analysis.gainMatchedEsr, 3) +
                  ".");
        if (analysis.issue == AmpCapture::TakeIssue::Latency) {
            lines.add("The Helix delay walked " + juce::String(analysis.endDrift) +
                      " samples. That slip is removed and the return is set to -6 dBFS. The amp gain stays as it is.");
        } else if (analysis.issue == AmpCapture::TakeIssue::None && !analysis.clipped && analysis.returnPeak > 1.0e-4f) {
            lines.add("The return is set to -6 dBFS automatically. The amp gain and the pedal drive stay as they are.");
        }
    }
    return lines.joinIntoString("\n");
}

juce::String cloudRejectReason(const AmpCapture::Analysis& analysis,
                               const std::optional<int>& previous,
                               bool full,
                               const juce::String& device) {
    const auto pedal = isPedal(device);
    if (analysis.returnPeak < 1.0e-4f) {
        return "The PC received silence. In Spectra, Options > Audio/MIDI Settings, untick Mute audio input. On the "
               "Helix, the return (the " +
               juce::String(pedal ? "pedal" : "Torpedo") +
               ") must be routed to the USB output Spectra is recording. Do not change the Helix send.";
    }
    if (!analysis.detected) {
        return "The clicks did not come back above the hiss (" + decibels(analysis.noiseRms) +
               "). This is the Helix return, not the " + juce::String(pedal ? "Drive" : "amp gain") +
               ". " + returnLevelKnob(device) +
               " Turn that up so the clicks are clearly louder than the hiss. Leave the " +
               juce::String(pedal ? "pedal Drive" : "amp Gain, Volume and channel") + " exactly where you want the clone.";
    }
    const auto clickGap = std::abs(analysis.blipLatency[0] - analysis.blipLatency[1]);
    if (clickGap >= AmpCapture::kBlipDisagreement) {
        const auto milliseconds = clickGap * 1000.0 / AmpCapture::kSampleRate;
        return "The two clicks disagree by " + juce::String(milliseconds, 1) +
               " ms (hiss " + decibels(analysis.noiseRms) +
               "). The hiss is as loud as the clicks on the Helix return. " + returnLevelKnob(device) +
               " Turn that until the clicks are the loudest peaks. Do not change the " +
               juce::String(pedal ? "pedal Drive" : "amp gain") + ".";
    }
    if (analysis.clipped) {
        return "The Helix return is clipping the PC (peak " + decibels(analysis.returnPeak) +
               "). Clipping cannot be repaired afterwards. " + returnLevelKnob(device) +
               " Turn that down until the peak is under -1 dBFS. Do not change the " +
               juce::String(pedal ? "pedal Drive" : "amp gain") + ": that is the sound being cloned.";
    }
    const auto peakDb = juce::Decibels::gainToDecibels(analysis.returnPeak, -120.0f);
    if (peakDb < -36.0f || analysis.returnPeak < analysis.noiseRms * 8.0f) {
        return "The PC sees the Helix return at " + juce::String(peakDb, 1) + " dBFS, buried near the hiss (" +
               decibels(analysis.noiseRms) + "). " + returnLevelKnob(device) +
               " Turn that up. There is no exact dB to hit: anything from -30 to -1 is corrected automatically. Do not "
               "change the " +
               juce::String(pedal ? "pedal Drive" : "amp gain") + ".";
    }
    if (previous.has_value() && std::abs(analysis.latency - *previous) > AmpCapture::kLatencyDriftLimit) {
        const auto moved = analysis.latency - *previous;
        return "The Helix USB delay moved by " + juce::String(moved > 0 ? "+" : "") + juce::String(moved) +
               " samples between the two tests. On the Helix, set USB Buffer Size and leave it. This is not the " +
               device + ". Run the test again without touching the Helix.";
    }
    if (full && analysis.replicateChecked && analysis.replicateEsr >= AmpCapture::kReplicateEsrLimit &&
        analysis.issue == AmpCapture::TakeIssue::Hiss) {
        return "The test tone is buried in hiss (ESR " + juce::String(analysis.replicateEsr, 2) +
               "). This is not a reason to lower the " + juce::String(pedal ? "Drive" : "amp gain") + ". " +
               returnLevelKnob(device) + " Turn that up so the tone sits well above the hiss, then capture again.";
    }
    if (full && analysis.replicateChecked && analysis.replicateEsr >= AmpCapture::kReplicateEsrLimit &&
        analysis.issue == AmpCapture::TakeIssue::Level) {
        return "The loudness changed during the 3:10. Raw ESR " + juce::String(analysis.replicateEsr, 2) +
               ", after matching the level " + juce::String(analysis.gainMatchedEsr, 2) +
               ". NAM will not train a take that is a different volume at the end. The " +
               juce::String(pedal ? "Drive" : "amp gain") +
               " stays where it is. Leave the Helix gate off and capture again.";
    }
    if (full && analysis.replicateChecked && analysis.replicateEsr >= AmpCapture::kReplicateEsrLimit &&
        analysis.issue == AmpCapture::TakeIssue::Changed) {
        return "The sound itself changed during the 3:10. Raw ESR " + juce::String(analysis.replicateEsr, 2) +
               ", after realigning the Helix delay " + juce::String(analysis.alignedEsr, 2) +
               ", after matching the level " + juce::String(analysis.gainMatchedEsr, 2) +
               ". It is not a dB target and not a small clock slip. Leave the Helix buffer fixed and let the " + device +
               " sit powered for a minute, then capture once. Do not change its gain.";
    }
    return {};
}

juce::File spectraDirectory() {
    auto directory = getSettingsFolder().getChildFile("Spectra");
    directory.createDirectory();
    return directory;
}

juce::String writeCapture(const juce::File& folder,
                          const juce::File& reampFile,
                          const std::vector<float>& recorded,
                          const AmpCapture::Analysis& analysis) {
    if (!folder.createDirectory().wasOk()) {
        return "Could not create " + folder.getFullPathName() + ".";
    }
    if (!reampFile.copyFileTo(folder.getChildFile("input.wav"))) {
        return "Could not copy input.wav.";
    }
    juce::String error;
    auto aligned = AmpCapture::align(recorded, analysis.shift, AmpCapture::kStandardFrames);
    AmpCapture::finishTake(aligned, analysis);
    if (!AmpCapture::writeWav(folder.getChildFile("output.wav"), aligned, &error)) {
        return error;
    }

    auto report = std::make_unique<juce::DynamicObject>();
    report->setProperty("sample_rate", AmpCapture::kSampleRate);
    report->setProperty("latency_samples", analysis.latency);
    report->setProperty("shift_applied", analysis.shift);
    report->setProperty("blip_latency", juce::Array<juce::var>{analysis.blipLatency[0], analysis.blipLatency[1]});
    report->setProperty("return_peak_dbfs", juce::Decibels::gainToDecibels(analysis.returnPeak, -120.0f));
    report->setProperty("noise_rms_dbfs", juce::Decibels::gainToDecibels(analysis.noiseRms, -120.0f));
    report->setProperty("clipped", analysis.clipped);
    report->setProperty("replicate_esr", analysis.replicateEsr);
    folder.getChildFile("capture.json").replaceWithText(juce::JSON::toString(juce::var(report.release())));
    return {};
}
}  // namespace

AmpCapturePanel::AmpCapturePanel(ProfilerAudioProcessor& processor, Target target)
    : _processor(processor),
      _target(target),
      _device(target == Target::Pedal ? "pedal" : "amp"),
      _reampFile(defaultReampFile()) {
    _title.setText(_target == Target::Pedal ? "Clone pedal" : "Clone amp", juce::dontSendNotification);
    if (_target == Target::Pedal) {
        _captureButton.setButtonText("Capture pedal");
    }
    _title.setFont(ProfilerStyle::Fonts::bold(20.0f));
    _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_title);

    _body.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _body.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    _body.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(_body);

    for (auto* label : {&_reampLabel, &_nameLabel}) {
        label->setFont(ProfilerStyle::Fonts::regular(13.0f));
        label->setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
        addAndMakeVisible(label);
    }
    _reampLabel.setText("Reamp file", juce::dontSendNotification);
    _nameLabel.setText("Name", juce::dontSendNotification);

    _reampPath.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _reampPath.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _reampPath.setMinimumHorizontalScale(0.6f);
    addAndMakeVisible(_reampPath);

    _nameEditor.setInputRestrictions(80);
    addAndMakeVisible(_nameEditor);

    _chooseButton.onClick = [this]() { chooseReamp(); };
    _roundTripButton.onClick = [this]() {
        _auto = false;
        start(Mode::RoundTrip);
    };
    _captureButton.onClick = [this]() { cloneAmp(); };
    _stopButton.onClick = [this]() {
        _auto = false;
        _stopRequested = true;
        _processor.stopAmpCapture();
    };
    _folderButton.onClick = [this]() {
        const auto folder = _lastFolder.isDirectory() ? _lastFolder : capturesFolder();
        folder.createDirectory();
        folder.startAsProcess();
    };
    for (auto* button : {&_chooseButton, &_roundTripButton, &_captureButton, &_stopButton, &_folderButton}) {
        addAndMakeVisible(button);
    }

    _status.setMultiLine(true, true);
    _status.setReadOnly(true);
    _status.setCaretVisible(false);
    _status.setScrollbarsShown(false);
    _status.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _status.setColour(juce::TextEditor::textColourId, ProfilerStyle::Colors::accent);
    _status.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    _status.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    _status.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    _status.setBorder({});
    addAndMakeVisible(_status);

    if (_processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
        _body.setText(
            "One click checks the round trip twice, records the " + _device +
                " and sends it to training. "
                "Sign in on the Library page first. The sealed profile appears there when it is ready.",
            juce::dontSendNotification);
    } else {
        _body.setText(
            "Capture plays the reamp signal and records the return. Use the standalone, which owns the audio device.",
            juce::dontSendNotification);
    }
    refreshButtons();
}

AmpCapturePanel::~AmpCapturePanel() {
    stopTimer();
    _processor.stopAmpCapture();
}

void AmpCapturePanel::paint(juce::Graphics& g) {
    g.setColour(ProfilerStyle::Colors::border.withAlpha(0.35f));
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
}

void AmpCapturePanel::resized() {
    auto area = getLocalBounds().reduced(0, 8);
    _title.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);
    _body.setBounds(area.removeFromTop(36));
    area.removeFromTop(10);

    auto reampRow = area.removeFromTop(32);
    _reampLabel.setBounds(reampRow.removeFromLeft(80));
    _chooseButton.setBounds(reampRow.removeFromRight(100));
    reampRow.removeFromRight(8);
    _reampPath.setBounds(reampRow);
    area.removeFromTop(8);

    auto nameRow = area.removeFromTop(32);
    _nameLabel.setBounds(nameRow.removeFromLeft(80));
    _nameEditor.setBounds(nameRow.removeFromLeft(juce::jmin(280, nameRow.getWidth())));
    area.removeFromTop(10);

    auto buttons = area.removeFromTop(34);
    _roundTripButton.setBounds(buttons.removeFromLeft(150));
    buttons.removeFromLeft(8);
    _captureButton.setBounds(buttons.removeFromLeft(150));
    buttons.removeFromLeft(8);
    _stopButton.setBounds(buttons.removeFromLeft(90));
    buttons.removeFromLeft(8);
    _folderButton.setBounds(buttons.removeFromLeft(120));
    area.removeFromTop(10);
    _status.setBounds(area);
}

void AmpCapturePanel::refreshButtons() {
    const auto standalone = _processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    _reampPath.setText(_reampFile.existsAsFile() ? _reampFile.getFullPathName() : "Choose NAM's input.wav",
                       juce::dontSendNotification);
    _chooseButton.setEnabled(!_busy);
    _roundTripButton.setEnabled(standalone && !_busy);
    _captureButton.setEnabled(standalone && !_busy);
    _stopButton.setEnabled(_busy && _mode != Mode::None && _processor.isAmpCaptureRunning());
}

void AmpCapturePanel::chooseReamp() {
    _chooser = std::make_unique<juce::FileChooser>("Choose NAM's input.wav", _reampFile, "*.wav");
    _chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& chooser) {
                              const auto file = chooser.getResult();
                              if (file.existsAsFile()) {
                                  _reampFile = file;
                              }
                              refreshButtons();
                          });
}

void AmpCapturePanel::cloneAmp() {
    if (_busy) {
        return;
    }
    if (_nameEditor.getText().trim().isEmpty()) {
        setStatus("Name the " + _device + " before capturing it.");
        return;
    }
    _busy = true;
    refreshButtons();
    setStatus("Checking the sign-in...");
    const auto device = _device;
    juce::Component::SafePointer<AmpCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, device]() {
        Marketplace::Session session;
        Marketplace::Client client;
        const auto library = client.loadSavedSession(session);
        const auto signedIn = library.status == Marketplace::Status::Success && session.accessToken.isNotEmpty();
        juce::MessageManager::callAsync([safeThis, signedIn, device]() {
            if (safeThis == nullptr) {
                return;
            }
            safeThis->_busy = false;
            safeThis->refreshButtons();
            if (!signedIn) {
                safeThis->setStatus(signInBeforeClone(device));
                return;
            }
            safeThis->_auto = true;
            safeThis->_roundTrips = 0;
            safeThis->_lastRoundTrip.reset();
            safeThis->_cloudTitle = safeThis->_nameEditor.getText().trim();
            safeThis->setStatus("Checking the round trip...");
            safeThis->start(Mode::RoundTrip);
        });
    });
}

void AmpCapturePanel::start(Mode mode) {
    if (_busy) {
        return;
    }
    if (mode == Mode::Full && _nameEditor.getText().trim().isEmpty()) {
        setStatus("Name the " + _device + " before capturing it.");
        return;
    }

    std::vector<float> send;
    juce::String error;
    if (!AmpCapture::loadReamp(_reampFile, send, &error)) {
        setStatus(error);
        return;
    }
    if (mode == Mode::RoundTrip) {
        send.resize(AmpCapture::kRoundTripFrames);
    } else if (send.size() != static_cast<std::size_t>(AmpCapture::kStandardFrames)) {
        setStatus("Capture needs NAM's standard input.wav (3:10 at 48 kHz).");
        return;
    }
    if (!_processor.beginAmpCapture(std::move(send), &error)) {
        setStatus(error);
        return;
    }

    _mode = mode;
    _busy = true;
    _stopRequested = false;
    setStatus("Playing the reamp signal...");
    refreshButtons();
    startTimerHz(10);
}

void AmpCapturePanel::timerCallback() {
    if (_polling && !_busy) {
        if (++_pollTick >= 5) {
            _pollTick = 0;
            pollCloud();
        }
        return;
    }
    if (_processor.releaseStoppedAmpCapture()) {
        stopTimer();
        setStatus(_stopRequested ? "Capture stopped." : "Capture stopped: the audio device restarted.");
        _busy = false;
        _mode = Mode::None;
        refreshButtons();
        return;
    }
    if (_processor.ampCaptureFinished()) {
        stopTimer();
        finish();
        return;
    }

    const auto progress = _processor.getAmpCaptureProgress();
    const auto totalSeconds = (_mode == Mode::Full ? AmpCapture::kStandardFrames : AmpCapture::kRoundTripFrames) /
                              AmpCapture::kSampleRate;
    const auto left = static_cast<int>(totalSeconds * (1.0 - progress));
    setStatus((_mode == Mode::Full ? "Capturing the " + _device + ": " : "Testing the round trip: ") +
              juce::String(juce::roundToInt(progress * 100.0f)) + "% (" + juce::String(left / 60) + ":" +
              juce::String(left % 60).paddedLeft('0', 2) + " left). Keep the room quiet.");
    refreshButtons();
}

void AmpCapturePanel::finish() {
    auto sent = std::make_shared<std::vector<float>>();
    auto recorded = std::make_shared<std::vector<float>>();
    if (!_processor.takeAmpCapture(*sent, *recorded)) {
        showResult("The capture did not finish.");
        return;
    }

    setStatus("Analysing the return...");
    const auto mode = _mode;
    const auto previous = _lastRoundTrip;
    const auto reampFile = _reampFile;
    auto stem = _nameEditor.getText().retainCharacters(
                                         "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ")
                    .trim();
    const auto folder = capturesFolder().getChildFile(
        (stem.isEmpty() ? (_target == Target::Pedal ? juce::String("Pedal") : juce::String("Amp")) : stem) + " " +
        juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S"));

    const auto automatic = _auto;
    const auto device = _device;
    juce::Component::SafePointer<AmpCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, sent, recorded, mode, previous, reampFile, folder, automatic, device]() {
        const auto analysis = AmpCapture::analyse(*sent, *recorded);
        auto text = describe(analysis, previous, device);
        juce::String saved;
        if (mode == Mode::Full && analysis.detected && !automatic) {
            const auto error = writeCapture(folder, reampFile, *recorded, analysis);
            text << "\n"
                 << (error.isEmpty() ? "Saved input.wav and output.wav in " + folder.getFullPathName() : error);
            if (error.isEmpty()) {
                saved = folder.getFullPathName();
            }
        }
        juce::MessageManager::callAsync([safeThis, text, analysis, saved, recorded, mode, previous]() {
            if (safeThis == nullptr) {
                return;
            }
            if (analysis.detected) {
                safeThis->_lastRoundTrip = analysis.latency;
            }
            if (saved.isNotEmpty()) {
                safeThis->_lastFolder = juce::File(saved);
            }
            if (!safeThis->_auto) {
                safeThis->showResult(text);
                return;
            }

            const auto reason = cloudRejectReason(analysis,
                                                  mode == Mode::RoundTrip && safeThis->_roundTrips == 0 ? std::optional<int>{}
                                                                                                        : previous,
                                                  mode == Mode::Full,
                                                  safeThis->_device);
            if (reason.isNotEmpty()) {
                safeThis->_auto = false;
                safeThis->showResult(text + "\n" + reason);
                return;
            }

            safeThis->_busy = false;
            if (mode == Mode::RoundTrip) {
                safeThis->_roundTrips++;
                if (safeThis->_roundTrips < 2) {
                    safeThis->setStatus(text + "\nLatency looks stable. Running the second test.");
                    safeThis->start(Mode::RoundTrip);
                    return;
                }
                safeThis->setStatus(text + "\nLatency is stable. Capturing the " + safeThis->_device + ".");
                safeThis->start(Mode::Full);
                return;
            }

            safeThis->uploadClone(recorded, analysis, safeThis->_cloudTitle);
        });
    });
}

void AmpCapturePanel::uploadClone(std::shared_ptr<std::vector<float>> recorded,
                                  AmpCapture::Analysis analysis,
                                  juce::String name) {
    _auto = false;
    _busy = true;
    _mode = Mode::None;
    refreshButtons();
    setStatus("Uploading the capture...");

    const auto reampFile = _reampFile;
    const auto device = _device;
    const auto kind = _target == Target::Pedal ? juce::String("pedal") : juce::String("amp");
    juce::Component::SafePointer<AmpCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, recorded, analysis, name, reampFile, device, kind]() {
        auto fail = [safeThis](juce::String message) {
            juce::MessageManager::callAsync([safeThis, message]() {
                if (safeThis != nullptr) {
                    safeThis->showResult(message);
                }
            });
        };

        Marketplace::Session session;
        Marketplace::Client client;
        auto library = client.loadSavedSession(session);
        if (library.status != Marketplace::Status::Success || session.accessToken.isEmpty()) {
            fail(signInBeforeClone(device) + keepFailedTake(reampFile, *recorded, analysis, name));
            return;
        }

        auto job = client.createAmpJob(session.accessToken, name, kind);
        if (job.status == Marketplace::Status::Unauthorized) {
            library = client.loadSavedSession(session);
            if (library.status == Marketplace::Status::Success) {
                job = client.createAmpJob(session.accessToken, name, kind);
            }
        }
        if (job.status != Marketplace::Status::Success) {
            const auto reason = job.status == Marketplace::Status::Unauthorized || job.message.contains("token")
                                    ? signInBeforeClone(device)
                                    : job.message;
            fail(reason + keepFailedTake(reampFile, *recorded, analysis, name));
            return;
        }

        const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("profiler-" + job.id + "-output.wav");
        juce::String writeError;
        auto aligned = AmpCapture::align(*recorded, analysis.shift, AmpCapture::kStandardFrames);
        AmpCapture::finishTake(aligned, analysis);
        if (!AmpCapture::writeWav(temp, aligned, &writeError)) {
            fail(writeError);
            return;
        }
        juce::MemoryBlock output;
        juce::MemoryBlock input;
        const auto outputOk = temp.loadFileAsData(output);
        temp.deleteFile();
        const auto inputOk = reampFile.loadFileAsData(input);
        if (!outputOk || !inputOk) {
            fail("Could not read the capture audio.");
            return;
        }
        juce::String uploadFailure;
        if (Marketplace::uploadSigned(job.inputUploadUrl, input.getData(), input.getSize(), &uploadFailure) !=
                Marketplace::Status::Success ||
            Marketplace::uploadSigned(job.outputUploadUrl, output.getData(), output.getSize(), &uploadFailure) !=
                Marketplace::Status::Success) {
            fail(uploadFailure.isNotEmpty()
                     ? uploadFailure + " The audio was not kept on this machine."
                     : "Could not upload the capture. The audio was not kept on this machine.");
            return;
        }

        auto train = client.startAmpTrain(session.accessToken, job.id);
        if (train.status == Marketplace::Status::Unauthorized &&
            client.loadSavedSession(session).status == Marketplace::Status::Success) {
            train = client.startAmpTrain(session.accessToken, job.id);
        }
        if (train.status != Marketplace::Status::Success) {
            const auto reason = train.status == Marketplace::Status::Unauthorized ? signInBeforeClone(device) : train.message;
            fail(reason.isNotEmpty() ? reason : "Could not start training.");
            return;
        }

        juce::MessageManager::callAsync([safeThis, jobId = job.id, name, device]() {
            if (safeThis == nullptr) {
                return;
            }
            safeThis->_cloudJobId = jobId;
            safeThis->_cloudTitle = name;
            safeThis->_polling = true;
            safeThis->_pollTick = 0;
            safeThis->_busy = false;
            safeThis->_mode = Mode::None;
            safeThis->refreshButtons();
            safeThis->setStatus("Training on the GPU. You can leave this page. The " + device +
                                " appears in the library when it is ready.");
            safeThis->startTimerHz(1);
        });
    });
}

void AmpCapturePanel::pollCloud() {
    if (_pollBusy || _cloudJobId.isEmpty()) {
        return;
    }
    _pollBusy = true;
    const auto jobId = _cloudJobId;
    const auto title = _cloudTitle;
    const auto device = _device;
    const auto pedal = _target == Target::Pedal;
    juce::Component::SafePointer<AmpCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, jobId, title, device, pedal]() {
        Marketplace::Session session;
        Marketplace::Client client;
        juce::String state;
        juce::String message;
        juce::MemoryBlock cipher;
        juce::MemoryBlock key;
        bool ready = false;
        if (client.loadSavedSession(session).status == Marketplace::Status::Success && session.accessToken.isNotEmpty()) {
            const auto status = client.ampStatus(session.accessToken, jobId);
            state = status.state;
            message = status.message;
            if (status.status == Marketplace::Status::Success && status.state == "ready" && status.fileUrl.isNotEmpty()) {
                const auto downloaded = client.download(status.fileUrl, session.accessToken);
                const auto keyResult = client.ampKey(session.accessToken, jobId);
                juce::MemoryOutputStream decodedStream;
                if (downloaded.status == Marketplace::Status::Success && keyResult.status == Marketplace::Status::Success &&
                    juce::Base64::convertFromBase64(decodedStream, keyResult.message)) {
                    const auto decoded = decodedStream.getMemoryBlock();
                    if (decoded.getSize() == 32) {
                        cipher = downloaded.body;
                        key = decoded;
                        ready = true;
                        if (!pedal) {
                            auto directory = spectraDirectory();
                            directory.getChildFile(jobId + ".spectra").replaceWithData(cipher.getData(), cipher.getSize());
                            directory.getChildFile(jobId + ".key").replaceWithData(key.getData(), key.getSize());
                        }
                    } else {
                        state = "error";
                        message = "The " + device + " key was invalid.";
                    }
                } else {
                    state = "error";
                    message = downloaded.message.isNotEmpty() ? downloaded.message
                                                              : "Could not download the sealed " + device + ".";
                }
            }
        }

        juce::MessageManager::callAsync([safeThis, jobId, title, state, message, cipher, key, ready, session, pedal]() {
            if (safeThis == nullptr) {
                return;
            }
            safeThis->_pollBusy = false;
            if (ready) {
                safeThis->_polling = false;
                safeThis->stopTimer();
                if (pedal) {
                    juce::String error;
                    const auto loaded = safeThis->_processor.unlockSpectraSession(session.accessToken) &&
                                        safeThis->_processor.importEncryptedProfile(ProfileCatalog::Kind::Pedal,
                                                                                    jobId,
                                                                                    title,
                                                                                    cipher,
                                                                                    key,
                                                                                    0,
                                                                                    0.0f,
                                                                                    false,
                                                                                    &error);
                    if (loaded) {
                        safeThis->_processor.ensurePedalInChain();
                        safeThis->setStatus(title + " is ready and loaded.");
                    } else {
                        safeThis->setStatus(error.isNotEmpty() ? error : title + " is in the library. Open it from the pedal block.");
                    }
                } else if (safeThis->_processor.unlockSpectraSession(session.accessToken) &&
                           safeThis->_processor.loadSpectraModel(cipher.getData(), cipher.getSize(),
                                                                 static_cast<const uint8_t*>(key.getData()), key.getSize())) {
                    safeThis->_processor.setAmpCondition(AmpCondition::kOff, 0.0f);
                    safeThis->setStatus(title + " is ready and loaded.");
                } else {
                    safeThis->setStatus(title + " is in the library. Open it from there to play.");
                }
                return;
            }
            if (state == "failed" || state == "error") {
                safeThis->_polling = false;
                safeThis->stopTimer();
                safeThis->setStatus(message.isNotEmpty() ? message : "Training failed.");
            }
        });
    });
}

void AmpCapturePanel::setStatus(const juce::String& text) {
    _status.setText(text, false);
}

void AmpCapturePanel::showResult(const juce::String& text) {
    setStatus(text);
    _busy = false;
    _mode = Mode::None;
    refreshButtons();
}
