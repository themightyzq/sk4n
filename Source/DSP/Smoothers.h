#pragma once

#include <algorithm>
#include <cmath>

namespace sk4n {

class OnePoleLowpass
{
public:
    void setSampleRate (double sr) { sampleRate = sr; setCutoff (currentFc); }
    void setCutoff (double fc)
    {
        currentFc = fc;
        if (sampleRate <= 0.0 || fc <= 0.0) { coeff = 0.0f; return; }
        const double w = 2.0 * M_PI * fc / sampleRate;
        coeff = static_cast<float> (1.0 - std::exp (-w));
    }
    void  reset (float v = 0.0f) { state = v; }
    inline float process (float x) { state += coeff * (x - state); return state; }
    float current() const { return state; }

private:
    double sampleRate = 44100.0;
    double currentFc  = 50.0;
    float  coeff = 0.0f, state = 0.0f;
};

class SlewLimiter
{
public:
    void setMaxRate (float samplesPerSampleStep) { maxStep = samplesPerSampleStep; }
    void reset (float v = 0.0f) { state = v; }
    inline float process (float target)
    {
        float d = target - state;
        if (d >  maxStep) d =  maxStep;
        if (d < -maxStep) d = -maxStep;
        state += d;
        return state;
    }
    float current() const { return state; }

private:
    float maxStep = 1.0e9f, state = 0.0f;
};

class OnePoleHighpass
{
public:
    void setSampleRate (double sr) { sampleRate = sr; setCutoff (currentFc); }
    void setCutoff (double fc)
    {
        currentFc = fc;
        if (sampleRate <= 0.0 || fc <= 0.0) { coeff = 0.0f; return; }
        const double w = 2.0 * M_PI * fc / sampleRate;
        coeff = static_cast<float> (1.0 - std::exp (-w));
    }
    void reset() { lpState = 0.0f; }
    inline float process (float x) { lpState += coeff * (x - lpState); return x - lpState; }

private:
    double sampleRate = 44100.0;
    double currentFc = 30.0;
    float coeff = 0.0f, lpState = 0.0f;
};

// Rectified-input lowpass for transient detection / amp env follow.
class EnvelopeFollower
{
public:
    void setSampleRate (double sr) { lp.setSampleRate (sr); lp.setCutoff (cutoff); }
    void setCutoff (double fc) { cutoff = fc; lp.setCutoff (fc); }
    void reset() { lp.reset(); }
    inline float process (float x) { return lp.process (std::fabs (x)); }
    float current() const { return lp.current(); }

private:
    OnePoleLowpass lp;
    double cutoff = 20.0;
};

// Simple linear ramp; used for click-free crossfades on mode switches.
class LinearRamp
{
public:
    void setSampleRate (double sr) { sampleRate = sr; }
    void rampTo (float target, float ms)
    {
        const float samples = std::max (1.0f, ms * 0.001f * static_cast<float> (sampleRate));
        step = (target - current_) / samples;
        targetVal = target;
        active = std::fabs (step) > 0.0f;
    }
    void setImmediate (float v) { current_ = v; targetVal = v; step = 0.0f; active = false; }
    inline float process()
    {
        if (! active) return current_;
        current_ += step;
        if ((step > 0.0f && current_ >= targetVal) || (step < 0.0f && current_ <= targetVal))
        { current_ = targetVal; active = false; step = 0.0f; }
        return current_;
    }
    float current() const { return current_; }
    bool  isActive() const { return active; }

private:
    double sampleRate = 44100.0;
    float  current_ = 0.0f, targetVal = 0.0f, step = 0.0f;
    bool   active = false;
};

} // namespace sk4n
