#include "SectionPanel.h"

namespace sk4n_ui {

SectionPanel::SectionPanel (const juce::String& t, juce::Colour a)
    : title (t.toUpperCase()), accent (a)
{
    setOpaque (false);
}

void SectionPanel::setLayout (std::function<void (juce::Rectangle<int>)> fn)
{
    layoutFn = std::move (fn);
    if (layoutFn) layoutFn (getContentBounds());
}

void SectionPanel::setHeaderRight (const juce::String& t)
{
    headerRight = t;
    repaint();
}

void SectionPanel::setDiceCallback (std::function<void (bool)> cb)
{
    if (! cb)
    {
        diceButton.reset();
        resized();
        repaint();
        return;
    }

    if (diceButton == nullptr)
    {
        diceButton = std::make_unique<DiceButton>();
        addAndMakeVisible (*diceButton);
    }
    auto cbCopy = cb;
    diceButton->setOnClick           ([cbCopy] { cbCopy (false); });
    diceButton->setOnConstrainedClick([cbCopy] { cbCopy (true);  });
    resized();
}

void SectionPanel::setDiceTooltip (const juce::String& tip)
{
    if (diceButton != nullptr) diceButton->setTooltipText (tip);
}

juce::Rectangle<int> SectionPanel::getContentBounds() const
{
    auto b = getLocalBounds();
    b.removeFromTop (kHeaderH);
    b.removeFromLeft (kAccentBarW);
    return b.reduced (kContentPadX, kContentPadY);
}

// Drawn like zqsfx::ui::Panel (gradient face, hard 1px border, platform-bold tracked title over
// a ruleTitle hairline, no rounded corners -- style guide section 6) with one SK4n-specific
// addition Panel.h has no notion of: a short accent bar beside the title carrying the section's
// channel colour (style guide section 3 -- the colour is always paired with the title text, so
// it never carries meaning alone).
void SectionPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (zqsfx::ui::gradients::panel (b));
    g.fillRect (b);
    g.setColour (juce::Colours::white.withAlpha (0.04f)); // inner top highlight
    g.fillRect (b.withHeight (1.0f).translated (0.0f, 1.0f));
    g.setColour (zqsfx::ui::colour::panelBorder);
    g.drawRect (b, 1.0f);

    auto accentArea = b.withWidth ((float) kAccentBarW);
    g.setColour (accent);
    g.fillRect (accentArea);

    auto header = juce::Rectangle<int> (kAccentBarW, 0,
                                        getWidth() - kAccentBarW, kHeaderH);
    g.setColour (zqsfx::ui::colour::silkTitle);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).withExtraKerningFactor (0.27f));
    g.drawText (title, header.reduced (8, 0),
                juce::Justification::centredLeft, false);

    if (headerRight.isNotEmpty())
    {
        auto rightArea = header;
        if (diceButton != nullptr)
            rightArea.removeFromRight (kDiceSize + 8);
        g.setColour (pal::textDim);
        g.setFont (font::diagnostic (9.5f, &getLookAndFeel()));
        g.drawText (headerRight, rightArea.reduced (8, 0),
                    juce::Justification::centredRight, false);
    }

    g.setColour (zqsfx::ui::colour::ruleTitle);
    g.fillRect (juce::Rectangle<int> (kAccentBarW, kHeaderH, getWidth() - kAccentBarW, 1));
}

void SectionPanel::resized()
{
    if (diceButton != nullptr)
    {
        // Use the full kDiceSize so the dice is comfortably clickable.
        const int diceH = kDiceSize;
        const int y = (kHeaderH - diceH) / 2;
        diceButton->setBounds (getWidth() - diceH - 8, y, diceH, diceH);
    }
    if (layoutFn) layoutFn (getContentBounds());
}

} // namespace sk4n_ui
