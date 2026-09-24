#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class DisclosureRow : public juce::Component
{
public:
    explicit DisclosureRow (const juce::String& title);

    void setExpanded (bool e);
    bool isExpanded() const { return expanded; }
    void setOnToggle (std::function<void (bool)> cb) { onToggle = std::move (cb); }

    void paint (juce::Graphics&) override;
    void mouseDown   (const juce::MouseEvent&) override;
    void mouseEnter  (const juce::MouseEvent&) override;
    void mouseExit   (const juce::MouseEvent&) override;

private:
    juce::String title;
    bool expanded = false;
    bool hovered  = false;
    std::function<void (bool)> onToggle;
};

} // namespace sk4n_ui
