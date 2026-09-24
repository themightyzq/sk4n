#include "OutputMeter.h"

#include <cmath>

namespace sk4n_ui {

OutputMeter::OutputMeter (std::function<float()> l, std::function<float()> r)
    : peakL (std::move (l)), peakR (std::move (r))
{
    setOpaque (false);
    setTitle ("Output meter");
    setDescription ("Stereo peak meter. Click to clear clip indicator.");
    startTimerHz (30);
}

void OutputMeter::timerCallback()
{
    const float pL = peakL ? peakL() : 0.0f;
    const float pR = peakR ? peakR() : 0.0f;
    displayedL = juce::jmax (pL, displayedL * 0.85f);
    displayedR = juce::jmax (pR, displayedR * 0.85f);

    const float pk = juce::jmax (displayedL, displayedR);
    if (pk >= 0.999f)
    {
        clipped = true;
        clipFrames = kClipFrames;
    }
    if (clipFrames > 0) --clipFrames;

    repaint();
}

void OutputMeter::mouseDown (const juce::MouseEvent&)
{
    clipped    = false;
    clipFrames = 0;
    repaint();
}

static float dbToSegments (float peak, int kSegments)
{
    if (peak <= 0.0f) return 0.0f;
    const float dB = 20.0f * std::log10 (peak);
    return juce::jlimit (0.0f, (float) kSegments,
                         (dB + 36.0f) / 42.0f * (float) kSegments);
}

// One of the style guide's named "Displays": phosphor screen well, hard edges, gradient
// anchored to fixed dB with amber above -6 dB and red at clip -- style guide section 6's exact
// meter description, using the house's own meterLo/Mid/Hi/Hot/Clip stops instead of a flat
// accentPrimary/accentWarn split.
void OutputMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Only reserve space for the CLIP label when it's actually lit. When
    // inactive, the segments use the full meter height.
    const bool clipActive = clipFrames > 0;
    if (clipActive)
    {
        juce::Rectangle<float> clipR (b.getX(), b.getY(), b.getWidth(), 12.0f);
        b.removeFromTop (12.0f);
        g.setColour (pal::accentWarn); // real clip -- the one legitimate use of house `warn` red
        g.fillRect (clipR.reduced (1.0f, 1.0f));
        g.setColour (pal::bgBase);
        g.setFont (font::diagnostic (8.0f, &getLookAndFeel()));
        g.drawText ("CLIP", clipR.toNearestInt(),
                    juce::Justification::centred, false);
    }

    zqsfx::ui::LookAndFeel::drawScreen (g, b, false);
    b = b.reduced (2.0f);

    // Two-bar (L | R) segmented meter
    const float segH = b.getHeight() / (float) kSegments;
    const float gap  = 1.0f;

    const float barW = (b.getWidth() - 2.0f) * 0.5f;
    auto leftBar  = juce::Rectangle<float> (b.getX(),                        b.getY(), barW, b.getHeight());
    auto rightBar = juce::Rectangle<float> (b.getX() + barW + 2.0f,          b.getY(), barW, b.getHeight());

    const float litL = dbToSegments (displayedL, kSegments);
    const float litR = dbToSegments (displayedR, kSegments);

    auto drawBar = [&] (juce::Rectangle<float> bar, float lit)
    {
        for (int i = 0; i < kSegments; ++i)
        {
            const float y = bar.getBottom() - (i + 1) * segH;
            juce::Rectangle<float> seg (bar.getX(), y + gap,
                                        bar.getWidth(), segH - 2.0f * gap);
            juce::Colour col;
            if ((float) i < lit)
            {
                // Bottom -> top: meterLo, meterMid, meterHi, meterHot for the top two segments
                // (the style guide's "amber above -6 dB"), matching zqsfx::ui::PeakMeter's own
                // fixed-dB-anchored stops.
                if (i >= kSegments - 2)      col = zqsfx::ui::colour::meterHot;
                else if (i >= kSegments * 7 / 10) col = zqsfx::ui::colour::meterHi;
                else if (i >= kSegments / 3) col = zqsfx::ui::colour::meterMid;
                else                          col = zqsfx::ui::colour::meterLo;
            }
            else col = pal::ledOff;
            g.setColour (col);
            g.fillRect (seg);
        }
    };
    drawBar (leftBar,  litL);
    drawBar (rightBar, litR);
}

} // namespace sk4n_ui
