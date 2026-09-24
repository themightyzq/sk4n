#pragma once

#include <vector>
#include "Smoothers.h"

namespace sk4n {

// Single-channel tuned delay line with feedback and lo-cut on feedback path.
// Tune is in semitones (delay time = sr / midiToFreq(tune)).
// At tuneSemis >= bypassThreshold, signal passes through unmodified.
class TunedDelay
{
public:
    void prepare (double sampleRate, float maxDelaySamples = 8192.0f);
    void reset();

    // tuneSemis: 0..200 (clamped). At >= 199, bypass.
    // feedbackAmt: 0..1
    // loCutHz: 10..5000
    float process (float input,
                   float tuneSemis,
                   float feedbackAmt,
                   float loCutHz);

private:
    inline float readLinear (float delaySamples) const;

    double sr = 44100.0;
    std::vector<float> buf;
    int   bufSize = 0;
    int   writeIdx = 0;

    OnePoleHighpass fbHP;
    float lastOutput = 0.0f;
};

} // namespace sk4n
