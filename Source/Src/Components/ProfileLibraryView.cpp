#include "Components/ProfileLibraryView.h"

#include "Stylesheet.h"

namespace {
void drawBadge(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text, juce::Colour colour) {
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(colour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.2f);
    g.setFont(ProfilerStyle::Fonts::bold(10.0f));
    g.drawFittedText(text, bounds.toNearestInt().reduced(6, 0), juce::Justification::centred, 1);
}
}  // namespace

class ProfileCardButton : public juce::Component {
   public:
    ProfileCardButton(ProfileLibraryView::Card card, std::function<void(const juce::String&)> clicked)
        : _card(std::move(card)),
          _clicked(std::move(clicked)) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xff101218));
        g.fillRoundedRectangle(bounds, 12.0f);

        auto art = bounds.removeFromTop(juce::jmax(64.0f, bounds.getHeight() * 0.42f));
        juce::ColourGradient gradient(_card.importWav ? juce::Colour(0xff12324a) : juce::Colour(0xff2c1452),
                                      art.getTopLeft(),
                                      _card.importWav ? juce::Colour(0xff1a4a62) : juce::Colour(0xff6d2454),
                                      art.getBottomRight(),
                                      false);
        g.setGradientFill(gradient);
        juce::Path artPath;
        artPath.addRoundedRectangle(art.getX(), art.getY(), art.getWidth(), art.getHeight(), 12.0f, 12.0f, true, true, false, false);
        g.fillPath(artPath);

        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.setFont(ProfilerStyle::Fonts::black(juce::jmin(34.0f, art.getHeight() * 0.42f)));
        g.drawFittedText(_card.importWav ? "WAV" : "PROFIL", art.toNearestInt(), juce::Justification::centred, 1);

        auto badges = art.reduced(8.0f).removeFromTop(18.0f);
        if (_card.spectra) {
            auto spectra = badges.removeFromLeft(62.0f);
            drawBadge(g, spectra, "SPECTRA", juce::Colour(0xffb026ff));
            badges.removeFromLeft(6.0f);
        }
        if (_card.badge.isNotEmpty()) {
            const auto badgeWidth = juce::jmin(badges.getWidth(), _card.badge.length() * 7.0f + 16.0f);
            drawBadge(g, badges.removeFromLeft(badgeWidth), _card.badge, juce::Colour(0xff3dd68c));
        }

        auto text = bounds.reduced(10.0f, 8.0f);
        g.setColour(ProfilerStyle::Colors::text);
        g.setFont(ProfilerStyle::Fonts::bold(13.0f));
        g.drawFittedText(_card.title, text.removeFromTop(34).toNearestInt(), juce::Justification::topLeft, 2);

        if (_card.author.isNotEmpty()) {
            g.setColour(ProfilerStyle::Colors::accentCyan);
            g.setFont(ProfilerStyle::Fonts::bold(11.0f));
            g.drawFittedText("By " + _card.author, text.removeFromTop(16).toNearestInt(), juce::Justification::centredLeft, 1);
        }

        if (_card.summary.isNotEmpty()) {
            text.removeFromTop(2.0f);
            g.setColour(ProfilerStyle::Colors::textMuted);
            g.setFont(ProfilerStyle::Fonts::regular(11.0f));
            g.drawFittedText(_card.summary, text.toNearestInt(), juce::Justification::topLeft, 3);
        }

        g.setColour(_card.selected ? ProfilerStyle::Colors::accentCyan : juce::Colour(0xff2a2c34));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 12.0f, _card.selected ? 2.0f : 1.0f);
    }

    void mouseUp(const juce::MouseEvent& event) override {
        if (!event.mouseWasClicked() || !_clicked) {
            return;
        }
        auto clicked = _clicked;
        const auto id = _card.id;
        juce::MessageManager::callAsync([clicked, id]() {
            if (clicked) {
                clicked(id);
            }
        });
    }

   private:
    ProfileLibraryView::Card _card;
    std::function<void(const juce::String&)> _clicked;
};

ProfileLibraryView::ProfileLibraryView() {
    _viewport.setViewedComponent(&_row, false);
    _viewport.setScrollBarsShown(false, true);
    _viewport.getHorizontalScrollBar().setColour(juce::ScrollBar::thumbColourId, ProfilerStyle::Colors::accentCyan);
    addAndMakeVisible(_viewport);
}

void ProfileLibraryView::setCards(std::vector<Card> cards) {
    _empty = cards.empty();
    _viewport.setVisible(!_empty);
    _buttons.clear();
    _row.removeAllChildren();
    for (const auto& card : cards) {
        auto button = std::make_unique<ProfileCardButton>(card, [this](const juce::String& id) {
            if (onCardClicked) {
                onCardClicked(id);
            }
        });
        _row.addAndMakeVisible(*button);
        _buttons.push_back(std::move(button));
    }
    layoutCards();
    repaint();
}

void ProfileLibraryView::resized() {
    _viewport.setBounds(getLocalBounds());
    layoutCards();
}

void ProfileLibraryView::paint(juce::Graphics& g) {
    if (!_empty) {
        return;
    }
    g.setColour(ProfilerStyle::Colors::textMuted);
    g.setFont(ProfilerStyle::Fonts::regular(13.0f));
    g.drawFittedText("Sign in on the Library page to see the profiles on this account.",
                     getLocalBounds().reduced(12),
                     juce::Justification::centred,
                     2);
}

void ProfileLibraryView::layoutCards() {
    constexpr int cardWidth = 176;
    constexpr int gap = 10;
    const auto height = juce::jmax(1, _viewport.getHeight());
    int x = 0;
    for (auto* child : _row.getChildren()) {
        child->setBounds(x, 0, cardWidth, height);
        x += cardWidth + gap;
    }
    _row.setSize(juce::jmax(x, _viewport.getWidth()), height);
}
