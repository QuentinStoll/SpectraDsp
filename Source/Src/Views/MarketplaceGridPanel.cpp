#include "Views/MarketplaceGridPanel.h"

#include "Components/CustomTextButton.h"
#include "Components/ProfileLibraryView.h"
#include "Stylesheet.h"

namespace {
constexpr int kCardGap = 12;
constexpr int kMinCardWidth = 150;
constexpr int kCardHeight = 196;
constexpr int kMaxColumns = 3;
constexpr int kMinColumns = 2;
}  // namespace

class MarketplaceGridPanel::Card : public juce::Component {
   public:
    explicit Card(std::function<void(const juce::String&)> download)
        : _download(std::move(download)) {
        _button.onClick = [this]() {
            if (!_downloaded && _download) {
                _download(_card.id);
            }
        };
        addAndMakeVisible(_button);
    }

    void setEntry(const Entry& entry, bool busy) {
        _card.id = entry.item.id;
        _card.title = entry.item.title.isNotEmpty() ? entry.item.title : "Profile";
        _card.author = entry.item.authorName;
        _card.badge = Marketplace::badgeFor(entry.item);
        _downloaded = entry.downloaded;
        _button.setButtonText(_downloaded ? "Downloaded" : "Download");
        _button.setTheme(_downloaded ? ProfilerStyle::Theme::Dark : ProfilerStyle::Theme::Orange);
        _button.setColour(juce::TextButton::textColourOffId,
                          _downloaded ? ProfilerStyle::Colors::text : juce::Colours::black);
        _button.setMouseCursor(_downloaded ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
        _button.setEnabled(_downloaded || !busy);
        repaint();
    }

    const juce::String& id() const noexcept { return _card.id; }

    void paint(juce::Graphics& g) override {
        ProfileLibraryView::paintCard(g, getLocalBounds().toFloat(), _card);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(10);
        _button.setBounds(area.removeFromBottom(28));
    }

   private:
    ProfileLibraryView::Card _card;
    bool _downloaded = false;
    CustomTextButton _button{"Download", ProfilerStyle::Theme::Orange};
    std::function<void(const juce::String&)> _download;
};

MarketplaceGridPanel::MarketplaceGridPanel() {
    _title.setText("Marketplace", juce::dontSendNotification);
    _title.setFont(ProfilerStyle::Fonts::bold(22.0f));
    _title.setColour(juce::Label::textColourId, ProfilerStyle::Colors::text);
    addAndMakeVisible(_title);

    _countLabel.setFont(ProfilerStyle::Fonts::regular(13.0f));
    _countLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::accent);
    addAndMakeVisible(_countLabel);

    _messageLabel.setFont(ProfilerStyle::Fonts::regular(12.0f));
    _messageLabel.setColour(juce::Label::textColourId, ProfilerStyle::Colors::textMuted);
    _messageLabel.setJustificationType(juce::Justification::centredRight);
    _messageLabel.setMinimumHorizontalScale(0.8f);
    addAndMakeVisible(_messageLabel);

    _viewport.setViewedComponent(&_cardHost, false);
    _viewport.setScrollBarsShown(true, false);
    _viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, ProfilerStyle::Colors::accent);
    addAndMakeVisible(_viewport);
}

MarketplaceGridPanel::~MarketplaceGridPanel() {
    _cardHost.removeAllChildren();
}

void MarketplaceGridPanel::setContent(MarketplaceAccountPanel::LibraryState state,
                                      std::vector<Entry> entries,
                                      const juce::String& message,
                                      bool busy) {
    using State = MarketplaceAccountPanel::LibraryState;
    _state = state;
    _message = message;
    if (state != State::Ready) {
        entries.clear();
    }

    auto sameCards = entries.size() == _cards.size();
    for (size_t index = 0; sameCards && index < entries.size(); ++index) {
        sameCards = _cards[index]->id() == entries[index].item.id;
    }
    if (!sameCards) {
        _cardHost.removeAllChildren();
        _cards.clear();
        for (size_t index = 0; index < entries.size(); ++index) {
            auto card = std::make_unique<Card>([this](const juce::String& id) {
                if (onDownloadClicked) {
                    onDownloadClicked(id);
                }
            });
            _cardHost.addAndMakeVisible(*card);
            _cards.push_back(std::move(card));
        }
    }
    for (size_t index = 0; index < entries.size(); ++index) {
        _cards[index]->setEntry(entries[index], busy);
    }

    _countLabel.setText(state == State::Ready ? juce::String(static_cast<int>(entries.size())) + " profiles on this account."
                                              : juce::String{},
                        juce::dontSendNotification);
    _messageLabel.setText(state == State::Ready ? message : juce::String{}, juce::dontSendNotification);
    _viewport.setVisible(!_cards.empty());
    if (!sameCards) {
        _viewport.setViewPosition(0, 0);
    }
    layoutCards();
    repaint();
}

juce::String MarketplaceGridPanel::emptyText() const {
    using State = MarketplaceAccountPanel::LibraryState;
    switch (_state) {
        case State::SignedOut:
            return "Sign in to see your profiles";
        case State::Loading:
            return "Loading profiles...";
        case State::Failed:
            return _message + "\nRetrying automatically.";
        case State::Ready:
        default:
            return "No amp or IR profiles on this account yet.";
    }
}

void MarketplaceGridPanel::paint(juce::Graphics& g) {
    if (!_cards.empty()) {
        return;
    }
    g.setColour(ProfilerStyle::Colors::textMuted);
    g.setFont(ProfilerStyle::Fonts::regular(13.0f));
    g.drawFittedText(emptyText(), _viewport.getBounds().reduced(12), juce::Justification::centred, 4);
}

void MarketplaceGridPanel::resized() {
    auto area = getLocalBounds().reduced(16);
    _title.setBounds(area.removeFromTop(32));
    auto statusLine = area.removeFromTop(20);
    _countLabel.setBounds(statusLine.removeFromLeft(juce::jmax(statusLine.getWidth() / 2, 200)));
    _messageLabel.setBounds(statusLine);
    area.removeFromTop(12);
    _viewport.setBounds(area);
    layoutCards();
}

void MarketplaceGridPanel::layoutCards() {
    const auto scrollbar = _viewport.getScrollBarThickness() + 4;
    const auto width = juce::jmax(0, _viewport.getWidth() - scrollbar);
    const auto columns = juce::jlimit(kMinColumns, kMaxColumns, (width + kCardGap) / (kMinCardWidth + kCardGap));
    const auto cardWidth = juce::jmax(1, (width - kCardGap * (columns - 1)) / columns);
    const auto rows = (static_cast<int>(_cards.size()) + columns - 1) / columns;

    for (int index = 0; index < static_cast<int>(_cards.size()); ++index) {
        const auto column = index % columns;
        const auto row = index / columns;
        _cards[static_cast<size_t>(index)]->setBounds(column * (cardWidth + kCardGap),
                                                      row * (kCardHeight + kCardGap),
                                                      cardWidth,
                                                      kCardHeight);
    }
    _cardHost.setSize(width, juce::jmax(0, rows * kCardHeight + juce::jmax(0, rows - 1) * kCardGap));
}
