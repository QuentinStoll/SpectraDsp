#include "Views/ProfilView.h"

#include "Modules/DeleteProfileConfirmationModule.h"
#include "Stylesheet.h"

namespace {
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
        listing.conditionProfile = Marketplace::conditionProfileIndex(item.conditionProfile);
        listing.conditionAmount = item.conditionAmount;
        if (item.kind == "ir") {
            listing.kind = ProfileCatalog::Kind::Ir;
            listing.badge = "IR";
            listings.push_back(listing);
            continue;
        }

        listing.kind = ProfileCatalog::Kind::Amp;
        listing.badge = item.hasIntegratedIr || item.hasSeparateIr ? "AMP + IR" : "AMP";
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
}  // namespace

ProfilView::ProfilView(ProfilerAudioProcessor& p)
    : _audioProcessor(p) {
    addAndMakeVisible(_viewport);
    _presetHeading.setText("PRESETS", juce::dontSendNotification);
    _presetHeading.setFont(ProfilerStyle::Fonts::moduleTitle());
    _presetHeading.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    _presetHeading.setJustificationType(juce::Justification::centredLeft);
    _libraryPage.addAndMakeVisible(_accountPanel);
    _libraryPage.addAndMakeVisible(_presetHeading);
    _libraryPage.addAndMakeVisible(_grid);
    _viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, ProfilerStyle::Colors::orange);
    _viewport.setViewedComponent(&_libraryPage, false);
    _viewport.setScrollBarsShown(true, false);
    refreshPresetGrid();

    _accountPanel.onLayoutChanged = [this]() { resized(); };
    _accountPanel.onLibraryChanged = [this](const std::vector<Marketplace::LibraryItem>& items) {
        _audioProcessor.getProfileCatalog().syncAccount(listingsFromAccount(items));
    };
    _accountPanel.onEncryptedProfile = [this](const juce::String& accessToken, const juce::String& id, const juce::String& title,
                                              const juce::String& kind, const juce::MemoryBlock& cipher, const juce::MemoryBlock& key,
                                              int conditionProfile, float conditionAmount, juce::String* errorMessage) {
        return storeEncryptedProfile(accessToken, id, title, kind, cipher, key, conditionProfile, conditionAmount, errorMessage);
    };
    _accountPanel.start();
    _audioProcessor.getPresetManager().addChangeListener(this);

    _grid.onAddProfileClicked = [this]() { saveCurrentChain(); };
    _grid.onProfileClicked = [this](int presetNumber) {
        juce::PopupMenu menu;
        menu.addItem(1, "Load preset");
        menu.addItem(2, "Delete preset");
        const juce::Component::SafePointer<ProfilView> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options(), [safeThis, presetNumber](int result) {
            if (safeThis == nullptr) {
                return;
            }
            if (result == 1) {
                juce::String error;
                if (safeThis->_audioProcessor.applyPreset(presetNumber - 1, &error)) {
                    safeThis->_notificationBanner.showMessage(error.isNotEmpty() ? error : "Preset loaded",
                                                              error.isNotEmpty() ? NotificationBanner::Type::Error
                                                                                 : NotificationBanner::Type::Success,
                                                              5000);
                } else {
                    safeThis->_notificationBanner.showMessage(error.isEmpty() ? "Could not load the preset." : error,
                                                              NotificationBanner::Type::Error,
                                                              5000);
                }
            } else if (result == 2) {
                safeThis->showDeletePresetModal(presetNumber);
            }
        });
    };

    addChildComponent(_modalOverlay);
    addChildComponent(_notificationBanner);
}

ProfilView::~ProfilView() {
    _audioProcessor.getPresetManager().removeChangeListener(this);
}

void ProfilView::downloadAccountProfile(const juce::String& id) {
    _accountPanel.downloadById(id);
}

void ProfilView::paint(juce::Graphics& g) {
    ProfilerStyle::Surfaces::fillPanel(g, getLocalBounds().toFloat());
}

void ProfilView::resized() {
    const auto area = getLocalBounds().reduced(25);
    constexpr auto scrollbarOffset = 15;
    const auto viewportArea = area.withRight(juce::jmin(getLocalBounds().getRight(), area.getRight() + scrollbarOffset));
    _viewport.setBounds(viewportArea);

    const auto contentWidth = area.getWidth();
    const auto accountHeight = _accountPanel.getPreferredHeight(contentWidth);
    constexpr auto headingHeight = 28;
    const auto gridHeight = _grid.getRequiredHeight(contentWidth);
    const auto pageHeight = juce::jmax(area.getHeight(), accountHeight + 16 + headingHeight + gridHeight);
    _libraryPage.setBounds(0, 0, contentWidth, pageHeight);
    _accountPanel.setBounds(0, 0, contentWidth, accountHeight);
    _presetHeading.setBounds(0, accountHeight + 16, contentWidth, headingHeight);
    _grid.setBounds(0, accountHeight + 16 + headingHeight, contentWidth, gridHeight);

    _modalOverlay.setBounds(getLocalBounds());
    const auto bannerWidth = juce::jmin(_notificationBanner.getIdealWidth(), juce::jmax(220, getWidth() - 50));
    _notificationBanner.setBounds(getLocalBounds()
                                      .withSizeKeepingCentre(bannerWidth, _notificationBanner.getIdealHeight())
                                      .withRightX(getWidth() - 25)
                                      .withY(25));
}

void ProfilView::saveCurrentChain() {
    juce::String error;
    const auto name = "Preset " + juce::String(_audioProcessor.getPresetManager().getCount() + 1);
    if (_audioProcessor.saveCurrentPreset(name, &error)) {
        refreshPresetGrid();
        _notificationBanner.showMessage("Preset saved", NotificationBanner::Type::Success, 5000);
    } else {
        _notificationBanner.showMessage(error.isNotEmpty() ? error : "Could not save the preset.",
                                        NotificationBanner::Type::Error,
                                        5000);
    }
}

void ProfilView::showDeletePresetModal(int presetNumber) {
    const auto* preset = _audioProcessor.getPresetManager().getPreset(presetNumber - 1);
    const auto name = preset != nullptr ? preset->name : "this preset";
    auto confirmation = std::make_unique<DeleteProfileConfirmationModule>(name);
    confirmation->onConfirm = [this, presetNumber]() {
        _modalOverlay.dismiss();
        deletePreset(presetNumber);
    };
    confirmation->onCancel = [this]() { _modalOverlay.dismiss(); };
    _modalOverlay.show(std::move(confirmation), juce::Rectangle<int>(0, 0, 296, 108));
}

void ProfilView::deletePreset(int presetNumber) {
    juce::String error;
    if (_audioProcessor.getPresetManager().deletePreset(presetNumber - 1, &error)) {
        refreshPresetGrid();
        _notificationBanner.showMessage("Preset deleted", NotificationBanner::Type::Success, 5000);
    } else {
        _notificationBanner.showMessage(error, NotificationBanner::Type::Error, 5000);
    }
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
    if (source == &_audioProcessor.getPresetManager()) {
        refreshPresetGrid();
    }
}

void ProfilView::refreshPresetGrid() {
    _grid.setProfileNames(_audioProcessor.getPresetManager().getNames());
    resized();
}
