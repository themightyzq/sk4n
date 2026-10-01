#include "PluginEditor.h"
#include "UI/ValueFormatters.h"

using namespace sk4n_ui;
namespace fmt = sk4n_ui::format;

namespace {

constexpr int kEditorW            = 1000;
constexpr int kCpuRowH            = 16;   // CPU-load readout row, added below the main header row
constexpr int kHeaderRowGap       = 2;
constexpr int kCompactH           = 680 + kHeaderRowGap + kCpuRowH;
constexpr int kHeaderH            = 40;
constexpr int kBufferH            = 95;
constexpr int kPerfRowH           = 110;
constexpr int kDisclosureRowH     = 28;
constexpr int kPad                = 6;
constexpr int kRowGap             = 6;
constexpr int kPanelGap           = 8;    // gap between disclosure panels (both directions)

// Everything above and below the morpher/disclosure region when a disclosure is open:
// padding, header, CPU row, buffer + gap, gap, performance row, disclosure bar, gap.
constexpr int kOpenChromeH        = 2 * kPad + kHeaderH + kHeaderRowGap + kCpuRowH + kBufferH
                                  + kRowGap + kRowGap + kPerfRowH + kDisclosureRowH + kRowGap;
constexpr int kMaxOpenH           = 760;  // tallest default window with a disclosure open: fits ~1280x800
constexpr int kMorpherMinH        = 200;  // below this the morpher is hidden to give the panels room
constexpr int kMorpherMaxH        = 380;
constexpr int kScrollBarW         = 22;   // house minimum hit target

// SectionPanel chrome: header + vertical padding, and accent bar + horizontal padding.
constexpr int kPanelChromeH       = sk4n_ui::SectionPanel::kHeaderH + 2 * sk4n_ui::SectionPanel::kContentPadY;
constexpr int kPanelChromeW       = sk4n_ui::SectionPanel::kAccentBarW + 2 * sk4n_ui::SectionPanel::kContentPadX;

// Natural "card" height for a knob of the given tier (matches
// KnobControl::resized math: 13 label + 4 gap + diam + 4 gap + 13 value).
int naturalCardH (KnobSize s)
{
    return 13 + 4 + diameterFor (s) + 4 + 13;
}

// Pitch knob card plus the live Hz caption KnobControl::setAuxText adds below it (4 gap + 11).
inline int oscPitchCardH()    { return naturalCardH (KnobSize::Medium) + 4 + 11; }

// Narrowest content widths of the disclosure panels (each is the widest row the panel's layout
// lambda places, plus kPanelChromeW). See flowItemsFor().
constexpr int kEnvW        = 278;   // 4 knobs of 58 + 3 gaps of 6, plus panel chrome
constexpr int kTriggerW    = 295;   // Thresh, Free, Rate, fire dot
constexpr int kLfoOneRowW  = 612;   // eight LFO controls in a single row
constexpr int kPositionW   = 665;   // Coarse Pos beside the seven secondary controls

// Centered, max-width-constrained version of layoutRow. Useful when a row's
// natural content is narrower than the panel: instead of letting items spread
// across the entire panel width (which leaves big gaps), we constrain the
// layout area to a sensible max width and centre it horizontally.
void layoutRowCentered (juce::Rectangle<int> bounds,
                        const std::vector<std::pair<juce::Component*, int>>& items,
                        int gap, int maxW)
{
    auto inner = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), maxW),
                                                bounds.getHeight());
    // layoutRow already does horizontal centering within its bounds.
    int totalFixed = 0, visible = 0;
    for (auto& it : items)
    {
        if (it.first != nullptr && ! it.first->isVisible()) continue;
        totalFixed += it.second;
        ++visible;
    }
    if (visible == 0) return;
    totalFixed += gap * (visible - 1);

    int x = inner.getX() + juce::jmax (0, (inner.getWidth() - totalFixed) / 2);
    const int y = inner.getY();
    const int h = inner.getHeight();
    for (auto& it : items)
    {
        if (it.first == nullptr) { x += it.second + gap; continue; }
        if (! it.first->isVisible()) continue;
        it.first->setBounds (x, y, it.second, h);
        x += it.second + gap;
    }
}

void layoutRow (juce::Rectangle<int> bounds,
                const std::vector<std::pair<juce::Component*, int>>& items,
                int gap = 4)
{
    if (items.empty()) return;
    int totalFixed = 0, visible = 0;
    for (auto& it : items)
    {
        if (it.first != nullptr && ! it.first->isVisible()) continue;
        totalFixed += it.second;
        ++visible;
    }
    if (visible == 0) return;
    totalFixed += gap * (visible - 1);

    int x = bounds.getX() + juce::jmax (0, (bounds.getWidth() - totalFixed) / 2);
    const int y = bounds.getY();
    const int h = bounds.getHeight();
    for (auto& it : items)
    {
        if (it.first == nullptr) { x += it.second + gap; continue; }
        if (! it.first->isVisible()) continue;
        it.first->setBounds (x, y, it.second, h);
        x += it.second + gap;
    }
}

std::pair<juce::Rectangle<int>, juce::Rectangle<int>> splitH2 (juce::Rectangle<int> band, int gap)
{
    auto left  = band.removeFromLeft (band.getWidth() / 2 - gap / 2);
    band.removeFromLeft (gap);
    return { left, band };
}

// Lays the items out left to right, wrapping to a new row when the next item's minimum width
// does not fit. Each row is as tall as its tallest column; the extra width of a row is shared
// equally between its items, and `extraH` (spare height) is shared equally between rows. Returns
// the natural height (without extraH). With apply == false nothing is moved.
template <typename Item>
int flowLayout (const std::vector<Item>& items, int width, int gap, int extraH, bool apply)
{
    struct Row { size_t first, last; int h; };
    std::vector<Row> rows;

    for (size_t i = 0; i < items.size();)
    {
        Row r { i, i, 0 };
        int used = 0;
        while (r.last < items.size())
        {
            const int w = juce::jmin (items[r.last].minW, width);
            const int need = used + (r.last > r.first ? gap : 0) + w;
            if (r.last > r.first && need > width) break;
            used = need;
            const int n = (int) items[r.last].stack.size();
            r.h = juce::jmax (r.h, n * items[r.last].h + (n - 1) * gap);
            ++r.last;
        }
        rows.push_back (r);
        i = r.last;
    }

    int natural = 0;
    for (auto& r : rows) natural += r.h;
    if (! rows.empty()) natural += gap * ((int) rows.size() - 1);

    if (apply && ! rows.empty())
    {
        const int extraEach = juce::jmax (0, extraH) / (int) rows.size();
        const int extraLast = juce::jmax (0, extraH) - extraEach * (int) rows.size();
        int y = 0;
        for (size_t ri = 0; ri < rows.size(); ++ri)
        {
            const auto& r = rows[ri];
            const int rowH = r.h + extraEach + (ri + 1 == rows.size() ? extraLast : 0);
            const int count = (int) (r.last - r.first);

            int used = gap * (count - 1);
            for (size_t k = r.first; k < r.last; ++k) used += juce::jmin (items[k].minW, width);
            const int spare = juce::jmax (0, width - used);

            int x = 0, given = 0;
            for (size_t k = r.first; k < r.last; ++k)
            {
                const bool lastInRow = (k + 1 == r.last);
                const int share = lastInRow ? spare - given : spare / count;
                given += share;
                const int w = juce::jmin (items[k].minW, width) + share;

                const int n = (int) items[k].stack.size();
                const int each = (rowH - (n - 1) * gap) / n;
                int py = y;
                for (int j = 0; j < n; ++j)
                {
                    const int ph = (j + 1 == n) ? (y + rowH - py) : each;
                    items[k].stack[(size_t) j]->setBounds (x, py, w, ph);
                    py += ph + gap;
                }
                x += w + gap;
            }
            y += rowH + gap;
        }
    }
    return natural;
}

} // namespace

template <typename T, typename... Args>
T* SK4nAudioProcessorEditor::addChild (EditorSection& s, Args&&... args)
{
    auto up = std::make_unique<T> (std::forward<Args> (args)...);
    auto* raw = up.get();
    s.panel->addAndMakeVisible (*raw);
    s.children.push_back (std::move (up));
    return raw;
}

KnobControl* SK4nAudioProcessorEditor::addKnob (EditorSection& s,
                                                 const juce::String& paramID,
                                                 const juce::String& displayName,
                                                 std::function<juce::String (float)> formatter,
                                                 KnobSize size)
{
    auto* k = addChild<KnobControl> (s, processorRef.apvts, paramID, displayName, std::move (formatter), size);
    k->connectToRandomizer (processorRef.randomizer);
    return k;
}

// ============================================================================

