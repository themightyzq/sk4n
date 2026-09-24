#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

class ChoiceControl : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    ChoiceControl (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramID,
                   const juce::String& displayName,
                   const juce::StringArray& items);

    ~ChoiceControl() override;

    void resized() override;
    void setAccentColor (juce::Colour);
    void setTooltipText (const juce::String& t);
    void setAccessibility (const juce::String& title, const juce::String& description);

    juce::ComboBox& getBox() { return box; }

private:
    juce::Label    nameLabel;
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
};

} // namespace sk4n_ui
