#include "DiceButton.h"

namespace sk4n_ui {

DiceButton::DiceButton()
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Randomize");
    startTimerHz (30);
}

void DiceButton::setTooltipText (const juce::String& t)
{
    setTooltip (t);
    setDescription (t);
    setHelpText (t);
}

void DiceButton::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void DiceButton::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }

void DiceButton::mouseDown (const juce::MouseEvent& e)
{
    flashCounter = kFlashFrames;
    repaint();
    if (e.mods.isShiftDown())
    {
        if (onConstrainedCb) onConstrainedCb();
    }
    else
    {
        if (onClickCb) onClickCb();
    }
}

void DiceButton::timerCallback()
{
    if (flashCounter > 0)
    {
        --flashCounter;
        if (flashCounter == 0) repaint();
    }
}

// A hard-edged button (style guide section 6: no rounded corners); the house has no bespoke
// "dice" concept, so this stays entirely custom-painted, restyled with house tokens. The click
// flash was accentWarn (house `warn`, reserved for clip/error) -- a successful randomize click
// is not an error, so this now flashes the house accent instead, a stronger version of the same
// accent already used on hover. See ui_migration_report.md "Deviations".
void DiceButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    juce::Colour fill = pal::bgPanelHi;
    if (flashCounter > 0)  fill = pal::accentPrimary.withAlpha (0.45f);
    else if (hovered)      fill = pal::bgPanelHi.brighter (0.10f);

    g.setColour (fill);
    g.fillRect (b);

    juce::Colour border = (flashCounter > 0 || hovered) ? pal::accentPrimary : pal::borderSubtle;
    g.setColour (border);
    g.drawRect (b, 1.0f);

    // Five-pip die face
    juce::Colour pip = hovered ? pal::accentPrimary : pal::textSecondary;
    if (flashCounter > 0) pip = pal::textPrimary;
    g.setColour (pip);

    const float dotSz = juce::jmax (1.5f, b.getWidth() * 0.10f);
    auto centre = b.getCentre();
    const float off = b.getWidth() * 0.22f;
    const juce::Point<float> dots[] = {
        { centre.x - off, centre.y - off },
        { centre.x + off, centre.y - off },
        { centre.x,       centre.y       },
        { centre.x - off, centre.y + off },
        { centre.x + off, centre.y + off }
    };
    for (auto& d : dots)
        g.fillEllipse (d.x - dotSz * 0.5f, d.y - dotSz * 0.5f, dotSz, dotSz);
}

} // namespace sk4n_ui
