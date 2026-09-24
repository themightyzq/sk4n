#include "EchoFlanger.h"
#include <cmath>
#include <algorithm>

namespace sk4n {

void EchoFlanger::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;
    bufSize = static_cast<int> (sampleRate * 1.6) + 64;
    bufL.assign (static_cast<size_t> (bufSize), 0.0f);
    bufR.assign (static_cast<size_t> (bufSize), 0.0f);
    writeIdx = 0;

    echoHpL.setSampleRate (sampleRate);
    echoHpR.setSampleRate (sampleRate);
    echoLpL.setSampleRate (sampleRate);
    echoLpR.setSampleRate (sampleRate);

    flgLfoPhase = 0.0;

    modeBlend   = 1.0f;
    modeStep    = 0.0f;
    modeRamping = false;
}

void EchoFlanger::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writeIdx = 0;
    echoHpL.reset(); echoHpR.reset();
    echoLpL.reset(); echoLpR.reset();
    flgLfoPhase = 0.0;
    modeBlend   = 1.0f;
    modeStep    = 0.0f;
    modeRamping = false;
}

void EchoFlanger::setMode (Mode m)
{
    if (m == currentMode && ! modeRamping) return;
    if (m == targetMode) return;
    targetMode  = m;
    modeRamping = true;
    const float steps = std::max (1.0f, 0.030f * static_cast<float> (sr));
    modeStep  = 1.0f / steps;
    modeBlend = 1.0f;
}

float EchoFlanger::readLinear (const std::vector<float>& buf, int wi, float delaySamples) const
{
    if (bufSize <= 0) return 0.0f;
    float fIdx = static_cast<float> (wi) - delaySamples;
    while (fIdx < 0.0f) fIdx += static_cast<float> (bufSize);
    while (fIdx >= static_cast<float> (bufSize)) fIdx -= static_cast<float> (bufSize);
    int   i0   = static_cast<int> (fIdx);
    float frac = fIdx - static_cast<float> (i0);
    int   i1   = (i0 + 1) % bufSize;
    return buf[static_cast<size_t> (i0)] * (1.0f - frac)
         + buf[static_cast<size_t> (i1)] * frac;
}

void EchoFlanger::process (float& l, float& r,
                           float echoTimeMs, float echoLrOffset, float echoFb,
                           float echoLoCutHz, float echoHiCutHz, float echoMix,
                           float flgTimeMs, float flgDepth, float flgRateHz,
                           float flgFb, float flgMix)
{
    if (modeRamping)
    {
        modeBlend -= modeStep;
        if (modeBlend <= 0.0f)
        {
            modeBlend   = 1.0f;
            currentMode = targetMode;
            modeRamping = false;
        }
    }

    const float dryL = l, dryR = r;

    // ----- Compute delay times for each mode (read both for cheap crossfade) -----
    float dLeftEcho, dRightEcho, dLeftFlg, dRightFlg;
    {
        const float echoBase = std::clamp (echoTimeMs, 1.0f, 1500.0f) * 0.001f * static_cast<float> (sr);
        const float echoOffsetSamps = std::clamp (echoLrOffset, -1.0f, 1.0f) * echoBase * 0.5f;
        dLeftEcho  = std::clamp (echoBase - echoOffsetSamps, 1.0f, static_cast<float> (bufSize - 2));
        dRightEcho = std::clamp (echoBase + echoOffsetSamps, 1.0f, static_cast<float> (bufSize - 2));

        const float lfoRate = std::clamp (flgRateHz, 0.01f, 20.0f);
        flgLfoPhase += static_cast<double> (lfoRate) / sr;
        if (flgLfoPhase >= 1.0) flgLfoPhase -= std::floor (flgLfoPhase);
        const float lfoVal = static_cast<float> (std::sin (flgLfoPhase * 2.0 * M_PI));
        const float baseTimeSamples = std::clamp (flgTimeMs, 0.1f, 20.0f) * 0.001f * static_cast<float> (sr);
        const float modAmt = std::clamp (flgDepth, 0.0f, 1.0f) * baseTimeSamples * 0.95f;
        dLeftFlg  = std::clamp (baseTimeSamples + lfoVal * modAmt, 1.0f, static_cast<float> (bufSize - 2));
        dRightFlg = std::clamp (baseTimeSamples - lfoVal * modAmt, 1.0f, static_cast<float> (bufSize - 2));
    }

    const float echoOutL = readLinear (bufL, writeIdx, dLeftEcho);
    const float echoOutR = readLinear (bufR, writeIdx, dRightEcho);
    const float flgOutL  = readLinear (bufL, writeIdx, dLeftFlg);
    const float flgOutR  = readLinear (bufR, writeIdx, dRightFlg);

    auto pickOutput = [&] (Mode m, float& outL, float& outR,
                           float& fbL_out, float& fbR_out, float& mix_out)
    {
        if (m == Mode::Echo)
        {
            echoHpL.setCutoff (std::clamp (static_cast<double> (echoLoCutHz), 10.0, 5000.0));
            echoHpR.setCutoff (std::clamp (static_cast<double> (echoLoCutHz), 10.0, 5000.0));
            echoLpL.setCutoff (std::clamp (static_cast<double> (echoHiCutHz), 100.0, 25000.0));
            echoLpR.setCutoff (std::clamp (static_cast<double> (echoHiCutHz), 100.0, 25000.0));
            const float fL = echoLpL.process (echoHpL.process (echoOutL));
            const float fR = echoLpR.process (echoHpR.process (echoOutR));
            outL = fL; outR = fR;
            const float fb = std::clamp (echoFb, 0.0f, 0.95f);
            fbL_out = fL * fb; fbR_out = fR * fb;
            mix_out = std::clamp (echoMix, 0.0f, 1.0f);
        }
        else
        {
            outL = flgOutL; outR = flgOutR;
            const float fb = std::clamp (flgFb, 0.0f, 0.95f);
            fbL_out = flgOutL * fb; fbR_out = flgOutR * fb;
            mix_out = std::clamp (flgMix, 0.0f, 1.0f);
        }
    };

    float aL, aR, aFbL, aFbR, aMix;
    pickOutput (currentMode, aL, aR, aFbL, aFbR, aMix);

    if (modeRamping)
    {
        float bL, bR, bFbL, bFbR, bMix;
        pickOutput (targetMode, bL, bR, bFbL, bFbR, bMix);
        const float t = 1.0f - modeBlend;   // 0 at start of ramp, 1 at end
        const float wetL = aL  * (1.0f - t) + bL  * t;
        const float wetR = aR  * (1.0f - t) + bR  * t;
        const float fbL  = aFbL * (1.0f - t) + bFbL * t;
        const float fbR  = aFbR * (1.0f - t) + bFbR * t;
        const float mix  = aMix * (1.0f - t) + bMix * t;

        bufL[static_cast<size_t> (writeIdx)] = dryL + fbL;
        bufR[static_cast<size_t> (writeIdx)] = dryR + fbR;
        writeIdx = (writeIdx + 1) % bufSize;

        l = dryL + (wetL - dryL) * mix;
        r = dryR + (wetR - dryR) * mix;
    }
    else
    {
        bufL[static_cast<size_t> (writeIdx)] = dryL + aFbL;
        bufR[static_cast<size_t> (writeIdx)] = dryR + aFbR;
        writeIdx = (writeIdx + 1) % bufSize;

        l = dryL + (aL - dryL) * aMix;
        r = dryR + (aR - dryR) * aMix;
    }
}

} // namespace sk4n