SK4nAudioProcessorEditor::SK4nAudioProcessorEditor (SK4nAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    laf = std::make_unique<SK4nLookAndFeel>();
    setLookAndFeel (laf.get());

    titleLabel.setText ("SK4n", juce::dontSendNotification);
    titleLabel.setFont (font::title (16.0f, &getLookAndFeel()));
    titleLabel.setColour (juce::Label::textColourId, pal::textPrimary);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("wave-scanning fx", juce::dontSendNotification);
    subtitleLabel.setFont (font::label (10.5f, &getLookAndFeel()));
    subtitleLabel.setColour (juce::Label::textColourId, pal::textSecondary);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (subtitleLabel);

    // The preset/snapshot name is a value readout, like a host lane display -- give it the
    // house LCD glass + VT323 treatment (see paint() below for the screen it sits on).
    presetLabel.setText ("Snapshot 1", juce::dontSendNotification);
    presetLabel.setFont (font::value (12.0f, &getLookAndFeel()));
    presetLabel.setColour (juce::Label::textColourId, pal::accentOk);
    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setInterceptsMouseClicks (false, false);
    presetLabel.setTitle ("Current preset");
    presetLabel.setDescription ("Active morpher snapshot (or transition between two snapshots).");
    SK4nLookAndFeel::markAsLcdReadout (presetLabel);
    addAndMakeVisible (presetLabel);

    // CPU load readout: same house LCD-glass + value-font treatment as presetLabel (see paint()
    // for the screen it sits on). Read-only, not bound to any parameter; updated at 4 Hz by
    // cpuLoadTimer below from the processor's already-computed atomic (getProcessLoad()).
    cpuLoadLabel.setText ("CPU 0%", juce::dontSendNotification);
    cpuLoadLabel.setFont (font::value (10.0f, &getLookAndFeel()));
    cpuLoadLabel.setColour (juce::Label::textColourId, pal::accentOk);
    cpuLoadLabel.setJustificationType (juce::Justification::centred);
    cpuLoadLabel.setInterceptsMouseClicks (true, false);
    cpuLoadLabel.setTitle ("CPU load");
    cpuLoadLabel.setDescription ("Audio thread load");
    cpuLoadLabel.setTooltip ("Audio thread load");
    SK4nLookAndFeel::markAsLcdReadout (cpuLoadLabel);
    addAndMakeVisible (cpuLoadLabel);

    cpuLoadTimer = std::make_unique<RateTimer> ([this]
    {
        const int pct = juce::roundToInt (juce::jlimit (0.0f, 100.0f, processorRef.getProcessLoad()));
        cpuLoadLabel.setText ("CPU " + juce::String (pct) + "%", juce::dontSendNotification);
    }, 4);

    helpButton.setClickingTogglesState (true);
    helpButton.setTooltip ("Show overlay describing what each region does.");
    helpButton.setTitle ("Help");
    helpButton.setDescription ("Show overlay describing what each region does.");
    helpButton.onClick = [this] { toggleHelp(); };
    addAndMakeVisible (helpButton);

    masterDice = std::make_unique<DiceButton>();
    masterDice->setTooltipText ("Randomize everything (shift-click for subtle variation; locked params stay).");
    masterDice->setOnClick           ([this] { processorRef.randomizer.randomizeAll (Randomizer::Mode::Full); });
    masterDice->setOnConstrainedClick([this] { processorRef.randomizer.randomizeAll (Randomizer::Mode::Constrained); });
    addAndMakeVisible (*masterDice);

    // The ZQ SFX mark: one instance per window, header row, far end from the product wordmark
    // (style guide section 5), and the About-box trigger.
    logo.onClick = [this] { showAboutBox(); };
    addAndMakeVisible (logo);

    bufferDisplay = std::make_unique<BufferDisplay> (processorRef);
    addAndMakeVisible (*bufferDisplay);
    bufferDisplay->setWantsKeyboardFocus (false);

    circularMorpher = std::make_unique<CircularMorpher> (processorRef);
    addAndMakeVisible (*circularMorpher);

    helpOverlay = std::make_unique<HelpOverlay>();
    addChildComponent (*helpOverlay);

    buildAllSections();
    buildPerformanceRow();
    buildDisclosureRows();

    allSectionPanels = {
        position.panel.get(), oscA.panel.get(), oscB.panel.get(),
        mixer.panel.get(), am.panel.get(), delay.panel.get(),
        filter.panel.get(), echoFlg.panel.get(), reverb.panel.get(),
        envA.panel.get(), envB.panel.get(), ampEnv.panel.get(),
        lfo.panel.get(), trigger.panel.get(), master.panel.get()
    };

    // The section panels were built as children of the editor; move them into the scrolling
    // disclosure viewport (addChildComponent removes a component from its previous parent).
    for (auto* panel : allSectionPanels)
        if (panel != nullptr) disclosureContent.addChildComponent (*panel);

    disclosureViewport.setViewedComponent (&disclosureContent, false);
    disclosureViewport.setScrollBarsShown (true, false);          // vertical only
    disclosureViewport.setScrollBarThickness (kScrollBarW);
    disclosureViewport.setTitle ("Section controls");
    disclosureViewport.setDescription ("Scrollable controls for the open section.");
    disclosureViewport.setHasFocusOutline (true);
    addChildComponent (disclosureViewport);
    juce::Desktop::getInstance().addFocusChangeListener (this);

    hideAllSectionPanels();

    // Restore which disclosure was open, persisted as a plain ValueTree property on the APVTS
    // state (see setDisclosure() below) rather than a parameter -- it's UI state, not something
    // automatable. Applied directly via applyDisclosureExpansion(), not setDisclosure(), so it
    // doesn't force a resize before the target window size below is known.
    {
        const int stored = juce::jlimit ((int) Disclosure::None, (int) Disclosure::Advanced,
            (int) processorRef.apvts.state.getProperty ("ui_disclosure", (int) Disclosure::None));
        openDisclosure = static_cast<Disclosure> (stored);
        applyDisclosureExpansion (openDisclosure);
    }

    const int minW = kEditorW - 100, minH = getCompactHeight() - 60;
    const int maxW = kEditorW + 400, maxH = kCompactH + kRowGap + 600;

    // Restore the persisted editor size (0 means "not yet known / use default" -- see
    // PluginProcessor::getEditorWidth()/getEditorHeight()). Falls back to the size implied by
    // the restored disclosure (compact, or that disclosure's expanded height) when nothing was
    // saved yet, or the saved size no longer fits the current resize limits.
    const int storedW  = processorRef.getEditorWidth();
    const int storedH  = processorRef.getEditorHeight();
    const int defaultH = openDisclosure == Disclosure::None ? getCompactHeight() : getExpandedHeight();
    int initW = kEditorW, initH = defaultH;
    if (storedW >= minW && storedW <= maxW && storedH >= minH && storedH <= maxH)
    {
        initW = storedW;
        initH = storedH;
    }
    // Sessions saved before the disclosure viewport existed stored the old auto-grown height
    // (up to 1244 px), which does not fit a 13-inch screen; a disclosure never opens taller
    // than kMaxOpenH by itself.
    if (openDisclosure != Disclosure::None)
        initH = juce::jmin (initH, kMaxOpenH);

    setSize (initW, initH);
    setResizable (true, true);
    setResizeLimits (minW, minH, maxW, maxH);

    // Tab order: header -> morpher -> performance macros -> dry/wet ->
    //            gain -> disclosure rows -> (disclosed-content children).
    int order = 1;
    setExplicitFocusOrder (order++);
    if (masterDice  != nullptr) masterDice->setExplicitFocusOrder (order++);
    helpButton.setExplicitFocusOrder (order++);
    logo.setExplicitFocusOrder (order++);
    if (circularMorpher != nullptr) circularMorpher->setExplicitFocusOrder (order++);
    for (auto* k : { kMovement.get(), kPitch.get(), kColor.get(),
                     kDrive.get(),    kSpace.get(), kTexture.get(),
                     kDryWet.get(),   kMasterGain.get() })
        if (k != nullptr) k->setExplicitFocusOrder (order++);
    for (auto& d : disclosures)
        if (d != nullptr) d->setExplicitFocusOrder (order++);

    startTimerHz (15);
}

SK4nAudioProcessorEditor::~SK4nAudioProcessorEditor()
{
    juce::Desktop::getInstance().removeFocusChangeListener (this);
    setLookAndFeel (nullptr);
}

int SK4nAudioProcessorEditor::getCompactHeight() const  { return kCompactH; }

