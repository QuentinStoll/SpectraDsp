#include "Views/ProfilView.h"

#include "Stylesheet.h"

namespace {
constexpr int kColumnGap = 20;
constexpr int kSideMargin = 12;
constexpr int kBottomMargin = 16;

std::vector<ProfileCatalog::Listing> listingsFromAccount(const std::vector<Marketplace::LibraryItem>& items) {
    std::vector<ProfileCatalog::Listing> listings;
    for (const auto& item : items) {
        if (item.id.isEmpty()) {
            continue;
        }

        ProfileCatalog::Listing listing;
        listing.id = item.id;
        listing.title = item.title;
        listing.author = item.authorName;
        listing.summary = item.summary;
        listing.badge = Marketplace::badgeFor(item);
        listing.conditionProfile = Marketplace::conditionProfileIndex(item.conditionProfile);
        listing.conditionAmount = item.conditionAmount;
        if (item.kind == "ir") {
            listing.kind = ProfileCatalog::Kind::Ir;
            listings.push_back(listing);
            continue;
        }

        listing.kind = ProfileCatalog::Kind::Amp;
        listings.push_back(listing);
        if (item.hasSeparateIr && !item.hasIntegratedIr) {
            ProfileCatalog::Listing impulse = listing;
            impulse.id = item.id + "-cab";
            impulse.title = item.title + " IR";
            impulse.kind = ProfileCatalog::Kind::Ir;
            impulse.badge = "IR";
            impulse.conditionProfile = 0;
            impulse.conditionAmount = 0.0f;
            listings.push_back(impulse);
        }
    }
    return listings;
}

bool isDownloaded(const ProfileCatalog& catalog, const Marketplace::LibraryItem& item) {
    const auto kind = item.kind == "ir" ? ProfileCatalog::Kind::Ir : ProfileCatalog::Kind::Amp;
    const auto* entry = catalog.find(item.id, kind);
    if (entry == nullptr || !entry->cipherOnDisk) {
        return false;
    }
    if (item.kind != "amp") {
        return true;
    }
    juce::MemoryBlock key;
    const auto unlocked = catalog.copyKey(item.id, kind, key);
    key.fillWith(0);
    return unlocked;
}
}  // namespace

ProfilView::ProfilView(ProfilerAudioProcessor& p)
    : _audioProcessor(p),
      _presetPanel(p) {
    addAndMakeVisible(_presetPanel);
    addAndMakeVisible(_marketplacePanel);
    addAndMakeVisible(_accountPanel);
    addChildComponent(_notificationBanner);

    _presetPanel.onPresetLoaded = [this](const juce::String& warning) {
        if (onPresetLoaded) {
            onPresetLoaded(warning);
        }
    };
    _presetPanel.onStatus = [this](const juce::String& message, bool success) { showMessage(message, success); };
    _presetPanel.onPresetsChanged = [this]() {
        _accountPanel.setSavedPresetCount(_audioProcessor.getPresetManager().getCount());
    };
    _accountPanel.setSavedPresetCount(_audioProcessor.getPresetManager().getCount());

    _marketplacePanel.onDownloadClicked = [this](const juce::String& id) { _accountPanel.downloadById(id); };

    _accountPanel.onStateChanged = [this]() { refreshMarketplace(); };
    _accountPanel.onLibraryChanged = [this](const std::vector<Marketplace::LibraryItem>& items) {
        _audioProcessor.getProfileCatalog().syncAccount(listingsFromAccount(items));
    };
    _accountPanel.onEncryptedProfile = [this](const juce::String& accessToken, const juce::String& id, const juce::String& title,
                                              const juce::String& kind, const juce::MemoryBlock& cipher, const juce::MemoryBlock& key,
                                              int conditionProfile, float conditionAmount, juce::String* errorMessage) {
        return storeEncryptedProfile(accessToken, id, title, kind, cipher, key, conditionProfile, conditionAmount, errorMessage);
    };
    _audioProcessor.getProfileCatalog().addChangeListener(this);
    _accountPanel.start();
    refreshMarketplace();
}

ProfilView::~ProfilView() {
    _audioProcessor.getProfileCatalog().removeChangeListener(this);
}

void ProfilView::downloadAccountProfile(const juce::String& id) {
    _accountPanel.downloadById(id);
}

void ProfilView::paint(juce::Graphics& g) {
    for (auto* column : {static_cast<juce::Component*>(&_presetPanel),
                         static_cast<juce::Component*>(&_marketplacePanel),
                         static_cast<juce::Component*>(&_accountPanel)}) {
        ProfilerStyle::Surfaces::fillPanel(g, column->getBounds().toFloat());
    }
}

void ProfilView::resized() {
    auto area = getLocalBounds().reduced(kSideMargin, 0).withTrimmedBottom(kBottomMargin);
    const auto unit = juce::jmax(0, area.getWidth() - 2 * kColumnGap) / 4;
    _presetPanel.setBounds(area.removeFromLeft(unit));
    area.removeFromLeft(kColumnGap);
    _marketplacePanel.setBounds(area.removeFromLeft(unit * 2));
    area.removeFromLeft(kColumnGap);
    _accountPanel.setBounds(area);

    const auto bannerWidth = juce::jmin(_notificationBanner.getIdealWidth(), juce::jmax(220, getWidth() - 50));
    _notificationBanner.setBounds(getLocalBounds()
                                      .withSizeKeepingCentre(bannerWidth, _notificationBanner.getIdealHeight())
                                      .withRightX(getWidth() - 25)
                                      .withY(25));
}

void ProfilView::visibilityChanged() {
    if (isShowing()) {
        _presetPanel.grabKeyboardFocus();
    }
}

void ProfilView::refreshMarketplace() {
    const auto& catalog = _audioProcessor.getProfileCatalog();
    std::vector<MarketplaceGridPanel::Entry> entries;
    for (const auto& item : _accountPanel.getItems()) {
        if (item.id.isEmpty()) {
            continue;
        }
        entries.push_back({item, isDownloaded(catalog, item)});
    }
    _marketplacePanel.setContent(_accountPanel.getLibraryState(),
                                 std::move(entries),
                                 _accountPanel.getLibraryMessage(),
                                 _accountPanel.isBusy());
}

void ProfilView::showMessage(const juce::String& message, bool success) {
    _notificationBanner.showMessage(message,
                                    success ? NotificationBanner::Type::Success : NotificationBanner::Type::Error,
                                    5000);
    _notificationBanner.toFront(false);
}

bool ProfilView::storeEncryptedProfile(const juce::String& accessToken,
                                       const juce::String& id,
                                       const juce::String& title,
                                       const juce::String& kind,
                                       const juce::MemoryBlock& cipher,
                                       const juce::MemoryBlock& key,
                                       int conditionProfile,
                                       float conditionAmount,
                                       juce::String* errorMessage) {
    if (accessToken.isNotEmpty()) {
        _audioProcessor.unlockSpectraSession(accessToken);
    }
    return _audioProcessor.importEncryptedProfile(kind == "ir" ? ProfileCatalog::Kind::Ir : ProfileCatalog::Kind::Amp,
                                                  id,
                                                  title,
                                                  cipher,
                                                  key,
                                                  conditionProfile,
                                                  conditionAmount,
                                                  false,
                                                  errorMessage);
}

void ProfilView::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &_audioProcessor.getProfileCatalog()) {
        refreshMarketplace();
    }
}
