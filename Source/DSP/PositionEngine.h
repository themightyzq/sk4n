#pragma once

#include "Smoothers.h"

namespace sk4n {

// Computes the read offset (in samples, behind writeIndex) from base position
// parameters PLUS five modulation sources, each with its own pre-sum smoother.
//
// Range mode:
//   ms : modulation range = ±100 ms (fixed)
//   pct: modulation range = full active size
class PositionEngine
{
public:
    enum class RangeMode { Ms, Pct };

    void prepare (double sampleRate, int blockSize);
    void setActiveSize (int n) { activeSize = n; }
    void setBlockSize (int n)  { blockSize = n; }
    void reset (float initialPos = 0.0f);

    // Per-sample call. All bipolar amounts are -1..+1; signals are -1..+1.
    float process (float coarsePos01,
                   float finePos11,
                   float fineRange01,
                   float speedParam,
                   RangeMode rangeMode,
                   // Modulation sources (-1..+1) and their amounts (-1..+1)
                   float oscASignal,  float oscAAmt,
                   float oscBSignal,  float oscBAmt,
                   float fbSignal,    float fbAmt,
                   float envSignal,   float envAmt,
                   float lfoSignal,   float lfoAmt);

    float getMinOffset() const { return minOffset; }

private:
    static float speedToFc (float speedParam);
    static float fineRangeMap (float r01);

    double sr        = 44100.0;
    int    blockSize = 512;
    int    activeSize = 0;
    float  minOffset  = 256.0f;

    OnePoleLowpass coarseLp, fineLp;
    SlewLimiter    coarseSlew;

    // Per-source modulation smoothers (~50 Hz cutoff)
    OnePoleLowpass smOscA, smOscB, smFB, smEnv, smLFO;

    bool initialized = false;
};

} // namespace sk4n