// Panels with a fixed content layout report their natural height here; the flow layout
// (flowLayout above) stacks them into rows at the viewport width. All of this is derived from
// the same numbers the panels' own layout lambdas use, so a panel is never given less room than
// its content needs.
std::vector<SK4nAudioProcessorEditor::FlowItem>
SK4nAudioProcessorEditor::flowItemsFor (Disclosure d, int width) const
{
    auto P = [] (const EditorSection& s) -> juce::Component* { return s.panel.get(); };

    switch (d)
    {
        case Disclosure::Oscillators:
            // Pitch (Medium + Hz readout) row, then the Small row.
            return { { { P (oscA) }, 330, kPanelChromeH + oscPitchCardH() + 8 + naturalCardH (KnobSize::Small) },
                     { { P (oscB) }, 330, kPanelChromeH + oscPitchCardH() + 8 + naturalCardH (KnobSize::Small) } };

        case Disclosure::Filter:
            // Mode switch + response display (>= 128) + one knob row.
            return { { { P (filter) }, 520, kPanelChromeH + 30 + 10 + 128 + 12 + naturalCardH (KnobSize::Small) } };

        case Disclosure::FX:
            // Mode switch + two knob rows.
            return { { { P (echoFlg) }, 380, kPanelChromeH + 30 + 10 + 2 * naturalCardH (KnobSize::Small) + 10 } };

        case Disclosure::Modulation:
        {
            // Envelope: 2 knob rows + shape + live meter. LFO: one row of eight controls when it
            // has the width for it (kLfoOneRowW), otherwise two rows. Trigger: one row + meter.
            const int envH = kPanelChromeH + 2 * naturalCardH (KnobSize::Small) + 8 + 6 + 28 + 4 + 12 + 2;
            const int oneRowH = kPanelChromeH + naturalCardH (KnobSize::Small) + 6 + 14;
            const int twoRowH = kPanelChromeH + 2 * naturalCardH (KnobSize::Small) + 8 + 6 + 14;
            const int lfoWideW = kLfoOneRowW + kPanelChromeW;
            const bool wide = width >= lfoWideW + kPanelGap + kTriggerW;
            return { { { P (envA) },    kEnvW, envH },
                     { { P (envB) },    kEnvW, envH },
                     { { P (ampEnv) },  kEnvW, envH },
                     { { P (lfo) },     wide ? lfoWideW : 345, wide ? oneRowH : twoRowH },
                     { { P (trigger) }, kTriggerW, oneRowH } };
        }

        case Disclosure::Advanced:
        {
            const int small = kPanelChromeH + naturalCardH (KnobSize::Small);
            return { { { P (position) },                kPositionW, kPanelChromeH + naturalCardH (KnobSize::Large)
                                                                     + 8 + naturalCardH (KnobSize::Tiny) },
                     { { P (mixer), P (master) },       291, small },
                     { { P (am) },                      245, small },
                     { { P (delay) },                   391, small },
                     { { P (reverb) },                  291, small } };
        }

        case Disclosure::None: break;
    }
    return {};
}

int SK4nAudioProcessorEditor::naturalContentHeight (Disclosure d, int width) const
{
    return flowLayout (flowItemsFor (d, width), width, kPanelGap, 0, false);
}

int SK4nAudioProcessorEditor::getExpandedHeight() const
{
    // Chrome + the open panels' natural height, but never taller than a 13-inch screen holds:
    // beyond kMaxOpenH the panels scroll inside their viewport instead of growing the window.
    const int width = (getWidth() > 0 ? getWidth() : kEditorW) - 2 * kPad;
    const int wanted = kOpenChromeH + naturalContentHeight (openDisclosure, width);
    return juce::jlimit (getCompactHeight() - 60, kMaxOpenH, wanted);
}

void SK4nAudioProcessorEditor::hideAllSectionPanels()
{
    for (auto* p : allSectionPanels) if (p) p->setVisible (false);
}

void SK4nAudioProcessorEditor::applyDisclosureExpansion (Disclosure d)
{
    for (size_t i = 0; i < disclosures.size(); ++i)
    {
        if (disclosures[i] != nullptr)
            disclosures[i]->setExpanded ((int) d == (int) i + 1);
    }
}

void SK4nAudioProcessorEditor::setDisclosure (Disclosure d)
{
    if (openDisclosure == d) return;
    openDisclosure = d;

    applyDisclosureExpansion (d);

    // Persist which disclosure is open as a plain ValueTree property (never a parameter) so it
    // survives editor close/reopen and full session save/restore -- getStateInformation already
    // serialises apvts.state via apvts.copyState(), so no processor-side change is needed here.
    processorRef.apvts.state.setProperty ("ui_disclosure", (int) d, nullptr);

    // Resize to whichever expanded height this disclosure needs (or back to compact).
    disclosureViewport.setViewPosition (0, 0);
    setSize (getWidth(),
             d == Disclosure::None ? getCompactHeight() : getExpandedHeight());

    resized();
}

void SK4nAudioProcessorEditor::setDisclosureScroll (int y)
{
    disclosureViewport.setViewPosition (0, juce::jmax (0, y));
}

int SK4nAudioProcessorEditor::getDisclosureScrollRange() const
{
    return juce::jmax (0, disclosureContent.getHeight() - disclosureViewport.getHeight());
}

// Keyboard users tab through the controls in the scrolling viewport; bring the focused one into
// view, otherwise focus could land on a knob that is scrolled out of sight.
void SK4nAudioProcessorEditor::globalFocusChanged (juce::Component* focused)
{
    if (focused == nullptr || ! disclosureContent.isParentOf (focused)) return;

    const auto r   = disclosureContent.getLocalArea (focused, focused->getLocalBounds());
    const auto vis = disclosureViewport.getViewArea();
    constexpr int margin = 6;

    if (r.getY() < vis.getY())
        disclosureViewport.setViewPosition (vis.getX(), juce::jmax (0, r.getY() - margin));
    else if (r.getBottom() > vis.getBottom())
        disclosureViewport.setViewPosition (vis.getX(), r.getBottom() - vis.getHeight() + margin);
}

void SK4nAudioProcessorEditor::toggleHelp()
{
    const bool show = helpButton.getToggleState();
    helpOverlay->setVisible (show);
    if (show)
    {
        helpOverlay->toFront (false);
        layoutHelpOverlay();
        helpOverlay->grabKeyboardFocus();
    }
}

void SK4nAudioProcessorEditor::layoutHelpOverlay()
{
    if (helpOverlay == nullptr) return;
    helpOverlay->setBounds (getLocalBounds());
    std::vector<HelpOverlay::Item> items;
    auto add = [&] (const juce::String& l, juce::Component* c)
    {
        if (c == nullptr || ! c->isVisible()) return;   // e.g. the morpher while a disclosure is open
        const auto r = getLocalArea (c, c->getLocalBounds());
        items.push_back ({ l, r });
    };
    add ("BUFFER  -  live recording",       bufferDisplay.get());
    add ("MORPHER  -  click pads to morph", circularMorpher.get());
    add ("PERFORMANCE MACROS  -  big knobs", kMovement.get());
    add ("DRY/WET",                          kDryWet.get());
    add ("OUTPUT",                           outputMeter.get());
    add ("DISCLOSURES  -  click to expand", disclosures[0].get());
    helpOverlay->setItems (std::move (items));
}

// ============================================================================
// Performance row
// ============================================================================

void SK4nAudioProcessorEditor::buildPerformanceRow()
{
    auto& a = processorRef.apvts;

    auto makeMacroKnob = [&] (const juce::String& id, const juce::String& name, const juce::String& tip)
    {
        auto k = std::make_unique<KnobControl> (a, id, name, fmt::pct, KnobSize::Large);
        k->connectToRandomizer (processorRef.randomizer);
        k->setTooltipText (tip + "  (shift-click to lock)");
        k->setAccessibility (name + " macro", tip);
        addAndMakeVisible (*k);
        return k;
    };

    kMovement = makeMacroKnob ("perfMacroMovement", "Movement",
        "Wave-scanning activity: window size, LFO depth, oscillator-to-position amount, LFO rate.");
    kPitch    = makeMacroKnob ("perfMacroPitch",    "Pitch",
        "Pitch of both oscillators (preserving their interval).");
    kColor    = makeMacroKnob ("perfMacroColor",    "Color",
        "Filter character: cutoff, resonance, and filter mix amount.");
    kDrive    = makeMacroKnob ("perfMacroDrive",    "Drive",
        "Cabinet engagement: filter mode, drive, fold, level.");
    kSpace    = makeMacroKnob ("perfMacroSpace",    "Space",
        "Reverb amount, room size, and a touch of echo.");
    kTexture  = makeMacroKnob ("perfMacroTexture",  "Texture",
        "Oscillator complexity: shape, feedback, AM mix.");

    kDryWet = std::make_unique<KnobControl> (a, "dryWet", "Dry/Wet", fmt::pct, KnobSize::Large);
    kDryWet->connectToRandomizer (processorRef.randomizer);
    kDryWet->setTooltipText ("Mix between dry input and synthesized signal. 0% = bypass, 100% = pure wet.");
    kDryWet->setAccessibility ("Dry/Wet", "Mix between dry and wet.");
    addAndMakeVisible (*kDryWet);

    kMasterGain = std::make_unique<KnobControl> (a, "masterGain", "Gain", fmt::db, KnobSize::Medium);
    kMasterGain->connectToRandomizer (processorRef.randomizer);
    kMasterGain->setTooltipText ("Output gain before the soft clipper.");
    kMasterGain->setAccessibility ("Master gain", "Output gain in dB.");
    addAndMakeVisible (*kMasterGain);

    outputMeter = std::make_unique<OutputMeter> (
        [this] { return processorRef.getOutputPeakL(); },
        [this] { return processorRef.getOutputPeakR(); });
    addAndMakeVisible (*outputMeter);
}

// ============================================================================
// Disclosure rows
// ============================================================================

