#include "CircularBuffer.h"
#include <cmath>
#include <algorithm>

namespace sk4n {

void CircularBuffer::prepare (double sampleRate, int maxSeconds)
{
    sr = sampleRate;
    allocatedSize = static_cast<int> (static_cast<double> (maxSeconds) * sampleRate);
    buffer.assign (static_cast<size_t> (allocatedSize), 0.0f);

    if (activeSize <= 0 || activeSize > allocatedSize)
        activeSize = allocatedSize;

    writeIndex.store (0, std::memory_order_relaxed);

    inputHpf.setSampleRate (sampleRate);
    inputHpf.setCutoff (30.0);
    inputHpf.reset();
}

void CircularBuffer::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex.store (0, std::memory_order_relaxed);
    inputHpf.reset();
}

void CircularBuffer::setActiveSeconds (float seconds)
{
    int newSize = static_cast<int> (seconds * static_cast<float> (sr));
    if (newSize < 64)              newSize = 64;
    if (newSize > allocatedSize)   newSize = allocatedSize;
    if (newSize == activeSize)     return;

    activeSize = newSize;

    int wi = writeIndex.load (std::memory_order_relaxed);
    wi %= activeSize;
    if (wi < 0) wi += activeSize;
    writeIndex.store (wi, std::memory_order_relaxed);
}

void CircularBuffer::writeSample (float monoIn)
{
    const float hp = inputHpf.process (monoIn);

    if (frozen || activeSize <= 0)
        return;

    int wi = writeIndex.load (std::memory_order_relaxed);
    buffer[static_cast<size_t> (wi)] = hp;
    wi += 1;
    if (wi >= activeSize) wi -= activeSize;
    writeIndex.store (wi, std::memory_order_relaxed);
}

float CircularBuffer::readCubic (float floatIndex) const
{
    if (activeSize < 4)
        return 0.0f;

    const float fActive = static_cast<float> (activeSize);
    float fi = std::fmod (floatIndex, fActive);
    if (fi < 0.0f) fi += fActive;

    int   i1   = static_cast<int> (fi);
    float frac = fi - static_cast<float> (i1);

    const int i0 = wrapIndex (i1 - 1);
    const int i2 = wrapIndex (i1 + 1);
    const int i3 = wrapIndex (i1 + 2);
    i1 = wrapIndex (i1);

    const float y0 = buffer[static_cast<size_t> (i0)];
    const float y1 = buffer[static_cast<size_t> (i1)];
    const float y2 = buffer[static_cast<size_t> (i2)];
    const float y3 = buffer[static_cast<size_t> (i3)];

    // 4-point, 3rd-order Hermite (Catmull-Rom)
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

} // namespace sk4n
