#pragma once

#include <JuceHeader.h>

#include <functional>

#include "Components/CustomTextButton.h"
#include "Components/CustomTextEditor.h"
#include "MarketplaceClient.h"

class MarketplaceAccountPanel : public juce::Component,
                                private juce::Timer {
   public:
    enum class LibraryState {
        SignedOut,
        Loading,
        Failed,
        Ready
    };

    MarketplaceAccountPanel();
    ~MarketplaceAccountPanel() override;

    std::function<void()> onStateChanged;
    std::function<void(const std::vector<Marketplace::LibraryItem>& items)> onLibraryChanged;
    std::function<bool(const juce::String& accessToken,
                       const juce::String& id,
                       const juce::String& title,
                       const juce::String& kind,
                       const juce::MemoryBlock& cipher,
                       const juce::MemoryBlock& key,
                       int conditionProfile,
                       float conditionAmount,
                       juce::String* errorMessage)>
        onEncryptedProfile;

    void start();
    void downloadById(const juce::String& id);
    void setSavedPresetCount(int count);

    LibraryState getLibraryState() const noexcept;
    const std::vector<Marketplace::LibraryItem>& getItems() const noexcept { return _items; }
    const juce::String& getLibraryMessage() const noexcept { return _libraryMessage; }
    bool isBusy() const noexcept { return _busy; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

   private:
    enum class Mode {
        Login,
        Code,
        Library
    };

    class RevealButton : public juce::Button {
       public:
        RevealButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    Marketplace::Client _client;
    juce::ThreadPool _jobs{1};
    Mode _mode = Mode::Login;
    bool _busy = false;
    bool _libraryLoaded = false;
    int _savedPresetCount = 0;
    Marketplace::Session _session;
    Marketplace::MfaChallenge _challenge;
    std::vector<Marketplace::LibraryItem> _items;
    juce::String _libraryMessage;

    juce::Label _titleLabel;
    juce::Label _emailLabel;
    juce::Label _passwordLabel;
    juce::Label _codeLabel;
    juce::Label _statusLabel;
    juce::Label _accountLabel;
    CustomTextEditor _emailEditor;
    CustomTextEditor _passwordEditor;
    RevealButton _revealPasswordButton;
    CustomTextEditor _codeEditor;
    CustomTextButton _submitButton{"Sign in", ProfilerStyle::Theme::Orange};
    CustomTextButton _backButton{"Back", ProfilerStyle::Theme::Dark};
    CustomTextButton _signOutButton{"Sign out", ProfilerStyle::Theme::Dark};
    juce::HyperlinkButton _signupLink;
    juce::Rectangle<int> _avatarBounds;
    juce::Rectangle<int> _statsBounds;

    void setBusy(bool busy);
    void setLibraryMessage(const juce::String& message);
    void showLogin(const juce::String& message);
    void showCodeEntry();
    void showLibrary();
    void applyLoginResult(const Marketplace::LoginResult& result, bool fromCode);
    void loadLibrary();
    void timerCallback() override;
    void downloadPack(int index);
    void setPasswordRevealed(bool revealed);
    void notifyStateChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarketplaceAccountPanel)
};
