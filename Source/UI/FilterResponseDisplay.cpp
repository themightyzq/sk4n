#include "FilterResponseDisplay.h"

#include <cmath>

namespace sk4n_ui {

namespace {
constexpr float kMinHz = 20.0f;
constexpr float kMaxHz = 20000.0f;
constexpr float kMinDb = -36.0f;
constexpr float kMaxDb = 6.0f;
}

FilterResponseDisplay::FilterResponseDisplay (juce::AudioProcessorValueTreeState& a,
                                              const juce::String& centerID,
                                              const juce::String& gapID,
                                              const juce::String& resonID,
                                              const juce::String& balanceID,
                                              const juce::String& modeID,
                                              const juce::String& cabDriveID,
                                              const juce::String& cabTiltID,
                                              const juce::String& cabFoldID)
    : apvts (a),
      idCenter (centerID), idGap (gapID), idReson (resonID),
      idBalance (balanceID), idMode (modeID),
      idCabDrive (cabDriveID), idCabTilt (cabTiltID), idCabFold (cabFoldID)
{
    for (const auto* id : { &idCenter, &idGap, &idReson, &idBalance, &idMode,
                            &idCabDrive, &idCabTilt, &idCabFold })
        apvts.addParameterListener (*id, this);

    // Accessibility floor (product spec item 8): a custom component showing data gets
    // setAccessible(true) plus a title and description.
    setAccessible (true);
    setTitle ("Filter response");
    setDescription ("Live curve of the current filter or cabinet settings.");

    startTimerHz (10);
}

FilterResponseDisplay::~FilterResponseDisplay()
{
    for (const auto* id : { &idCenter, &idGap, &idReson, &idBalance, &idMode,
                            &idCabDrive, &idCabTilt, &idCabFold })
        apvts.removeParameterListener (*id, this);
}

void FilterResponseDisplay::parameterChanged (const juce::String&, float)
{
    dirty = true;
}

void FilterResponseDisplay::timerCallback()
{
    if (dirty) { dirty = false; repaint(); }
}

float FilterResponseDisplay::magnitudeAt (float freq, float center, float gap, float reso, float balance)
{
    // Approximate magnitude of two parallel 4-pole (2x biquad LP and 2x biquad HP)
    // crossfaded by Balance.
    auto biquadMagLP = [] (float f, float fc, float Q)
    {
        if (fc <= 0.0f) return 0.0f;
        const float w = f / fc;
        const float w2 = w * w;
        const float denom = std::sqrt ((1.0f - w2) * (1.0f - w2) + (w / Q) * (w / Q));
        if (denom <= 0.0f) return 1.0e6f;
        return 1.0f / denom;
    };
    auto biquadMagHP = [] (float f, float fc, float Q)
    {
        if (fc <= 0.0f) return 0.0f;
        const float w = f / fc;
        const float w2 = w * w;
        const float denom = std::sqrt ((1.0f - w2) * (1.0f - w2) + (w / Q) * (w / Q));
        if (denom <= 0.0f) return 1.0e6f;
        return w2 / denom;
    };

    const float Q = juce::jmap (juce::jlimit (0.0f, 1.0f, reso), 0.5f, 8.0f);
    const float lpC = std::clamp (center * (1.0f - gap * 0.5f), 5.0f, 20000.0f);
    const float hpC = std::clamp (center * (1.0f + gap * 0.5f), 5.0f, 20000.0f);

    const float lpMag = biquadMagLP (freq, lpC, Q) * biquadMagLP (freq, lpC, Q);
    const float hpMag = biquadMagHP (freq, hpC, Q) * biquadMagHP (freq, hpC, Q);
    const float bal = juce::jlimit (0.0f, 1.0f, (balance + 1.0f) * 0.5f);
    const float mag = lpMag * (1.0f - bal) + hpMag * bal;
    return mag;
}

void FilterResponseDisplay::paint8P (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto getF = [&] (const juce::String& id) {
        if (auto* p = apvts.getRawParameterValue (id)) return p->load();
        return 0.0f;
    };
    const float center  = getF (idCenter);
    const float gap     = getF (idGap);
    const float reson   = getF (idReson);
    const float balance = getF (idBalance);

    // Grid lines
    g.setColour (pal::gridLine);
    auto fxToX = [&] (float f) {
        const float lo = std::log (kMinHz);
        const float hi = std::log (kMaxHz);
        return bounds.getX() + (std::log (f) - lo) / (hi - lo) * bounds.getWidth();
    };
    auto dbToY = [&] (float d) {
        return bounds.getBottom() - (d - kMinDb) / (kMaxDb - kMinDb) * bounds.getHeight();
    };

    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = fxToX (f);
        g.drawVerticalLine ((int) x, bounds.getY(), bounds.getBottom());
    }
    for (float d : { 0.0f, -6.0f, -12.0f, -18.0f })
    {
        const float y = dbToY (d);
        g.drawHorizontalLine ((int) y, bounds.getX(), bounds.getRight());
    }

    // Curve: sample log-spaced from kMinHz to kMaxHz, clamp dB and Y to visible.
    // Defensive: if a magnitude value is non-finite (NaN / inf from the biquad
    // approximation near edge cases), fall back to the previous valid dB so the
    // curve doesn't cliff to zero.
    juce::Path curve;
    const int N = 240;
    float lastDb = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / (float) (N - 1);
        const float f = std::exp (std::log (kMinHz) + t * (std::log (kMaxHz) - std::log (kMinHz)));
        float mag = magnitudeAt (f, center, gap, reson, balance);
        if (! std::isfinite (mag) || mag < 0.0f) mag = 1.0e-6f;
        if (mag > 1.0e6f) mag = 1.0e6f;

