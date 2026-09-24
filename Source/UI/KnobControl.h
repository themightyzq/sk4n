#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "SK4nLookAndFeel.h"
#include "Randomizer.h"

namespace sk4n_ui {

enum class KnobSize { Tiny, Small, Medium, Large, XL };
enum class FillStyle { Unipolar, Bipolar };

inline int diameterFor (KnobSize s)
{
    // Pass 9 retier: Tiny floor at 32 (above pro-plugin minimum); secondary
    // tiers bumped accordingly. Large / XL unchanged (already correctly sized).
    switch (s)
    {
        case KnobSize::Tiny:   return 32;
        case KnobSize::Small:  return 40;
        case KnobSize::Medium: return 52;
        case KnobSize::Large:  return 64;
        case KnobSize::XL:     return 80;
    }
    return 52;
}

// A juce::Slider subclass that lets a parent intercept shift-click
// (used for the lock toggle on KnobControl).
class LockableSlider : public juce::Slider
{
public:
    std::function<bool (const juce::MouseEvent&)> shiftClickHandler;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isShiftDown() && shiftClickHandler && shiftClickHandler (e))
            return;
        juce::Slider::mouseDown (e);
    }
};

class KnobControl : public juce::Component,
                    public juce::SettableTooltipClient,
                    public Randomizer::Listener,
                    private juce::Slider::Listener,
                    private juce::Timer
{
public:
    KnobControl (juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramID,
                 const juce::String& displayName,
                 std::function<juce::String (float)> formatter,
                 KnobSize size = KnobSize::Medium);

    ~KnobControl() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    void setAccentColor (juce::Colour c);
    void setAuxText     (const juce::String& aux);
    void setShowName    (bool s) { showName = s; resized(); repaint(); }
    void setFillStyle   (FillStyle s);
    void setTooltipText (const juce::String& t);
    void setAccessibility (const juce::String& title, const juce::String& description);

    void connectToRandomizer (Randomizer& r);
    bool isLocked() const { return locked; }

    juce::Slider& getSlider() { return slider; }

    // Randomizer::Listener
    void locksChanged() override;

private:
    void sliderValueChanged (juce::Slider*) override;
    void timerCallback() override;
    void updateText();

    LockableSlider slider;
    juce::Label    nameLabel;
    juce::Label    valueLabel;
    juce::Label    auxLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    std::function<juce::String (float)> fmt;
    KnobSize     size;
    juce::Colour accent;
    juce::String auxText;
    juce::String paramID;
    Randomizer*  randomizer = nullptr;
    bool         showName   = true;
    bool         wasFocused = false;
    bool         locked     = false;
};

} // namespace sk4n_ui
