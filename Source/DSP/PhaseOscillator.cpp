#include "PhaseOscillator.h"
#include <cmath>
#include <algorithm>

namespace sk4n {

void PhaseOscillator::prepare (double sampleRate)
{
    sr      = sampleRate;
    phase   = 0.0;
    lastOut = 0.0f;
    ampWindow = 1.0f;
}

void PhaseOscillator::reset()
{
    phase   = 0.0;
    lastOut = 0.0f;
    ampWindow = 1.0f;
}

float PhaseOscillator::negPCurve (float p)
{
    // p is MIDI semitones (A4 = 69, so p=69 -> 440 Hz, p=60 -> ~262 Hz / C4).
    // Above midi 9 (~13.75 Hz) we use the standard exponential pitch curve.
    // Between -51 and 9 we ease quadratically toward 0 Hz so very low pitches
    // smoothly approach silence instead of becoming arbitrarily slow.
    if (p >= 9.0f)
        return 440.0f * std::pow (2.0f, (p - 69.0f) / 12.0f);

    const float freqAt9 = 440.0f * std::pow (2.0f, (9.0f - 69.0f) / 12.0f);  // ~13.75 Hz
    float t = (p - (-51.0f)) / 60.0f;       // 0 at p=-51, 1 at p=9
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return freqAt9 * t * t;
}

float PhaseOscillator::smoothTriangle (float theta)
{
    const float raw = (2.0f / static_cast<float> (M_PI))
                    * std::asin (std::sin (theta));
    return std::tanh (raw * 1.2f);
}

float PhaseOscillator::bentSaw (float theta01, float bend)
{
    // theta01 in [0, 1)
    const float bent = std::pow (theta01, 1.0f + bend * 2.0f);
    return bent * 2.0f - 1.0f;
}

float PhaseOscillator::sineShaper (float theta, float shape, float& windowOut)
{
    const float base = std::sin (theta);
    if (shape <= 0.5f)
    {
        const float blend = shape * 2.0f;
        const float tri   = smoothTriangle (theta);
        windowOut = 1.0f;
        return base + (tri - base) * blend;
    }
    const float blend = (shape - 0.5f) * 2.0f;
    const float tri   = smoothTriangle (theta);
    float th01 = theta / (2.0f * static_cast<float> (M_PI));
    th01 -= std::floor (th01);
    const float saw   = bentSaw (th01, blend);
    const float window = 1.0f - blend * 0.3f;
    windowOut = window;
    return tri + (saw * window - tri) * blend;
}

float PhaseOscillator::process (float pitchSemis, float fb, float shape)
{
    const float freq = negPCurve (pitchSemis);
    const double inc = static_cast<double> (freq) / sr;

    const double pmInput = static_cast<double> (fb) * static_cast<double> (lastOut);
    phase += inc;
    phase -= std::floor (phase);
    const double modPhase = phase + pmInput;

    const float theta = static_cast<float> (modPhase * 2.0 * M_PI);
    float window = 1.0f;
    const float out = sineShaper (theta, std::clamp (shape, 0.0f, 1.0f), window);
    lastOut   = out;
    ampWindow = window;
    return out;
}

} // namespace sk4n
