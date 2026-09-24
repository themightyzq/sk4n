#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <zqsfx_ui/zqsfx_ui.h>

// SK4n now shares the ZQ SFX house look (../../docs/ZQSFX_UI_STYLE_GUIDE.md). `sk4n_ui::pal`
// stays the single colour source for SK4n's own code (project rule, CLAUDE.md) -- every name
// below is unchanged so no call site elsewhere needed to know about the migration -- but every
// value is now a direct alias of a house token instead of an independent literal, so the two can
// never drift apart. NO hardcoded juce::Colours:: or 0xff literal belongs outside this file.
namespace sk4n_ui {

namespace pal {

inline const juce::Colour bgBase           = zqsfx::ui::colour::chassisMid;   // window background
inline const juce::Colour bgPanel          = zqsfx::ui::colour::panelBot;     // panel face (gradient bottom)
inline const juce::Colour bgPanelHi        = zqsfx::ui::colour::panelTop;     // panel face (gradient top) / raised chrome
inline const juce::Colour borderSubtle     = zqsfx::ui::colour::ruleInner;    // inner dividers
inline const juce::Colour borderAccent     = zqsfx::ui::colour::ruleTitle;    // hairlines under titles

inline const juce::Colour textPrimary      = zqsfx::ui::colour::btnText;      // legends
inline const juce::Colour textSecondary    = zqsfx::ui::colour::silkLabel;    // control labels
inline const juce::Colour textDim          = zqsfx::ui::colour::silkCaption;  // captions, disabled text

inline const juce::Colour accentPrimary    = zqsfx::ui::colour::accent;       // the one house accent: active/lit/focused
inline const juce::Colour accentOscA       = zqsfx::ui::comp::sky;            // colour-blind-safe channel 1
inline const juce::Colour accentOscB       = zqsfx::ui::comp::yellow;         // colour-blind-safe channel 2
inline const juce::Colour accentEnv        = zqsfx::ui::comp::purple;         // colour-blind-safe channel 3
inline const juce::Colour accentLfo        = zqsfx::ui::comp::green;         // colour-blind-safe channel 4
inline const juce::Colour accentFb         = zqsfx::ui::comp::white;          // colour-blind-safe channel 5
inline const juce::Colour accentWarn       = zqsfx::ui::colour::warn;         // errors / real clip only (style guide section 2)
inline const juce::Colour accentOk         = zqsfx::ui::colour::lcdText;      // LCD-glass positive/status cue

inline const juce::Colour knobRingInactive = zqsfx::ui::colour::ledOffRim;    // unlit LED rim / inactive ring

// Additive aliases (not in the literal 12-entry remap table): needed so a few existing SK4n
// visuals keep a distinct, correctly-scoped house token instead of colliding with one of the
// above. See docs/ui_migration_report.md "Deviations" for why each exists.
inline const juce::Colour accentHot        = zqsfx::ui::colour::meterHot;     // "getting hot" intensity cue -- NOT an error, so not accentWarn
inline const juce::Colour gridLine         = zqsfx::ui::colour::lcdFaint2;    // grid lines / scale ticks on phosphor screens
inline const juce::Colour screenWell       = zqsfx::ui::colour::lcdScreenDark;// meter wells
inline const juce::Colour ledOff           = zqsfx::ui::colour::ledOffFill;   // unlit LED fill
inline const juce::Colour ledOffRing       = zqsfx::ui::colour::ledOffRim;    // unlit LED rim
inline const juce::Colour helpScrim        = juce::Colour (0xB0000000);       // help-overlay dim (shadow, not a themed colour)

} // namespace pal

namespace font {

// Routes through the house LookAndFeel's own embedded typefaces when one is reachable (the
// component that's asking has a zqsfx::ui::LookAndFeel installed, which is true everywhere in
// SK4n once SK4nLookAndFeel is installed on the editor), falling back to a generic FontOptions
// otherwise (e.g. before a LookAndFeel is attached). Per the product spec: sectionHeader/label/
// title route through silkFont (Barlow Condensed); value/diagnostic/monoSmall route through
// lcdFont (VT323).
inline const zqsfx::ui::LookAndFeel* asHouse (const juce::LookAndFeel* lnf)
{
    return dynamic_cast<const zqsfx::ui::LookAndFeel*> (lnf);
}

inline juce::Font sectionHeader (float pt = 11.0f, const juce::LookAndFeel* lnf = nullptr)
{
    if (auto* h = asHouse (lnf)) return h->silkFont (pt, true);
    return juce::Font (juce::FontOptions (pt).withStyle ("Bold"));
}
inline juce::Font label (float pt = 10.0f, const juce::LookAndFeel* lnf = nullptr)
{
    if (auto* h = asHouse (lnf)) return h->silkFont (pt, false);
    return juce::Font (juce::FontOptions (pt));
}
inline juce::Font value (float pt = 11.0f, const juce::LookAndFeel* lnf = nullptr)
{
    if (auto* h = asHouse (lnf)) return h->lcdFont (pt);
    return juce::Font (juce::FontOptions (pt).withStyle ("Medium"));
}
inline juce::Font diagnostic (float pt = 9.0f, const juce::LookAndFeel* lnf = nullptr)
{
    if (auto* h = asHouse (lnf)) return h->lcdFont (pt);
    return juce::Font (juce::FontOptions (pt));
}
inline juce::Font title (float pt = 14.0f, const juce::LookAndFeel* lnf = nullptr)
{
    if (auto* h = asHouse (lnf)) return h->silkFont (pt, true);
    return juce::Font (juce::FontOptions (pt));
}
inline juce::Font monoSmall (float pt = 9.5f, const juce::LookAndFeel* lnf = nullptr)
{
    if (auto* h = asHouse (lnf)) return h->lcdFont (pt);
    return juce::Font (juce::FontOptions (pt));
}

} // namespace font

// SK4nLookAndFeel is now a THIN SUBCLASS of zqsfx::ui::LookAndFeel: the house LookAndFeel
// supplies rotary knobs (CC0 filmstrips, picked by dial size), combo boxes (LCD dropdowns),
// slider text-box readouts (LCD glass + glow), and TextButton chrome automatically once its
// drawRotarySlider / drawComboBox / positionComboBoxText / getComboBoxFont / getPopupMenuFont /
// drawButtonBackground are left un-overridden. This subclass keeps only the overrides the house
// LookAndFeel has no equivalent for:
//   - drawToggleButton: a plain juce::ToggleButton drawn as a hard-edged tick box (ToggleControl
//     wraps juce::ToggleButton directly, not the house's bound LitToggle/TextToggle).
//   - drawLinearSlider: the morph/speed LinearHorizontal sliders (CircularMorpher, SnapshotRow).
//   - getLabelFont: a NARROW override that routes only labels explicitly tagged "sk4nLcd" (a
//     value readout that isn't a child of a juce::Slider, so the house's own drawLabel Slider-
//     parent check never sees it) through lcdFont; every other label still falls through to the
//     house's own getLabelFont (Barlow Condensed silk face). See KnobControl for the tag.
// Deleted outright: drawRotarySlider (hand-drawn arc), drawButtonBackground (flat rounded rect),
// drawComboBox / getComboBoxFont / getPopupMenuFont (rounded LCD-less combo), drawTickBox (dead
// code -- drawToggleButton fully overrides painting and never falls back to it), drawFocusOutline
// (dead code -- see Source/UI/*.cpp: nothing ever called it; each focusable component drew its
// own ring by hand instead, now replaced by the house's automatic createFocusOutlineForComponent,
// wired via Component::setHasFocusOutline(true) on each focusable child).
class SK4nLookAndFeel : public zqsfx::ui::LookAndFeel
{
public:
    SK4nLookAndFeel();
    ~SK4nLookAndFeel() override = default;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;

    // Marks a Label as an LCD-style value readout (VT323, house LCD text colour) even though it
    // is not a child of a juce::Slider -- the one case the house LookAndFeel has no hook for.
    static void markAsLcdReadout (juce::Label& l) { l.getProperties().set ("sk4nLcd", true); }
};

} // namespace sk4n_ui
