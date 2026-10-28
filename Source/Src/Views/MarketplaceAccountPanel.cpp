#include "Views/MarketplaceAccountPanel.h"

#include "SettingsPath.h"
#include "Stylesheet.h"

namespace {
juce::String safeFileStem(const juce::String& value) {
    auto stem = value.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_");
    return stem.isNotEmpty() ? stem : "pack";
}
}  // namespace

struct MarketplaceAccountPanel::PackRow : public juce::Component {
    PackRow(const Marketplace::LibraryItem& item, std::function<void()> download)
        : _item(item),
          _downloadButton(item.kind == "amp" ? "Play" : "Download", ProfilerStyle::Theme::Orange) {
        _title.setText(item.title.isNotEmpty() ? item.title : "Pack", juce::dontSendNotification);
        _title.setFont(ProfilerStyle::Fonts::medium(15.0f));
        _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
        addAndMakeVisible(_title);

        const auto sourceLabel = item.kind == "amp"                     ? "Your amp"
                                 : item.source.equalsIgnoreCase("free") ? "Free"
                                                                        : "Purchase";
        auto meta = item.authorName;
        if (meta.isNotEmpty()) {
            meta << "  ·  ";
        }
        meta << sourceLabel;
        if (item.hasIntegratedIr) {
            meta << "  ·  Cabinet included";
        }
        const auto condition = item.conditionProfile.toLowerCase();
        juce::String conditionName;
        if (condition == "black") {
            conditionName = "Black";
        } else if (condition == "death") {
            conditionName = "Death";
        } else if (condition == "raw") {
            conditionName = "Raw";
        }
        if (conditionName.isNotEmpty() && item.conditionAmount >= 1.0f) {
            meta << "  ·  " << conditionName << " " << juce::String(juce::roundToInt(item.conditionAmount)) << "%";
        }
        _meta.setText(meta, juce::dontSendNotification);
        _meta.setFont(ProfilerStyle::Fonts::regular(12.0f));
        _meta.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
        addAndMakeVisible(_meta);

        _downloadButton.onClick = std::move(download);
        addAndMakeVisible(_downloadButton);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(12, 8);
        _downloadButton.setBounds(area.removeFromRight(120).withSizeKeepingCentre(120, 32));
        area.removeFromRight(12);
        _title.setBounds(area.removeFromTop(22));
        _meta.setBounds(area.removeFromTop(18));
    }

    void paint(juce::Graphics& g) override {
        ProfilerStyle::Surfaces::fillPanel(g, getLocalBounds().toFloat());
    }

    void setBusy(bool busy) {
        _downloadButton.setEnabled(!busy);
    }

    Marketplace::LibraryItem _item;

   private:
    juce::Label _title;
    juce::Label _meta;
    CustomTextButton _downloadButton;
};

