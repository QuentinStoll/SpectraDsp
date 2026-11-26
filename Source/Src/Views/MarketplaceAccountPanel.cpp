#include "Views/MarketplaceAccountPanel.h"

#include <algorithm>

#include "SettingsPath.h"
#include "Stylesheet.h"

namespace {
constexpr juce::juce_wchar kPasswordBullet = 0x2022;
constexpr int kRevealButtonWidth = 32;
constexpr int kStatRowHeight = 30;
}  // namespace

MarketplaceAccountPanel::RevealButton::RevealButton()
    : juce::Button("Show password") {
    setClickingTogglesState(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(false);
    setTooltip("Show password");
}

void MarketplaceAccountPanel::RevealButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool) {
    const auto area = getLocalBounds().toFloat().withSizeKeepingCentre(18.0f, 12.0f);
    const auto colour = (getToggleState() ? ProfilerStyle::Colors::accent : ProfilerStyle::Colors::textMuted)
                            .withAlpha(isMouseOverButton || getToggleState() ? 1.0f : 0.8f);

    juce::Path eye;
    eye.startNewSubPath(area.getX(), area.getCentreY());
    eye.quadraticTo(area.getCentreX(), area.getY() - area.getHeight() * 0.45f, area.getRight(), area.getCentreY());
    eye.quadraticTo(area.getCentreX(), area.getBottom() + area.getHeight() * 0.45f, area.getX(), area.getCentreY());
    eye.closeSubPath();

    g.setColour(colour);
    g.strokePath(eye, juce::PathStrokeType(1.4f));
    const auto pupil = area.getHeight() * 0.42f;
    g.fillEllipse(juce::Rectangle<float>(pupil, pupil).withCentre(area.getCentre()));
    if (!getToggleState()) {
        g.drawLine(area.getX() + 1.0f, area.getBottom() + 2.0f, area.getRight() - 1.0f, area.getY() - 2.0f, 1.4f);
    }
}

MarketplaceAccountPanel::MarketplaceAccountPanel() {
    _titleLabel.setText("Account", juce::dontSendNotification);
    _titleLabel.setFont(ProfilerStyle::Fonts::bold(22.0f));
    _titleLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_titleLabel);

    _emailLabel.setText("Email", juce::dontSendNotification);
    _passwordLabel.setText("Password", juce::dontSendNotification);
    _codeLabel.setText("6-digit code", juce::dontSendNotification);
    _accountLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _accountLabel.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _accountLabel.setMinimumHorizontalScale(0.7f);
    for (auto* label : {&_emailLabel, &_passwordLabel, &_codeLabel}) {
        label->setFont(ProfilerStyle::Fonts::regular(13.0f));
        label->setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
        addAndMakeVisible(label);
    }
    addAndMakeVisible(_accountLabel);

    _statusLabel.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _statusLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::accent);
    _statusLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(_statusLabel);

    _passwordEditor.setPasswordCharacter(kPasswordBullet);
    _passwordEditor.setBorder(juce::BorderSize<int>(1, 1, 1, kRevealButtonWidth));
    _codeEditor.setInputRestrictions(6, "0123456789");
    _codeEditor.setJustification(juce::Justification::centred);
    addAndMakeVisible(_emailEditor);
    addAndMakeVisible(_passwordEditor);
    addAndMakeVisible(_revealPasswordButton);
    addAndMakeVisible(_codeEditor);

    _revealPasswordButton.onClick = [this]() {
        setPasswordRevealed(_revealPasswordButton.getToggleState());
    };

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
        _statusLabel.setText("Signing in...", juce::dontSendNotification);
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
        showLogin({});
    };
    addAndMakeVisible(_signOutButton);

    _signupLink.setButtonText(juce::String(u8"Pas de compte ? Créer un compte sur le marketplace"));
    _signupLink.setURL(juce::URL(Marketplace::signupUrl));
    _signupLink.setFont(ProfilerStyle::Fonts::regular(12.0f), false, juce::Justification::centredLeft);
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
    showLibrary();
    loadLibrary();
    startTimer(15000);
}

