#pragma once

#include <vector>
#include <atomic>
#include "Smoothers.h"

namespace sk4n {

// Mono circular buffer with input HPF applied before write.
// Storage is sized to maxSeconds * sr at prepare time and never reallocated.
// activeSize is the wrap modulus and may change at runtime.
class CircularBuffer
{
public:
    void prepare (double sampleRate, int maxSeconds = 8);
    void reset();

    void setActiveSeconds (float seconds);
    void setFrozen (bool f) { frozen = f; }

    // Apply input HPF (~30 Hz) and write monoIn at writeIndex.
    // Advances writeIndex unless frozen. HPF state continues to update either way.
    void writeSample (float monoIn);

    // 4-point Hermite cubic interpolation. All four taps wrap mod activeSize independently.
    float readCubic (float floatIndex) const;

    int getWriteIndex() const { return writeIndex.load (std::memory_order_relaxed); }
    int getActiveSize() const { return activeSize; }
    int getAllocatedSize() const { return allocatedSize; }
    double getSampleRate() const { return sr; }
    const float* data() const { return buffer.data(); }

private:
    inline int wrapIndex (int i) const
    {
        // assumes activeSize > 0
        i %= activeSize;
        if (i < 0) i += activeSize;
        return i;
    }

    std::vector<float>  buffer;
    int                 allocatedSize = 0;
    int                 activeSize    = 0;
    std::atomic<int>    writeIndex { 0 };
    bool                frozen        = false;
    double              sr            = 44100.0;

    OnePoleHighpass     inputHpf;
};

} // namespace sk4n
