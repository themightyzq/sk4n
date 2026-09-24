#include "HelpOverlay.h"

namespace sk4n_ui {

HelpOverlay::HelpOverlay()
{
    // Allow knobs underneath to remain interactive — overlay is purely informational.
    setInterceptsMouseClicks (false, false);
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);
}

void HelpOverlay::setItems (std::vector<Item> i)
{
    items = std::move (i);
    repaint();
}

void HelpOverlay::paint (juce::Graphics& g)
{
    // Dim the world.
    g.fillAll (pal::helpScrim);

    g.setFont (font::title (14.0f, &getLookAndFeel()));
    for (const auto& it : items)
    {
        if (it.bounds.isEmpty()) continue;

        // Compute label box at the centroid of the section.
        const auto centre = it.bounds.getCentre();
        const int  textW  = juce::jmax (180, 8 + (int) g.getCurrentFont().getStringWidthFloat (it.label));
        const int  textH  = 28;
        juce::Rectangle<int> labelBox (centre.x - textW / 2, centre.y - textH / 2, textW, textH);

        // Hard-edged label chip (style guide section 6: no rounded corners).
        g.setColour (pal::bgPanelHi);
        g.fillRect (labelBox);
        g.setColour (pal::accentPrimary);
        g.drawRect (labelBox.toFloat(), 1.5f);

        g.setColour (pal::textPrimary);
        g.drawText (it.label, labelBox.reduced (8, 4),
                    juce::Justification::centred, false);
    }

    // Hint at the bottom centre.
    g.setColour (pal::textPrimary);
    g.setFont (font::diagnostic (10.5f, &getLookAndFeel()));
    g.drawText ("Click ? again, or press Escape, to dismiss",
                getLocalBounds().removeFromBottom (32).reduced (4),
                juce::Justification::centred, false);
}

bool HelpOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        setVisible (false);
        return true;
    }
    return false;
}

} // namespace sk4n_ui
