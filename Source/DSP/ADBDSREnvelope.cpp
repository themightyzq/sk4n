#include "ADBDSREnvelope.h"
#include <algorithm>

namespace sk4n {

void ADBDSREnvelope::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void ADBDSREnvelope::reset()
{
    stage = Stage::Idle;
    current = 0.0f;
    velocity = 1.0f;
}

void ADBDSREnvelope::setTimes (float aMs, float d1Ms, float d2Ms, float rMs)
{
    attackMs  = std::max (1.0f, aMs);
    decay1Ms  = std::max (1.0f, d1Ms);
    decay2Ms  = std::max (1.0f, d2Ms);
    releaseMs = std::max (1.0f, rMs);
}

void ADBDSREnvelope::setLevels (float breakLevel, float sustainLevel)
{
    breakLvl   = std::clamp (breakLevel, 0.0f, 1.0f);
    sustainLvl = std::clamp (sustainLevel, 0.0f, 1.0f);
}

void ADBDSREnvelope::trigger (float vel)
{
    velocity = std::clamp (vel, 0.0f, 1.0f);
    if (velocity <= 0.0f) velocity = 1.0f;
    stage = Stage::Attack;
}

void ADBDSREnvelope::release()
{
    if (stage != Stage::Idle && stage != Stage::Release)
        stage = Stage::Release;
}

} // namespace sk4n
