#include "SnapshotRow.h"
#include "../PluginProcessor.h"

namespace sk4n_ui {

SnapshotRow::SnapshotRow (SK4nAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < 8; ++i)
    {
        slots[i] = std::make_unique<SlotButton> (i, *this);
        addAndMakeVisible (*slots[i]);
    }

    slotsLabel.setJustificationType (juce::Justification::centredLeft);
    slotsLabel.setFont (font::diagnostic (10.0f));
    slotsLabel.setColour (juce::Label::textColourId, pal::textDim);
    addAndMakeVisible (slotsLabel);

    morphSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    morphSlider.setColour (juce::Slider::rotarySliderFillColourId, pal::accentPrimary);
    addAndMakeVisible (morphSlider);
    morphAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "morphPosition", morphSlider);

    speedSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    speedSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    speedSlider.setColour (juce::Slider::rotarySliderFillColourId, pal::accentFb);
    addAndMakeVisible (speedSlider);
    speedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "morphSpeed", speedSlider);

    speedLabel.setText ("Speed", juce::dontSendNotification);
    speedLabel.setJustificationType (juce::Justification::centredRight);
    speedLabel.setFont (font::label (10.0f));
    speedLabel.setColour (juce::Label::textColourId, pal::textSecondary);
    addAndMakeVisible (speedLabel);

    if (auto* a = processor.apvts.getRawParameterValue ("snapshotA")) currentA = (int) a->load();
    if (auto* b = processor.apvts.getRawParameterValue ("snapshotB")) currentB = (int) b->load();

    startTimerHz (30);
}

SnapshotRow::~SnapshotRow() = default;

void SnapshotRow::timerCallback()
{
    int newA = currentA, newB = currentB;
    if (auto* a = processor.apvts.getRawParameterValue ("snapshotA")) newA = (int) a->load();
    if (auto* b = processor.apvts.getRawParameterValue ("snapshotB")) newB = (int) b->load();
    if (newA != currentA || newB != currentB)
    {
        currentA = newA; currentB = newB;
        for (auto& s : slots) s->repaint();
    }
    slotsLabel.setText ("A:" + juce::String (currentA + 1)
                        + "   B:" + juce::String (currentB + 1),
                        juce::dontSendNotification);
    repaint(); // for the morph dot
}

void SnapshotRow::onSlotClicked (int idx, const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { showSlotMenu (idx); return; }

    // Move B-target to clicked slot, push old B to A; reset morphPosition to 0 to start the morph.
    auto setIntParam = [this] (const juce::String& id, int value)
    {
        if (auto* p = processor.apvts.getParameter (id))
        {
            const float norm = p->convertTo0to1 (static_cast<float> (value));
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    };
    auto setFloatParam = [this] (const juce::String& id, float value)
    {
        if (auto* p = processor.apvts.getParameter (id))
        {
            const float norm = p->convertTo0to1 (value);
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    };

    setIntParam ("snapshotA", currentB);
    setIntParam ("snapshotB", idx);

    setFloatParam ("morphPosition", 0.0f);
    juce::Timer::callAfterDelay (40, [this] {
        if (auto* p = processor.apvts.getParameter ("morphPosition"))
        {
            const float norm = p->convertTo0to1 (1.0f);
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    });
}

void SnapshotRow::showSlotMenu (int idx)
{
    juce::PopupMenu m;
    m.addItem (1, "Save current state to slot " + juce::String (idx + 1));
    m.addItem (2, "Set as A");
    m.addItem (3, "Set as B");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slots[idx].get()),
                     [this, idx] (int result)
    {
        if (result == 1) processor.saveSnapshotToSlot (idx);
        else if (result == 2)
        {
            if (auto* p = processor.apvts.getParameter ("snapshotA"))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
                p->endChangeGesture();
            }
        }
        else if (result == 3)
        {
            if (auto* p = processor.apvts.getParameter ("snapshotB"))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
                p->endChangeGesture();
            }
        }
    });
}

void SnapshotRow::resized()
{
    auto b = getLocalBounds().reduced (4);
    auto top    = b.removeFromTop (40);
    auto bottom = b;

    // Top row: 8 slot buttons + slot label
    const int slotW = 36;
    const int slotPad = 4;
    int x = top.getX();
    for (int i = 0; i < 8; ++i)
    {
        slots[i]->setBounds (x, top.getY() + 2, slotW, slotW - 4);
        x += slotW + slotPad;
    }
    slotsLabel.setBounds (x + 12, top.getY(), 140, 36);

    // Bottom row: morph slider takes most of width, speed slider on right
    const int speedW = 130;
    auto speedRow = bottom.removeFromRight (speedW);
    bottom.removeFromRight (8);
    speedLabel.setBounds (speedRow.removeFromLeft (40));
    speedSlider.setBounds (speedRow);
    morphSlider.setBounds (bottom);
}

void SnapshotRow::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);

    // The morph slider is already drawn by the LookAndFeel.
    // Overlay the slewed currentMorphPos as a small marker.
    const float pos = juce::jlimit (0.0f, 1.0f, processor.getCurrentMorphPos());
    auto sb = morphSlider.getBounds();
    if (sb.isEmpty()) return;
    const float dotX = sb.getX() + pos * sb.getWidth();
    const float dotY = sb.getCentreY();
    juce::Graphics::ScopedSaveState ss (g);
    g.setColour (pal::accentOk);
    g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
    g.setColour (pal::bgBase);
    g.drawEllipse (dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f, 1.0f);
}

void SnapshotRow::SlotButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool isA = (parent.currentA == index);
    const bool isB = (parent.currentB == index);

    juce::Colour fill = pal::bgPanelHi;
    if (isA && isB) fill = zqsfx::ui::colour::pointer.withAlpha (0.30f);
    else if (isA)   fill = pal::accentPrimary.withAlpha (0.30f);
    else if (isB)   fill = zqsfx::ui::colour::silkTitle.withAlpha (0.30f);
    else if (hovered) fill = pal::bgPanelHi.brighter (0.10f);

    g.setColour (fill);
    g.fillRect (b);

    juce::Colour border = pal::borderSubtle;
    if (isA && isB) border = zqsfx::ui::colour::pointer;
    else if (isA)   border = pal::accentPrimary;
    else if (isB)   border = zqsfx::ui::colour::silkTitle;
    g.setColour (border);
    g.drawRect (b, 1.0f);

    g.setColour (pal::textPrimary);
    g.setFont (font::value (12.0f, &getLookAndFeel()));
    g.drawText (juce::String (index + 1), b.toNearestInt(),
                juce::Justification::centred, false);

    if (! parent.processor.morpher.isSnapshotValid (index))
    {
        g.setColour (pal::textDim);
        g.drawLine (b.getX() + 4.0f, b.getBottom() - 4.0f,
                    b.getRight() - 4.0f, b.getBottom() - 4.0f, 1.0f);
    }
}

void SnapshotRow::SlotButton::mouseDown (const juce::MouseEvent& e)
{
    parent.onSlotClicked (index, e);
}

} // namespace sk4n_ui
