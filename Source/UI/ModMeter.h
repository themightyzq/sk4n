#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class ModMeter : public juce::Component, private juce::Timer
{
public:
    enum class Mode { Unipolar, Bipolar };

    ModMeter (Mode mode, std::function<float()> source, juce::Colour accent);

    void setAccentColor (juce::Colour c) { accent = c; repaint(); }
    void setShowFlash (bool b) { showFlash = b; }
    void triggerFlash() { flashCounter = flashFrames; }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    Mode mode;
    std::function<float()> source;
    juce::Colour accent;
    float lastValue = 0.0f;
    bool  showFlash = false;
    int   flashCounter = 0;
    static constexpr int flashFrames = 6;
};

} // namespace sk4n_ui
