#pragma once

#include <JuceHeader.h>

class FormField : public juce::Component {
   public:
    FormField(const juce::Identifier& fieldId, const juce::String& labelText);
    ~FormField() override = default;

    const juce::Identifier& getFieldId() const noexcept;
    juce::String getLabelText() const;
    void setLabelText(const juce::String& text);
    void setLabelWidth(int width);
    int getLabelWidth() const noexcept;

    virtual int getPreferredHeight() const;
    virtual juce::var getValue() const = 0;
    virtual void setValue(const juce::var& value) = 0;

    void resized() override;

    std::function<void(FormField&)> onValueChanged;

   protected:
    void notifyValueChanged(const juce::String& statusMessage = {});
    virtual void resizedControl(juce::Rectangle<int> controlArea) = 0;

   private:
    juce::Identifier _fieldId;
    juce::Label _label;
    int _labelWidth = 132;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FormField)
};
