#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

// Static ADBDSR shape preview based on current parameter values.
// Listens to its 6 parameters and repaints on change.
class EnvelopeMeter : public juce::Component,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    EnvelopeMeter (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& aID,
                   const juce::String& d1ID,
                   const juce::String& bID,
                   const juce::String& d2ID,
                   const juce::String& sID,
                   const juce::String& rID,
                   juce::Colour accent);

    ~EnvelopeMeter() override;

    void setAccentColor (juce::Colour c) { accent = c; repaint(); }
    void paint (juce::Graphics&) override;

private:
    void parameterChanged (const juce::String&, float) override;

    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray ids;
    juce::Colour      accent;
};

} // namespace sk4n_ui
