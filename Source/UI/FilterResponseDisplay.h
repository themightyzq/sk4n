#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class FilterResponseDisplay : public juce::Component,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::Timer
{
public:
    FilterResponseDisplay (juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& centerID,
                           const juce::String& gapID,
                           const juce::String& resonID,
                           const juce::String& balanceID,
                           const juce::String& modeID,
                           const juce::String& cabDriveID,
                           const juce::String& cabTiltID,
                           const juce::String& cabFoldID);

    ~FilterResponseDisplay() override;

    void paint (juce::Graphics&) override;

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void paint8P (juce::Graphics&, juce::Rectangle<float>);
    void paintCabinet (juce::Graphics&, juce::Rectangle<float>);
    static float magnitudeAt (float freq, float center, float gap, float reso, float balance);

    juce::AudioProcessorValueTreeState& apvts;
    juce::String idCenter, idGap, idReson, idBalance, idMode;
    juce::String idCabDrive, idCabTilt, idCabFold;
    bool dirty = true;
};

} // namespace sk4n_ui
