#pragma once

#include <algorithm>
#include <cmath>

namespace sk4n {

// Topology-Preserving Transform state-variable filter (Vadim Zavalishin / Andy Simper).
// Single 2-pole section. Use cascaded for steeper slopes.
class SVFFilter
{
public:
    void setSampleRate (double sr) { fs = sr; recompute(); }
    void setCutoff (float hz)      { cutoff = hz; recompute(); }
    void setResonance (float r01)  { reso = std::clamp (r01, 0.0f, 0.99f); recompute(); }
    void reset()                   { ic1eq = 0.0f; ic2eq = 0.0f; }

    inline void process (float in, float& lp, float& hp, float& bp)
    {
        const float v3 = in - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        lp = v2;
        hp = in - k * v1 - v2;
        bp = v1;
    }

    inline float processLP (float in) { float lp, hp, bp; process (in, lp, hp, bp); return lp; }
    inline float processHP (float in) { float lp, hp, bp; process (in, lp, hp, bp); return hp; }

private:
    void recompute()
    {
        const float fcLim = std::clamp (cutoff, 5.0f, static_cast<float> (fs) * 0.45f);
        const float g = std::tan (static_cast<float> (M_PI) * fcLim / static_cast<float> (fs));
        // Map reso 0..0.99 to k 2..0.04 (lower k => higher Q)
        k = 2.0f - 1.96f * reso;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double fs = 44100.0;
    float  cutoff = 1000.0f, reso = 0.0f;
    float  a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 2.0f;
    float  ic1eq = 0.0f, ic2eq = 0.0f;
};

} // namespace sk4n
