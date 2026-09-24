#pragma once

#include <algorithm>
#include <cmath>

namespace sk4n {

// 6-stage envelope: Attack → Decay1 → break → Decay2 → Sustain → Release.
// Linear ramps per stage; "decay" segments target specific levels (B, S, 0).
class ADBDSREnvelope
{
public:
    enum class Stage { Idle, Attack, Decay1, Decay2, Sustain, Release };

    void prepare (double sampleRate);
    void reset();

    // ms times, normalized levels
    void setTimes (float aMs, float d1Ms, float d2Ms, float rMs);
    void setLevels (float breakLevel, float sustainLevel);

    void trigger (float velocity = 1.0f);
    void release();
    bool isActive() const { return stage != Stage::Idle; }
    Stage getStage() const { return stage; }

    inline float process();

private:
    double sr = 44100.0;

    Stage stage = Stage::Idle;
    float current = 0.0f;
    float velocity = 1.0f;

    float attackMs = 10.0f, decay1Ms = 100.0f, decay2Ms = 300.0f, releaseMs = 500.0f;
    float breakLvl = 0.7f, sustainLvl = 0.5f;
};

inline float ADBDSREnvelope::process()
{
    auto rate = [this] (float ms) {
        const float dur = std::max (0.001f, ms * 0.001f);
        return 1.0f / (dur * static_cast<float> (sr));
    };
    switch (stage)
    {
        case Stage::Idle: return 0.0f;
        case Stage::Attack:
            current += velocity * rate (attackMs);
            if (current >= velocity) { current = velocity; stage = Stage::Decay1; }
            return current;
        case Stage::Decay1:
        {
            const float target = breakLvl * velocity;
            current += (target - velocity) * rate (decay1Ms);
            if ((velocity > target && current <= target)
             || (velocity <= target && current >= target))
            { current = target; stage = Stage::Decay2; }
            return current;
        }
        case Stage::Decay2:
        {
            const float target = sustainLvl * velocity;
            const float prevBreak = breakLvl * velocity;
            current += (target - prevBreak) * rate (decay2Ms);
            if ((prevBreak > target && current <= target)
             || (prevBreak <= target && current >= target))
            { current = target; stage = Stage::Sustain; }
            return current;
        }
        case Stage::Sustain: return current;
        case Stage::Release:
        {
            current -= current * rate (releaseMs);
            if (current <= 1.0e-5f) { current = 0.0f; stage = Stage::Idle; }
            return current;
        }
    }
    return current;
}

} // namespace sk4n
