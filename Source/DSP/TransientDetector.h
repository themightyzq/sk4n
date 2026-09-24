#pragma once

#include "Smoothers.h"

namespace sk4n {

// Watches an input envelope follower; fires a one-sample "trigger" pulse when
// the follower crosses upward through (threshold). Hysteresis applied via
// (threshold - hysteresis) for the off comparison and re-arming.
class TransientDetector
{
public:
    void prepare (double sampleRate);
    void reset();

    // Returns true once at the rising-edge sample.
    bool process (float input, float threshold, float hysteresis = 0.05f);

    // Below-threshold detection (for envelope release).
    bool isBelow (float threshold, float hysteresis = 0.05f) const;

    float currentEnvelope() const { return follower.current(); }
    float currentFollower() const { return follower.current(); }

private:
    EnvelopeFollower follower;
    bool armed = true;
};

} // namespace sk4n