void MarketplaceAccountPanel::setSavedPresetCount(int count) {
    if (_savedPresetCount != count) {
        _savedPresetCount = count;
        repaint(_statsBounds);
    }
}

MarketplaceAccountPanel::LibraryState MarketplaceAccountPanel::getLibraryState() const noexcept {
    if (_mode != Mode::Library) {
        return LibraryState::SignedOut;
    }
    if (_libraryLoaded) {
        return LibraryState::Ready;
    }
    return _libraryMessage.isNotEmpty() && !_busy ? LibraryState::Failed : LibraryState::Loading;
}

void MarketplaceAccountPanel::paint(juce::Graphics& g) {
    if (_mode != Mode::Library) {
        return;
    }

    if (!_avatarBounds.isEmpty()) {
        const auto avatar = _avatarBounds.toFloat().reduced(1.0f);
        g.setColour(ProfilerStyle::Colors::accent.withAlpha(0.12f));
        g.fillEllipse(avatar);
        g.setColour(ProfilerStyle::Colors::accent);
        g.drawEllipse(avatar, 1.6f);
        const auto initial = _session.email.trim().substring(0, 1).toUpperCase();
        g.setFont(ProfilerStyle::Fonts::bold(avatar.getHeight() * 0.42f));
        g.drawText(initial.isNotEmpty() ? initial : "?", avatar, juce::Justification::centred, false);
    }

    if (_statsBounds.isEmpty()) {
        return;
    }

    auto stats = _statsBounds;
    g.setColour(ProfilerStyle::Colors::caption);
    g.setFont(ProfilerStyle::Fonts::moduleTitle());
    g.drawText("STATS", stats.removeFromTop(22), juce::Justification::centredLeft, false);

    auto row = [&g, &stats](const juce::String& label, const juce::String& value) {
        auto line = stats.removeFromTop(kStatRowHeight);
        ProfilerStyle::Surfaces::drawHairline(g, line.removeFromTop(1).toFloat());
        g.setColour(ProfilerStyle::Colors::textMuted);
        g.setFont(ProfilerStyle::Fonts::regular(12.0f));
        g.drawText(label, line, juce::Justification::centredLeft, true);
        g.setColour(ProfilerStyle::Colors::text);
        g.setFont(ProfilerStyle::Fonts::medium(12.0f));
        g.drawText(value, line, juce::Justification::centredRight, false);
    };

    if (_libraryLoaded) {
        const auto purchases = std::count_if(_items.begin(), _items.end(), [](const Marketplace::LibraryItem& item) {
            return item.source.equalsIgnoreCase("purchase");
        });
        row("Profiles", juce::String(static_cast<int>(_items.size())));
        row("Purchases", juce::String(static_cast<int>(purchases)));
    }
    row("Saved presets", juce::String(_savedPresetCount));
}

