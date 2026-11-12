#pragma once

#include <JuceHeader.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4 {
   public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override = default;
    void applyColourScheme();
    void refreshTheme(juce::Component& root);

    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;
    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont(juce::Slider& slider) override;
    juce::Font getAlertWindowFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getMenuBarFont(juce::MenuBarComponent& menuBar, int itemIndex, const juce::String& itemText) override;
    juce::Font getTabButtonFont(juce::TabBarButton& button, float height) override;
    juce::Font getUiFont(float height, juce::Font::FontStyleFlags style = juce::Font::plain) const;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool isMouseOverButton, bool isButtonDown) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawTickBox(juce::Graphics& g, juce::Component& button,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled, bool isMouseOver, bool isMouseDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive,
                           bool isHighlighted, bool isChecked,
                           bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColourToUse) override;

    void drawTabbedButtonBarBackground(juce::TabbedButtonBar& bar, juce::Graphics& g) override;

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
                       bool isMouseOver, bool isMouseDown) override;

    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar& bar, juce::Graphics& g, int w, int h) override;

    int getTabButtonBestWidth(juce::TabBarButton& button, int tabDepth) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    juce::Font getTooltipFont();
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText,
                                          juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;

    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g,
                                    int w, int h, int titleSpaceX, int titleSpaceW,
                                    const juce::Image* icon, bool drawTitleTextOnLeft) override;
    void fillResizableWindowBackground(juce::Graphics& g, int w, int h,
                                       const juce::BorderSize<int>& border,
                                       juce::ResizableWindow& window) override;
    void drawResizableWindowBorder(juce::Graphics& g, int w, int h,
                                   const juce::BorderSize<int>& border,
                                   juce::ResizableWindow& window) override;

    void paintFlatButtonBackground(juce::Graphics& g,
                                   juce::Rectangle<float> bounds,
                                   juce::Colour backgroundColour,
                                   bool isMouseOver,
                                   bool isButtonDown,
                                   bool isOn,
                                   bool outlineVisible = false,
                                   juce::Colour outlineColour = {}) const;

    enum class RigIcon {
        InputJack,
        AmpHead,
        Cabinet,
        EqFaders,
        Pedal,
        Speaker,
        Pitch,
        Reverb,
        Delay,
        Mod,
        Dynamics,
        Tuner
    };

    void drawRaisedPanel(juce::Graphics& g,
                         juce::Rectangle<float> bounds,
                         bool glow = false,
                         float cornerRadius = 6.0f) const;

    static constexpr float kSignalBusGlowWidth = 5.1f;
    static constexpr float kSignalBusCoreWidth = 1.72f;

    void drawSignalChainBlock(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              juce::Colour categoryColour,
                              RigIcon icon,
                              bool isActive,
                              bool isMouseOver,
                              bool ledOn) const;

    void drawSignalIoNode(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          bool isActive,
                          bool isMouseOver,
                          bool ledOn = true,
                          bool showLed = true,
                          float signalLevel = 0.0f) const;

    static juce::Rectangle<float> getSignalChainLedBounds(juce::Rectangle<float> bounds);
    static juce::Rectangle<float> getSignalIoLedBounds(juce::Rectangle<float> bounds);

    void drawSignalBus(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to) const;

    void drawEmptySignalSlot(juce::Graphics& g, juce::Rectangle<float> bounds) const;

    void drawSignalCable(juce::Graphics& g,
                         juce::Point<float> from,
                         juce::Point<float> to,
                         bool isEnergized) const;

    void drawDropZone(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      const juce::String& title,
                      const juce::String& detail,
                      bool isLoaded,
                      bool isDragOver,
                      RigIcon icon,
                      juce::Colour categoryColour) const;

    void drawGearIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) const;
    void drawLibraryIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) const;
    void drawRigIcon(juce::Graphics& g, juce::Rectangle<float> bounds, RigIcon icon, juce::Colour colour) const;

   private:
    juce::Typeface::Ptr _orbitronRegular;
    juce::Typeface::Ptr _orbitronMedium;
    juce::Typeface::Ptr _orbitronBold;
    juce::Typeface::Ptr _orbitronExtraBold;
    juce::Typeface::Ptr _orbitronBlack;

    void loadOrbitronTypefaces();
    juce::Typeface::Ptr typefaceForFont(const juce::Font& font) const;
    void drawAccentGlow(juce::Graphics& g, juce::Rectangle<float> bounds, float intensity,
                        juce::Colour colour = {}) const;
    void drawSwitch(juce::Graphics& g, juce::Rectangle<float> area,
                    bool ticked, bool isEnabled, bool isMouseOver, bool isMouseDown) const;
    juce::String ellipsize(const juce::Font& font, const juce::String& text, float maxWidth) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeel)
};