MarketplaceAccountPanel::MarketplaceAccountPanel() {
    _titleLabel.setText("Marketplace", juce::dontSendNotification);
    _titleLabel.setFont(ProfilerStyle::Fonts::bold(22.0f));
    _titleLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_titleLabel);

    _emailLabel.setText("Email", juce::dontSendNotification);
    _passwordLabel.setText("Password", juce::dontSendNotification);
    _codeLabel.setText("6-digit code", juce::dontSendNotification);
    _accountLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _accountLabel.setFont(ProfilerStyle::Fonts::regular(14.0f));
    for (auto* label : {&_emailLabel, &_passwordLabel, &_codeLabel}) {
        label->setFont(ProfilerStyle::Fonts::regular(13.0f));
        label->setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
        addAndMakeVisible(label);
    }
    addAndMakeVisible(_accountLabel);

    _statusLabel.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _statusLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::accent);
    addAndMakeVisible(_statusLabel);

    _passwordEditor.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
    _codeEditor.setInputRestrictions(6, "0123456789");
    _codeEditor.setJustification(juce::Justification::centred);
    addAndMakeVisible(_emailEditor);
    addAndMakeVisible(_passwordEditor);
    addAndMakeVisible(_codeEditor);

    _submitButton.onClick = [this]() {
        if (_busy) {
            return;
        }

        if (_mode == Mode::Code) {
            const auto code = _codeEditor.getText().trim();
            if (!Marketplace::Client::isSixDigitCode(code)) {
                _statusLabel.setText("Enter the 6-digit code.", juce::dontSendNotification);
                return;
            }

            setBusy(true);
            const auto factorId = _challenge.factorId;
            const auto challengeId = _challenge.challengeId;
            juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
            _jobs.addJob([safeThis, factorId, challengeId, code]() {
                Marketplace::LoginResult result;
                if (safeThis != nullptr) {
                    result = safeThis->_client.submitCode(factorId, challengeId, code);
                }
                juce::MessageManager::callAsync([safeThis, result]() {
                    if (safeThis != nullptr) {
                        safeThis->applyLoginResult(result, true);
                    }
                });
            });
            return;
        }

        const auto email = _emailEditor.getText().trim();
        const auto password = _passwordEditor.getText();
        if (email.isEmpty() || password.isEmpty()) {
            _statusLabel.setText("Enter your marketplace email and password.", juce::dontSendNotification);
            return;
        }

        setBusy(true);
        juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
        _jobs.addJob([safeThis, email, password]() {
            Marketplace::LoginResult result;
            if (safeThis != nullptr) {
                result = safeThis->_client.login(email, password);
            }
            juce::MessageManager::callAsync([safeThis, result]() {
                if (safeThis != nullptr) {
                    safeThis->applyLoginResult(result, false);
                }
            });
        });
    };
    addAndMakeVisible(_submitButton);

    _backButton.onClick = [this]() {
        _challenge = {};
        _codeEditor.clear();
        showLogin({});
    };
    addAndMakeVisible(_backButton);

    _signOutButton.onClick = [this]() {
        Marketplace::clearSession(Marketplace::defaultSessionFile());
        _session = {};
        _challenge = {};
        _items.clear();
        showLogin({});
    };
    addAndMakeVisible(_signOutButton);

    _signupLink.setButtonText(juce::String(u8"Pas de compte ? Créer un compte sur le marketplace"));
    _signupLink.setURL(juce::URL(Marketplace::signupUrl));
    _signupLink.setFont(ProfilerStyle::Fonts::regular(13.0f), false, juce::Justification::centredLeft);
    _signupLink.setColour(juce::HyperlinkButton::textColourId, ProfilerStyle::Colors::accent);
    addAndMakeVisible(_signupLink);

    showLogin({});
}

MarketplaceAccountPanel::~MarketplaceAccountPanel() {
    _jobs.removeAllJobs(true, 4000);
}

void MarketplaceAccountPanel::start() {
    Marketplace::Session stored;
    if (!Marketplace::loadSession(Marketplace::defaultSessionFile(), stored)) {
        showLogin({});
        return;
    }

    _session = std::move(stored);
    _accountLabel.setText(_session.email, juce::dontSendNotification);
    showLibrary();
    loadLibrary();
    startTimer(15000);
}

int MarketplaceAccountPanel::getPreferredHeight(int width) const {
    juce::ignoreUnused(width);
    if (_mode == Mode::Login) {
        return 392;
    }
    if (_mode == Mode::Code) {
        return 348;
    }

    return 118 + static_cast<int>(_rows.size()) * 72;
}

