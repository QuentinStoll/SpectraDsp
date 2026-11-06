#include "Components/CustomImageButton.h"

#include "Components/CustomLookAndFeel.h"

//=============================================================================
// CustomImageButton Implementation
//=============================================================================

CustomImageButton::CustomImageButton(const juce::String& buttonName,
                                     const void* binaryData,
                                     size_t dataSize,
                                     ProfilerStyle::Theme theme) : juce::Button(buttonName),
                                                                   _drawable(juce::Drawable::createFromImageData(binaryData, dataSize)) {
    if (_drawable != nullptr) {
        _drawable->replaceColour(juce::Colour(0xff000000), ProfilerStyle::Colors::white);
    }
    setTheme(theme);
}

void CustomImageButton::setTheme(ProfilerStyle::Theme theme) {
    switch (theme) {
        case ProfilerStyle::Theme::Darker:
            _backgroundColour = ProfilerStyle::Colors::darkestGrey;
            setOutlineVisible(true);
            break;
        case ProfilerStyle::Theme::Dark:
            _backgroundColour = ProfilerStyle::Colors::darkerGrey;
            break;
        case ProfilerStyle::Theme::Light:
            _backgroundColour = ProfilerStyle::Colors::darkGrey;
            break;
        case ProfilerStyle::Theme::Orange:
            _backgroundColour = ProfilerStyle::Colors::orange;
            break;
    }
}

void CustomImageButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) {
    auto area = getLocalBounds();
    auto cornerSize = 4.0f;
    auto fillArea = _outlineVisible ? area.reduced(2) : area;

    auto buttonText = getButtonText();
    auto hasImage = (_drawable != nullptr);
    auto hasText = buttonText.isNotEmpty();
    auto gap = 0;
    auto font = ProfilerStyle::Fonts::regular(16.0f);
    auto contentArea = area.reduced(10);
    auto imageArea = hasText ? contentArea : contentArea.reduced(juce::roundToInt(getWidth() * 0.3f));
    auto textArea = contentArea;

    if (hasImage && hasText) {
        gap = 10;
        auto removedHeight = juce::roundToInt(contentArea.getHeight() * 0.25f);
        contentArea.removeFromTop(removedHeight);
        contentArea.removeFromBottom(removedHeight);
        auto textHeight = juce::roundToInt(font.getHeight()) + 2;
        textArea = contentArea.removeFromBottom(juce::jmin(textHeight, contentArea.getHeight()));
        imageArea = contentArea;
    }

    if (auto* customLf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel())) {
        customLf->paintFlatButtonBackground(g,
                                            area.toFloat(),
                                            _backgroundColour,
                                            isMouseOverButton,
                                            isButtonDown,
                                            getToggleState(),
                                            _outlineVisible,
                                            _outlineColour);
    } else {
        auto baseColour = _backgroundColour;
        if (isButtonDown)
            baseColour = baseColour.darker(0.12f);
        else if (isMouseOverButton)
            baseColour = baseColour.brighter(0.06f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(fillArea.toFloat(), cornerSize);

        if (_outlineVisible) {
            g.setColour(_outlineColour.withAlpha(0.45f));
            g.drawRoundedRectangle(area.toFloat(), cornerSize + 1.0f, 1.0f);
        }
    }

    auto alpha = (isEnabled() ? 1.0f : 0.5f) * (isButtonDown ? 0.8f : 1.0f);

    if (hasImage) {
        drawImageContent(g, imageArea.reduced(0, gap).toFloat(), alpha);
    }

    if (hasText) {
        g.setFont(font);
        g.setColour(ProfilerStyle::Contrast::enabled
                        ? (isButtonDown || getToggleState() ? juce::Colours::black : juce::Colours::white)
                        : _textColour.withMultipliedAlpha(alpha));
        g.drawText(buttonText, textArea, juce::Justification::centred, true);
    }
}

void CustomImageButton::drawImageContent(juce::Graphics& g, juce::Rectangle<float> imageArea, float alpha) {
    if (_drawable != nullptr) {
        if (ProfilerStyle::Contrast::enabled) {
            auto copy = _drawable->createCopy();
            const auto colour = isDown() || getToggleState() ? juce::Colours::black : juce::Colours::white;
            copy->replaceColour(ProfilerStyle::Colors::white, colour);
            copy->replaceColour(juce::Colours::white, colour);
            copy->drawWithin(g, imageArea, juce::RectanglePlacement::centred, 1.0f);
            return;
        }
        _drawable->drawWithin(g,
                              imageArea,
                              juce::RectanglePlacement::centred,
                              alpha);
    }
}
