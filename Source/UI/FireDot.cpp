#include "FireDot.h"

namespace sk4n_ui {

FireDot::FireDot()
{
    setOpaque (false);
    setTitle ("Trigger fire indicator");
    setDescription ("Flashes when an envelope retrigger fires.");
    startTimerHz (kFrameRate);
}

void FireDot::trigger() { countdownFrames = kFlashFrames; }

void FireDot::timerCallback()
{
    if (countdownFrames > 0)
    {
        --countdownFrames;
        repaint();
    }
}

// A round status LED (style guide section 6: "LEDs: round, accent with halo when lit,
// ledOff* when not"). Was accentWarn while flashing -- a trigger firing is normal activity, not
// an error, so it now matches the house's own LED idiom (accent + soft halo when lit) instead of
// the reserved clip/error red. See ui_migration_report.md "Deviations".
void FireDot::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float diam = juce::jmin (b.getWidth(), b.getHeight()) - 2.0f;
    juce::Rectangle<float> r (b.getCentreX() - diam * 0.5f,
                              b.getCentreY() - diam * 0.5f,
                              diam, diam);

    if (countdownFrames > 0)
    {
        const float alpha = (float) countdownFrames / (float) kFlashFrames;
        g.setColour (pal::accentPrimary.withAlpha (juce::jmax (0.35f, alpha)));
        g.fillEllipse (r.expanded (3.0f)); // soft halo, matching zqsfx::ui::Led
        g.setColour (pal::accentPrimary);
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (r, 1.0f);
    }
    else
    {
        g.setColour (pal::ledOff);
        g.fillEllipse (r);
        g.setColour (pal::ledOffRing);
        g.drawEllipse (r, 1.0f);
    }
}

} // namespace sk4n_ui
