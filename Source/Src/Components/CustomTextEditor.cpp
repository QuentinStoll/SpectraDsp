#include "Components/CustomTextEditor.h"

#include "Stylesheet.h"

CustomTextEditor::CustomTextEditor() {
    setMultiLine(false);
    setReturnKeyStartsNewLine(false);
    setScrollbarsShown(false);
    setSelectAllWhenFocused(true);
    setJustification(juce::Justification::centredLeft);
    setIndents(8, 0);
    setFont(ProfilerStyle::Fonts::regular(15.0f));
}
