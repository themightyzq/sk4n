#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "../Source/DSP/CircularBuffer.h"
#include "../Source/DSP/PhaseOscillator.h"
#include "../Source/DSP/EightPoleFilter.h"
#include "../Source/DSP/TransientDetector.h"
#include "../Source/DSP/PositionEngine.h"
#include "../Source/DSP/ADBDSREnvelope.h"
#include "../Source/DSP/SoftClipper.h"

#include <cmath>

class CircularBufferTests : public juce::UnitTest
{
public:
    CircularBufferTests() : juce::UnitTest ("CircularBuffer") {}

    void runTest() override
    {
        beginTest ("readCubic wrap correctness");
        sk4n::CircularBuffer buf;
        buf.prepare (48000.0, 1);
        buf.setActiveSeconds (0.5f);
        const int active = buf.getActiveSize();
        // Fill with a known sine
        for (int i = 0; i < active; ++i)
        {
            const float v = std::sin (static_cast<float> (i) * 0.01f);
            buf.writeSample (v);
        }

        // Compare reads near the wrap boundary
        const float a = buf.readCubic (1.5f);
        const float b = buf.readCubic (1.5f + static_cast<float> (active));
        expect (std::fabs (a - b) < 1.0e-3f, "wrap should produce identical sample");

        // Negative offset wraps
        const float c = buf.readCubic (-2.5f);
        const float d = buf.readCubic (static_cast<float> (active) - 2.5f);
        expect (std::fabs (c - d) < 1.0e-3f, "negative wraps to positive equiv");

        beginTest ("cubic interpolation passes through integer samples");
        sk4n::CircularBuffer b2;
        b2.prepare (48000.0, 1);
        b2.setActiveSeconds (1.0f);
        const int N = b2.getActiveSize();
        const float fHz = 100.0f;
        for (int i = 0; i < N; ++i)
        {
            const float t = static_cast<float> (i) / 48000.0f;
            b2.writeSample (std::sin (2.0f * static_cast<float> (M_PI) * fHz * t));
        }
        float maxErr = 0.0f;
        for (int k = 1000; k < 1100; ++k)
        {
            const float interp = b2.readCubic (static_cast<float> (k));
            const float at_k   = b2.readCubic (static_cast<float> (k) + 1.0e-6f);
            maxErr = std::max (maxErr, std::fabs (interp - at_k));
        }
        expect (maxErr < 1.0e-3f, "cubic should be continuous at integer indices");
    }
};

class PhaseOscillatorTests : public juce::UnitTest
{
public:
    PhaseOscillatorTests() : juce::UnitTest ("PhaseOscillator") {}

    void runTest() override
    {
        beginTest ("phase stability over long run");
        sk4n::PhaseOscillator o;
        o.prepare (96000.0);
        // Run for ~10 minutes simulated
        const int seconds = 600;
        const int N = 96000 * seconds;
        float maxAbs = 0.0f;
        for (int i = 0; i < N; i += 1024)
        {
            const float v = o.process (0.0f, 0.0f, 0.0f);  // A4 sine, no FB, pure sine
            maxAbs = std::max (maxAbs, std::fabs (v));
        }
        expect (maxAbs <= 1.001f, "no runaway amplitude");
        expect (maxAbs > 0.99f,   "should reach near full sine amplitude");

        beginTest ("negPCurve endpoints (MIDI: A4 = 69)");
        const float fA4    = sk4n::PhaseOscillator::negPCurve (69.0f);   // 440 Hz
        const float fC4    = sk4n::PhaseOscillator::negPCurve (60.0f);   // ~262 Hz
        const float fScrub = sk4n::PhaseOscillator::negPCurve (9.0f);    // ~13.75 Hz, top of fade
        const float fSilent = sk4n::PhaseOscillator::negPCurve (-51.0f); // 0 Hz, bottom of fade
        expect (fA4    > 435.0f && fA4    < 445.0f, "p=69 -> 440 Hz (A4)");
        expect (fC4    > 258.0f && fC4    < 266.0f, "p=60 -> ~262 Hz (C4)");
        expect (fScrub > 13.0f  && fScrub < 14.5f,  "p=9 -> ~13.75 Hz");
        expect (fSilent < 0.001f,                   "p=-51 -> 0 Hz");
    }
};

