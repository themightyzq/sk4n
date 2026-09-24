#pragma once

#include <vector>
#include "Smoothers.h"

namespace sk4n {

// One stereo delay line that runs as either Echo (longer, post-delay filters,
// L/R offset) or Flanger (short, internally-LFO-modulated, no post filters).
// Mode switch crossfades over ~30 ms to prevent clicks.
class EchoFlanger
{
public:
    enum class Mode { Echo, Flanger };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setMode (Mode m);

    // Per-sample stereo process. Echo params are used in Echo mode; flanger params in Flanger mode.
    void process (float& l, float& r,
                  // Echo
                  float echoTimeMs, float echoLrOffset, float echoFb,
                  float echoLoCutHz, float echoHiCutHz, float echoMix,
                  // Flanger
                  float flgTimeMs, float flgDepth, float flgRateHz, float flgFb, float flgMix);

private:
    inline float readLinear (const std::vector<float>& buf, int writeIdx, float delaySamples) const;

    double sr = 44100.0;
    int    bufSize = 0;
    std::vector<float> bufL, bufR;
    int    writeIdx = 0;

    OnePoleHighpass echoHpL, echoHpR;
    OnePoleLowpass  echoLpL, echoLpR;

    // Internal flanger LFO
    double flgLfoPhase = 0.0;

    Mode    currentMode = Mode::Echo;
    Mode    targetMode  = Mode::Echo;
    float   modeBlend   = 1.0f;     // 1 = fully on currentMode, 0 = fully on targetMode
    float   modeStep    = 0.0f;
    bool    modeRamping = false;
};

} // namespace sk4n