void MarketplaceAccountPanel::resized() {
    auto area = getLocalBounds();
    _titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(8);

    const auto loginFields = _mode != Mode::Library;
    _emailLabel.setVisible(_mode == Mode::Login);
    _passwordLabel.setVisible(_mode == Mode::Login);
    _emailEditor.setVisible(_mode == Mode::Login);
    _passwordEditor.setVisible(_mode == Mode::Login);
    _signupLink.setVisible(_mode == Mode::Login);
    _codeLabel.setVisible(_mode == Mode::Code);
    _codeEditor.setVisible(_mode == Mode::Code);
    _submitButton.setVisible(loginFields);
    _backButton.setVisible(_mode == Mode::Code);
    _accountLabel.setVisible(_mode == Mode::Library);
    _signOutButton.setVisible(_mode == Mode::Library);

    if (_mode == Mode::Login) {
        _emailLabel.setBounds(area.removeFromTop(18));
        _emailEditor.setBounds(area.removeFromTop(36));
        area.removeFromTop(12);
        _passwordLabel.setBounds(area.removeFromTop(18));
        _passwordEditor.setBounds(area.removeFromTop(36));
        area.removeFromTop(16);
        _submitButton.setBounds(area.removeFromTop(36).removeFromLeft(160));
        area.removeFromTop(16);
        _signupLink.setBounds(area.removeFromTop(24));
        area.removeFromTop(12);
        _statusLabel.setBounds(area.removeFromTop(48));
        return;
    }

    if (_mode == Mode::Code) {
        _codeLabel.setBounds(area.removeFromTop(18));
        _codeEditor.setBounds(area.removeFromTop(36).removeFromLeft(180));
        area.removeFromTop(16);
        _submitButton.setBounds(area.removeFromTop(36).removeFromLeft(160));
        area.removeFromTop(12);
        _backButton.setBounds(area.removeFromTop(36).removeFromLeft(160));
        area.removeFromTop(12);
        _statusLabel.setBounds(area.removeFromTop(48));
        return;
    }

    auto header = area.removeFromTop(36);
    _signOutButton.setBounds(header.removeFromRight(120));
    header.removeFromRight(12);
    _accountLabel.setBounds(header);
    area.removeFromTop(8);
    _statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    for (auto& row : _rows) {
        row->setBounds(area.removeFromTop(64));
        area.removeFromTop(8);
        row->setBusy(_busy);
    }
}

void MarketplaceAccountPanel::setBusy(bool busy) {
    _busy = busy;
    _submitButton.setEnabled(!busy);
    _backButton.setEnabled(!busy);
    _emailEditor.setEnabled(!busy);
    _passwordEditor.setEnabled(!busy);
    _codeEditor.setEnabled(!busy);
    _signOutButton.setEnabled(!busy);
    for (auto& row : _rows) {
        row->setBusy(busy);
    }
}

void MarketplaceAccountPanel::showLogin(const juce::String& message) {
    _mode = Mode::Login;
    _submitButton.setButtonText("Sign in");
    _statusLabel.setText(message, juce::dontSendNotification);
    setBusy(false);
    notifyLayout();
}

void MarketplaceAccountPanel::showCodeEntry() {
    _mode = Mode::Code;
    _codeEditor.clear();
    _submitButton.setButtonText("Confirm");
    _statusLabel.setText("Enter the 6-digit code from your authenticator. It expires in 5 minutes.",
                         juce::dontSendNotification);
    setBusy(false);
    notifyLayout();
}

void MarketplaceAccountPanel::showLibrary() {
    _mode = Mode::Library;
    _accountLabel.setText(_session.email.isNotEmpty() ? _session.email : "Signed in",
                          juce::dontSendNotification);
    setBusy(false);
    rebuildRows();
    notifyLayout();
}

void MarketplaceAccountPanel::applyLoginResult(const Marketplace::LoginResult& result, bool fromCode) {
    if (result.status == Marketplace::Status::MfaRequired && !fromCode) {
        _challenge = result.challenge;
        _session = {};
        showCodeEntry();
        return;
    }

    if (result.status == Marketplace::Status::Success) {
        _session = result.session;
        _challenge = {};
        _passwordEditor.clear();
        _codeEditor.clear();
        Marketplace::saveSession(Marketplace::defaultSessionFile(), _session);
        showLibrary();
        loadLibrary();
        return;
    }

    if (fromCode && result.status == Marketplace::Status::Unauthorized) {
        _challenge = {};
        showLogin(result.message);
        return;
    }

    _statusLabel.setText(result.message, juce::dontSendNotification);
    setBusy(false);
}

void MarketplaceAccountPanel::visibilityChanged() {
    if (isShowing() && _mode == Mode::Library) {
        loadLibrary();
    }
}

void MarketplaceAccountPanel::timerCallback() {
    if (isShowing() && _mode == Mode::Library) {
        loadLibrary();
    }
}

