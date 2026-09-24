#pragma once

#include <juce_core/juce_core.h>

namespace sk4n {

// Free-running LFO with sine / triangle / sample-and-hold shapes,
// optional symmetry, phase offset, and a fade-in ramp.
// Tempo-sync rate is computed externally and passed in as Hz.
class GlobalLFO
{
public:
    enum class Shape { Sine = 0, Triangle = 1, SampleHold = 2 };

    void prepare (double sampleRate);
    void reset();
    void setShape (Shape s) { shape = s; }
    void setSymmetry (float s) { symmetry = std::clamp (s, -0.99f, 0.99f); }
    void setPhase (float p)    { phaseOffset = p; }
    void setFade (float seconds) { fadeSec = std::max (0.0f, seconds); }
    void retriggerWithFade()   { fadeT = 0.0f; samplesIntoFade = 0; }

    // Returns -1..+1
    float process (float rateHz);

private:
    double sr = 44100.0;
    double phase = 0.0;
    Shape  shape = Shape::Sine;
    float  symmetry = 0.0f;
    float  phaseOffset = 0.0f;
    float  fadeSec = 0.0f;
    float  fadeT = 1.0f;
    int    samplesIntoFade = 0;

    float  shState = 0.0f;
    float  prevPhase = 1.0f;

    juce::Random rng;
};

} // namespace sk4n
