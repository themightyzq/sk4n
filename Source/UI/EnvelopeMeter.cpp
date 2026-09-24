#include "EnvelopeMeter.h"

namespace sk4n_ui {

EnvelopeMeter::EnvelopeMeter (juce::AudioProcessorValueTreeState& a,
                              const juce::String& aID,
                              const juce::String& d1ID,
                              const juce::String& bID,
                              const juce::String& d2ID,
                              const juce::String& sID,
                              const juce::String& rID,
                              juce::Colour acc)
    : apvts (a), accent (acc)
{
    ids.add (aID); ids.add (d1ID); ids.add (bID);
    ids.add (d2ID); ids.add (sID); ids.add (rID);
    for (const auto& id : ids) apvts.addParameterListener (id, this);

    // Accessibility floor (product spec item 8): a custom component showing data gets
    // setAccessible(true) plus a title and description.
    setAccessible (true);
    setTitle ("Envelope shape");
    setDescription ("Static preview of the current attack/decay/break/decay/sustain/release shape.");
}

EnvelopeMeter::~EnvelopeMeter()
{
    for (const auto& id : ids) apvts.removeParameterListener (id, this);
}

void EnvelopeMeter::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync ([this] { repaint(); });
}

// One of the style guide's named "Displays": phosphor screen background, hard edges.
void EnvelopeMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    zqsfx::ui::LookAndFeel::drawScreen (g, bounds, false);

    auto inner = bounds.reduced (4.0f, 3.0f);
    if (inner.getWidth() < 8.0f || inner.getHeight() < 8.0f) return;

    auto get = [&] (const juce::String& id) -> float
    {
        if (auto* raw = apvts.getRawParameterValue (id)) return raw->load();
        return 0.0f;
    };
    const float aMs   = juce::jmax (1.0f, get (ids[0]));
    const float d1Ms  = juce::jmax (1.0f, get (ids[1]));
    const float bLvl  = juce::jlimit (0.0f, 1.0f, get (ids[2]));
    const float d2Ms  = juce::jmax (1.0f, get (ids[3]));
    const float sLvl  = juce::jlimit (0.0f, 1.0f, get (ids[4]));
    const float rMs   = juce::jmax (1.0f, get (ids[5]));

    // Map total time (A + D1 + D2 + sustainHold + R) to width.
    // Use a fixed sustainHold = 25% of total active time so the curve looks balanced.
    const float adbdsr = aMs + d1Ms + d2Ms + rMs;
    const float sustainHold = adbdsr * 0.25f;
    const float total = adbdsr + sustainHold;

    auto pxFor = [&] (float t) {
        return inner.getX() + (t / total) * inner.getWidth();
    };
    auto yFor = [&] (float lvl) {
        return inner.getBottom() - lvl * inner.getHeight();
    };

    juce::Path curve;
    curve.startNewSubPath (pxFor (0.0f), yFor (0.0f));
    curve.lineTo (pxFor (aMs),                             yFor (1.0f));
    curve.lineTo (pxFor (aMs + d1Ms),                      yFor (bLvl));
    curve.lineTo (pxFor (aMs + d1Ms + d2Ms),               yFor (sLvl));
    curve.lineTo (pxFor (aMs + d1Ms + d2Ms + sustainHold), yFor (sLvl));
    curve.lineTo (pxFor (total),                            yFor (0.0f));

    juce::Path fill = curve;
    fill.lineTo (pxFor (total), yFor (0.0f));
    fill.lineTo (pxFor (0.0f),  yFor (0.0f));
    fill.closeSubPath();

    g.setColour (accent.withAlpha (0.20f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (1.5f));

    // Sustain hold line in dim -- a hint on the phosphor screen, so the screen's own faint-grid
    // token rather than a silkscreen caption colour.
    g.setColour (pal::gridLine.withAlpha (0.8f));
    const float sustainDashes[] = { 2.0f, 2.0f }; // MSVC rejects a C-style compound literal here
    g.drawDashedLine (juce::Line<float> (pxFor (aMs + d1Ms + d2Ms), yFor (sLvl),
                                         pxFor (aMs + d1Ms + d2Ms + sustainHold), yFor (sLvl)),
                     sustainDashes, 2);
}

} // namespace sk4n_ui
