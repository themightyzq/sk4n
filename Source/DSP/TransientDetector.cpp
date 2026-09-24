#include "TransientDetector.h"

namespace sk4n {

void TransientDetector::prepare (double sampleRate)
{
    follower.setSampleRate (sampleRate);
    follower.setCutoff (20.0);
    follower.reset();
    armed = true;
}

void TransientDetector::reset()
{
    follower.reset();
    armed = true;
}

bool TransientDetector::process (float input, float threshold, float hysteresis)
{
    const float env = follower.process (input);
    bool fired = false;
    if (armed && env > threshold)
    {
        fired = true;
        armed = false;
    }
    else if (! armed && env < (threshold - hysteresis))
    {
        armed = true;
    }
    return fired;
}

bool TransientDetector::isBelow (float threshold, float hysteresis) const
{
    return follower.current() < (threshold - hysteresis * 2.0f);
}

} // namespace sk4n
