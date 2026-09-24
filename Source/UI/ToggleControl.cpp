#include "ToggleControl.h"

namespace sk4n_ui {

ToggleControl::ToggleControl (juce::AudioProcessorValueTreeState& apvts,
                              const juce::String& paramID,
                              const juce::String& displayName)
{
    button.setButtonText (displayName);
    button.setClickingTogglesState (true);
    button.setWantsKeyboardFocus (true);
    button.setHasFocusOutline (true); // house LookAndFeel draws the ring automatically
    addAndMakeVisible (button);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramID, button);

    // Accessibility floor baseline (see KnobControl.cpp for the full rationale): every
    // ToggleButton gets a title/description/help/tooltip even when the call site never calls
    // setTooltipText()/setAccessibility() explicitly.
    const auto accessibleName = displayName.isNotEmpty() ? displayName : paramID;
    button.setTitle (accessibleName);
    button.setDescription (accessibleName);
    button.setHelpText (accessibleName);
    button.setTooltip (accessibleName);
    setTitle (displayName);
}

ToggleControl::~ToggleControl() = default;

void ToggleControl::resized()
{
    auto area = getLocalBounds().reduced (2);
    button.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 80),
                                                   juce::jmin (area.getHeight(), 24)));
}

void ToggleControl::setAccentColor (juce::Colour c)
{
    button.setColour (juce::ToggleButton::tickColourId, c);
    button.setColour (juce::TextButton::buttonOnColourId, c.withAlpha (0.30f));
    repaint();
}

void ToggleControl::setTooltipText (const juce::String& t)
{
    button.setTooltip (t);
    button.setDescription (t);
    button.setHelpText (t);
    setTooltip (t);
}

void ToggleControl::setAccessibility (const juce::String& title, const juce::String& description)
{
    button.setTitle (title);
    button.setDescription (description);
    button.setHelpText (description);
    setTitle (title);
    setDescription (description);
}

} // namespace sk4n_ui