void SK4nAudioProcessorEditor::buildDisclosureRows()
{
    const std::array<juce::String, 5> titles { "Oscillators", "Filter", "FX", "Modulation", "Advanced" };
    const std::array<Disclosure, 5> ds {
        Disclosure::Oscillators, Disclosure::Filter, Disclosure::FX,
        Disclosure::Modulation,  Disclosure::Advanced
    };

    for (size_t i = 0; i < disclosures.size(); ++i)
    {
        auto row = std::make_unique<DisclosureRow> (titles[i]);
        auto target = ds[i];
        row->setOnToggle ([this, target] (bool nowExpanded)
        {
            setDisclosure (nowExpanded ? target : Disclosure::None);
        });
        addAndMakeVisible (*row);
        disclosures[i] = std::move (row);
    }
}

// ============================================================================
// All sections built (reused from prior passes; mostly unchanged)
// ============================================================================

void SK4nAudioProcessorEditor::buildAllSections()
{
    buildPosition();
    buildOscA();
    buildOscB();
    buildAM();
    buildDelay();
    buildMixer();
    buildFilter();
    buildEchoFlanger();
    buildReverb();
    buildEnv (envA,   "Env A",   pal::accentOscA, "envA",   "A: ", false);
    buildEnv (envB,   "Env B",   pal::accentOscB, "envB",   "B: ", false);
    buildEnv (ampEnv, "Amp Env", pal::accentEnv,  "ampEnv", "",    true);
    buildLfo();
    buildTrigger();
    buildMaster();
}

void SK4nAudioProcessorEditor::buildPosition()
{
    position.panel = std::make_unique<SectionPanel> ("Position", pal::accentPrimary);
    addAndMakeVisible (*position.panel);

    auto& a = processorRef.apvts;
    auto* coarse  = addKnob (position, "coarsePos", "Coarse Pos", fmt::pct, KnobSize::Large);
    coarse->setTooltipText ("Where in the buffer to read from. (shift-click to lock)");

    auto* bufLen = addKnob (position, "bufferLen", "Buffer", fmt::seconds, KnobSize::Small);
    bufLen->setTooltipText ("Recording buffer length.");
    auto* freeze = addChild<ToggleControl> (position, a, "freeze", "Freeze");
    freeze->setTooltipText ("Stop recording new audio.");
    auto* finePos = addKnob (position, "finePos", "Fine Pos", fmt::pctBipolar, KnobSize::Small);
    finePos->setFillStyle (FillStyle::Bipolar);
    auto* fineRng = addKnob (position, "fineRange", "Fine Rng", fmt::pct, KnobSize::Small);
    auto* speed  = addKnob (position, "speed", "Speed",
        [] (float v) { return fmt::number (v, v < 1.0f ? 2 : 1); }, KnobSize::Small);
    auto* range  = addChild<ChoiceControl> (position, a, "rangeMode", "Range",
        juce::StringArray { "ms", "%" });
    auto* window = addKnob (position, "window", "Window", fmt::pct, KnobSize::Small);

    struct ModInfo { const char* id; const char* label; juce::Colour col; };
    const ModInfo mods[] = {
        { "oscAToPos", "A>Pos",   pal::accentOscA },
        { "oscBToPos", "B>Pos",   pal::accentOscB },
        { "fbToPos",   "FB>Pos",  pal::accentFb   },
        { "envToPos",  "Env>Pos", pal::accentEnv  },
        { "lfoToPos",  "LFO>Pos", pal::accentLfo  }
    };
    std::vector<KnobControl*> modKnobs;
    for (auto& m : mods)
    {
        auto* k = addKnob (position, m.id, m.label, fmt::pctBipolar, KnobSize::Tiny);
        k->setAccentColor (m.col);
        k->setFillStyle (FillStyle::Bipolar);
        modKnobs.push_back (k);
    }

    position.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("position",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    position.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        // Two rows (was three, which made this the tallest panel in the editor):
        //   row 1: Coarse Pos | Buffer, Freeze, Fine Pos, Fine Rng, Speed, Range, Window
        //   row 2: the five modulation amounts
        // Each row is its natural knob-card height; spare height pads above and below.
        const int row1 = naturalCardH (KnobSize::Large);
        const int row2 = naturalCardH (KnobSize::Small);    // secondary controls, centred in row 1
        const int row3 = naturalCardH (KnobSize::Tiny);     // mod amounts
        const int gap  = 8;

        const int total = row1 + gap + row3;
        bounds.removeFromTop (juce::jmax (0, (bounds.getHeight() - total) / 2));

        auto r1 = bounds.removeFromTop (row1); bounds.removeFromTop (gap);
        auto r3 = bounds.removeFromTop (row3);

        constexpr int coarseW = 120, groupGap = 24, secondaryW = 492;
        auto group = r1.withSizeKeepingCentre (juce::jmin (r1.getWidth(), coarseW + groupGap + secondaryW), row1);
        coarse->setBounds (group.removeFromLeft (coarseW));
        group.removeFromLeft (groupGap);

        layoutRowCentered (group.withSizeKeepingCentre (group.getWidth(), row2), {
            { bufLen, 60 }, { freeze, 72 }, { finePos, 60 }, { fineRng, 60 },
            { speed, 60 }, { range, 84 }, { window, 60 }
        }, 6, secondaryW);

        layoutRowCentered (r3, {
            { modKnobs[0], 70 }, { modKnobs[1], 70 }, { modKnobs[2], 70 },
            { modKnobs[3], 70 }, { modKnobs[4], 70 }
        }, 10, 480);
    });
}

void SK4nAudioProcessorEditor::buildOscA()
{
    oscA.panel = std::make_unique<SectionPanel> ("Osc A", pal::accentOscA);
    addAndMakeVisible (*oscA.panel);

    auto* pitch = addKnob (oscA, "oscAPitch", "A: Pitch",
        [] (float v) { return juce::String (juce::roundToInt (v)) + " (" + fmt::midiNote (v) + ")"; },
        KnobSize::Medium);
    pitch->setAccentColor (pal::accentOscA);
    auto* fine  = addKnob (oscA, "oscAFine",  "A: Fine",  fmt::cents, KnobSize::Small);
    auto* shape = addKnob (oscA, "oscAShape", "A: Shape", fmt::pct,   KnobSize::Small);
    auto* fb    = addKnob (oscA, "oscAFb",    "A: FB",    fmt::pct,   KnobSize::Small);
    auto* envP  = addKnob (oscA, "oscAEnvPitch", "A: Env>P", fmt::pctBipolar, KnobSize::Small);
    auto* lfoP  = addKnob (oscA, "oscALfoPitch", "A: LFO>P", fmt::pctBipolar, KnobSize::Small);
    auto* envQ  = addKnob (oscA, "oscAEnvQuant", "A: Env Q", fmt::pct, KnobSize::Tiny);
    auto* lfoQ  = addKnob (oscA, "oscALfoQuant", "A: LFO Q", fmt::pct, KnobSize::Tiny);
    for (auto* k : { fine, shape, fb, envP, lfoP, envQ, lfoQ }) k->setAccentColor (pal::accentOscA);
    envP->setFillStyle (FillStyle::Bipolar);
    lfoP->setFillStyle (FillStyle::Bipolar);

    pitch->setAuxText (fmt::hz (processorRef.getOscAFrequencyHz()));
    oscAPitchKnob = pitch;

    oscA.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("oscA",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    oscA.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int row1 = oscPitchCardH();  // Pitch is Medium, with its live Hz caption
        const int row2 = naturalCardH (KnobSize::Small);
        const int gap  = 8;
        const int total = row1 + gap + row2;
        const int top   = juce::jmax (0, (bounds.getHeight() - total) / 2);
        bounds.removeFromTop (top);

        auto r1 = bounds.removeFromTop (row1); bounds.removeFromTop (gap);
        auto r2 = bounds.removeFromTop (row2);

        layoutRowCentered (r1, {
            { pitch, 92 }, { fine, 60 }, { shape, 60 }, { fb, 60 }
        }, 8, 380);
        layoutRowCentered (r2, {
            { envP, 60 }, { lfoP, 60 }, { envQ, 52 }, { lfoQ, 52 }
        }, 8, 320);
    });
}