        float dB  = 20.0f * std::log10 (juce::jmax (1.0e-6f, mag));
        if (! std::isfinite (dB)) dB = lastDb;
        dB = juce::jlimit (kMinDb, kMaxDb, dB);
        lastDb = dB;

        const float x = juce::jlimit (bounds.getX(), bounds.getRight(),  fxToX (f));
        const float y = juce::jlimit (bounds.getY(), bounds.getBottom(), dbToY (dB));
        if (i == 0) curve.startNewSubPath (x, y);
        else        curve.lineTo (x, y);
    }

    // Fill below curve
    juce::Path fill = curve;
    fill.lineTo (bounds.getRight(), bounds.getBottom());
    fill.lineTo (bounds.getX(),     bounds.getBottom());
    fill.closeSubPath();
    g.setColour (pal::accentPrimary.withAlpha (0.18f));
    g.fillPath (fill);

    g.setColour (pal::accentPrimary);
    g.strokePath (curve, juce::PathStrokeType (1.5f));

    // Frequency labels
    g.setColour (pal::textDim);
    g.setFont (font::diagnostic (8.5f, &getLookAndFeel()));
    for (auto pair : { std::pair<float, const char*> (100.0f, "100"),
                       std::pair<float, const char*> (1000.0f, "1k"),
                       std::pair<float, const char*> (10000.0f, "10k") })
    {
        const float x = fxToX (pair.first);
        g.drawText (pair.second,
                    juce::Rectangle<float> (x - 16.0f, bounds.getBottom() - 11.0f, 32.0f, 10.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }
}

void FilterResponseDisplay::paintCabinet (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto getF = [&] (const juce::String& id) {
        if (auto* p = apvts.getRawParameterValue (id)) return p->load();
        return 0.0f;
    };
    const float drive = juce::jlimit (0.0f, 1.0f, getF (idCabDrive));
    const float tilt  = juce::jlimit (-1.0f, 1.0f, getF (idCabTilt));
    const float fold  = juce::jlimit (0.0f, 1.0f, getF (idCabFold));

    g.setColour (pal::textSecondary);
    g.setFont (font::sectionHeader (10.0f, &getLookAndFeel()));
    g.drawText ("CABINET", bounds.toNearestInt().removeFromTop (14),
                juce::Justification::centredLeft, false);

    auto inner = bounds.reduced (8.0f, 18.0f);
    if (inner.getHeight() < 30.0f) return;

    // Tilt indicator: bar leaning bass-bright
    auto tiltRow = inner.removeFromTop (14.0f);
    g.setColour (pal::bgPanelHi);
    g.fillRect (tiltRow);
    const float tiltMid = tiltRow.getCentreX();
    const float tiltX = tiltMid + tilt * (tiltRow.getWidth() * 0.5f - 4.0f);
    g.setColour (pal::accentPrimary);
    g.fillEllipse (tiltX - 4.0f, tiltRow.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (pal::textDim);
    g.setFont (font::diagnostic (8.5f, &getLookAndFeel()));
    g.drawText ("BASS", tiltRow.toNearestInt().removeFromLeft (40), juce::Justification::centredLeft, false);
    g.drawText ("BRIGHT", tiltRow.toNearestInt().removeFromRight (50), juce::Justification::centredRight, false);

    inner.removeFromTop (4.0f);

    // Drive bar. Was accentWarn (house `warn`, reserved for clip/error, style guide section 2)
    // -- a continuous "how much drive" amount is not an error, so this is remapped to
    // accentHot (house meterHot amber, the same "getting hot" intensity cue the house's own
    // meters use above -6 dB) instead. See ui_migration_report.md "Deviations".
    auto driveRow = inner.removeFromTop (14.0f);
    g.setColour (pal::bgPanelHi);
    g.fillRect (driveRow);
    g.setColour (pal::accentHot.withAlpha (0.85f));
    g.fillRect (driveRow.withWidth (driveRow.getWidth() * drive));
    g.setColour (pal::textDim);
    g.drawText ("DRIVE", driveRow.toNearestInt().reduced (4, 0), juce::Justification::centredLeft, false);

    inner.removeFromTop (4.0f);

    // Fold bar. Was accentLfo -- that token is now the reserved colour-blind-safe LFO MODULE
    // channel (style guide section 3), used everywhere else in this plugin to mean "this is LFO
    // modulation." Cabinet Fold has nothing to do with the LFO section, so reusing that channel
    // here would misleadingly suggest a connection that doesn't exist. Remapped to the generic
    // accentPrimary "active amount" cue instead (distinct from Drive's accentHot).
    auto foldRow = inner.removeFromTop (14.0f);
    g.setColour (pal::bgPanelHi);
    g.fillRect (foldRow);
    g.setColour (pal::accentPrimary.withAlpha (0.85f));
    g.fillRect (foldRow.withWidth (foldRow.getWidth() * fold));
    g.setColour (pal::textDim);
    g.drawText ("FOLD", foldRow.toNearestInt().reduced (4, 0), juce::Justification::centredLeft, false);
}

// One of the style guide's named "Displays": phosphor screen background, hard edges.
void FilterResponseDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    zqsfx::ui::LookAndFeel::drawScreen (g, b, false);
    b = b.reduced (1.0f);

    int mode = 0;
    if (auto* p = apvts.getRawParameterValue (idMode)) mode = juce::jlimit (0, 1, (int) p->load());

    if (mode == 0) paint8P (g, b.reduced (4.0f));
    else            paintCabinet (g, b.reduced (4.0f));
}

} // namespace sk4n_ui
