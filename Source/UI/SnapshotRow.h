#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

class SK4nAudioProcessor;

namespace sk4n_ui {

class SnapshotRow : public juce::Component, private juce::Timer
{
public:
    explicit SnapshotRow (SK4nAudioProcessor& p);
    ~SnapshotRow() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void onSlotClicked (int idx, const juce::MouseEvent&);
    void showSlotMenu (int idx);

    class SlotButton : public juce::Component
    {
    public:
        SlotButton (int idx, SnapshotRow& owner) : index (idx), parent (owner) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

        int  index;
        bool hovered = false;
        SnapshotRow& parent;
    };

    SK4nAudioProcessor& processor;

    std::array<std::unique_ptr<SlotButton>, 8> slots;
    juce::Label slotsLabel;

    juce::Slider morphSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttach;

    juce::Slider speedSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttach;
    juce::Label  speedLabel;

    int currentA = 0;
    int currentB = 1;
};

} // namespace sk4n_ui
