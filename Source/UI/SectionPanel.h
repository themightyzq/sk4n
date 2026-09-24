#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "SK4nLookAndFeel.h"
#include "DiceButton.h"

namespace sk4n_ui {

class SectionPanel : public juce::Component
{
public:
    SectionPanel (const juce::String& title, juce::Colour accentColor);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setLayout (std::function<void (juce::Rectangle<int>)> layoutFn);
    juce::Rectangle<int> getContentBounds() const;

    juce::Colour getAccent() const { return accent; }
    void setHeaderRight (const juce::String& text);

    // Adds a small dice icon to the section header. Callback receives `constrained` (true on shift-click).
    void setDiceCallback (std::function<void (bool constrained)> cb);
    void setDiceTooltip  (const juce::String& tooltip);

    static constexpr int kHeaderH      = 26;  // Pass 9: taller to accommodate larger dice button
    static constexpr int kContentPadX  = 12;  // >= 6px house minimum inset from the panel face
    static constexpr int kContentPadY  = 10;  // >= 8px house minimum so readouts never touch the border
    static constexpr int kAccentBarW   = 3;
    static constexpr int kDiceSize     = 22;

private:
    juce::String title;
    juce::String headerRight;
    juce::Colour accent;
    std::function<void (juce::Rectangle<int>)> layoutFn;

    std::unique_ptr<DiceButton> diceButton;
};

} // namespace sk4n_ui
