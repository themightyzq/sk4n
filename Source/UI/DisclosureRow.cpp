#include "DisclosureRow.h"

namespace sk4n_ui {

DisclosureRow::DisclosureRow (const juce::String& t) : title (t.toUpperCase())
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (true);
    setHasFocusOutline (true); // house LookAndFeel draws the ring automatically
    setTitle (title);
    setDescription ("Click to expand " + title.toLowerCase() + " parameters.");
    setHelpText ("Click to expand " + title.toLowerCase() + " parameters.");
}

void DisclosureRow::setExpanded (bool e)
{
    if (expanded == e) return;
    expanded = e;
    repaint();
}

void DisclosureRow::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void DisclosureRow::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }

void DisclosureRow::mouseDown (const juce::MouseEvent&)
{
    expanded = ! expanded;
    if (onToggle) onToggle (expanded);
    repaint();
}

void DisclosureRow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    juce::Colour fill = pal::bgPanel;
    if (expanded)      fill = pal::bgPanelHi;
    else if (hovered)  fill = pal::bgPanelHi.darker (0.05f);
    g.setColour (fill);
    g.fillRect (b);

    // Hover gets a brighter, slightly thicker border to clearly read as
    // "this is a button" rather than a static section header.
    juce::Colour border = pal::borderSubtle;
    float        borderW = 1.0f;
    if (expanded)      { border = pal::accentPrimary;            borderW = 1.5f; }
    else if (hovered)  { border = pal::accentPrimary.withAlpha (0.7f); borderW = 1.5f; }
    g.setColour (border);
    g.drawRect (b, borderW);

    auto inner = b.toNearestInt().reduced (12, 0);

    // Reserve right-side space for the chevron so the title can never run under it.
    auto titleArea  = inner;
    titleArea.removeFromRight (24);

    g.setColour (expanded ? pal::textPrimary : pal::textSecondary);
    g.setFont (font::sectionHeader (12.0f, &getLookAndFeel()));
    g.drawText (title, titleArea, juce::Justification::centredLeft, false);

    // Indicator: simple chevron drawn with a small path
    const float cx = (float) inner.getRight() - 10.0f;
    const float cy = inner.toFloat().getCentreY();
    juce::Path chevron;
    if (expanded)
    {
        chevron.startNewSubPath (cx - 5.0f, cy + 2.5f);
        chevron.lineTo          (cx,         cy - 2.5f);
        chevron.lineTo          (cx + 5.0f, cy + 2.5f);
    }
    else
    {
        chevron.startNewSubPath (cx - 5.0f, cy - 2.5f);
        chevron.lineTo          (cx,         cy + 2.5f);
        chevron.lineTo          (cx + 5.0f, cy - 2.5f);
    }
    g.setColour (expanded ? pal::accentPrimary : pal::textSecondary);
    g.strokePath (chevron, juce::PathStrokeType (1.5f));

    // No manual focus ring: setHasFocusOutline(true) (ctor) plus the house LookAndFeel's
    // createFocusOutlineForComponent now draw it automatically.
}

} // namespace sk4n_ui
