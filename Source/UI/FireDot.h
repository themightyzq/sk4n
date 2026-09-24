#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

// Small circular indicator that flashes when trigger() is called and fades over ~200 ms.
class FireDot : public juce::Component, private juce::Timer
{
public:
    FireDot();
    ~FireDot() override = default;

    void paint (juce::Graphics&) override;
    void trigger();

private:
    void timerCallback() override;

    int   countdownFrames = 0;
    static constexpr int kFrameRate = 30;
    static constexpr int kFlashFrames = 6; // 200 ms at 30 Hz
};

} // namespace sk4n_ui
