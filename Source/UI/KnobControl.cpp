#include "KnobControl.h"

namespace sk4n_ui {

KnobControl::KnobControl (juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& pid,
                          const juce::String& displayName,
                          std::function<juce::String (float)> formatter,
                          KnobSize sz)
    : fmt (std::move (formatter)),
      size (sz),
      accent (pal::accentPrimary),
      paramID (pid)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * -0.75f,
                                juce::MathConstants<float>::pi *  0.75f, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setWantsKeyboardFocus (true);
    slider.setHasFocusOutline (true); // house LookAndFeel draws the ring; see paintOverChildren
    slider.addListener (this);
    addAndMakeVisible (slider);

    // Shift-click on the slider toggles the lock for this parameter.
    slider.shiftClickHandler = [this] (const juce::MouseEvent& /*e*/) -> bool
    {
        if (randomizer == nullptr) return false;
        randomizer->setLocked (paramID, ! randomizer->isLocked (paramID));
        return true;  // consume the event
    };

    // Consistent label / value typography across all tiers -- prevents Tiny
    // controls reading as "smaller in every dimension" than their siblings.
    const float nameFontH  = 10.5f;
    const float valueFontH = 10.5f;

    nameLabel.setText (displayName, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font::label (nameFontH));
    nameLabel.setColour (juce::Label::textColourId, pal::textSecondary);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    // The value readout is the house's "small LCD under the knob" (style guide section 6): it
    // isn't a child of the Slider (KnobControl positions it separately below the knob), so the
    // house LookAndFeel's own drawLabel never gives it LCD glass automatically. Tag it so
    // SK4nLookAndFeel::getLabelFont routes its font through the house's VT323 lcdFont, colour it
    // with the house's LCD text colour, and paint the phosphor screen glass behind it ourselves
    // in paint() below.
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (font::value (valueFontH));
    valueLabel.setColour (juce::Label::textColourId, pal::accentOk);
    valueLabel.setInterceptsMouseClicks (false, false);
    SK4nLookAndFeel::markAsLcdReadout (valueLabel);
    addAndMakeVisible (valueLabel);

    // The aux caption (e.g. a live Hz readout under Pitch) stays a plain silkscreen caption, not
    // a second LCD screen crowding the first.
    auxLabel.setJustificationType (juce::Justification::centred);
    auxLabel.setFont (font::diagnostic (8.5f));
    auxLabel.setColour (juce::Label::textColourId, pal::textDim);
    auxLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (auxLabel);

    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);

    const bool autoBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0);
    slider.getProperties().set ("sk4n_fillStyle",
                                autoBipolar ? juce::var ("bipolar") : juce::var ("unipolar"));

    // Accessibility floor baseline (style guide section 8 / product spec item 8): every Slider
    // gets a title, description, help text, and a tooltip even when the call site never calls
    // setTooltipText()/setAccessibility() explicitly (most of the ~90 knobs across the editor's
    // section builders never did before this migration). setTooltipText() below overwrites the
    // description/help text with real copy where a call site does supply one; setAccessibility()
    // overrides the title too. See ui_migration_report.md "Deviations" for why this is centralised
    // here rather than at each of the ~90 call sites.
    const auto accessibleName = displayName.isNotEmpty() ? displayName : paramID;
    slider.setTitle (accessibleName);
    slider.setDescription (accessibleName);
    slider.setHelpText (accessibleName);
    slider.setTooltip (accessibleName);
    setTitle (displayName);

    updateText();
    startTimerHz (15);
}

KnobControl::~KnobControl()
{
    if (randomizer != nullptr) randomizer->removeListener (this);
    slider.removeListener (this);
}

void KnobControl::connectToRandomizer (Randomizer& r)
{
    if (randomizer != nullptr) randomizer->removeListener (this);
    randomizer = &r;
    randomizer->addListener (this);
    locked = randomizer->isLocked (paramID);
    repaint();
}

void KnobControl::locksChanged()
{
    if (randomizer == nullptr) return;
    const bool nowLocked = randomizer->isLocked (paramID);
    if (nowLocked != locked)
    {
        locked = nowLocked;
        repaint();
    }
}

// The house filmstrip knob carries its own fixed pointer colour and consults no per-slider
// colour at all, so tinting the SLIDER no longer does anything visible (removed per the product
// spec's "remove per-slider fill-colour tricks that only fed an old arc knob"). The module-
// identity colour now lives on the control's own label instead, which is always already paired
// with a module-prefixed label text ("A: Pitch", "B>Pos", ...) -- colour and text together, per
// style guide section 3 rule 2 ("colour is never the only signal").
void KnobControl::setAccentColor (juce::Colour c)
{
    accent = c;
    nameLabel.setColour (juce::Label::textColourId, c);
    repaint();
}

void KnobControl::setAuxText (const juce::String& a)
{
    auxText = a;
    auxLabel.setText (a, juce::dontSendNotification);
}

