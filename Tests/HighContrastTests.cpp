#include "Components/CustomLookAndFeel.h"
#include "Stylesheet.h"
#include "TestRunner.h"

#include <cmath>

namespace profiler_tests {
namespace {
double luminance(juce::Colour colour) {
    const auto linear = [](double value) {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(colour.getFloatRed()) + 0.7152 * linear(colour.getFloatGreen())
           + 0.0722 * linear(colour.getFloatBlue());
}

double contrastRatio(juce::Colour a, juce::Colour b) {
    const auto first = luminance(a);
    const auto second = luminance(b);
    return (juce::jmax(first, second) + 0.05) / (juce::jmin(first, second) + 0.05);
}

class HighContrastTests : public juce::UnitTest {
   public:
    HighContrastTests() : juce::UnitTest("HighContrast", "Profiler") {}

    void runTest() override {
        const juce::String caseName("high contrast colours and theme restoration");
        if (requestedTestCase.isNotEmpty() && requestedTestCase != caseName) {
            return;
        }
        beginTest(caseName);
        const juce::ScopedValueSetter<bool> restore(ProfilerStyle::Contrast::enabled, false);
        CustomLookAndFeel lookAndFeel;
        juce::Component root;
        root.setLookAndFeel(&lookAndFeel);
        juce::TextEditor editor;
        editor.setColour(juce::TextEditor::textColourId, ProfilerStyle::Colors::accent);
        editor.setText("Existing text");
        root.addAndMakeVisible(editor);
        const auto original = editor.findColour(juce::TextEditor::textColourId);

        for (int pass = 0; pass < 2; ++pass) {
            ProfilerStyle::Contrast::enabled = true;
            lookAndFeel.refreshTheme(root);
            lookAndFeel.refreshTheme(root); // Repeated refreshes must preserve original overrides.
            const auto black = juce::Colours::black;
            expect(contrastRatio(ProfilerStyle::Contrast::foreground(), black) >= 7.0);
            expect(contrastRatio(ProfilerStyle::Contrast::accent(), black) >= 7.0);
            expect(contrastRatio(ProfilerStyle::Contrast::outline(), black) >= 3.0);
            expect(contrastRatio(editor.findColour(juce::TextEditor::textColourId), black) >= 7.0);
            expect(contrastRatio(editor.findColour(juce::TextEditor::highlightColourId),
                                 editor.findColour(juce::TextEditor::highlightedTextColourId)) >= 7.0);

            const auto render = [](auto&& paint) {
                juce::Image result(juce::Image::RGB, 120, 40, true, juce::SoftwareImageType());
                {
                    juce::Graphics graphics(result);
                    paint(graphics);
                }
                return result;
            };
            const juce::Rectangle<float> buttonBounds(0.0f, 0.0f, 120.0f, 40.0f);
            auto image = render([&](juce::Graphics& graphics) {
                lookAndFeel.paintFlatButtonBackground(graphics, buttonBounds, black, false, false, false);
            });
            expect(image.getPixelAt(1, 20) == juce::Colours::white, "Idle button outlines must be white");
            expect(image.getPixelAt(60, 20) == black);
            image = render([&](juce::Graphics& graphics) {
                lookAndFeel.paintFlatButtonBackground(graphics, buttonBounds, black, false, false, true);
            });
            expect(image.getPixelAt(60, 20) == ProfilerStyle::Contrast::accent(), "Selected buttons must have a yellow background");

            juce::ComboBox combo;
            root.addAndMakeVisible(combo);
            combo.setSize(120, 40);
            image = render([&](juce::Graphics& graphics) {
                lookAndFeel.drawComboBox(graphics, 120, 40, true, 96, 0, 24, 40, combo);
            });
            expect(image.getPixelAt(60, 20) == black, "An open combo must retain a dark background for its white label");

            juce::Label warning;
            warning.setText("Muted", juce::dontSendNotification);
            warning.setSize(120, 40);
            warning.setColour(juce::Label::textColourId, black);
            image = render([&](juce::Graphics& graphics) {
                graphics.fillAll(juce::Colours::yellow);
                lookAndFeel.drawLabel(graphics, warning);
            });
            int darkPixels = 0;
            for (int y = 0; y < image.getHeight(); ++y) {
                for (int x = 0; x < image.getWidth(); ++x) {
                    darkPixels += image.getPixelAt(x, y).getBrightness() < 0.5f ? 1 : 0;
                }
            }
            expect(darkPixels > 0, "Black text on the standalone warning must stay readable");

            juce::TextEditor lateEditor;
            root.addAndMakeVisible(lateEditor);
            expect(lateEditor.findColour(juce::TextEditor::textColourId) == juce::Colours::white);

            ProfilerStyle::Contrast::enabled = false;
            lookAndFeel.refreshTheme(root);
            expect(editor.findColour(juce::TextEditor::textColourId) == original);
            expect(!editor.isColourSpecified(juce::TextEditor::highlightColourId));
            expect(ProfilerStyle::Contrast::outline() == ProfilerStyle::Colors::border);
            expect(ProfilerStyle::Contrast::foreground() == ProfilerStyle::Colors::text);
        }
        root.setLookAndFeel(nullptr);
    }
};
HighContrastTests highContrastTests;
}  // namespace
}  // namespace profiler_tests