void MarketplaceAccountPanel::resized() {
    auto area = getLocalBounds().reduced(16);
    _titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    const auto loginFields = _mode != Mode::Library;
    _emailLabel.setVisible(_mode == Mode::Login);
    _passwordLabel.setVisible(_mode == Mode::Login);
    _emailEditor.setVisible(_mode == Mode::Login);
    _passwordEditor.setVisible(_mode == Mode::Login);
    _revealPasswordButton.setVisible(_mode == Mode::Login);
    _signupLink.setVisible(_mode == Mode::Login);
    _codeLabel.setVisible(_mode == Mode::Code);
    _codeEditor.setVisible(_mode == Mode::Code);
    _submitButton.setVisible(loginFields);
    _backButton.setVisible(_mode == Mode::Code);
    _accountLabel.setVisible(_mode == Mode::Library);
    _signOutButton.setVisible(_mode == Mode::Library);
    _statusLabel.setVisible(loginFields);
    _avatarBounds = {};
    _statsBounds = {};

    if (_mode == Mode::Login) {
        _emailLabel.setBounds(area.removeFromTop(18));
        _emailEditor.setBounds(area.removeFromTop(34));
        area.removeFromTop(12);
        _passwordLabel.setBounds(area.removeFromTop(18));
        _passwordEditor.setBounds(area.removeFromTop(34));
        _revealPasswordButton.setBounds(_passwordEditor.getBounds().removeFromRight(kRevealButtonWidth).reduced(1));
        area.removeFromTop(16);
        _submitButton.setBounds(area.removeFromTop(34));
        area.removeFromTop(12);
        _signupLink.setBounds(area.removeFromTop(36));
        area.removeFromTop(8);
        _statusLabel.setBounds(area.removeFromTop(juce::jmin(72, area.getHeight())));
        return;
    }

    if (_mode == Mode::Code) {
        _codeLabel.setBounds(area.removeFromTop(18));
        _codeEditor.setBounds(area.removeFromTop(34));
        area.removeFromTop(16);
        _submitButton.setBounds(area.removeFromTop(34));
        area.removeFromTop(10);
        _backButton.setBounds(area.removeFromTop(34));
        area.removeFromTop(12);
        _statusLabel.setBounds(area.removeFromTop(juce::jmin(72, area.getHeight())));
        return;
    }

    auto identity = area.removeFromTop(44);
    _avatarBounds = identity.removeFromLeft(44);
    identity.removeFromLeft(12);
    _accountLabel.setBounds(identity);
    area.removeFromTop(14);
    _signOutButton.setBounds(area.removeFromTop(32));
    area.removeFromTop(24);
    _statsBounds = area.removeFromTop(22 + 3 * kStatRowHeight);
}

void MarketplaceAccountPanel::setBusy(bool busy) {
    _busy = busy;
    _submitButton.setEnabled(!busy);
    _backButton.setEnabled(!busy);
    _emailEditor.setEnabled(!busy);
    _passwordEditor.setEnabled(!busy);
    _codeEditor.setEnabled(!busy);
    _signOutButton.setEnabled(!busy);
    notifyStateChanged();
}

void MarketplaceAccountPanel::setLibraryMessage(const juce::String& message) {
    _libraryMessage = message;
    notifyStateChanged();
}

void MarketplaceAccountPanel::setPasswordRevealed(bool revealed) {
    _revealPasswordButton.setToggleState(revealed, juce::dontSendNotification);
    _revealPasswordButton.setTooltip(revealed ? "Hide password" : "Show password");
    _passwordEditor.setPasswordCharacter(revealed ? 0 : kPasswordBullet);
}

void MarketplaceAccountPanel::showLogin(const juce::String& message) {
    _mode = Mode::Login;
    _items.clear();
    _libraryLoaded = false;
    _libraryMessage.clear();
    _submitButton.setButtonText("Sign in");
    _statusLabel.setText(message, juce::dontSendNotification);
    setPasswordRevealed(false);
    setBusy(false);
    resized();
    repaint();
}

void MarketplaceAccountPanel::showCodeEntry() {
    _mode = Mode::Code;
    _codeEditor.clear();
    _submitButton.setButtonText("Confirm");
    _statusLabel.setText("Enter the 6-digit code from your authenticator. It expires in 5 minutes.",
                         juce::dontSendNotification);
    setBusy(false);
    resized();
    repaint();
}

