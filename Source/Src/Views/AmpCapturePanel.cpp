#include "Views/AmpCapturePanel.h"

#include <cstdlib>

#include "MarketplaceClient.h"
#include "PluginProcessor.h"
#include "SettingsPath.h"
#include "Stylesheet.h"

namespace {
juce::File capturesFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Profiler")
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

juce::String describe(const AmpCapture::Analysis& analysis, const std::optional<int>& previous) {
    if (analysis.returnPeak < 1.0e-4f) {
        return "Input 1 stayed silent. Untick \"Mute audio input\" in Options > Audio/MIDI Settings, then check the "
               "cable from the amp and the interface input gain.";
    }
    if (!analysis.detected) {
        return "No blip came back above the noise (return peak " + decibels(analysis.returnPeak) +
               "). Check that the amp feeds input 1.";
    }

    const auto milliseconds = analysis.latency * 1000.0 / AmpCapture::kSampleRate;
    juce::StringArray lines;
    lines.add("Round trip: " + juce::String(analysis.latency) + " samples (" + juce::String(milliseconds, 1) +
              " ms). Blips: " + juce::String(analysis.blipLatency[0]) + " / " + juce::String(analysis.blipLatency[1]) + ".");
    if (std::abs(analysis.blipLatency[0] - analysis.blipLatency[1]) >= AmpCapture::kBlipDisagreement) {
        lines.add("The two blips disagree. The return is unstable or too noisy.");
    }
    if (previous.has_value()) {
        const auto drift = analysis.latency - *previous;
        lines.add(drift == 0 ? "Same latency as the previous test."
                             : "Previous test: " + juce::String(*previous) + " samples (" +
                                   juce::String(drift > 0 ? "+" : "") + juce::String(drift) + ").");
    }
    lines.add("Return peak " + decibels(analysis.returnPeak) + ", noise " + decibels(analysis.noiseRms) + ".");
    if (analysis.clipped) {
        lines.add("The return clips. Lower the amp output or the interface input gain.");
    } else if (analysis.returnPeak < 0.01f) {
        lines.add("The return is very quiet. Raise the interface input gain.");
    }
    if (analysis.replicateChecked) {
        const auto passed = analysis.replicateEsr < AmpCapture::kReplicateEsrLimit;
        lines.add("Start vs end ESR " + juce::String(analysis.replicateEsr, 5) +
                  (passed ? " (OK, NAM wants under 0.01)."
                          : " (too high: the amp or the capture changed between start and end)."));
    }
    return lines.joinIntoString("\n");
}

juce::String cloudRejectReason(const AmpCapture::Analysis& analysis, const std::optional<int>& previous, bool full) {
    if (!analysis.detected) {
        return "No stable return. Check the send, the amp and input 1.";
    }
    if (std::abs(analysis.blipLatency[0] - analysis.blipLatency[1]) >= AmpCapture::kBlipDisagreement) {
        return "The two blips disagree. The return is unstable or too noisy.";
    }
    if (analysis.clipped) {
        return "The return clips. Lower the amp output or the interface input gain.";
    }
    const auto peakDb = juce::Decibels::gainToDecibels(analysis.returnPeak, -120.0f);
    if (peakDb < -12.0f) {
        return "The return is quieter than -12 dBFS. Raise the interface input gain.";
    }
    if (peakDb > -0.1f) {
        return "The return is too hot. Lower the amp output or the interface input gain.";
    }
    if (previous.has_value() && std::abs(analysis.latency - *previous) > 2) {
        return "Latency moved by more than 2 samples. Keep the routing still and try again.";
    }
    if (full && analysis.replicateChecked && analysis.replicateEsr >= AmpCapture::kReplicateEsrLimit) {
        return "Start vs end ESR is too high. The amp changed during the capture.";
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
    const auto aligned = AmpCapture::align(recorded, analysis.shift, AmpCapture::kStandardFrames);
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

AmpCapturePanel::AmpCapturePanel(ProfilerAudioProcessor& processor)
    : _processor(processor), _reampFile(defaultReampFile()) {
    _title.setText("Clone amp", juce::dontSendNotification);
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
            "One click checks the round trip twice, records the amp and sends it to training. "
            "Sign in on the Library page first. The sealed profile appears there when it is ready.",
            juce::dontSendNotification);
    } else {
        _body.setText(
            "Amp capture plays the reamp signal and records the return. Use the standalone, which owns the "
            "audio device.",
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
        setStatus("Name the amp before capturing it.");
        return;
    }
    Marketplace::Session session;
    if (!Marketplace::loadSession(Marketplace::defaultSessionFile(), session) || session.accessToken.isEmpty()) {
        setStatus("Sign in on the Library page before cloning an amp.");
        return;
    }

    _auto = true;
    _roundTrips = 0;
    _lastRoundTrip.reset();
    _cloudTitle = _nameEditor.getText().trim();
    setStatus("Checking the round trip...");
    start(Mode::RoundTrip);
}

void AmpCapturePanel::start(Mode mode) {
    if (_busy) {
        return;
    }
    if (mode == Mode::Full && _nameEditor.getText().trim().isEmpty()) {
        setStatus("Name the amp before capturing it.");
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
        setStatus("Capture amp needs NAM's standard input.wav (3:10 at 48 kHz).");
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
    setStatus((_mode == Mode::Full ? "Capturing the amp: " : "Testing the round trip: ") +
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
        (stem.isEmpty() ? juce::String("Amp") : stem) + " " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S"));

    const auto automatic = _auto;
    juce::Component::SafePointer<AmpCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, sent, recorded, mode, previous, reampFile, folder, automatic]() {
        const auto analysis = AmpCapture::analyse(*sent, *recorded);
        auto text = describe(analysis, previous);
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
                                                  mode == Mode::Full);
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
                safeThis->setStatus(text + "\nLatency is stable. Capturing the amp.");
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
    juce::Component::SafePointer<AmpCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, recorded, analysis, name, reampFile]() {
        auto fail = [safeThis](juce::String message) {
            juce::MessageManager::callAsync([safeThis, message]() {
                if (safeThis != nullptr) {
                    safeThis->showResult(message);
                }
            });
        };

        Marketplace::Session session;
        if (!Marketplace::loadSession(Marketplace::defaultSessionFile(), session) || session.accessToken.isEmpty()) {
            fail("Sign in on the Library page before cloning an amp.");
            return;
        }

        Marketplace::Client client;
        auto refreshed = client.loadLibrary(session);
        if (refreshed.status == Marketplace::Status::Success) {
            Marketplace::saveSession(Marketplace::defaultSessionFile(), session);
        }
        auto job = client.createAmpJob(session.accessToken, name);
        if (job.status != Marketplace::Status::Success) {
            fail(job.message);
            return;
        }

        const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("profiler-" + job.id + "-output.wav");
        juce::String writeError;
        const auto aligned = AmpCapture::align(*recorded, analysis.shift, AmpCapture::kStandardFrames);
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
        if (Marketplace::uploadSigned(job.inputUploadUrl, input.getData(), input.getSize()) != Marketplace::Status::Success ||
            Marketplace::uploadSigned(job.outputUploadUrl, output.getData(), output.getSize()) != Marketplace::Status::Success) {
            fail("Could not upload the capture. The audio was not kept on this machine.");
            return;
        }

        const auto train = client.startAmpTrain(session.accessToken, job.id);
        if (train.status != Marketplace::Status::Success) {
            fail(train.message.isNotEmpty() ? train.message : "Could not start training.");
            return;
        }

        juce::MessageManager::callAsync([safeThis, jobId = job.id, name]() {
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
            safeThis->setStatus("Training on the GPU. You can leave this page. The amp appears in the library when it is ready.");
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
    juce::Component::SafePointer<AmpCapturePanel> safeThis(this);
    _jobs.addJob([safeThis, jobId, title]() {
        Marketplace::Session session;
        Marketplace::Client client;
        juce::String state;
        juce::String message;
        juce::MemoryBlock cipher;
        juce::MemoryBlock key;
        bool ready = false;
        if (Marketplace::loadSession(Marketplace::defaultSessionFile(), session) && session.accessToken.isNotEmpty()) {
            client.loadLibrary(session);
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
                        auto directory = spectraDirectory();
                        directory.getChildFile(jobId + ".spectra").replaceWithData(cipher.getData(), cipher.getSize());
                        directory.getChildFile(jobId + ".key").replaceWithData(key.getData(), key.getSize());
                    } else {
                        state = "error";
                        message = "The amp key was invalid.";
                    }
                } else {
                    state = "error";
                    message = downloaded.message.isNotEmpty() ? downloaded.message : "Could not download the sealed amp.";
                }
            }
        }

        juce::MessageManager::callAsync([safeThis, jobId, title, state, message, cipher, key, ready, session]() {
            if (safeThis == nullptr) {
                return;
            }
            safeThis->_pollBusy = false;
            if (ready) {
                safeThis->_polling = false;
                safeThis->stopTimer();
                if (safeThis->_processor.unlockSpectraSession(session.accessToken) &&
                    safeThis->_processor.loadSpectraModel(cipher.getData(), cipher.getSize(),
                                                          static_cast<const uint8_t*>(key.getData()), key.getSize())) {
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
