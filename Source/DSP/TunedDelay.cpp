#include "TunedDelay.h"
#include <cmath>
#include <algorithm>

namespace sk4n {

void TunedDelay::prepare (double sampleRate, float maxDelaySamples)
{
    sr = sampleRate;
    bufSize = static_cast<int> (std::ceil (maxDelaySamples));
    if (bufSize < 64) bufSize = 64;
    buf.assign (static_cast<size_t> (bufSize), 0.0f);
    writeIdx = 0;
    fbHP.setSampleRate (sampleRate);
    fbHP.setCutoff (80.0);
    fbHP.reset();
    lastOutput = 0.0f;
}

void TunedDelay::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    writeIdx = 0;
    fbHP.reset();
    lastOutput = 0.0f;
}

float TunedDelay::readLinear (float delaySamples) const
{
    if (bufSize <= 0) return 0.0f;
    float fIdx = static_cast<float> (writeIdx) - delaySamples;
    while (fIdx < 0.0f)               fIdx += static_cast<float> (bufSize);
    while (fIdx >= static_cast<float> (bufSize)) fIdx -= static_cast<float> (bufSize);
    int   i0   = static_cast<int> (fIdx);
    float frac = fIdx - static_cast<float> (i0);
    int   i1   = (i0 + 1) % bufSize;
    return buf[static_cast<size_t> (i0)] * (1.0f - frac)
         + buf[static_cast<size_t> (i1)] * frac;
}

float TunedDelay::process (float input,
                           float tuneSemis,
                           float feedbackAmt,
                           float loCutHz)
{
    if (tuneSemis >= 199.0f)
    {
        // bypass
        lastOutput = input;
        return input;
    }

    const float clampedTune = std::clamp (tuneSemis, 0.0f, 200.0f);
    const float freq = 440.0f * std::pow (2.0f, (clampedTune - 69.0f) / 12.0f);
    float delaySamples = static_cast<float> (sr) / std::max (0.5f, freq);
    delaySamples = std::clamp (delaySamples, 1.0f, static_cast<float> (bufSize - 2));

    const float delayed = readLinear (delaySamples);

    // High-pass the feedback path
    fbHP.setCutoff (std::clamp (static_cast<double> (loCutHz), 10.0, 5000.0));
    const float fb = fbHP.process (delayed) * std::clamp (feedbackAmt, 0.0f, 0.99f);

    const float toWrite = input + fb;
    buf[static_cast<size_t> (writeIdx)] = toWrite;
    writeIdx = (writeIdx + 1) % bufSize;

    lastOutput = delayed;
    return delayed;
}

} // namespace sk4n
