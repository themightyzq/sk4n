#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SK4nLookAndFeel.h"

class SK4nAudioProcessor;

namespace sk4n_ui {

class CircularMorpher : public juce::Component, private juce::Timer
{
public:
    explicit CircularMorpher (SK4nAudioProcessor& processor);
    ~CircularMorpher() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;
    void mouseMove  (const juce::MouseEvent&) override;

    static constexpr int kNumPads = 8;

private:
    void timerCallback() override;

    int  hitTestPad (juce::Point<float> pos) const;
    juce::Point<float> padCentre (int padIndex) const;
    float padRadius() const;
    juce::Point<float> centreOfWidget() const;

    void clickPad   (int padIndex, const juce::MouseEvent& e);
    void showRightClickMenu (int padIndex);
    void setIntParam   (const juce::String& id, int value);
    void setFloatParam (const juce::String& id, float value);

    SK4nAudioProcessor& processor;

    juce::Slider speedSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttach;
    juce::Label  speedLabel;
    juce::Label  abLabel;

    int hoveredPad = -1;

    static constexpr int kPadSize = 64;
};

} // namespace sk4n_ui