void SK4nAudioProcessorEditor::buildOscB()
{
    oscB.panel = std::make_unique<SectionPanel> ("Osc B", pal::accentOscB);
    addAndMakeVisible (*oscB.panel);

    auto* pitch = addKnob (oscB, "oscBPitch", "B: Pitch",
        [] (float v) { return juce::String (juce::roundToInt (v)) + " (" + fmt::midiNote (v) + ")"; },
        KnobSize::Medium);
    pitch->setAccentColor (pal::accentOscB);
    auto* fine  = addKnob (oscB, "oscBFine",  "B: Fine",  fmt::cents, KnobSize::Small);
    auto* shape = addKnob (oscB, "oscBShape", "B: Shape", fmt::pct,   KnobSize::Small);
    auto* fb    = addKnob (oscB, "oscBFb",    "B: FB",    fmt::pct,   KnobSize::Small);
    auto* envP  = addKnob (oscB, "oscBEnvPitch", "B: Env>P", fmt::pctBipolar, KnobSize::Small);
    auto* lfoP  = addKnob (oscB, "oscBLfoPitch", "B: LFO>P", fmt::pctBipolar, KnobSize::Small);
    auto* envQ  = addKnob (oscB, "oscBEnvQuant", "B: Env Q", fmt::pct, KnobSize::Tiny);
    auto* lfoQ  = addKnob (oscB, "oscBLfoQuant", "B: LFO Q", fmt::pct, KnobSize::Tiny);
    for (auto* k : { fine, shape, fb, envP, lfoP, envQ, lfoQ }) k->setAccentColor (pal::accentOscB);
    envP->setFillStyle (FillStyle::Bipolar);
    lfoP->setFillStyle (FillStyle::Bipolar);

    pitch->setAuxText (fmt::hz (processorRef.getOscBFrequencyHz()));
    oscBPitchKnob = pitch;

    oscB.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("oscB",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    oscB.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int row1 = oscPitchCardH();
        const int row2 = naturalCardH (KnobSize::Small);
        const int gap  = 8;
        const int total = row1 + gap + row2;
        const int top   = juce::jmax (0, (bounds.getHeight() - total) / 2);
        bounds.removeFromTop (top);

        auto r1 = bounds.removeFromTop (row1); bounds.removeFromTop (gap);
        auto r2 = bounds.removeFromTop (row2);

        layoutRowCentered (r1, {
            { pitch, 92 }, { fine, 60 }, { shape, 60 }, { fb, 60 }
        }, 8, 380);
        layoutRowCentered (r2, {
            { envP, 60 }, { lfoP, 60 }, { envQ, 52 }, { lfoQ, 52 }
        }, 8, 320);
    });
}

void SK4nAudioProcessorEditor::buildMixer()
{
    mixer.panel = std::make_unique<SectionPanel> ("Mixer", pal::accentPrimary);
    addAndMakeVisible (*mixer.panel);

    auto* sample = addKnob (mixer, "sampleMix", "Sample", fmt::pctBipolar, KnobSize::Small);
    auto* amM    = addKnob (mixer, "amMix",     "AM",     fmt::pctBipolar, KnobSize::Small);
    auto* delayM = addKnob (mixer, "delayMix",  "Delay",  fmt::pctBipolar, KnobSize::Small);
    auto* filtM  = addKnob (mixer, "filterMix", "Filter", fmt::pct,        KnobSize::Small);
    sample->setFillStyle (FillStyle::Bipolar);
    amM   ->setFillStyle (FillStyle::Bipolar);
    delayM->setFillStyle (FillStyle::Bipolar);

    mixer.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("mixer",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    mixer.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int rowH = naturalCardH (KnobSize::Small);
        const int top  = juce::jmax (0, (bounds.getHeight() - rowH) / 2);
        bounds.removeFromTop (top);
        auto row = bounds.removeFromTop (rowH);
        layoutRowCentered (row, {
            { sample, 60 }, { amM, 60 }, { delayM, 60 }, { filtM, 60 }
        }, 8, 320);
    });
}

void SK4nAudioProcessorEditor::buildAM()
{
    am.panel = std::make_unique<SectionPanel> ("AM", pal::accentPrimary);
    addAndMakeVisible (*am.panel);

    auto& a = processorRef.apvts;
    auto* blend = addKnob   (am, "amBlend",  "A<>B Blend", fmt::pct, KnobSize::Small);
    auto* sq    = addChild<ToggleControl> (am, a, "amSquare", "X^2");
    auto* mix   = addKnob   (am, "amMix",    "Mix",        fmt::pctBipolar, KnobSize::Small);
    mix->setFillStyle (FillStyle::Bipolar);

    am.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("am",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    am.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int rowH = naturalCardH (KnobSize::Small);
        const int top  = juce::jmax (0, (bounds.getHeight() - rowH) / 2);
        bounds.removeFromTop (top);
        auto row = bounds.removeFromTop (rowH);
        layoutRowCentered (row, {
            { blend, 64 }, { sq, 70 }, { mix, 64 }
        }, 8, 320);
    });
}

void SK4nAudioProcessorEditor::buildDelay()
{
    delay.panel = std::make_unique<SectionPanel> ("Delay", pal::accentPrimary);
    addAndMakeVisible (*delay.panel);

    auto* tune  = addKnob (delay, "delayTune", "Tune",
        [] (float v) { return juce::String (juce::roundToInt (v)) + " (" + fmt::midiNote (v) + ")"; },
        KnobSize::Small);
    auto* fb    = addKnob (delay, "delayFeedback", "FB",     fmt::pct, KnobSize::Small);
    auto* lfoT  = addKnob (delay, "delayLfo",      "LFO>T",  fmt::pct, KnobSize::Small);
    auto* envT  = addKnob (delay, "delayEnv",      "Env>T",  fmt::pct, KnobSize::Small);
    auto* loCut = addKnob (delay, "delayLoCut",    "LoCut",  fmt::hz,  KnobSize::Small);
    auto* mix   = addKnob (delay, "delayMix",      "Mix",    fmt::pctBipolar, KnobSize::Small);
    mix->setFillStyle (FillStyle::Bipolar);

    delay.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("delay",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    delay.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        layoutRowCentered (bounds, {
            { tune, 64 }, { fb, 56 }, { lfoT, 56 }, { envT, 56 }, { loCut, 56 }, { mix, 56 }
        }, 4, 364);
    });
}

void SK4nAudioProcessorEditor::buildFilter()
{
    filter.panel = std::make_unique<SectionPanel> ("Filter", pal::accentPrimary);
    addAndMakeVisible (*filter.panel);

    auto& a = processorRef.apvts;
    filterModeSwitch = addChild<ModeSwitcher> (filter, a, "filterMode", "8-POLE", "CABINET");
    // Re-run the panel layout rather than only toggling visibility: the Filter row's item list
    // itself differs per mode, so the knobs need repositioning, not just showing and hiding.
    // SectionPanel::resized re-invokes the layout lambda, which applies the visibility first.
    filterModeSwitch->onModeChanged = [this] (int)
    { if (filter.panel != nullptr) filter.panel->resized(); };

    auto* center = addKnob (filter, "filterCenter",   "Center", fmt::hz,         KnobSize::Small);
    auto* gap    = addKnob (filter, "filterGap",      "Gap",    fmt::pctBipolar, KnobSize::Small);
    auto* reson  = addKnob (filter, "filterReson",    "Reson",  fmt::pct,        KnobSize::Small);
    auto* bal    = addKnob (filter, "filterBalance",  "Bal",    fmt::pctBipolar, KnobSize::Small);
    auto* lrOff  = addKnob (filter, "filterLrOffset", "L/R",    fmt::pct,        KnobSize::Small);
    auto* mix    = addKnob (filter, "filterMix",      "Mix",    fmt::pct,        KnobSize::Small);
    gap->setFillStyle (FillStyle::Bipolar);
    bal->setFillStyle (FillStyle::Bipolar);
    filter8PControls = { center, gap, reson, bal, lrOff };

    auto* drive  = addKnob (filter, "cabDrive", "Drive", fmt::pct,        KnobSize::Small);
    auto* fold   = addKnob (filter, "cabFold",  "Fold",  fmt::pct,        KnobSize::Small);
    auto* tilt   = addKnob (filter, "cabTilt",  "Tilt",  fmt::pctBipolar, KnobSize::Small);
    auto* hiCut  = addKnob (filter, "cabHiCut", "HiCut", fmt::hz,         KnobSize::Small);
    auto* level  = addKnob (filter, "cabLevel", "Level", fmt::db,         KnobSize::Small);
    tilt->setFillStyle (FillStyle::Bipolar);
    cabinetControls = { drive, fold, tilt, hiCut, level };

    filterResp = addChild<FilterResponseDisplay> (filter, a,
        juce::String ("filterCenter"), juce::String ("filterGap"),
        juce::String ("filterReson"),  juce::String ("filterBalance"),
        juce::String ("filterMode"),
        juce::String ("cabDrive"),     juce::String ("cabTilt"),
        juce::String ("cabFold"));

    filter.panel->setDiceCallback ([this] (bool c)
    {
        const int mode = processorRef.apvts.getRawParameterValue ("filterMode") != nullptr
                       ? juce::jlimit (0, 1, (int) processorRef.apvts.getRawParameterValue ("filterMode")->load())
                       : 0;
        processorRef.randomizer.randomizeSection (mode == 0 ? "filter" : "cabinet",
            c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full);
    });

    filter.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        auto switchArea = bounds.removeFromTop (30);
        filterModeSwitch->setBounds (switchArea.removeFromLeft (180));
        bounds.removeFromTop (10);

        // Before the row layout, not after: layoutRowCentered skips controls that are not
        // visible, so the visibility must already be correct when it runs.
        applyFilterModeVisibility();

        const int rowH = naturalCardH (KnobSize::Small);
        const int respH = juce::jmax (80, bounds.getHeight() - rowH - 12);
        filterResp->setBounds (bounds.removeFromTop (respH));
        bounds.removeFromTop (12);

        auto knobsRow = bounds.removeFromTop (rowH);

        // ONE call, with the row chosen by mode. Mix belongs to both modes, so laying out both
        // rows would let the second call re-centre Mix on its own (every other control in that
        // list being hidden), dropping it into the middle of the row on top of its neighbours.
        const bool cabinet = filterModeSwitch->getCurrentIndex() == 1;
        if (cabinet)
            layoutRowCentered (knobsRow, {
                { drive, 72 }, { fold, 72 }, { tilt, 72 }, { hiCut, 72 }, { level, 72 }, { mix, 72 }
            }, 10, 540);
        else
            layoutRowCentered (knobsRow, {
                { center, 72 }, { gap, 72 }, { reson, 72 }, { bal, 72 }, { lrOff, 72 }, { mix, 72 }
            }, 10, 540);
    });
}

