#include "BufferDisplay.h"
#include "../PluginProcessor.h"

#include <cmath>

namespace sk4n_ui {

BufferDisplay::BufferDisplay (SK4nAudioProcessor& p) : processor (p)
{
    setOpaque (true);
    startTimerHz (30);
    setTitle ("Buffer view");
    setDescription ("Live recording buffer. Orange marker is the write head; A and B markers are the two oscillator read positions.");
}

// One of the style guide's named "Displays": phosphor screen background (bezel + glass +
// scanlines -- this is the largest display in the plugin, so scanlines are on), hard edges.
void BufferDisplay::paint (juce::Graphics& g)
{
    zqsfx::ui::LookAndFeel::drawScreen (g, getLocalBounds().toFloat(), true);
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // Everything below still computes content in a 0-based (W x H) coordinate frame exactly as
    // it did before the screen bezel was added; translate the graphics context by the glass
    // inset instead of touching every coordinate below.
    juce::Graphics::ScopedSaveState glassInset (g);
    g.addTransform (juce::AffineTransform::translation (bounds.getX(), bounds.getY()));
    bounds = bounds.withPosition (0.0f, 0.0f);

    const int    activeSize = processor.getActiveSize();
    const float* data       = processor.getBufferData();
    const int    W = static_cast<int> (bounds.getWidth());
    const int    H = static_cast<int> (bounds.getHeight());

    // Diagnostic readout in the TOP-right of the waveform area (away from the
    // bottom time-scale labels). Always visible -- invaluable when debugging
    // whether audio is actually flowing into the buffer.
    auto drawDiagnostic = [&] (const juce::String& msg)
    {
        g.setColour (pal::textDim);
        g.setFont (font::diagnostic (9.0f, &getLookAndFeel()));
        // wfArea may not exist yet at the early-return path -- guard for that case.
        const auto target = (W >= 4 && H >= 4)
            ? juce::Rectangle<float> (bounds.getX(),
                                      bounds.getY(),
                                      bounds.getWidth(),
                                      juce::jmin ((float) H - 14.0f, bounds.getHeight()))
            : bounds;
        g.drawText (msg,
                    target.toNearestInt().reduced (8, 4),
                    juce::Justification::topRight, false);
    };

    if (activeSize <= 0 || data == nullptr || W < 4 || H < 4)
    {
        g.setColour (pal::textDim);
        g.setFont (font::diagnostic (9.0f, &getLookAndFeel()));
        g.drawText ("waiting for audio...",
                    bounds.toNearestInt(),
                    juce::Justification::centred, false);
        drawDiagnostic (juce::String::formatted ("active=%d  data=%p  size=%dx%d",
                                                  activeSize, (const void*) data, W, H));
        return;
    }

    constexpr int scaleH = 14;
    auto wfArea    = bounds.withTrimmedBottom ((float) scaleH);
    auto scaleArea = bounds.withTop (bounds.getBottom() - (float) scaleH);

    const float midY = wfArea.getCentreY();
    const float amp  = wfArea.getHeight() * 0.45f;
    const float bufferLengthSec = static_cast<float> (activeSize)
                                / static_cast<float> (juce::jmax (1.0, processor.getCurrentSampleRate()));

    // Centre line first (so the waveform draws on top) -- a grid line on the phosphor screen.
    g.setColour (pal::gridLine.withAlpha (0.7f));
    g.drawHorizontalLine ((int) midY, bounds.getX(), bounds.getRight());

    // Compute peak per X bin and overall peak (for the diagnostic).
    const int binSize = juce::jmax (1, activeSize / juce::jmax (1, W));
    std::vector<float> binPeaks;
    binPeaks.reserve ((size_t) W);
    float overallPeak = 0.0f;
    for (int x = 0; x < W; ++x)
    {
        const int start = (activeSize * x) / W;
        const int end   = juce::jmin (activeSize, start + binSize);
        float pk = 0.0f;
        for (int i = start; i < end; ++i)
        {
            const float s = std::fabs (data[i]);
            if (s > pk) pk = s;
        }
        binPeaks.push_back (pk);
        if (pk > overallPeak) overallPeak = pk;
    }

    // If we got actual signal, draw the filled waveform.
    if (overallPeak > 1.0e-5f)
    {
        juce::Path top, bot;
        top.startNewSubPath (0.0f, midY);
        bot.startNewSubPath (0.0f, midY);
        for (int x = 0; x < W; ++x)
        {
            const float h = binPeaks[(size_t) x] * amp;
            top.lineTo ((float) x, midY - h);
            bot.lineTo ((float) x, midY + h);
        }
        top.lineTo ((float) W, midY);
        bot.lineTo ((float) W, midY);
        top.closeSubPath();
        bot.closeSubPath();

        // Solid-colour fill — gradients across thin paths can render invisibly on
        // some macOS Metal paths, so be defensive here.
        g.setColour (pal::accentPrimary.withAlpha (0.55f));
        g.fillPath (top);
        g.fillPath (bot);

        g.setColour (pal::accentPrimary);
        g.strokePath (top, juce::PathStrokeType (1.0f));
        g.strokePath (bot, juce::PathStrokeType (1.0f));
    }
    else
    {
        // Visible placeholder so the user knows the display is alive even with no signal.
        g.setColour (pal::textDim.withAlpha (0.6f));
        g.setFont (font::diagnostic (9.0f, &getLookAndFeel()));
        g.drawText ("(silent - no signal in buffer)",
                    wfArea.toNearestInt(),
                    juce::Justification::centred, false);
    }

    // ----- Markers -----
    auto markerX = [&] (float samplesFromStart)
    {
        return (samplesFromStart / static_cast<float> (activeSize)) * (float) W;
    };

    const int writeIdx = processor.getWriteIndex();
    const float wxF = markerX ((float) writeIdx);
    g.setColour (pal::accentPrimary);
    g.fillRect (juce::Rectangle<float> (wxF - 1.0f, wfArea.getY(), 2.0f, wfArea.getHeight()));
    juce::Path wTri;
    wTri.addTriangle (wxF - 4.0f, wfArea.getY(),
                      wxF + 4.0f, wfArea.getY(),
                      wxF,         wfArea.getY() + 6.0f);
    g.fillPath (wTri);

    auto drawReader = [&] (float posBehind, juce::Colour c, const juce::String& tag)
    {
        if (activeSize <= 0) return;
        float readSamp = static_cast<float> (writeIdx) - posBehind;
        while (readSamp < 0.0f)                              readSamp += static_cast<float> (activeSize);
        while (readSamp >= static_cast<float> (activeSize)) readSamp -= static_cast<float> (activeSize);
        const float rxF = markerX (readSamp);

        g.setColour (c);
        g.fillRect (juce::Rectangle<float> (rxF - 0.75f, wfArea.getY(), 1.5f, wfArea.getHeight()));

        const float tagW = 14.0f, tagH = 12.0f;
        juce::Rectangle<float> tagRect (rxF - tagW * 0.5f, wfArea.getY(), tagW, tagH);
        g.setColour (c);
        g.fillRect (tagRect);
        g.setColour (pal::bgBase);
        g.setFont (font::diagnostic (9.0f, &getLookAndFeel()));
        g.drawText (tag, tagRect.toNearestInt(), juce::Justification::centred, false);
    };
    drawReader (processor.getCurrentReadPosA(), pal::accentOscA, "A");
    drawReader (processor.getCurrentReadPosB(), pal::accentOscB, "B");

    // FROZEN badge. Was accentWarn (house `warn` red, reserved for clip/error): the very same
    // boolean state is also shown by the "Freeze" toggle a few pixels below in the Position
    // panel, which -- like every house on-state toggle -- fills accent orange. Two different
    // colours for the identical state would contradict each other, so this now matches the
    // toggle's own accent fill instead of red. See ui_migration_report.md "Deviations".
    if (auto* p = processor.apvts.getRawParameterValue ("freeze"))
    {
        if (p->load() >= 0.5f)
        {
            const float bw = 70.0f, bh = 16.0f;
            auto bRect = juce::Rectangle<float> (bounds.getRight() - bw - 8.0f,
                                                 bounds.getY() + 6.0f, bw, bh);
            g.setColour (pal::accentPrimary.withAlpha (0.30f));
            g.fillRect (bRect);
            g.setColour (pal::accentPrimary);
            g.drawRect (bRect, 1.0f);
            g.setFont (font::sectionHeader (9.5f, &getLookAndFeel()));
            g.drawText ("FROZEN", bRect.toNearestInt(), juce::Justification::centred, false);
        }
    }

    // Time scale ticks -- a grid line on the phosphor screen.
    g.setColour (pal::gridLine);
    g.drawHorizontalLine ((int) scaleArea.getY(), 0.0f, (float) W);
    g.setColour (pal::textDim);
    g.setFont (font::diagnostic (8.5f, &getLookAndFeel()));
    const float tickEvery = bufferLengthSec >= 4.0f ? 1.0f
                          : bufferLengthSec >= 1.5f ? 0.5f
                          : 0.25f;
    for (float t = 0.0f; t <= bufferLengthSec + 1.0e-3f; t += tickEvery)
    {
        const float fx = (t / bufferLengthSec) * (float) W;
        g.fillRect (juce::Rectangle<float> (fx - 0.5f, scaleArea.getY(), 1.0f, 4.0f));
        const juce::String label = (t == 0.0f) ? juce::String ("0")
                                               : juce::String (t, t < 1.0f ? 2 : 1) + "s";
        g.drawText (label,
                    juce::Rectangle<float> (fx - 30.0f, scaleArea.getY() + 3.0f, 60.0f, scaleH - 4.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }

    // Diagnostic in bottom-right (small, dim, but always present so audio-flow
    // problems are visible without looking at logs).
    drawDiagnostic (juce::String::formatted ("wIdx %d / %d  pk %.3f",
                                              writeIdx, activeSize, overallPeak));
}

} // namespace sk4n_ui
