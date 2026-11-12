#include "Components/CustomTabBar.h"

CustomTabBar::CustomTabBar(juce::TabbedButtonBar::Orientation orientation)
    : juce::TabbedButtonBar(orientation) {
}

void CustomTabBar::resized() {
    juce::TabbedButtonBar::resized();

    const int numTabs = getNumTabs();
    if (numTabs == 0) return;

    const int tabW = getWidth() / numTabs;
    const int tabH = getHeight();

    for (int i = 0; i < numTabs; ++i) {
        if (auto* btn = getTabButton(i))
            btn->setBounds(tabW * i, 0, tabW, tabH);
    }
}

void CustomTabBar::currentTabChanged(int newCurrentTabIndex, const juce::String& newCurrentTabName) {
    juce::ignoreUnused(newCurrentTabName);
    DBG("Current tab changed to index: " << newCurrentTabIndex << ", name: " << newCurrentTabName);
    if (onTabChanged) {
        onTabChanged(newCurrentTabIndex);
    }
}
