#pragma once

#include <JuceHeader.h>

#include <functional>

#include "Components/CustomTextButton.h"
#include "Components/CustomTextEditor.h"
#include "MarketplaceClient.h"

class MarketplaceAccountPanel : public juce::Component {
   public:
    MarketplaceAccountPanel();
    ~MarketplaceAccountPanel() override;

    std::function<void()> onLayoutChanged;
    std::function<bool(const juce::File& profileFile,
                       const juce::File& irFile,
                       bool hasIntegratedIr,
                       juce::String* errorMessage)>
        onInstallPack;
    std::function<bool(const juce::MemoryBlock& cipher, const juce::MemoryBlock& key, juce::String* errorMessage)>
        onPlaySpectra;

    void start();
    int getPreferredHeight(int width) const;

    void resized() override;

   private:
    enum class Mode {
        Login,
        Code,
        Library
    };

    Marketplace::Client _client;
    juce::ThreadPool _jobs{1};
    Mode _mode = Mode::Login;
    bool _busy = false;
    Marketplace::Session _session;
    Marketplace::MfaChallenge _challenge;
    std::vector<Marketplace::LibraryItem> _items;

    juce::Label _titleLabel;
    juce::Label _emailLabel;
    juce::Label _passwordLabel;
    juce::Label _codeLabel;
    juce::Label _statusLabel;
    juce::Label _accountLabel;
    CustomTextEditor _emailEditor;
    CustomTextEditor _passwordEditor;
    CustomTextEditor _codeEditor;
    CustomTextButton _submitButton{"Sign in", ProfilerStyle::Theme::Orange};
    CustomTextButton _backButton{"Back", ProfilerStyle::Theme::Dark};
    CustomTextButton _signOutButton{"Sign out", ProfilerStyle::Theme::Dark};
    juce::HyperlinkButton _signupLink;

    struct PackRow;
    std::vector<std::unique_ptr<PackRow>> _rows;

    void setBusy(bool busy);
    void showLogin(const juce::String& message);
    void showCodeEntry();
    void showLibrary();
    void applyLoginResult(const Marketplace::LoginResult& result, bool fromCode);
    void loadLibrary();
    void downloadPack(int index);
    void rebuildRows();
    void notifyLayout();
    juce::File marketplaceDirectory() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarketplaceAccountPanel)
};
