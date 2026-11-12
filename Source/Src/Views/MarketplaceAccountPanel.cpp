#include "Views/MarketplaceAccountPanel.h"

#include "ActionFeedback.h"
#include "SettingsPath.h"
#include "Stylesheet.h"

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
        ActionFeedback::post(*this, _mode == Mode::Code ? "Checking verification code..." : "Signing in...");
        if (_busy) {
            return;
        }

        if (_mode == Mode::Code) {
            const auto code = _codeEditor.getText().trim();
            if (!Marketplace::Client::isSixDigitCode(code)) {
                setStatus("Enter the 6-digit code.");
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
            setStatus("Enter your marketplace email and password.");
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
        ActionFeedback::post(*this, "Sign-in form opened.");
    };
    addAndMakeVisible(_backButton);

    _signOutButton.onClick = [this]() {
        Marketplace::clearSession(Marketplace::defaultSessionFile());
        _session = {};
        _challenge = {};
        _items.clear();
        showLogin({});
        ActionFeedback::post(*this, "Signed out of the marketplace.");
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
    setStatus(message);
    setBusy(false);
    notifyLayout();
}

void MarketplaceAccountPanel::showCodeEntry() {
    _mode = Mode::Code;
    _codeEditor.clear();
    _submitButton.setButtonText("Confirm");
    setStatus("Enter the 6-digit code from your authenticator. It expires in 5 minutes.");
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

    setStatus(result.message);
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
                safeThis->setStatus(result.message);
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
            safeThis->setStatus(safeThis->_items.empty()
                                    ? "No amp or IR profiles on this account yet."
                                    : juce::String(safeThis->_items.size()) + " profiles on this account.");
            if (safeThis->onLibraryChanged) {
                safeThis->onLibraryChanged(safeThis->_items);
            }
            safeThis->showLibrary();
        });
    });
}