void MarketplaceAccountPanel::loadLibrary() {
    if (_busy) {
        return;
    }
    if (_session.accessToken.isEmpty() && _session.refreshToken.isEmpty()) {
        showLogin({});
        return;
    }

    setBusy(true);
    const auto session = _session;
    juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
    _jobs.addJob([safeThis, session]() {
        auto working = session;
        Marketplace::LibraryResult result;
        if (safeThis != nullptr) {
            result = safeThis->_client.loadLibrary(working);
        }
        juce::MessageManager::callAsync([safeThis, working, result]() {
            if (safeThis == nullptr) {
                return;
            }

            if (result.status == Marketplace::Status::Unauthorized) {
                Marketplace::clearSession(Marketplace::defaultSessionFile());
                safeThis->_session = {};
                safeThis->_challenge = {};
                safeThis->showLogin(result.message);
                return;
            }

            if (working.accessToken != safeThis->_session.accessToken || working.refreshToken != safeThis->_session.refreshToken) {
                safeThis->_session.accessToken = working.accessToken;
                safeThis->_session.refreshToken = working.refreshToken;
                safeThis->_session.expiresIn = working.expiresIn;
                if (working.userId.isNotEmpty()) {
                    safeThis->_session.userId = working.userId;
                }
                if (working.email.isNotEmpty()) {
                    safeThis->_session.email = working.email;
                }
                Marketplace::saveSession(Marketplace::defaultSessionFile(), safeThis->_session);
            }

            if (result.status != Marketplace::Status::Success) {
                safeThis->_statusLabel.setText(result.message, juce::dontSendNotification);
                safeThis->setBusy(false);
                return;
            }

            safeThis->_items = result.items;
            for (auto& item : safeThis->_items) {
                item.fileUrl.clear();
                item.irFileUrl.clear();
            }
            if (result.email.isNotEmpty()) {
                safeThis->_session.email = result.email;
            }
            safeThis->_statusLabel.setText(safeThis->_items.empty()
                                               ? "No packs on this account yet. Profiles you create here stay on this computer."
                                               : juce::String(safeThis->_items.size()) + " on this account.",
                                           juce::dontSendNotification);
            safeThis->showLibrary();
        });
    });
}

