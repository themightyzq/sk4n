#include "ModeSwitcher.h"

namespace sk4n_ui {

ModeSwitcher::ModeSwitcher (juce::AudioProcessorValueTreeState& a,
                            const juce::String& pid,
                            const juce::String& left,
                            const juce::String& right)
    : apvts (a), paramID (pid), leftLabel (left), rightLabel (right)
{
    if (auto* raw = apvts.getRawParameterValue (paramID))
    {
        targetIdx    = juce::jlimit (0, 1, static_cast<int> (raw->load()));
        displayedIdx = static_cast<float> (targetIdx);
    }
    apvts.addParameterListener (paramID, this);

    // Accessibility floor: a bespoke interactive control gets an accessible name/description and
    // a tooltip (via the SettableTooltipClient base this class already inherits) even though it
    // is mouse-only (click left/right half) like every other bespoke, non-keyboard-operable
    // custom control already in this file (e.g. DisclosureRow, predating this migration).
    const auto accessibleName = leftLabel + " / " + rightLabel;
    setTitle (accessibleName);
    setDescription ("Click the left or right half to switch between " + leftLabel + " and " + rightLabel + ".");
    setTooltip (getDescription());

    startTimerHz (60);
}

ModeSwitcher::~ModeSwitcher()
{
    apvts.removeParameterListener (paramID, this);
}

int ModeSwitcher::getCurrentIndex() const { return targetIdx; }

void ModeSwitcher::parameterChanged (const juce::String&, float v)
{
    targetIdx = juce::jlimit (0, 1, static_cast<int> (v));
}

void ModeSwitcher::timerCallback()
{
    // Publish the mode before animating, so the owner's visibility switch happens on the first
    // tick after the change rather than after the ~200 ms ease finishes.
    if (targetIdx != lastNotifiedIdx)
    {
        lastNotifiedIdx = targetIdx;
        if (onModeChanged != nullptr)
            onModeChanged (targetIdx);
    }

    const float target = static_cast<float> (targetIdx);
    const float diff   = target - displayedIdx;
    if (std::fabs (diff) < 0.01f)
    {
        if (displayedIdx != target) { displayedIdx = target; repaint(); }
        return;
    }
    displayedIdx += diff * 0.25f;  // ~200 ms ease
    repaint();
}

void ModeSwitcher::mouseDown (const juce::MouseEvent& e)
{
    const int idx = (e.position.x < getWidth() * 0.5f) ? 0 : 1;
    setIndexFromUser (idx);
}

void ModeSwitcher::setIndexFromUser (int idx)
{
    if (auto* p = apvts.getParameter (paramID))
    {
        const float norm = p->convertTo0to1 (static_cast<float> (idx));
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void ModeSwitcher::paint (juce::Graphics& g)
{
    // Hard-edged two-segment switch (style guide section 6: no rounded corners); the house has
    // no equivalent bespoke control, so this stays entirely custom-painted, restyled with house
    // tokens only.
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (pal::bgPanelHi);
    g.fillRect (b);
    g.setColour (pal::borderSubtle);
    g.drawRect (b, 1.0f);

    // Active highlight
    const float halfW = b.getWidth() * 0.5f;
    auto highlight = juce::Rectangle<float> (b.getX() + displayedIdx * halfW,
                                             b.getY(), halfW, b.getHeight());
    g.setColour (pal::accentPrimary.withAlpha (0.32f));
    g.fillRect (highlight.reduced (2.0f));

    g.setColour (pal::accentPrimary);
    g.drawRect (highlight.reduced (2.0f), 1.0f);

    // Labels
    auto leftR  = juce::Rectangle<float> (b.getX(),         b.getY(), halfW, b.getHeight());
    auto rightR = juce::Rectangle<float> (b.getX() + halfW, b.getY(), halfW, b.getHeight());
    g.setFont (font::sectionHeader (11.0f, &getLookAndFeel()));

    const float leftAlpha  = juce::jmap (1.0f - displayedIdx, 0.45f, 1.0f);
    const float rightAlpha = juce::jmap (displayedIdx,        0.45f, 1.0f);

    g.setColour (pal::textPrimary.withAlpha (leftAlpha));
    g.drawText (leftLabel,  leftR.toNearestInt(),  juce::Justification::centred, false);
    g.setColour (pal::textPrimary.withAlpha (rightAlpha));
    g.drawText (rightLabel, rightR.toNearestInt(), juce::Justification::centred, false);
}

} // namespace sk4n_ui
