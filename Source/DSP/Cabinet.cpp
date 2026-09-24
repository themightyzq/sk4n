#include "Cabinet.h"
#include <cmath>
#include <algorithm>

namespace sk4n {

void Cabinet::prepare (double sampleRate)
{
    sr = sampleRate;
    loShelfLP.setSampleRate (sampleRate);
    loShelfLP.setCutoff (200.0);
    hiCutLP.setSampleRate (sampleRate);
    hiCutLP.setCutoff (12000.0);
    reset();
}

void Cabinet::reset()
{
    loShelfLP.reset();
    hiCutLP.reset();
}

float Cabinet::process (float in,
                        float drive,
                        float fold,
                        float tilt,
                        float hiCutHz,
                        float levelDb)
{
    drive  = std::clamp (drive, 0.0f, 1.0f);
    fold   = std::clamp (fold, 0.0f, 1.0f);
    tilt   = std::clamp (tilt, -1.0f, 1.0f);
    hiCutHz = std::clamp (hiCutHz, 100.0f, 20000.0f);

    // Tilt EQ: split into low band (LP at 200 Hz) and high band (in - low),
    // then weight them oppositely by tilt.
    const float low  = loShelfLP.process (in);
    const float high = in - low;
    const float tilted = low * (1.0f - tilt * 0.5f) + high * (1.0f + tilt * 0.5f);

    // Drive: scale up to 10x
    const float driven = tilted * (1.0f + drive * 9.0f);

    // Hyperbolic saturation
    const float sat = std::tanh (driven);

    // Sine fold: increases harmonic content as fold increases
    const float folded = std::sin (sat * (1.0f + fold * 3.0f));

    // Hi-cut LP after distortion
    hiCutLP.setCutoff (static_cast<double> (hiCutHz));
    const float lp = hiCutLP.process (folded);

    // Output level (dB to linear)
    const float gain = std::pow (10.0f, std::clamp (levelDb, -24.0f, 12.0f) / 20.0f);
    return lp * gain;
}

} // namespace sk4n
