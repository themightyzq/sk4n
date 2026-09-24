#include "ReverbStage.h"
#include <algorithm>

namespace sk4n {

void ReverbStage::prepare (double sampleRate)
{
    sr = sampleRate;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    loCutL.setSampleRate (sampleRate);
    loCutR.setSampleRate (sampleRate);
}

void ReverbStage::reset()
{
    reverb.reset();
    loCutL.reset();
    loCutR.reset();
}

void ReverbStage::processBlock (float* l, float* r, int numSamples,
                                float size01, float loCutHz, float hiCutHz, float mix01)
{
    const float size = std::clamp (size01, 0.0f, 1.0f);
    const float mix  = std::clamp (mix01, 0.0f, 1.0f);
    const float hc   = std::clamp (hiCutHz, 100.0f, 20000.0f);

    juce::Reverb::Parameters p;
    p.roomSize   = size;
    p.damping    = std::clamp (1.0f - hc / 20000.0f, 0.0f, 1.0f);
    p.wetLevel   = mix;
    p.dryLevel   = 1.0f - mix;
    p.width      = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);

    loCutL.setCutoff (std::clamp (static_cast<double> (loCutHz), 10.0, 5000.0));
    loCutR.setCutoff (std::clamp (static_cast<double> (loCutHz), 10.0, 5000.0));

    // Pre-HP per sample
    for (int i = 0; i < numSamples; ++i)
    {
        l[i] = loCutL.process (l[i]);
        r[i] = loCutR.process (r[i]);
    }

    reverb.processStereo (l, r, numSamples);
}

} // namespace sk4n
