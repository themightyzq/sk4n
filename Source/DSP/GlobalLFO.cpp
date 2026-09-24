#include "GlobalLFO.h"
#include <cmath>
#include <algorithm>

namespace sk4n {

void GlobalLFO::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void GlobalLFO::reset()
{
    phase = 0.0;
    shState = 0.0f;
    prevPhase = 1.0f;
    fadeT = 1.0f;
    samplesIntoFade = 0;
}

float GlobalLFO::process (float rateHz)
{
    const float r = std::clamp (rateHz, 0.001f, 50.0f);
    phase += static_cast<double> (r) / sr;
    if (phase >= 1.0) phase -= std::floor (phase);

    // Phase offset (-0.5..+0.5)
    double p = phase + static_cast<double> (phaseOffset);
    p -= std::floor (p);

    // Symmetry: skews phase to bias toward up-half or down-half
    const double sym = (static_cast<double> (symmetry) + 1.0) * 0.5;  // 0..1
    double pSkew;
    if (sym < 1.0e-6) pSkew = 0.0;
    else if (sym > 1.0 - 1.0e-6) pSkew = 1.0;
    else if (p < sym) pSkew = 0.5 * p / sym;
    else              pSkew = 0.5 + 0.5 * (p - sym) / (1.0 - sym);

    float out = 0.0f;
    switch (shape)
    {
        case Shape::Sine:
            out = static_cast<float> (std::sin (pSkew * 2.0 * M_PI));
            break;
        case Shape::Triangle:
            out = static_cast<float> (1.0 - 4.0 * std::fabs (pSkew - 0.5));
            break;
        case Shape::SampleHold:
        {
            // Sample new value when phase wraps
            if (prevPhase > p) // wrapped
                shState = rng.nextFloat() * 2.0f - 1.0f;
            out = shState;
            break;
        }
    }
    prevPhase = static_cast<float> (p);

    // Fade-in
    if (fadeT < 1.0f)
    {
        const float fadeSamples = std::max (1.0f, fadeSec * static_cast<float> (sr));
        fadeT = std::clamp (static_cast<float> (samplesIntoFade) / fadeSamples, 0.0f, 1.0f);
        ++samplesIntoFade;
        out *= fadeT;
    }

    return out;
}

} // namespace sk4n