void KnobControl::setFillStyle (FillStyle s)
{
    slider.getProperties().set ("sk4n_fillStyle",
                                s == FillStyle::Bipolar ? juce::var ("bipolar") : juce::var ("unipolar"));
    repaint();
}

void KnobControl::setTooltipText (const juce::String& t)
{
    slider.setTooltip (t);
    slider.setDescription (t);
    slider.setHelpText (t);
    setTooltip (t);
}

void KnobControl::setAccessibility (const juce::String& title, const juce::String& description)
{
    slider.setTitle (title);
    slider.setDescription (description);
    slider.setHelpText (description);
    setTitle (title);
    setDescription (description);
}

void KnobControl::resized()
{
    auto area = getLocalBounds();
    const int diam   = diameterFor (size);
    const int nameH  = 13;          // unified across tiers
    const int valueH = 13;
    const int auxH   = auxText.isNotEmpty() ? 11 : 0;
    const int gap    = 4;

    // Natural card height: label + gap + knob + gap + value (+ gap + aux).
    // Centering the card vertically keeps the knob + labels feeling like one
    // unit even when the parent row is taller than the card needs.
    const int cardH = (showName ? nameH + gap : 0)
                    + diam + gap + valueH
                    + (auxH > 0 ? gap + auxH : 0);

    const int yOff = juce::jmax (0, (area.getHeight() - cardH) / 2);
    auto card = area.withTrimmedTop (yOff).withHeight (juce::jmin (cardH, area.getHeight()));

    if (showName)
    {
        nameLabel.setBounds (card.removeFromTop (nameH));
        card.removeFromTop (gap);
    }
    else
    {
        nameLabel.setBounds ({});
    }

    auto sliderRow = card.removeFromTop (diam);
    slider.setBounds (sliderRow.withSizeKeepingCentre (diam, diam));
    card.removeFromTop (gap);

    valueLabel.setBounds (card.removeFromTop (valueH));

    if (auxH > 0)
    {
        card.removeFromTop (gap);
        auxLabel.setBounds (card.removeFromTop (auxH));
    }
    else
    {
        auxLabel.setBounds ({});
    }
}

// The value readout's phosphor-screen glass (style guide section 6: "a small LCD under the
// knob"). valueLabel is a plain juce::Label, not a child of the Slider, so the house LookAndFeel
// never draws this glass for it automatically (see SK4nLookAndFeel::getLabelFont); KnobControl
// paints it behind the label itself instead.
void KnobControl::paint (juce::Graphics& g)
{
    if (! valueLabel.getBounds().isEmpty())
        zqsfx::ui::LookAndFeel::drawScreen (g, valueLabel.getBounds().toFloat(), false);
}

void KnobControl::paintOverChildren (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    if (locked)
    {
        // Hard-edged outline around the whole knob box (style guide section 6: no rounded
        // corners). `accentWarn` here is a deliberate caution, not an error: it tells the user
        // this control will not move under Randomize, which is worth a warning-adjacent colour.
        g.setColour (pal::accentWarn.withAlpha (0.40f));
        g.drawRect (bounds, 1.0f);

        // Padlock icon in the top-right corner of the knob area
        const float lockSize = 11.0f;
        const float lockX = bounds.getRight() - lockSize - 1.0f;
        const float lockY = bounds.getY() + 1.0f;

        // Body of the lock
        juce::Rectangle<float> body (lockX, lockY + lockSize * 0.45f,
                                     lockSize, lockSize * 0.55f);
        g.setColour (pal::accentWarn);
        g.fillRect (body);

        // Shackle (arc above the body)
        juce::Path shackle;
        const float arcW = lockSize * 0.55f;
        const float arcH = lockSize * 0.55f;
        const float arcX = lockX + (lockSize - arcW) * 0.5f;
        const float arcY = lockY;
        shackle.addArc (arcX, arcY, arcW, arcH,
                        -juce::MathConstants<float>::pi * 0.5f,
                         juce::MathConstants<float>::pi * 0.5f, true);
        g.strokePath (shackle, juce::PathStrokeType (1.2f));
    }

    // No manual focus ring: slider.setHasFocusOutline(true) (ctor) plus the house LookAndFeel's
    // createFocusOutlineForComponent now draw it automatically. Per the house's own contract,
    // the ring hugs the SLIDER's bounds (the dial), not the whole label+knob+value card -- a
    // deliberate, slightly narrower ring than the old hand-drawn one; see ui_migration_report.md.
}

void KnobControl::sliderValueChanged (juce::Slider*) { updateText(); }

void KnobControl::timerCallback()
{
    updateText();
    const bool nowFocused = slider.hasKeyboardFocus (true);
    if (nowFocused != wasFocused) { wasFocused = nowFocused; repaint(); }
}

void KnobControl::updateText()
{
    const float v = static_cast<float> (slider.getValue());
    valueLabel.setText (fmt ? fmt (v) : juce::String (v, 2), juce::dontSendNotification);
}

} // namespace sk4n_ui
