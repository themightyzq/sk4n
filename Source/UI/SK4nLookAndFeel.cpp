#include "SK4nLookAndFeel.h"

namespace sk4n_ui {

SK4nLookAndFeel::SK4nLookAndFeel()
{
    // The base zqsfx::ui::LookAndFeel constructor already set every house colour this class
    // used to set itself: ComboBox/PopupMenu -> LCD glass, Slider textbox -> LCD glass + glow,
    // TextButton -> btn gradient / accent-on, TooltipWindow/AlertWindow/TextEditor -> house
    // tokens, Label -> silkLabel text. Nothing here needs to re-set any of that.
    //
    // rotarySliderFillColourId / rotarySliderOutlineColourId / thumbColourId are gone too for
    // rotary knobs: the house's filmstrip knobs carry their own pointer and consult no
    // per-slider colour at all. rotarySliderFillColourId is still read directly by this class's
    // own drawLinearSlider below (SK4n's existing convention for tinting the morph/speed
    // sliders), so it is NOT one of the colours removed.
    //
    // ToggleButton::textColourId / tickColourId are kept: this class's own drawToggleButton
    // (below) reads textColourId for the off-state legend, matching LFlOw's identical pattern.
    setColour (juce::ToggleButton::textColourId, pal::textSecondary);
    setColour (juce::ToggleButton::tickColourId, pal::accentPrimary);

    // Label backgrounds transparent (not a themed colour -- see ../../CLAUDE.md's own note on
    // this literal). Everything else about Label painting (font, text colour) comes from the
    // house LookAndFeel's own getLabelFont/drawLabel, or from getLabelFont below for the small
    // set of labels tagged as LCD readouts.
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

// ---------------------------------------------------------------------- Toggle buttons
//
// The house LookAndFeel has no drawToggleButton override (its own bound LitToggle/TextToggle
// components draw themselves instead), so this stays -- restyled with house tokens: hard-edged
// rectangle (no pill, no corner radius, style guide section 6), `btn` gradient off-state,
// `accent` fill + `accentInk` text on-state, matching zqsfx::ui::LookAndFeel::drawButtonBackground
// exactly.
void SK4nLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    namespace colour = zqsfx::ui::colour;

    const float alphaMul = button.isEnabled() ? 1.0f : zqsfx::ui::geom::dimAlpha;
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    if (on)
    {
        g.setColour (colour::accent.withAlpha (alphaMul));
        g.fillRect (bounds);
        g.setColour (juce::Colours::black.withAlpha (0.35f * alphaMul));
        g.fillRect (bounds.withTop (bounds.getBottom() - 2.0f));
    }
    else
    {
        g.setGradientFill (zqsfx::ui::gradients::button (bounds, button.isEnabled()));
        g.fillRect (bounds);
        g.setColour (juce::Colours::white.withAlpha (button.isEnabled() ? 0.07f : 0.0f));
        g.fillRect (bounds.removeFromTop (1.0f));
    }
    g.setColour (colour::btnBorder.withAlpha (alphaMul));
    g.drawRect (button.getLocalBounds().toFloat(), 1.0f);

    const auto textColour = ! button.isEnabled() ? colour::silkCaption
                           : on                   ? colour::accentInk
                                                   : button.findColour (juce::ToggleButton::textColourId);
    g.setColour (textColour.withAlpha (alphaMul));
    g.setFont (silkFont (12.0f, true));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

// ---------------------------------------------------------------------- Linear sliders (morph / speed)
//
// The house LookAndFeel has no drawLinearSlider override, so this stays -- track in
// `lcdScreenDark` with a `ruleTitle` border, filled portion in the slider's own tinted colour
// (rotarySliderFillColourId, as SK4n already used before this migration), slim rectangular thumb
// -- no stock white ball, no rounded pill (style guide section 6: hard edges, no rounded corners).
void SK4nLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                        juce::Slider::SliderStyle style, juce::Slider& slider)
{
    namespace colour = zqsfx::ui::colour;

    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
    {
        zqsfx::ui::LookAndFeel::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                  sliderPos, sliderPos, style, slider);
        return;
    }

    const float alphaMul = slider.isEnabled() ? 1.0f : zqsfx::ui::geom::dimAlpha;
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();

    constexpr float trackH = 4.0f;
    const float trackY = bounds.getCentreY() - trackH * 0.5f;
    const juce::Rectangle<float> track (bounds.getX(), trackY, bounds.getWidth(), trackH);
    g.setColour (colour::lcdScreenDark.withAlpha (alphaMul));
    g.fillRect (track);
    g.setColour (colour::ruleTitle.withAlpha (alphaMul));
    g.drawRect (track, 1.0f);

    const auto fillColour = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const float fillW = juce::jlimit (0.0f, bounds.getWidth(), sliderPos - bounds.getX());
    if (fillW > 0.0f)
    {
        const juce::Rectangle<float> fill (bounds.getX(), trackY, fillW, trackH);
        g.setColour (fillColour.withAlpha (alphaMul));
        g.fillRect (fill);
    }

    constexpr float thumbW = 5.0f;
    constexpr float thumbH = 14.0f;
    const juce::Rectangle<float> thumb (sliderPos - thumbW * 0.5f, bounds.getCentreY() - thumbH * 0.5f,
                                        thumbW, thumbH);
    g.setColour (colour::pointer.withAlpha (alphaMul));
    g.fillRect (thumb);
}

// ---------------------------------------------------------------------- LCD-tagged labels
//
// The house LookAndFeel's own drawLabel gives LCD-glass treatment only to a Label parented to a
// juce::Slider (a Slider's built-in text box). SK4n's KnobControl and PluginEditor's presetLabel
// are plain juce::Label value readouts NOT parented to a Slider, so the house's Slider-parent
// check never fires for them. This narrow override routes ONLY labels explicitly tagged via
// markAsLcdReadout() through the house's lcdFont (VT323); every other label -- which is most of
// them -- falls straight through to the house's own getLabelFont (Barlow Condensed silk face),
// so this is not a re-implementation of what the house already does, just a hook the house has
// no equivalent for.
juce::Font SK4nLookAndFeel::getLabelFont (juce::Label& l)
{
    if ((bool) l.getProperties().getWithDefault ("sk4nLcd", false))
        return lcdFont (l.getFont().getHeight());
    return zqsfx::ui::LookAndFeel::getLabelFont (l);
}

} // namespace sk4n_ui
