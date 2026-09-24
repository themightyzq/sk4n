#include "CircularMorpher.h"
#include "../PluginProcessor.h"

#include <cmath>

namespace sk4n_ui {

CircularMorpher::CircularMorpher (SK4nAudioProcessor& p) : processor (p)
{
    speedSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    speedSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    speedSlider.setColour (juce::Slider::rotarySliderFillColourId, pal::accentFb);
    addAndMakeVisible (speedSlider);
    speedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "morphSpeed", speedSlider);

    abLabel.setJustificationType (juce::Justification::centred);
    abLabel.setFont (font::value (11.0f));
    abLabel.setColour (juce::Label::textColourId, pal::textPrimary);
    addAndMakeVisible (abLabel);

    speedLabel.setJustificationType (juce::Justification::centred);
    speedLabel.setFont (font::diagnostic (9.5f));
    speedLabel.setColour (juce::Label::textColourId, pal::textSecondary);
    speedLabel.setText ("Speed", juce::dontSendNotification);
    addAndMakeVisible (speedLabel);

    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    // Accessibility floor (product spec item 8): a custom component showing data (and, here,
    // taking clicks) gets setAccessible(true) plus a title and description.
    setAccessible (true);
    setTitle ("Snapshot morpher");
    setDescription ("Eight snapshot pads arranged in a ring. Click a pad to morph into it; "
                    "shift-click to set it as snapshot A without morphing; right-click for more options.");

    startTimerHz (30);
}

CircularMorpher::~CircularMorpher() = default;

juce::Point<float> CircularMorpher::centreOfWidget() const
{
    return getLocalBounds().toFloat().getCentre();
}

float CircularMorpher::padRadius() const
{
    const float w = (float) getWidth();
    const float h = (float) getHeight();
    const float minDim = juce::jmin (w, h);
    // Pads sit on a ring such that the pad rectangles fit within the widget.
    return minDim * 0.5f - kPadSize * 0.5f - 6.0f;
}

juce::Point<float> CircularMorpher::padCentre (int padIndex) const
{
    const auto c = centreOfWidget();
    const float r = padRadius();
    // Convention: 270 deg = top, increasing clockwise. Pad i = 270 + i*45.
    const float deg = 270.0f + static_cast<float> (padIndex) * 45.0f;
    const float rad = deg * juce::MathConstants<float>::pi / 180.0f;
    return { c.x + r * std::cos (rad),
             c.y + r * std::sin (rad) };
}

int CircularMorpher::hitTestPad (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumPads; ++i)
    {
        const auto c = padCentre (i);
        const juce::Rectangle<float> rect (c.x - kPadSize * 0.5f,
                                            c.y - kPadSize * 0.5f,
                                            (float) kPadSize, (float) kPadSize);
        if (rect.contains (pos)) return i;
    }
    return -1;
}

void CircularMorpher::mouseEnter (const juce::MouseEvent&) {}
void CircularMorpher::mouseExit  (const juce::MouseEvent&) { if (hoveredPad != -1) { hoveredPad = -1; repaint(); } }

void CircularMorpher::mouseMove (const juce::MouseEvent& e)
{
    const int hit = hitTestPad (e.position);
    if (hit != hoveredPad) { hoveredPad = hit; repaint(); }
}

void CircularMorpher::mouseDown (const juce::MouseEvent& e)
{
    const int hit = hitTestPad (e.position);
    if (hit < 0) return;

    if (e.mods.isRightButtonDown())
    {
        showRightClickMenu (hit);
        return;
    }
    clickPad (hit, e);
}

