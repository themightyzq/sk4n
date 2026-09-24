#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class HelpOverlay : public juce::Component
{
public:
    struct Item
    {
        juce::String label;
        juce::Rectangle<int> bounds;
    };

    HelpOverlay();
    ~HelpOverlay() override = default;

    void setItems (std::vector<Item> items);
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    std::vector<Item> items;
};

} // namespace sk4n_ui