void SK4nAudioProcessorEditor::buildEchoFlanger()
{
    echoFlg.panel = std::make_unique<SectionPanel> ("FX", pal::accentPrimary);
    addAndMakeVisible (*echoFlg.panel);

    auto& a = processorRef.apvts;
    echoModeSwitch = addChild<ModeSwitcher> (echoFlg, a, "echoFlangerMode", "ECHO", "FLANGER");
    echoModeSwitch->onModeChanged = [this] (int)
    { if (echoFlg.panel != nullptr) echoFlg.panel->resized(); };

    auto* eTime  = addKnob (echoFlg, "echoTime",  "Time",  fmt::ms,         KnobSize::Small);
    auto* eLrOff = addKnob (echoFlg, "echoLrOff", "L/R",   fmt::pctBipolar, KnobSize::Small);
    auto* eFb    = addKnob (echoFlg, "echoFb",    "FB",    fmt::pct,        KnobSize::Small);
    auto* eLoCut = addKnob (echoFlg, "echoLoCut", "LoCut", fmt::hz,         KnobSize::Small);
    auto* eHiCut = addKnob (echoFlg, "echoHiCut", "HiCut", fmt::hz,         KnobSize::Small);
    auto* eSync  = addChild<ToggleControl> (echoFlg, a, "echoSync", "Sync");
    auto* eMix   = addKnob (echoFlg, "echoMix",   "Mix",   fmt::pct,        KnobSize::Small);
    eLrOff->setFillStyle (FillStyle::Bipolar);
    echoControls = { eTime, eLrOff, eFb, eLoCut, eHiCut, eSync, eMix };

    auto* fTime  = addKnob (echoFlg, "flgTime",  "Time",  fmt::ms,  KnobSize::Small);
    auto* fDepth = addKnob (echoFlg, "flgDepth", "Depth", fmt::pct, KnobSize::Small);
    auto* fRate  = addKnob (echoFlg, "flgRate",  "Rate",  fmt::hz,  KnobSize::Small);
    auto* fFb    = addKnob (echoFlg, "flgFb",    "FB",    fmt::pct, KnobSize::Small);
    auto* fSync  = addChild<ToggleControl> (echoFlg, a, "flgSync", "Sync");
    auto* fMix   = addKnob (echoFlg, "flgMix",   "Mix",   fmt::pct, KnobSize::Small);
    flangerControls = { fTime, fDepth, fRate, fFb, fSync, fMix };

    echoFlg.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("fx",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    echoFlg.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        auto switchArea = bounds.removeFromTop (30);
        echoModeSwitch->setBounds (switchArea.removeFromLeft (180));
        bounds.removeFromTop (10);

        // Before the row layout: layoutRowCentered skips invisible controls, and returns early
        // when a whole set is hidden. Echo and Flanger share no control, so unlike the Filter
        // panel both rows can stay as separate calls.
        applyEchoModeVisibility();

        // Two rows of knobs -- 4 / 3 split for Echo, 3 / 3 for Flanger.
        // Echo and Flanger share the same row positions; visibility filtering
        // handles which set draws.
        const int cardH = naturalCardH (KnobSize::Small);
        const int gap   = 10;
        const int total = cardH * 2 + gap;
        const int top   = juce::jmax (0, (bounds.getHeight() - total) / 2);
        bounds.removeFromTop (top);

        auto row1 = bounds.removeFromTop (cardH);
        bounds.removeFromTop (gap);
        auto row2 = bounds.removeFromTop (cardH);

        // Echo: row1 = Time | L/R | FB | LoCut    row2 = HiCut | Sync | Mix
        layoutRowCentered (row1, {
            { eTime, 76 }, { eLrOff, 76 }, { eFb, 76 }, { eLoCut, 76 }
        }, 10, 460);
        layoutRowCentered (row2, {
            { eHiCut, 76 }, { eSync, 80 }, { eMix, 76 }
        }, 12, 380);

        // Flanger: row1 = Time | Depth | Rate    row2 = FB | Sync | Mix
        layoutRowCentered (row1, {
            { fTime, 76 }, { fDepth, 76 }, { fRate, 76 }
        }, 12, 380);
        layoutRowCentered (row2, {
            { fFb, 76 }, { fSync, 80 }, { fMix, 76 }
        }, 12, 380);
    });
}

void SK4nAudioProcessorEditor::buildReverb()
{
    reverb.panel = std::make_unique<SectionPanel> ("Reverb", pal::accentPrimary);
    addAndMakeVisible (*reverb.panel);

    auto* size = addKnob (reverb, "reverbSize",  "Size",  fmt::pct, KnobSize::Small);
    auto* loC  = addKnob (reverb, "reverbLoCut", "LoCut", fmt::hz,  KnobSize::Small);
    auto* hiC  = addKnob (reverb, "reverbHiCut", "HiCut", fmt::hz,  KnobSize::Small);
    auto* mix  = addKnob (reverb, "reverbMix",   "Mix",   fmt::pct, KnobSize::Small);

    reverb.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("reverb",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    reverb.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int rowH = naturalCardH (KnobSize::Small);
        const int top  = juce::jmax (0, (bounds.getHeight() - rowH) / 2);
        bounds.removeFromTop (top);
        auto row = bounds.removeFromTop (rowH);
        layoutRowCentered (row, {
            { size, 60 }, { loC, 60 }, { hiC, 60 }, { mix, 60 }
        }, 8, 320);
    });
}

