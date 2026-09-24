#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class DiceButton : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    DiceButton();
    ~DiceButton() override = default;

    void setOnClick            (std::function<void()> cb) { onClickCb       = std::move (cb); }
    void setOnConstrainedClick (std::function<void()> cb) { onConstrainedCb = std::move (cb); }

    void setTooltipText (const juce::String& t);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    bool hovered = false;
    int  flashCounter = 0;
    static constexpr int kFlashFrames = 5;

    std::function<void()> onClickCb;
    std::function<void()> onConstrainedCb;
};

} // namespace sk4n_ui
