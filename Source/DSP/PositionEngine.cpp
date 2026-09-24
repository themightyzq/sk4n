#include "PositionEngine.h"
#include <cmath>
#include <algorithm>

namespace sk4n {

float PositionEngine::speedToFc (float speedParam)
{
    const float s = std::max (0.001f, speedParam);
    return 8.0f * s;
}

float PositionEngine::fineRangeMap (float r01)
{
    r01 = std::clamp (r01, 0.0f, 1.0f);
    return std::pow (r01, 2.5f);
}

void PositionEngine::prepare (double sampleRate, int bs)
{
    sr        = sampleRate;
    blockSize = bs;
    coarseLp.setSampleRate (sampleRate);
    fineLp  .setSampleRate (sampleRate);
    smOscA.setSampleRate (sampleRate);  smOscA.setCutoff (50.0);
    smOscB.setSampleRate (sampleRate);  smOscB.setCutoff (50.0);
    smFB  .setSampleRate (sampleRate);  smFB  .setCutoff (50.0);
    smEnv .setSampleRate (sampleRate);  smEnv .setCutoff (50.0);
    smLFO .setSampleRate (sampleRate);  smLFO .setCutoff (50.0);
    initialized = false;
    minOffset = std::max (static_cast<float> (blockSize + 64),
                          static_cast<float> (sampleRate) * 0.005f);
}

void PositionEngine::reset (float initialPos)
{
    coarseLp.reset (initialPos);
    fineLp  .reset (0.0f);
    coarseSlew.reset (initialPos);
    smOscA.reset(); smOscB.reset(); smFB.reset(); smEnv.reset(); smLFO.reset();
    initialized = true;
}

float PositionEngine::process (float coarsePos01,
                               float finePos11,
                               float fineRange01,
                               float speedParam,
                               RangeMode rangeMode,
                               float oscASignal, float oscAAmt,
                               float oscBSignal, float oscBAmt,
                               float fbSignal,   float fbAmt,
                               float envSignal,  float envAmt,
                               float lfoSignal,  float lfoAmt)
{
    if (activeSize <= 0) return 0.0f;

    if (! initialized)
    {
        const float startPos = coarsePos01 * static_cast<float> (activeSize);
        reset (startPos);
    }

    const float fc = speedToFc (speedParam);
    coarseLp.setCutoff (fc);
    fineLp  .setCutoff (fc);
    coarseSlew.setMaxRate (speedParam);

    const float coarseTarget = coarsePos01 * static_cast<float> (activeSize);
    const float coarseSm     = coarseLp.process (coarseTarget);
    const float coarseFinal  = coarseSlew.process (coarseSm);

    const float modRangeSamples = (rangeMode == RangeMode::Ms)
        ? 0.1f * static_cast<float> (sr)
        : static_cast<float> (activeSize);

    const float fineRangeSamples = fineRangeMap (fineRange01) * 0.1f * static_cast<float> (sr);
    const float fineTarget = finePos11 * fineRangeSamples;
    const float fineSm     = fineLp.process (fineTarget);

    // Each source smoothed independently before scaling
    const float modSum = oscAAmt * smOscA.process (oscASignal)
                       + oscBAmt * smOscB.process (oscBSignal)
                       + fbAmt   * smFB  .process (fbSignal)
                       + envAmt  * smEnv .process (envSignal)
                       + lfoAmt  * smLFO .process (lfoSignal);
    const float modSamples = modSum * modRangeSamples;

    float position = coarseFinal + fineSm + modSamples;

    minOffset = std::max (static_cast<float> (blockSize + 64),
                          static_cast<float> (sr) * 0.005f);
    float maxOffset = static_cast<float> (activeSize - 1);
    if (maxOffset < minOffset) maxOffset = minOffset;

    if (position < minOffset) position = minOffset;
    if (position > maxOffset) position = maxOffset;

    return position;
}

} // namespace sk4n