void MarketplaceAccountPanel::downloadPack(int index) {
    if (_busy || index < 0 || index >= static_cast<int>(_items.size())) {
        return;
    }

    const auto packId = _items[static_cast<size_t>(index)].id;
    const auto entitlementId = _items[static_cast<size_t>(index)].entitlementId;
    const auto title = _items[static_cast<size_t>(index)].title;
    const auto kind = _items[static_cast<size_t>(index)].kind;
    const auto session = _session;
    const auto listedProfile = Marketplace::conditionProfileIndex(_items[static_cast<size_t>(index)].conditionProfile);
    const auto listedAmount = _items[static_cast<size_t>(index)].conditionAmount;
    if (kind == "amp") {
        setBusy(true);
        _statusLabel.setText("Opening " + title + "...", juce::dontSendNotification);
        juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
        _jobs.addJob([safeThis, packId, session, title, listedProfile, listedAmount]() {
            auto directory = getSettingsFolder().getChildFile("Spectra");
            directory.createDirectory();
            const auto cipherFile = directory.getChildFile(packId + ".spectra");
            const auto keyFile = directory.getChildFile(packId + ".key");
            juce::MemoryBlock cipher;
            juce::MemoryBlock key;
            juce::String error;
            auto working = session;
            auto conditionProfile = listedProfile;
            auto conditionAmount = listedAmount;
            const bool cached = cipherFile.existsAsFile() && keyFile.existsAsFile() && cipherFile.loadFileAsData(cipher) &&
                                keyFile.loadFileAsData(key) && key.getSize() == 32;
            if (cached && safeThis != nullptr) {
                const auto status = safeThis->_client.ampStatus(working.accessToken, packId);
                if (status.status == Marketplace::Status::Success && status.hasCondition) {
                    conditionProfile = Marketplace::conditionProfileIndex(status.conditionProfile);
                    conditionAmount = status.conditionAmount;
                }
            } else if (safeThis != nullptr) {
                const auto library = safeThis->_client.loadLibrary(working);
                const Marketplace::LibraryItem* fresh = nullptr;
                if (library.status == Marketplace::Status::Success) {
                    for (const auto& item : library.items) {
                        if (item.id == packId && item.kind == "amp") {
                            fresh = &item;
                            break;
                        }
                    }
                }
                if (fresh == nullptr || fresh->fileUrl.isEmpty()) {
                    error = library.status == Marketplace::Status::Success ? "This amp is not ready yet." : library.message;
                } else {
                    const auto downloaded = safeThis->_client.download(fresh->fileUrl, working.accessToken);
                    const auto keyResult = safeThis->_client.ampKey(working.accessToken, packId);
                    juce::MemoryOutputStream decodedKey;
                    if (downloaded.status != Marketplace::Status::Success || keyResult.status != Marketplace::Status::Success ||
                        !juce::Base64::convertFromBase64(decodedKey, keyResult.message) || decodedKey.getDataSize() != 32) {
                        error = downloaded.message.isNotEmpty() ? downloaded.message : "Could not open this amp.";
                    } else {
                        cipher = downloaded.body;
                        key = decodedKey.getMemoryBlock();
                        cipherFile.replaceWithData(cipher.getData(), cipher.getSize());
                        keyFile.replaceWithData(key.getData(), key.getSize());
                        conditionProfile = Marketplace::conditionProfileIndex(fresh->conditionProfile);
                        conditionAmount = fresh->conditionAmount;
                    }
                }
            }

            juce::MessageManager::callAsync([safeThis, working, cipher, key, error, conditionProfile, conditionAmount]() {
                if (safeThis == nullptr) {
                    return;
                }
                if (working.accessToken.isNotEmpty()) {
                    safeThis->_session.accessToken = working.accessToken;
                    safeThis->_session.refreshToken = working.refreshToken;
                    Marketplace::saveSession(Marketplace::defaultSessionFile(), safeThis->_session);
                }
                if (error.isNotEmpty()) {
                    safeThis->_statusLabel.setText(error, juce::dontSendNotification);
                    safeThis->setBusy(false);
                    return;
                }
                juce::String playError;
                const auto played = safeThis->onPlaySpectra != nullptr &&
                                    safeThis->onPlaySpectra(cipher, key, conditionProfile, conditionAmount, &playError);
                juce::String loaded = "Amp loaded.";
                if (played && conditionProfile > 0 && conditionAmount >= 1.0f) {
                    const char* names[] = {"Off", "Black", "Death", "Raw"};
                    loaded = juce::String("Amp loaded · ") + names[juce::jlimit(0, 3, conditionProfile)] + " " +
                             juce::String(juce::roundToInt(conditionAmount)) + "%.";
                }
                safeThis->_statusLabel.setText(played ? loaded : playError, juce::dontSendNotification);
                safeThis->setBusy(false);
            });
        });
        return;
    }
    setBusy(true);
    _statusLabel.setText("Downloading " + title + "...", juce::dontSendNotification);

    juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
    _jobs.addJob([safeThis, packId, entitlementId, session]() {
        auto working = session;
        Marketplace::LibraryResult library;
        Marketplace::DownloadResult profile;
        Marketplace::DownloadResult impulse;
        bool hasIntegratedIr = false;
        juce::String profileUrl;
        juce::String irUrl;
        if (safeThis != nullptr) {
            library = safeThis->_client.loadLibrary(working);
            if (library.status == Marketplace::Status::Success) {
                const Marketplace::LibraryItem* fresh = nullptr;
                for (const auto& item : library.items) {
                    if ((packId.isNotEmpty() && item.id == packId) || (entitlementId.isNotEmpty() && item.entitlementId == entitlementId)) {
                        fresh = &item;
                        break;
                    }
                }
                if (fresh != nullptr) {
                    hasIntegratedIr = fresh->hasIntegratedIr;
                    profileUrl = fresh->fileUrl;
                    irUrl = fresh->irFileUrl;
                    profile = safeThis->_client.download(profileUrl, working.accessToken);
                    if (profile.status == Marketplace::Status::Success && irUrl.isNotEmpty()) {
                        impulse = safeThis->_client.download(irUrl, working.accessToken);
                    }
                }
            }
        }

        juce::MessageManager::callAsync([safeThis, packId, working, library, profile, impulse, hasIntegratedIr, profileUrl, irUrl]() {
            if (safeThis == nullptr) {
                return;
            }

            if (working.accessToken != safeThis->_session.accessToken || working.refreshToken != safeThis->_session.refreshToken) {
                safeThis->_session.accessToken = working.accessToken;
                safeThis->_session.refreshToken = working.refreshToken;
                safeThis->_session.expiresIn = working.expiresIn;
                Marketplace::saveSession(Marketplace::defaultSessionFile(), safeThis->_session);
            }

            if (library.status == Marketplace::Status::Unauthorized) {
                Marketplace::clearSession(Marketplace::defaultSessionFile());
                safeThis->_session = {};
                safeThis->_challenge = {};
                safeThis->showLogin(library.message);
                return;
            }

            if (library.status != Marketplace::Status::Success) {
                safeThis->_statusLabel.setText(library.message, juce::dontSendNotification);
                safeThis->setBusy(false);
                return;
            }

            if (profileUrl.isEmpty()) {
                safeThis->_statusLabel.setText("This pack has no file to download.", juce::dontSendNotification);
                safeThis->setBusy(false);
                return;
            }

            if (profile.status != Marketplace::Status::Success) {
                safeThis->_statusLabel.setText(profile.message, juce::dontSendNotification);
                safeThis->setBusy(false);
                return;
            }

            if (irUrl.isNotEmpty() && impulse.status != Marketplace::Status::Success) {
                safeThis->_statusLabel.setText(impulse.message, juce::dontSendNotification);
                safeThis->setBusy(false);
                return;
            }

            const auto directory = safeThis->marketplaceDirectory();
            if (!directory.isDirectory() && !directory.createDirectory().wasOk()) {
                safeThis->_statusLabel.setText("Could not create the marketplace download folder.", juce::dontSendNotification);
                safeThis->setBusy(false);
                return;
            }

            const auto stem = safeFileStem(packId);
            const auto profileFile = directory.getChildFile(stem + ".model");
            if (!profileFile.replaceWithData(profile.body.getData(), profile.body.getSize())) {
                safeThis->_statusLabel.setText("Could not save the profile file.", juce::dontSendNotification);
                safeThis->setBusy(false);
                return;
            }

            juce::File irFile;
            if (irUrl.isNotEmpty()) {
                irFile = directory.getChildFile(stem + ".wav");
                if (!irFile.replaceWithData(impulse.body.getData(), impulse.body.getSize())) {
                    safeThis->_statusLabel.setText("Could not save the impulse response.", juce::dontSendNotification);
                    safeThis->setBusy(false);
                    return;
                }
            }

            juce::String errorMessage;
            const auto installed = safeThis->onInstallPack != nullptr &&
                                   safeThis->onInstallPack(profileFile, irFile, hasIntegratedIr, &errorMessage);
            safeThis->_statusLabel.setText(installed ? "Pack added to your library." : errorMessage,
                                           juce::dontSendNotification);
            safeThis->setBusy(false);
        });
    });
}

void MarketplaceAccountPanel::rebuildRows() {
    for (auto& row : _rows) {
        removeChildComponent(row.get());
    }
    _rows.clear();

    for (int index = 0; index < static_cast<int>(_items.size()); ++index) {
        auto row = std::make_unique<PackRow>(_items[static_cast<size_t>(index)], [this, index]() {
            downloadPack(index);
        });
        addAndMakeVisible(*row);
        _rows.push_back(std::move(row));
    }
}

void MarketplaceAccountPanel::notifyLayout() {
    resized();
    if (onLayoutChanged != nullptr) {
        onLayoutChanged();
    }
}

juce::File MarketplaceAccountPanel::marketplaceDirectory() const {
    return getSettingsFolder().getChildFile("Marketplace");
}