void SK4nAudioProcessorEditor::buildEnv (EditorSection& s, const juce::String& title,
                                          juce::Colour accent, const juce::String& prefix,
                                          const juce::String& letterPrefix, bool showDepth)
{
    s.panel = std::make_unique<SectionPanel> (title, accent);
    addAndMakeVisible (*s.panel);

    auto* aK   = addKnob (s, prefix + "A",     letterPrefix + "A",     fmt::ms,  KnobSize::Small);
    auto* d1K  = addKnob (s, prefix + "D1",    letterPrefix + "D1",    fmt::ms,  KnobSize::Small);
    auto* brK  = addKnob (s, prefix + "Break", letterPrefix + "Break", fmt::pct, KnobSize::Small);
    auto* d2K  = addKnob (s, prefix + "D2",    letterPrefix + "D2",    fmt::ms,  KnobSize::Small);
    auto* sK   = addKnob (s, prefix + "S",     letterPrefix + "S",     fmt::pct, KnobSize::Small);
    auto* rK   = addKnob (s, prefix + "R",     letterPrefix + "R",     fmt::ms,  KnobSize::Small);
    auto* velK = addKnob (s, prefix + "Vel",   letterPrefix + "Vel",   fmt::pct, KnobSize::Small);
    for (auto* k : { aK, d1K, brK, d2K, sK, rK, velK }) k->setAccentColor (accent);

    KnobControl* depthKnob = nullptr;
    if (showDepth)
    {
        depthKnob = addKnob (s, "ampEnvDepth", "Depth", fmt::pct, KnobSize::Small);
        depthKnob->setAccentColor (accent);
    }

    auto* shape = addChild<EnvelopeMeter> (s, processorRef.apvts,
        prefix + "A", prefix + "D1", prefix + "Break",
        prefix + "D2", prefix + "S", prefix + "R", accent);

    std::function<float()> meterSrc;
    if (prefix == "envA")        meterSrc = [this] { return processorRef.getEnvAValue();  };
    else if (prefix == "envB")   meterSrc = [this] { return processorRef.getEnvBValue();  };
    else                          meterSrc = [this] { return processorRef.getAmpEnvValue();};
    auto* live = addChild<ModMeter> (s, ModMeter::Mode::Unipolar, std::move (meterSrc), accent);

    const juce::String tag = (prefix == "envA") ? "envA" : (prefix == "envB" ? "envB" : "ampEnv");
    s.panel->setDiceCallback ([this, tag] (bool c)
    { processorRef.randomizer.randomizeSection (tag,
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    s.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int meterH = 12;
        const int shapeH = 28;
        const int knobH  = naturalCardH (KnobSize::Small);
        const int gap    = 8;
        const int totalNeeded = knobH * 2 + gap + 6 + shapeH + 4 + meterH;

        // If the panel is too short to fit everything, hide the shape/meter
        // and let the knobs dominate.
        if (bounds.getHeight() < totalNeeded)
        {
            shape->setVisible (false);
            live ->setVisible (false);
        }
        else
        {
            shape->setVisible (true);
            live ->setVisible (true);
        }

        // Top padding centers the block when there's extra room.
        const int blockH = shape->isVisible() ? totalNeeded
                                              : (knobH * 2 + gap);
        const int topPad = juce::jmax (0, (bounds.getHeight() - blockH) / 2);
        bounds.removeFromTop (topPad);

        auto row1 = bounds.removeFromTop (knobH);
        bounds.removeFromTop (gap);
        auto row2 = bounds.removeFromTop (knobH);

        // 58 px cards, 6 px gaps: four of them are 250 px, the width kEnvW budgets for.
        layoutRowCentered (row1, {
            { aK, 58 }, { d1K, 58 }, { brK, 58 }, { d2K, 58 }
        }, 6, 280);

        std::vector<std::pair<juce::Component*, int>> bottom {
            { sK, 58 }, { rK, 58 }, { velK, 58 }
        };
        if (depthKnob != nullptr) bottom.push_back ({ depthKnob, 58 });
        layoutRowCentered (row2, bottom, 6, 280);

        if (shape->isVisible())
        {
            bounds.removeFromTop (6);
            auto shapeRow = bounds.removeFromTop (shapeH);
            shape->setBounds (shapeRow.reduced (8, 0));
            bounds.removeFromTop (4);
            auto liveRow = bounds.removeFromTop (meterH);
            live ->setBounds (liveRow.reduced (8, 0));
        }
    });
}

void SK4nAudioProcessorEditor::buildLfo()
{
    lfo.panel = std::make_unique<SectionPanel> ("LFO", pal::accentLfo);
    addAndMakeVisible (*lfo.panel);

    auto& a = processorRef.apvts;
    auto* rate  = addKnob (lfo, "lfoRate",     "Rate", fmt::hz, KnobSize::Small);
    auto* shape = addChild<ChoiceControl> (lfo, a, "lfoShape", "Shape",
        juce::StringArray { "Sine", "Tri", "S&H" });
    auto* sync  = addChild<ToggleControl> (lfo, a, "lfoSync", "Sync");
    auto* div   = addChild<ChoiceControl> (lfo, a, "lfoSyncDiv", "Div",
        juce::StringArray { "1/64","1/32","1/16","1/8","1/4","1/2","1/1","2/1","4/1" });
    auto* sym   = addKnob (lfo, "lfoSymmetry", "Sym",   fmt::pctBipolar, KnobSize::Small);
    auto* phase = addKnob (lfo, "lfoPhase",    "Phase", fmt::pctBipolar, KnobSize::Small);
    auto* fade  = addKnob (lfo, "lfoFade",     "Fade",  fmt::pct,        KnobSize::Small);
    auto* keyS  = addChild<ToggleControl> (lfo, a, "lfoKeySync", "Key");
    rate->setAccentColor (pal::accentLfo);
    sym->setFillStyle (FillStyle::Bipolar);
    phase->setFillStyle (FillStyle::Bipolar);

    auto* meter = addChild<ModMeter> (lfo, ModMeter::Mode::Bipolar,
        [this] { return processorRef.getLfoValue(); }, pal::accentLfo);

    lfo.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("lfo",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    lfo.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int meterH = 14;
        const int rowH   = naturalCardH (KnobSize::Small);
        const int gap    = 8;

        // All eight controls fit in one row when the panel is wide enough (kLfoOneRowW);
        // otherwise two rows of four.
        const bool oneRow = bounds.getWidth() >= kLfoOneRowW;
        const int  rows   = oneRow ? 1 : 2;
        const int totalNeeded = rowH * rows + gap * (rows - 1) + 6 + meterH;
        const bool meterFits = bounds.getHeight() >= totalNeeded;
        meter->setVisible (meterFits);

        const int blockH = meterFits ? totalNeeded : (rowH * rows + gap * (rows - 1));
        const int topPad = juce::jmax (0, (bounds.getHeight() - blockH) / 2);
        bounds.removeFromTop (topPad);

        auto row1 = bounds.removeFromTop (rowH);
        if (oneRow)
        {
            layoutRowCentered (row1, {
                { rate, 64 }, { shape, 80 }, { sync, 70 }, { div, 80 },
                { sym, 64 }, { phase, 64 }, { fade, 64 }, { keyS, 70 }
            }, 8, kLfoOneRowW);
        }
        else
        {
            bounds.removeFromTop (gap);
            auto row2 = bounds.removeFromTop (rowH);
            layoutRowCentered (row1, {
                { rate, 64 }, { shape, 80 }, { sync, 70 }, { div, 80 }
            }, 8, 360);
            layoutRowCentered (row2, {
                { sym, 64 }, { phase, 64 }, { fade, 64 }, { keyS, 70 }
            }, 8, 360);
        }

        if (meterFits)
        {
            bounds.removeFromTop (6);
            meter->setBounds (bounds.removeFromTop (meterH).reduced (8, 0));
        }
    });
}

void SK4nAudioProcessorEditor::buildTrigger()
{
    trigger.panel = std::make_unique<SectionPanel> ("Trigger", pal::accentEnv);
    addAndMakeVisible (*trigger.panel);

    auto* thresh  = addKnob (trigger, "trigThresh",  "Thresh", fmt::pct, KnobSize::Small);
    auto* freeRun = addChild<ToggleControl> (trigger, processorRef.apvts, "freeRun", "Free");
    auto* rate    = addKnob (trigger, "freeRunRate", "Rate",   fmt::hz,  KnobSize::Small);
    thresh->setAccentColor (pal::accentEnv);

    fireDot = addChild<FireDot> (trigger);
    triggerMeter = addChild<ModMeter> (trigger, ModMeter::Mode::Unipolar,
        [this] { return processorRef.getTransientFollower(); }, pal::accentEnv);

    trigger.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("trigger",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    trigger.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int meterH = 14;
        const int rowH   = naturalCardH (KnobSize::Small);
        const int totalNeeded = rowH + 6 + meterH;
        const bool meterFits = bounds.getHeight() >= totalNeeded;
        triggerMeter->setVisible (meterFits);

        const int blockH = meterFits ? totalNeeded : rowH;
        const int topPad = juce::jmax (0, (bounds.getHeight() - blockH) / 2);
        bounds.removeFromTop (topPad);

        auto row = bounds.removeFromTop (rowH);
        layoutRowCentered (row, {
            { thresh, 70 }, { freeRun, 70 }, { rate, 70 }, { fireDot, 28 }
        }, 10, 360);

        if (meterFits)
        {
            bounds.removeFromTop (6);
            triggerMeter->setBounds (bounds.removeFromTop (meterH).reduced (8, 0));
        }
    });
}

void SK4nAudioProcessorEditor::buildMaster()
{
    master.panel = std::make_unique<SectionPanel> ("Master", pal::accentPrimary);
    addAndMakeVisible (*master.panel);

    auto* gain  = addKnob (master, "masterGain",  "Gain",    fmt::db,  KnobSize::Small);
    auto* dw    = addKnob (master, "dryWet",      "Dry/Wet", fmt::pct, KnobSize::Small);
    auto* depth = addKnob (master, "ampEnvDepth", "AmpEnv",  fmt::pct, KnobSize::Small);

    master.panel->setDiceCallback ([this] (bool c)
    { processorRef.randomizer.randomizeSection ("master",
        c ? Randomizer::Mode::Constrained : Randomizer::Mode::Full); });

    master.panel->setLayout ([=] (juce::Rectangle<int> bounds)
    {
        const int rowH = naturalCardH (KnobSize::Small);
        const int top  = juce::jmax (0, (bounds.getHeight() - rowH) / 2);
        bounds.removeFromTop (top);
        auto row = bounds.removeFromTop (rowH);
        layoutRowCentered (row, {
            { gain, 64 }, { dw, 64 }, { depth, 64 }
        }, 8, 360);
    });
}

// Both mode-switched panels lay their two control sets into the SAME rectangle (see buildFilter
// and buildEchoFlanger, which call layoutRowCentered twice over one `knobsRow`). Every control is
// added with addAndMakeVisible, so unless exactly one set is hidden here they draw on top of each
// other and their labels and LCD readouts render as unreadable overlap. The shared Mix knob
// belongs to neither set and stays visible in both modes.
void SK4nAudioProcessorEditor::applyFilterModeVisibility()
{
    const bool cabinet = filterModeSwitch != nullptr && filterModeSwitch->getCurrentIndex() == 1;
    for (auto* c : filter8PControls) if (c != nullptr) c->setVisible (! cabinet);
    for (auto* c : cabinetControls)  if (c != nullptr) c->setVisible (cabinet);
}

void SK4nAudioProcessorEditor::applyEchoModeVisibility()
{
    const bool flanger = echoModeSwitch != nullptr && echoModeSwitch->getCurrentIndex() == 1;
    for (auto* c : echoControls)    if (c != nullptr) c->setVisible (! flanger);
    for (auto* c : flangerControls) if (c != nullptr) c->setVisible (flanger);
}

// ============================================================================

void SK4nAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (zqsfx::ui::gradients::chassis (getLocalBounds().toFloat()));
    g.fillRect (getLocalBounds());

    // presetLabel's LCD glass (see its markAsLcdReadout() tag in the constructor): a small
    // phosphor screen behind the value readout, exactly like KnobControl's own value readouts.
    if (! presetLabel.getBounds().isEmpty())
        zqsfx::ui::LookAndFeel::drawScreen (g, presetLabel.getBounds().toFloat(), false);

    // cpuLoadLabel's LCD glass, same treatment.
    if (! cpuLoadLabel.getBounds().isEmpty())
        zqsfx::ui::LookAndFeel::drawScreen (g, cpuLoadLabel.getBounds().toFloat(), false);
}

