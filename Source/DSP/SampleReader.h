#pragma once

#include "Smoothers.h"

namespace sk4n {

class CircularBuffer;

// Reads from CircularBuffer at (writeIndex - position + window * oscPhase),
// applies a one-pole anti-aliasing LP, and an amplitude window from the
// oscillator's shape (used when the bent-saw shape kicks in).
class SampleReader
{
public:
    void prepare (double sampleRate);
    void reset();

    // Sets the AA filter cutoff (Hz). Call once per block when sample rate or
    // user setting changes; otherwise leave at the default 18 kHz.
    void setAntiAliasCutoff (double hz);

    float read (const CircularBuffer& buf,
                int writeIndex,
                float positionSamples,
                float oscPhase,    // -1..+1
                float window01,
                float ampWindow = 1.0f);

private:
    OnePoleLowpass aaLp;
};

} // namespace sk4n
