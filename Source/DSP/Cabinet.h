#pragma once

#include "Smoothers.h"

namespace sk4n {

// Drive → tanh → sine fold → tilt EQ (pre) → hi-cut (post) → level.
// Operates on a single channel; instantiate two for stereo.
class Cabinet
{
public:
    void prepare (double sampleRate);
    void reset();

    // drive 0..1, fold 0..1, tilt -1..+1, hiCutHz 100..20000, levelDb -24..+12
    float process (float in,
                   float drive,
                   float fold,
                   float tilt,
                   float hiCutHz,
                   float levelDb);

private:
    OnePoleLowpass loShelfLP;  // for tilt's bass component (extract low band)
    OnePoleLowpass hiCutLP;
    double sr = 44100.0;
};

} // namespace sk4n
