#include "ModMeter.h"

namespace sk4n_ui {

ModMeter::ModMeter (Mode m, std::function<float()> s, juce::Colour a)
    : mode (m), source (std::move (s)), accent (a)
{
    setOpaque (false);

    // Accessibility floor (product spec item 8): a custom component showing data gets
    // setAccessible(true) plus a title and description.
    setAccessible (true);
    setTitle ("Modulation level");
    setDescription (mode == Mode::Bipolar ? "Live bipolar modulation level, centred at zero."
                                          : "Live modulation level.");

    startTimerHz (30);
}

void ModMeter::timerCallback()
{
    const float v = source ? source() : 0.0f;
    if (std::fabs (v - lastValue) > 0.005f)
    {
        lastValue = v;
        repaint();
    }
    if (flashCounter > 0)
    {
        --flashCounter;
        repaint();
    }
}

// One of the style guide's named "Displays" (BufferDisplay, FilterResponseDisplay,
// EnvelopeMeter, ModMeter, OutputMeter): phosphor screen background, hard edges, no rounded
// corners.
void ModMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    zqsfx::ui::LookAndFeel::drawScreen (g, b, false);

    auto inner = b.reduced (2.0f);
    if (mode == Mode::Unipolar)
    {
        const float v = juce::jlimit (0.0f, 1.0f, lastValue);
        const float w = inner.getWidth() * v;
        if (w > 0.5f)
        {
            g.setColour (accent.withAlpha (0.85f));
            g.fillRect (inner.withWidth (w));
        }
    }
    else
    {
        const float v = juce::jlimit (-1.0f, 1.0f, lastValue);
        const float halfW = inner.getWidth() * 0.5f;
        const float centerX = inner.getX() + halfW;

        // center tick
        g.setColour (pal::gridLine);
        g.fillRect (juce::Rectangle<float> (centerX - 0.5f, inner.getY(), 1.0f, inner.getHeight()));

        const float w = std::fabs (v) * halfW;
        if (w > 0.5f)
        {
            juce::Rectangle<float> bar = (v >= 0.0f)
                ? juce::Rectangle<float> (centerX, inner.getY(), w, inner.getHeight())
                : juce::Rectangle<float> (centerX - w, inner.getY(), w, inner.getHeight());
            g.setColour (accent.withAlpha (0.85f));
            g.fillRect (bar);
        }
    }

    // Flash dot
    if (showFlash && flashCounter > 0)
    {
        const float alpha = static_cast<float> (flashCounter) / static_cast<float> (flashFrames);
        const float r = inner.getHeight() * 0.6f;
        g.setColour (pal::accentWarn.withAlpha (alpha));
        g.fillEllipse (inner.getRight() - r - 1.0f,
                       inner.getY() + (inner.getHeight() - r) * 0.5f,
                       r, r);
    }
}

} // namespace sk4n_ui