void SK4nAudioProcessorEditor::showAboutBox()
{
    // ASCII-only (product spec item 6); product name + version from the real build
    // (JucePlugin_VersionString, generated from CMakeLists.txt's project(... VERSION ...)), not
    // a hand-maintained literal that could drift from it.
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "About SK4n",
        juce::String ("SK4n ") + JucePlugin_VersionString +
            "\n\nZQ SFX - https://www.zq-sfx.com - connect@zq-sfx.com\n"
            "Free software under GPL-3.0-or-later. Built with JUCE.\n"
            "Fonts: Barlow Condensed, VT323, IBM Plex Mono (SIL OFL).\n"
            "Knobs: CC0 designs from the g200kg KnobGallery.",
        "Close", this);
}

void SK4nAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (kPad);

    // Header. Far right first: the ZQ SFX mark (style guide section 5: "far end from the
    // product wordmark", the mark's About-box trigger), never shrunk; help and randomize sit
    // just inside it.
    auto headerBounds = area.removeFromTop (kHeaderH);
    logo.setBounds (headerBounds.removeFromRight (36).withSizeKeepingCentre (28, 28));
    headerBounds.removeFromRight (6);
    helpButton.setBounds (headerBounds.removeFromRight (32).reduced (4));
    headerBounds.removeFromRight (4);
    if (masterDice != nullptr)
        masterDice->setBounds (headerBounds.removeFromRight (28).reduced (3));
    headerBounds.removeFromRight (8);

    // Title (left). If the header is tight at the minimum window size, shrink the title area,
    // never the logo (style guide section 5 / product spec item 6).
    const int titleW = juce::jmin (160, juce::jmax (80, headerBounds.getWidth() / 3));
    auto titleArea = headerBounds.removeFromLeft (titleW);
    titleLabel   .setBounds (titleArea.removeFromTop    (22).reduced (4, 0));
    subtitleLabel.setBounds (titleArea.removeFromBottom (14).reduced (4, 0));

    // Preset name (center)
    presetLabel.setBounds (headerBounds);

    // CPU load readout: a thin row of its own directly under the header, so nothing above has to
    // shrink to make room for it. kCompactH already includes this row's height (see the constant
    // definitions above), so the rest of the layout below is unaffected.
    area.removeFromTop (kHeaderRowGap);
    auto cpuRow = area.removeFromTop (kCpuRowH);
    cpuLoadLabel.setBounds (cpuRow.removeFromRight (70));

    // Buffer display
    auto bufferBounds = area.removeFromTop (kBufferH);
    bufferDisplay->setBounds (bufferBounds);
    area.removeFromTop (kRowGap);

    // Vertical budget between the buffer display and the bottom edge, top to bottom:
    //   morpher region | gap | performance row | disclosure bar | (open: gap | disclosure viewport)
    // Closed, the morpher takes everything left over (as it always did). Open, the panels get the
    // room first -- at their natural size if it fits, otherwise the viewport scrolls -- and the
    // morpher keeps whatever is left, or is hidden when that is under kMorpherMinH.
    const bool open = openDisclosure != Disclosure::None;
    const int  belowMorpher = kRowGap + kPerfRowH + kDisclosureRowH + (open ? kRowGap : 0);
    const int  avail = juce::jmax (0, area.getHeight() - belowMorpher);   // morpher + viewport

    int morpherRegionH = avail;
    if (open)
    {
        const int spare = avail - naturalContentHeight (openDisclosure, area.getWidth());
        morpherRegionH = spare >= kMorpherMinH ? juce::jmin (spare, kMorpherMaxH) : 0;
    }

    auto morpherArea = area.removeFromTop (morpherRegionH);
    area.removeFromTop (kRowGap);
    auto perfArea = area.removeFromTop (kPerfRowH);
    auto disclosureBar = area.removeFromTop (kDisclosureRowH);
    if (open) area.removeFromTop (kRowGap);
    auto viewportArea = area;   // empty when closed

    // Morpher fills its region.
    {
        circularMorpher->setVisible (morpherRegionH > 0);
        const int availW = morpherArea.getWidth();
        const int availH = morpherArea.getHeight();
        const int side   = juce::jmax (0, juce::jmin (kMorpherMaxH, juce::jmin (availW, availH)));
        const int x = morpherArea.getX() + (availW - side) / 2;
        const int y = morpherArea.getY() + (availH - side) / 2;
        circularMorpher->setBounds (x, y, side, side);
    }

    // Performance row layout: 6 macros | spacer | Dry/Wet | Gain | Meter
    {
        auto row = perfArea.reduced (10, 6);
        const int macroW = 90;
        const int gap    = 4;
        const int meterW = 38;

        // Right side first
        auto right = row.removeFromRight (meterW + 8 + 80 + 8 + 80);
        right.removeFromLeft (8);
        outputMeter ->setBounds (right.removeFromRight (meterW));
        right.removeFromRight (8);
        kMasterGain ->setBounds (right.removeFromRight (80));
        right.removeFromRight (8);
        kDryWet     ->setBounds (right.removeFromRight (80));

        // Left side: 6 macros
        const int macrosTotal = macroW * 6 + gap * 5;
        const int macroX = row.getX() + juce::jmax (0, (row.getWidth() - macrosTotal) / 2);
        int x = macroX;
        for (auto* k : { kMovement.get(), kPitch.get(), kColor.get(),
                         kDrive.get(),    kSpace.get(), kTexture.get() })
        {
            k->setBounds (x, row.getY(), macroW, row.getHeight());
            x += macroW + gap;
        }
    }

    // Disclosure bar (5 buttons across one band)
    {
        const int drW = (disclosureBar.getWidth() - 4 * 6) / 5;
        int drx = disclosureBar.getX();
        for (auto& d : disclosures)
        {
            if (d != nullptr)
                d->setBounds (drx, disclosureBar.getY(), drW, disclosureBar.getHeight());
            drx += drW + 6;
        }
    }

    // Disclosure content
    disclosureViewport.setVisible (open);
    if (open)
    {
        disclosureViewport.setBounds (viewportArea);
        layoutDisclosureContent (viewportArea.getWidth(), viewportArea.getHeight());
    }
    else
    {
        hideAllSectionPanels();
    }

    if (helpOverlay != nullptr && helpOverlay->isVisible())
        layoutHelpOverlay();

    // Persist window size (0 means "not yet known / use default"; see
    // PluginProcessor::getEditorWidth()/getEditorHeight() and getStateInformation()).
    processorRef.setEditorSize (getWidth(), getHeight());
}

// Lays the open disclosure's panels out inside disclosureContent. If their natural height fits
// the viewport they are stretched to fill it (no scrollbar); otherwise the content keeps its
// natural height and the viewport scrolls. The scrollbar's width is reserved only when it is
// needed, and the panels are re-flowed at the narrower width.
void SK4nAudioProcessorEditor::layoutDisclosureContent (int viewportW, int viewportH)
{
    hideAllSectionPanels();
    if (openDisclosure == Disclosure::None) return;

    int w = viewportW;
    int natural = naturalContentHeight (openDisclosure, w);
    const bool scrolls = natural > viewportH;
    if (scrolls)
    {
        w = viewportW - disclosureViewport.getScrollBarThickness();
        natural = naturalContentHeight (openDisclosure, w);
    }

    const auto items = flowItemsFor (openDisclosure, w);
    for (auto& item : items)
        for (auto* c : item.stack)
            if (c != nullptr) c->setVisible (true);

    disclosureContent.setSize (w, scrolls ? natural : viewportH);
    flowLayout (items, w, kPanelGap, scrolls ? 0 : viewportH - natural, true);
}

void SK4nAudioProcessorEditor::timerCallback()
{
    if (oscAPitchKnob != nullptr) oscAPitchKnob->setAuxText (fmt::hz (processorRef.getOscAFrequencyHz()));
    if (oscBPitchKnob != nullptr) oscBPitchKnob->setAuxText (fmt::hz (processorRef.getOscBFrequencyHz()));

    if (processorRef.consumeTriggerFired() && fireDot != nullptr)
        fireDot->trigger();

    // Preset name: show A:N or "A->B" if mid-morph.
    int slotA = 0, slotB = 0;
    if (auto* a = processorRef.apvts.getRawParameterValue ("snapshotA")) slotA = juce::jlimit (0, 7, (int) a->load());
    if (auto* b = processorRef.apvts.getRawParameterValue ("snapshotB")) slotB = juce::jlimit (0, 7, (int) b->load());
    const float pos = processorRef.getCurrentMorphPos();

    juce::String preset;
    if (slotA == slotB || pos < 0.02f)
        preset = "Snapshot " + juce::String (slotA + 1);
    else if (pos > 0.98f)
        preset = "Snapshot " + juce::String (slotB + 1);
    else
        preset = "Snapshot " + juce::String (slotA + 1) + " -> " + juce::String (slotB + 1);
    presetLabel.setText (preset, juce::dontSendNotification);
}
