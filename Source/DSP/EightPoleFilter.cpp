#include "EightPoleFilter.h"
#include <algorithm>

namespace sk4n {

void EightPoleFilter::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto* f : { &lpL[0], &lpL[1], &lpR[0], &lpR[1], &hpL[0], &hpL[1], &hpR[0], &hpR[1] })
        f->setSampleRate (sampleRate);
    reset();
}

void EightPoleFilter::reset()
{
    for (auto* f : { &lpL[0], &lpL[1], &lpR[0], &lpR[1], &hpL[0], &hpL[1], &hpR[0], &hpR[1] })
        f->reset();
}

void EightPoleFilter::setCutoffs (float centerHz, float gap, float reso, float lrOffset)
{
    centerHz = std::clamp (centerHz, 20.0f, 20000.0f);
    gap      = std::clamp (gap, -1.0f, 1.0f);
    reso     = std::clamp (reso, 0.0f, 1.0f);
    lrOffset = std::clamp (lrOffset, 0.0f, 1.0f);

    const float lpCenter = centerHz * (1.0f - gap * 0.5f);
    const float hpCenter = centerHz * (1.0f + gap * 0.5f);

    const float lScale = 1.0f - lrOffset * 0.05f;
    const float rScale = 1.0f + lrOffset * 0.05f;

    for (int i = 0; i < 2; ++i)
    {
        lpL[i].setCutoff   (std::clamp (lpCenter * lScale, 5.0f, 20000.0f));
        lpR[i].setCutoff   (std::clamp (lpCenter * rScale, 5.0f, 20000.0f));
        hpL[i].setCutoff   (std::clamp (hpCenter * lScale, 5.0f, 20000.0f));
        hpR[i].setCutoff   (std::clamp (hpCenter * rScale, 5.0f, 20000.0f));
        lpL[i].setResonance (reso);
        lpR[i].setResonance (reso);
        hpL[i].setResonance (reso);
        hpR[i].setResonance (reso);
    }
}

void EightPoleFilter::process (float& l, float& r,
                               float centerHz, float gap, float reso,
                               float balance, float lrOffset)
{
    setCutoffs (centerHz, gap, reso, lrOffset);

    // Cascade two SVF stages each, taking only LP / HP outputs.
    const float lLp = lpL[1].processLP (lpL[0].processLP (l));
    const float rLp = lpR[1].processLP (lpR[0].processLP (r));
    const float lHp = hpL[1].processHP (hpL[0].processHP (l));
    const float rHp = hpR[1].processHP (hpR[0].processHP (r));

    const float bal = std::clamp ((balance + 1.0f) * 0.5f, 0.0f, 1.0f);
    l = lLp * (1.0f - bal) + lHp * bal;
    r = rLp * (1.0f - bal) + rHp * bal;
}

} // namespace sk4n
