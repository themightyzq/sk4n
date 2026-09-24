#pragma once

namespace sk4n {

// Ring-mod-style amplitude modulation.
// modulator = mix(oscA, oscB, blend), optionally squared, multiplied by sample.
class AMSection
{
public:
    static inline float process (float sampleIn,
                                 float oscA,
                                 float oscB,
                                 float abBlend01,
                                 bool  squareMod)
    {
        float mod = oscA + (oscB - oscA) * abBlend01;
        if (squareMod) mod *= mod;
        return sampleIn * mod;
    }
};

} // namespace sk4n
