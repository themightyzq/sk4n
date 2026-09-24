#include "ChoiceControl.h"

namespace sk4n_ui {

ChoiceControl::ChoiceControl (juce::AudioProcessorValueTreeState& apvts,
                              const juce::String& paramID,
                              const juce::String& displayName,
                              const juce::StringArray& items)
{
    nameLabel.setText (displayName, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font::label (11.0f));
    nameLabel.setColour (juce::Label::textColourId, pal::textSecondary);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    box.addItemList (items, 1);
    box.setWantsKeyboardFocus (true);
    box.setHasFocusOutline (true); // house LookAndFeel draws the ring automatically
    addAndMakeVisible (box);

    attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, box);

    // Accessibility floor baseline (see KnobControl.cpp for the full rationale): every ComboBox
    // gets a title/description/help/tooltip even when the call site never calls
    // setTooltipText()/setAccessibility() explicitly.
    const auto accessibleName = displayName.isNotEmpty() ? displayName : paramID;
    box.setTitle (accessibleName);
    box.setDescription (accessibleName);
    box.setHelpText (accessibleName);
    box.setTooltip (accessibleName);
    setTitle (displayName);
}

ChoiceControl::~ChoiceControl() = default;

void ChoiceControl::resized()
{
    auto area = getLocalBounds();
    nameLabel.setBounds (area.removeFromTop (14));
    box.setBounds (area.reduced (2, 2).withSizeKeepingCentre (
        juce::jmin (area.getWidth() - 4, 100),
        juce::jmin (area.getHeight() - 4, 28)));
}

void ChoiceControl::setAccentColor (juce::Colour) {}

void ChoiceControl::setTooltipText (const juce::String& t)
{
    box.setTooltip (t);
    box.setDescription (t);
    box.setHelpText (t);
    setTooltip (t);
}

void ChoiceControl::setAccessibility (const juce::String& title, const juce::String& description)
{
    box.setTitle (title);
    box.setDescription (description);
    box.setHelpText (description);
    setTitle (title);
    setDescription (description);
}

} // namespace sk4n_ui
