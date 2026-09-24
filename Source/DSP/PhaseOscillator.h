#pragma once

namespace sk4n {

// Single-oscillator sine with phase-modulation feedback and a sine→tri→bent-saw shaper.
// Pitch is mapped through negPCurve (smooth ease to 0 Hz at -120 semi).
class PhaseOscillator
{
public:
    void prepare (double sampleRate);
    void reset();

    // pitchSemis: total semitone offset from A4=69 (with cents already added).
    // fb: 0..1 PM feedback amount.
    // shape: 0..1 (0 = sine, 0.5 = smoothed triangle, 1 = bent-saw with windowing).
    // Returns shaped output in -1..+1.
    float process (float pitchSemis, float fb, float shape);

    // Amplitude window factor that the SampleReader applies when shape > 0.5.
    // Returns 1.0 at shape <= 0.5, decreases to 0.7 at shape = 1.
    float currentAmpWindow() const { return ampWindow; }

    static float negPCurve (float pitchSemis);

private:
    static float sineShaper (float theta, float shape, float& windowOut);
    static float smoothTriangle (float theta);
    static float bentSaw (float theta01, float bend);

    double sr      = 44100.0;
    double phase   = 0.0;
    float  lastOut = 0.0f;
    float  ampWindow = 1.0f;
};

} // namespace sk4n