void MarketplaceAccountPanel::showLibrary() {
    _mode = Mode::Library;
    _accountLabel.setText(_session.email.isNotEmpty() ? _session.email : "Signed in",
                          juce::dontSendNotification);
    setBusy(false);
    resized();
    repaint();
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
        setPasswordRevealed(false);
        Marketplace::saveSession(Marketplace::defaultSessionFile(), _session);
        showLibrary();
        loadLibrary();
        startTimer(15000);
        return;
    }

    if (fromCode && result.status == Marketplace::Status::Unauthorized) {
        _challenge = {};
        showLogin(result.message);
        return;
    }

    _statusLabel.setText(result.message.isNotEmpty() ? result.message : "Could not sign in. Check your connection and try again.",
                         juce::dontSendNotification);
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
            result = safeThis->_client.loadSavedSession(working);
        }
        juce::MessageManager::callAsync([safeThis, working, result]() {
            if (safeThis == nullptr) {
                return;
            }

            if (result.status == Marketplace::Status::Unauthorized) {
                Marketplace::Session onDisk;
                if (Marketplace::loadSession(Marketplace::defaultSessionFile(), onDisk) &&
                    onDisk.accessToken.isNotEmpty() && onDisk.accessToken != working.accessToken) {
                    safeThis->_session = onDisk;
                    safeThis->setBusy(false);
                    safeThis->loadLibrary();
                    return;
                }
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
                safeThis->_libraryMessage = result.message.isNotEmpty() ? result.message : "Could not reach the marketplace.";
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
            safeThis->_libraryLoaded = true;
            safeThis->_libraryMessage.clear();
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
    const juce::String message = "Sign in and refresh the library to open this profile.";
    if (_mode == Mode::Library) {
        setLibraryMessage(message);
    } else {
        _statusLabel.setText(message, juce::dontSendNotification);
    }
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
    if (kind == "amp" || kind == "pedal") {
        _libraryMessage = "Opening " + title + "...";
        setBusy(true);
        juce::Component::SafePointer<MarketplaceAccountPanel> safeThis(this);
        _jobs.addJob([safeThis, packId, session, title, kind, listedProfile, listedAmount]() {
            juce::MemoryBlock cipher;
            juce::MemoryBlock key;
            juce::MemoryBlock irCipher;
            juce::String error;
            auto working = session;
            auto conditionProfile = listedProfile;
            auto conditionAmount = listedAmount;
            bool separateIr = false;
            if (safeThis != nullptr) {
                const auto library = safeThis->_client.loadSavedSession(working);
                const Marketplace::LibraryItem* fresh = nullptr;
                if (library.status == Marketplace::Status::Success) {
                    for (const auto& item : library.items) {
                        if (item.id == packId && item.kind == kind) {
                            fresh = &item;
                            break;
                        }
                    }
                }
                const auto noun = kind == "pedal" ? juce::String("pedal") : juce::String("amp");
                if (fresh == nullptr || fresh->fileUrl.isEmpty()) {
                    error = library.status == Marketplace::Status::Success ? "This " + noun + " is not ready yet." : library.message;
                } else {
                    const auto downloaded = safeThis->_client.download(fresh->fileUrl, working.accessToken);
                    const auto keyResult = safeThis->_client.ampKey(working.accessToken, packId);
                    juce::MemoryOutputStream decodedKey;
                    if (downloaded.status != Marketplace::Status::Success || keyResult.status != Marketplace::Status::Success ||
                        !juce::Base64::convertFromBase64(decodedKey, keyResult.message) || decodedKey.getDataSize() != 32) {
                        error = downloaded.message.isNotEmpty() ? downloaded.message : "Could not open this " + noun + ".";
                    } else {
                        cipher = downloaded.body;
                        key = decodedKey.getMemoryBlock();
                        conditionProfile = Marketplace::conditionProfileIndex(fresh->conditionProfile);
                        conditionAmount = fresh->conditionAmount;
                        separateIr = kind == "amp" && fresh->hasSeparateIr && !fresh->hasIntegratedIr;
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

            juce::MessageManager::callAsync([safeThis, packId, title, kind, working, cipher, key, irCipher, error, conditionProfile, conditionAmount, separateIr]() mutable {
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
                    safeThis->_libraryMessage = error;
                    safeThis->setBusy(false);
                    return;
                }
                juce::String playError;
                const auto played = safeThis->onEncryptedProfile != nullptr &&
                                    safeThis->onEncryptedProfile(working.accessToken, packId, title, kind, cipher, key,
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
                const auto loadedMessage = kind == "pedal" ? juce::String("Pedal profile loaded.") : juce::String("Amp profile loaded.");
                safeThis->_libraryMessage = played && irPlayed ? loadedMessage : playError;
                safeThis->setBusy(false);
            });
        });
        return;
    }
    _libraryMessage = "Downloading " + title + "...";
    setBusy(true);

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
            library = safeThis->_client.loadSavedSession(working);
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
                safeThis->_libraryMessage = message;
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

void MarketplaceAccountPanel::notifyStateChanged() {
    if (onStateChanged != nullptr) {
        onStateChanged();
    }
}
