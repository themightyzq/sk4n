#pragma once

#include <cmath>

namespace sk4n {

// tanh-style soft clipper. Engages audibly above ~-3 dBFS.
class SoftClipper
{
public:
    static inline float process (float x)
    {
        // Pre-compute 1 / tanh(0.7) ≈ 1.6359
        constexpr float invTanh07 = 1.6359036932f;
        return std::tanh (x * 0.7f) * invTanh07;
    }
};

} // namespace sk4n
