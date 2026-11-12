#pragma once

#include <JuceHeader.h>

#include "UiSettings.h"

// Scroll an untransformed canvas so scrollbar ranges remain in window pixels.
// Only its child is scaled, preserving the host's own editor/DPI transform.
class ScalableViewport : public juce::Viewport {
   public:
    ScalableViewport(int minimumWidth, int minimumHeight)
        : _minimumWidth(minimumWidth), _minimumHeight(minimumHeight) {
        _canvas.addAndMakeVisible(_content);
        setViewedComponent(&_canvas, false);
        setScrollBarsShown(true, true);
        setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    }

    ~ScalableViewport() override {
        setViewedComponent(nullptr, false);
    }

    juce::Component& getContent() { return _content; }

    void setScalePercent(int percent) {
        _scale = UiSettings::normaliseInterfaceScalePercent(percent) / 100.0f;
        _content.setTransform(juce::AffineTransform::scale(_scale));
        resized();
        setViewPosition(0, 0);
    }

    void resized() override {
        juce::Viewport::resized();

        const int minimumPixelWidth = juce::roundToInt(_minimumWidth * _scale);
        const int minimumPixelHeight = juce::roundToInt(_minimumHeight * _scale);
        int availableWidth = getWidth();
        int availableHeight = getHeight();
        // A scrollbar on one axis can make the other axis overflow too.
        for (int pass = 0; pass < 2; ++pass) {
            availableWidth = getWidth() - (minimumPixelHeight > availableHeight ? getScrollBarThickness() : 0);
            availableHeight = getHeight() - (minimumPixelWidth > availableWidth ? getScrollBarThickness() : 0);
        }

        const int logicalWidth = juce::jmax(_minimumWidth, static_cast<int>(availableWidth / _scale));
        const int logicalHeight = juce::jmax(_minimumHeight, static_cast<int>(availableHeight / _scale));
        _content.setBounds(0, 0, logicalWidth, logicalHeight);
        _canvas.setSize(juce::roundToInt(logicalWidth * _scale),
                        juce::roundToInt(logicalHeight * _scale));
    }

   private:
    const int _minimumWidth;
    const int _minimumHeight;
    float _scale = 1.0f;
    juce::Component _canvas;
    juce::Component _content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScalableViewport)
};
