#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class ToggleControl : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    ToggleControl (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramID,
                   const juce::String& displayName);

    ~ToggleControl() override;

    void resized() override;
    void setAccentColor (juce::Colour);
    void setTooltipText (const juce::String& t);
    void setAccessibility (const juce::String& title, const juce::String& description);

    juce::ToggleButton& getButton() { return button; }

private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
};

} // namespace sk4n_ui