void CircularMorpher::setIntParam (const juce::String& id, int value)
{
    if (auto* p = processor.apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (static_cast<float> (value));
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void CircularMorpher::setFloatParam (const juce::String& id, float value)
{
    if (auto* p = processor.apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (value);
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void CircularMorpher::clickPad (int padIndex, const juce::MouseEvent& e)
{
    int currentB = 0;
    if (auto* raw = processor.apvts.getRawParameterValue ("snapshotB"))
        currentB = juce::jlimit (0, kNumPads - 1, (int) raw->load());

    if (e.mods.isShiftDown())
    {
        // Shift-click: set as A only.
        setIntParam ("snapshotA", padIndex);
        return;
    }

    // Default: previous B becomes A; pad becomes the new B; morph from 0 to 1.
    setIntParam   ("snapshotA",     currentB);
    setIntParam   ("snapshotB",     padIndex);
    setFloatParam ("morphPosition", 0.0f);

    // Schedule morph to 1.0 a moment later so the slewing is audible.
    juce::Component::SafePointer<CircularMorpher> safe (this);
    juce::Timer::callAfterDelay (40, [safe]
    {
        if (safe == nullptr) return;
        safe->setFloatParam ("morphPosition", 1.0f);
    });
}

void CircularMorpher::showRightClickMenu (int padIndex)
{
    juce::PopupMenu m;
    m.addItem (1, "Save current state to slot " + juce::String (padIndex + 1));
    m.addItem (2, "Reset slot " + juce::String (padIndex + 1));
    m.addSeparator();
    m.addItem (3, "Set as A");
    m.addItem (4, "Set as B");

    juce::Component::SafePointer<CircularMorpher> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe, padIndex] (int result)
    {
        if (safe == nullptr) return;
        if (result == 1) safe->processor.saveSnapshotToSlot (padIndex);
        else if (result == 2) safe->processor.morpher.populateDefaults();   // restore defaults
        else if (result == 3) safe->setIntParam ("snapshotA", padIndex);
        else if (result == 4) safe->setIntParam ("snapshotB", padIndex);
    });
}

void CircularMorpher::resized()
{
    const auto centre = centreOfWidget();

    // Centre stack: A:N B:N text on top, Speed label, Speed slider.
    const int textW = 140;
    abLabel.setBounds (juce::Rectangle<int>().withCentre (centre.toInt())
                                              .withSizeKeepingCentre (textW, 18)
                                              .translated (0, -30));

    speedLabel.setBounds (juce::Rectangle<int>().withCentre (centre.toInt())
                                                 .withSizeKeepingCentre (70, 14)
                                                 .translated (0, -4));

    speedSlider.setBounds (juce::Rectangle<int>().withCentre (centre.toInt())
                                                  .withSizeKeepingCentre (110, 20)
                                                  .translated (0, 18));
}

// This is the plugin's centerpiece custom control -- a click-pad snapshot selector -- one of
// the style guide's named "custom displays (spectrum, meters, waveform, XY pads)": phosphor
// screen background, hard-edged pads (no rounded corners).
void CircularMorpher::paint (juce::Graphics& g)
{
    zqsfx::ui::LookAndFeel::drawScreen (g, getLocalBounds().toFloat(), false);

    int currentA = 0, currentB = 1;
    if (auto* a = processor.apvts.getRawParameterValue ("snapshotA")) currentA = juce::jlimit (0, 7, (int) a->load());
    if (auto* b = processor.apvts.getRawParameterValue ("snapshotB")) currentB = juce::jlimit (0, 7, (int) b->load());

    abLabel.setText ("A:" + juce::String (currentA + 1) + "   B:" + juce::String (currentB + 1),
                     juce::dontSendNotification);

    // Faint guide ring through the pad centres
    const auto c = centreOfWidget();
    const float r = padRadius();
    g.setColour (pal::borderSubtle);
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);

    // Axis line A -> B drawn behind the pads
    {
        const auto a = padCentre (currentA);
        const auto b = padCentre (currentB);
        if (currentA != currentB)
        {
            g.setColour (pal::borderAccent.withAlpha (0.45f));
            g.drawLine (a.x, a.y, b.x, b.y, 1.0f);
        }
    }

    // Pads
    for (int i = 0; i < kNumPads; ++i)
    {
        const auto pc = padCentre (i);
        juce::Rectangle<float> rect (pc.x - kPadSize * 0.5f, pc.y - kPadSize * 0.5f,
                                     (float) kPadSize, (float) kPadSize);

        const bool isA = (i == currentA);
        const bool isB = (i == currentB);
        const bool isHover = (i == hoveredPad);
        const bool isValid = processor.morpher.isSnapshotValid (i);

        juce::Colour fill = pal::bgPanelHi;
        if (isA && isB) fill = zqsfx::ui::colour::pointer.withAlpha (0.30f);
        else if (isA)   fill = pal::accentPrimary.withAlpha (0.25f);
        else if (isB)   fill = zqsfx::ui::colour::silkTitle.withAlpha (0.25f);
        else if (isHover) fill = pal::bgPanelHi.brighter (0.10f);

        g.setColour (fill);
        g.fillRect (rect);

        juce::Colour border = pal::borderSubtle;
        float borderW = 1.0f;
        if (isA && isB) { border = zqsfx::ui::colour::pointer;     borderW = 2.0f; }
        else if (isA)   { border = pal::accentPrimary; borderW = 2.0f; }
        else if (isB)   { border = zqsfx::ui::colour::silkTitle;    borderW = 2.0f; }
        else if (isHover) { border = pal::borderAccent; borderW = 1.0f; }

        g.setColour (border);
        g.drawRect (rect, borderW);

        // Number
        g.setColour (isValid ? pal::textPrimary : pal::textDim);
        g.setFont (font::value (14.0f, &getLookAndFeel()));
        g.drawText (juce::String (i + 1), rect.toNearestInt(),
                    juce::Justification::centred, false);

        // A / B tag in top-left corner
        if (isA || isB)
        {
            juce::String tag;
            if (isA && isB) tag = "AB";
            else if (isA)   tag = "A";
            else            tag = "B";
            g.setFont (font::diagnostic (8.5f, &getLookAndFeel()));
            g.setColour (border);
            g.drawText (tag, rect.toNearestInt().reduced (3, 2),
                        juce::Justification::topLeft, false);
        }

        // Empty-slot indicator: dim slash through the pad
        if (! isValid)
        {
            g.setColour (pal::textDim.withAlpha (0.6f));
            g.drawLine (rect.getX() + 6.0f, rect.getBottom() - 6.0f,
                        rect.getRight() - 6.0f, rect.getY() + 6.0f, 1.0f);
        }
    }

    // Position dot
    {
        const auto a = padCentre (currentA);
        const auto b = padCentre (currentB);
        const float t = juce::jlimit (0.0f, 1.0f, processor.getCurrentMorphPos());
        const auto dot = a + (b - a) * t;

        g.setColour (pal::accentPrimary);
        g.fillEllipse (dot.x - 5.0f, dot.y - 5.0f, 10.0f, 10.0f);
        g.setColour (pal::bgBase);
        g.drawEllipse (dot.x - 5.0f, dot.y - 5.0f, 10.0f, 10.0f, 1.5f);
    }
}

void CircularMorpher::timerCallback() { repaint(); }

} // namespace sk4n_ui
