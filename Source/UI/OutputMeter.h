#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class OutputMeter : public juce::Component, private juce::Timer
{
public:
    OutputMeter (std::function<float()> getPeakL,
                 std::function<float()> getPeakR);
    ~OutputMeter() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    std::function<float()> peakL;
    std::function<float()> peakR;
    bool  clipped    = false;
    int   clipFrames = 0;
    float displayedL = 0.0f;
    float displayedR = 0.0f;

    static constexpr int kSegments    = 15;
    static constexpr int kClipFrames  = 60; // ~2 s at 30 Hz
};

} // namespace sk4n_ui
