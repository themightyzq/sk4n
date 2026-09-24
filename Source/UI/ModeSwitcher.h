#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

// Two-segment pill switch attached to a Choice parameter (index 0/1).
class ModeSwitcher : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::Timer
{
public:
    ModeSwitcher (juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramID,
                  const juce::String& leftLabel,
                  const juce::String& rightLabel);

    ~ModeSwitcher() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int  getCurrentIndex() const;

    // Fired on the message thread whenever the index actually changes, from a user click or from
    // host automation alike. Owners use it to show/hide the control set each mode owns.
    // parameterChanged can arrive on the audio thread, so the notification is deferred to the
    // timer rather than sent from there.
    std::function<void (int)> onModeChanged;

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void setIndexFromUser (int idx);

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    juce::String leftLabel, rightLabel;

    int   targetIdx       = 0;
    int   lastNotifiedIdx = -1;   // -1 so the first timer tick always publishes the initial mode
    float displayedIdx    = 0.0f;
};

} // namespace sk4n_ui