void MarketplaceAccountPanel::downloadById(const juce::String& id) {
    for (int index = 0; index < static_cast<int>(_items.size()); ++index) {
        const auto& item = _items[static_cast<size_t>(index)];
        if (item.id == id || item.id + "-cab" == id) {
            downloadPack(index);
            return;
        }
    }
    setStatus("Sign in and refresh the library to open this profile.");
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
        setStatus("Opening " + title + "...");
        juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
        _jobs.addJob([safeThis, packId, session, title, listedProfile, listedAmount]() {
            juce::MemoryBlock cipher;
            juce::MemoryBlock key;
            juce::MemoryBlock irCipher;
            juce::String error;
            auto working = session;
            auto conditionProfile = listedProfile;
            auto conditionAmount = listedAmount;
            bool separateIr = false;
            if (safeThis != nullptr) {
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
                        conditionProfile = Marketplace::conditionProfileIndex(fresh->conditionProfile);
                        conditionAmount = fresh->conditionAmount;
                        separateIr = fresh->hasSeparateIr && !fresh->hasIntegratedIr;
                        if (separateIr) {
                            const auto impulse = safeThis->_client.download(fresh->irFileUrl, working.accessToken);
                            if (impulse.status == Marketplace::Status::Success) {
                                irCipher = impulse.body;
                            } else {
                                separateIr = false;
                                error = impulse.message.isNotEmpty() ? impulse.message : "Could not download the IR profile.";
                            }
                        }
                    }
                }
            }

            juce::MessageManager::callAsync([safeThis, packId, title, working, cipher, key, irCipher, error, conditionProfile, conditionAmount, separateIr]() mutable {
                if (safeThis == nullptr) {
                    return;
                }
                getSettingsFolder().getChildFile("Spectra").getChildFile(packId + ".key").deleteFile();
                if (working.accessToken.isNotEmpty()) {
                    safeThis->_session.accessToken = working.accessToken;
                    safeThis->_session.refreshToken = working.refreshToken;
                    Marketplace::saveSession(Marketplace::defaultSessionFile(), safeThis->_session);
                }
                if (error.isNotEmpty()) {
                    cipher.fillWith(0);
                    key.fillWith(0);
                    irCipher.fillWith(0);
                    safeThis->setStatus(error);
                    safeThis->setBusy(false);
                    return;
                }
                juce::String playError;
                const auto played = safeThis->onEncryptedProfile != nullptr &&
                                    safeThis->onEncryptedProfile(working.accessToken, packId, title, "amp", cipher, key,
                                                                 conditionProfile, conditionAmount, &playError);
                auto irPlayed = true;
                if (played && separateIr && irCipher.getSize() > 0) {
                    juce::MemoryBlock emptyKey;
                    irPlayed = safeThis->onEncryptedProfile(working.accessToken, packId + "-cab", title + " IR", "ir", irCipher,
                                                            emptyKey, 0, 0.0f, &playError);
                }
                cipher.fillWith(0);
                key.fillWith(0);
                irCipher.fillWith(0);
                safeThis->setStatus(played && irPlayed ? "Amp profile loaded." : playError);
                safeThis->setBusy(false);
            });
        });
        return;
    }
    setBusy(true);
    setStatus("Downloading " + title + "...");

    juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
    _jobs.addJob([safeThis, packId, entitlementId, title, kind, session]() {
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

        juce::MessageManager::callAsync([safeThis, packId, title, kind, working, library, profile, impulse, hasIntegratedIr, profileUrl, irUrl]() mutable {
            if (safeThis == nullptr) {
                return;
            }

            if (working.accessToken != safeThis->_session.accessToken || working.refreshToken != safeThis->_session.refreshToken) {
                safeThis->_session.accessToken = working.accessToken;
                safeThis->_session.refreshToken = working.refreshToken;
                safeThis->_session.expiresIn = working.expiresIn;
                Marketplace::saveSession(Marketplace::defaultSessionFile(), safeThis->_session);
            }

            auto finish = [safeThis](const juce::String& message) {
                if (safeThis == nullptr) {
                    return;
                }
                safeThis->setStatus(message);
                safeThis->setBusy(false);
            };

            if (library.status == Marketplace::Status::Unauthorized) {
                Marketplace::clearSession(Marketplace::defaultSessionFile());
                safeThis->_session = {};
                safeThis->_challenge = {};
                safeThis->showLogin(library.message);
                profile.body.fillWith(0);
                impulse.body.fillWith(0);
                return;
            }

            if (library.status != Marketplace::Status::Success) {
                finish(library.message);
                profile.body.fillWith(0);
                impulse.body.fillWith(0);
                return;
            }

            if (profileUrl.isEmpty()) {
                finish("This profile has no file to download.");
                return;
            }

            if (profile.status != Marketplace::Status::Success) {
                finish(profile.message);
                profile.body.fillWith(0);
                return;
            }

            if (irUrl.isNotEmpty() && impulse.status != Marketplace::Status::Success) {
                finish(impulse.message);
                profile.body.fillWith(0);
                impulse.body.fillWith(0);
                return;
            }

            const auto profileKind = kind == "ir" ? "ir" : "amp";
            juce::MemoryBlock emptyKey;
            juce::String errorMessage;
            const auto stored = safeThis->onEncryptedProfile != nullptr &&
                                safeThis->onEncryptedProfile(working.accessToken, packId, title, profileKind, profile.body, emptyKey,
                                                             0, 0.0f, &errorMessage);
            auto irStored = true;
            if (stored && profileKind == "amp" && irUrl.isNotEmpty() && !hasIntegratedIr) {
                irStored = safeThis->onEncryptedProfile(working.accessToken, packId + "-cab", title + " IR", "ir", impulse.body,
                                                        emptyKey, 0, 0.0f, &errorMessage);
            } else if (stored && profileKind == "ir" && irUrl.isNotEmpty()) {
                irStored = safeThis->onEncryptedProfile(working.accessToken, packId, title, "ir", impulse.body, emptyKey, 0,
                                                        0.0f, &errorMessage);
            }
            profile.body.fillWith(0);
            impulse.body.fillWith(0);
            finish(stored && irStored ? "Encrypted profile stored." : errorMessage);
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
void MarketplaceAccountPanel::setStatus(const juce::String& message) {
    const bool changed = _statusLabel.getText() != message;
    _statusLabel.setText(message, juce::dontSendNotification);
    if (changed) ActionFeedback::post(*this, message);
}
