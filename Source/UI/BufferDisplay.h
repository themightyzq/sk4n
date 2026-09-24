#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

class SK4nAudioProcessor;

namespace sk4n_ui {

class BufferDisplay : public juce::Component, private juce::Timer
{
public:
    explicit BufferDisplay (SK4nAudioProcessor& p);
    ~BufferDisplay() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    SK4nAudioProcessor& processor;
};

} // namespace sk4n_ui
