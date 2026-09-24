#pragma once

#include "SVFFilter.h"

namespace sk4n {

// Two parallel 4-pole filters (one LP cascade, one HP cascade) crossfaded by Balance.
// Stereo. L/R Offset detunes left vs right cutoffs by ±5 % per offset unit.
class EightPoleFilter
{
public:
    void prepare (double sampleRate);
    void reset();

    // centerHz: 20..20000
    // gap: -1..+1   (HP cutoff = center * (1 + gap*0.5); LP cutoff = center * (1 - gap*0.5))
    // reso: 0..1
    // balance: -1..+1   (-1 = LP only, +1 = HP only, 0 = equal mix)
    // lrOffset: 0..1
    void process (float& l, float& r,
                  float centerHz, float gap, float reso,
                  float balance, float lrOffset);

private:
    void setCutoffs (float centerHz, float gap, float reso, float lrOffset);

    SVFFilter lpL[2], lpR[2];
    SVFFilter hpL[2], hpR[2];

    double sr = 44100.0;
};

} // namespace sk4n
