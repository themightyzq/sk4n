#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Smoothers.h"

namespace sk4n {

// Wraps juce::Reverb with pre-HP and post-mapped HiCut → damping.
class ReverbStage
{
public:
    void prepare (double sampleRate);
    void reset();

    // Block-rate processing (juce::Reverb operates per-block).
    void processBlock (float* l, float* r, int numSamples,
                       float size01, float loCutHz, float hiCutHz, float mix01);

private:
    juce::Reverb reverb;
    OnePoleHighpass loCutL, loCutR;
    double sr = 44100.0;
};

} // namespace sk4n