class EightPoleFilterTests : public juce::UnitTest
{
public:
    EightPoleFilterTests() : juce::UnitTest ("EightPoleFilter") {}

    void runTest() override
    {
        beginTest ("LP attenuates high-frequency, passes low");
        sk4n::EightPoleFilter f;
        f.prepare (48000.0);
        // Push a 12 kHz sine, balance = -1 (LP only), center = 1 kHz, reso 0
        // Should be heavily attenuated.
        float sumIn = 0.0f, sumOut = 0.0f;
        const int N = 4096;
        for (int i = 0; i < N; ++i)
        {
            float l = std::sin (2.0f * static_cast<float> (M_PI) * 12000.0f * static_cast<float> (i) / 48000.0f);
            float r = l;
            sumIn += l * l;
            f.process (l, r, 1000.0f, 0.0f, 0.0f, -1.0f, 0.0f);
            sumOut += l * l;
        }
        const float ratio = sumOut / sumIn;
        expect (ratio < 0.05f, "12 kHz should be -25 dB or lower below 1 kHz LP");
    }
};

class TransientDetectorTests : public juce::UnitTest
{
public:
    TransientDetectorTests() : juce::UnitTest ("TransientDetector") {}

    void runTest() override
    {
        beginTest ("fires once on rising edge");
        sk4n::TransientDetector d;
        d.prepare (48000.0);

        int fires = 0;
        // 100 ms quiet, then loud burst, then quiet
        for (int i = 0; i < 4800; ++i) if (d.process (0.001f, 0.2f)) ++fires;
        for (int i = 0; i < 2400; ++i) if (d.process (0.8f,   0.2f)) ++fires;
        for (int i = 0; i < 4800; ++i) if (d.process (0.001f, 0.2f)) ++fires;
        for (int i = 0; i < 2400; ++i) if (d.process (0.8f,   0.2f)) ++fires;

        expect (fires == 2, juce::String ("expected 2 fires, got ") + juce::String (fires));
    }
};

class EnvelopeTests : public juce::UnitTest
{
public:
    EnvelopeTests() : juce::UnitTest ("ADBDSREnvelope") {}

    void runTest() override
    {
        beginTest ("reaches break and sustain levels");
        sk4n::ADBDSREnvelope e;
        e.prepare (48000.0);
        e.setTimes (5.0f, 20.0f, 30.0f, 50.0f);
        e.setLevels (0.7f, 0.5f);
        e.trigger (1.0f);

        // Run long enough to reach sustain
        float v = 0.0f;
        for (int i = 0; i < 48000; ++i) v = e.process();
        expect (std::fabs (v - 0.5f) < 0.02f, "should be at sustain level (0.5)");

        e.release();
        for (int i = 0; i < 48000; ++i) v = e.process();
        expect (v < 0.01f, "should fully release to 0");
    }
};

class SoftClipperTests : public juce::UnitTest
{
public:
    SoftClipperTests() : juce::UnitTest ("SoftClipper") {}

    void runTest() override
    {
        beginTest ("monotonic and bounded");
        const float c0 = sk4n::SoftClipper::process (0.0f);
        const float c1 = sk4n::SoftClipper::process (1.0f);
        const float c2 = sk4n::SoftClipper::process (2.0f);
        expect (std::fabs (c0) < 1.0e-6f, "0 in -> 0 out");
        expect (c1 > c0, "monotonic");
        expect (c2 > c1, "monotonic");
        expect (std::fabs (c2) < 2.0f, "bounded near unity");
    }
};

static CircularBufferTests   t_cb;
static PhaseOscillatorTests  t_po;
static EightPoleFilterTests  t_8p;
static TransientDetectorTests t_td;
static EnvelopeTests         t_env;
static SoftClipperTests      t_sc;

int main (int /*argc*/, char** /*argv*/)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        if (r->failures > 0) ++failures;
    }
    return failures > 0 ? 1 : 0;
}
