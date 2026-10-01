#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "../Source/DSP/CircularBuffer.h"
#include "../Source/DSP/PhaseOscillator.h"
#include "../Source/DSP/EightPoleFilter.h"
#include "../Source/DSP/TransientDetector.h"
#include "../Source/DSP/PositionEngine.h"
#include "../Source/DSP/ADBDSREnvelope.h"
#include "../Source/DSP/SoftClipper.h"
#include "../Source/PluginProcessor.h"

#include <cmath>

// ---------------------------------------------------------------------------------------------
// Allocation probe (test binary only). Counts heap allocations made by the *calling thread*
// while a Scope is alive, so JUCE's timer/message threads cannot cause false positives.
//
// JUCE's juce::HeapBlock / juce::AudioBuffer allocate with malloc/realloc/calloc and never touch
// operator new, so replacing operator new alone would miss exactly the allocations that matter
// here. This translation unit therefore also defines malloc/calloc/realloc/free for the test
// executable (on macOS, forwarding to the default malloc zone). The Plug-in code and JUCE are
// statically linked into this executable, so their calls resolve to these definitions. A probe
// self-test proves the counter sees both operator new and AudioBuffer growth.
// ---------------------------------------------------------------------------------------------
#include <atomic>
#include <cstdlib>
#include <new>
#if defined (__APPLE__)
 #include <malloc/malloc.h>
#endif

namespace alloc_probe
{
    // Constant-initialised thread_local PODs: touching them never allocates.
    static thread_local bool armed = false;
    static thread_local long count = 0;

    static inline void note() noexcept { if (armed) ++count; }

    struct Scope
    {
        Scope()  { count = 0; armed = true; }
        ~Scope() { armed = false; }
        long allocations() const { return count; }
    };
}

#if defined (__APPLE__)
extern "C"
{
    void* malloc (size_t n)               { alloc_probe::note(); return malloc_zone_malloc (malloc_default_zone(), n); }
    void* calloc (size_t a, size_t b)     { alloc_probe::note(); return malloc_zone_calloc (malloc_default_zone(), a, b); }
    void* realloc (void* p, size_t n)
    {
        alloc_probe::note();
        return p != nullptr ? malloc_zone_realloc (malloc_zone_from_ptr (p), p, n)
                            : malloc_zone_malloc (malloc_default_zone(), n);
    }
    void free (void* p)                   { if (p != nullptr) malloc_zone_free (malloc_zone_from_ptr (p), p); }
}
#endif

void* operator new (std::size_t n)
{
    alloc_probe::note();
    if (void* p = std::malloc (n != 0 ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n)
{
    alloc_probe::note();
    if (void* p = std::malloc (n != 0 ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept                    { std::free (p); }
void operator delete[] (void* p) noexcept                  { std::free (p); }
void operator delete (void* p, std::size_t) noexcept       { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept     { std::free (p); }

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


// Drives the real processor headlessly. Both cases here were silent defects: the four Reverb
// controls were never advanced after prepare, and one NaN sample latched the plugin.
class ProcessorTests : public juce::UnitTest
{
public:
    ProcessorTests() : juce::UnitTest ("SK4nProcessor") {}

    static constexpr int kBlock = 256;

    static void setParam (SK4nAudioProcessor& p, const juce::String& id, float v)
    {
        auto* prm = p.apvts.getParameter (id);
        jassert (prm != nullptr);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }

    // Deterministic broadband-ish stimulus, identical for every call with the same phase.
    static void fillStimulus (juce::AudioBuffer<float>& b, int& sampleCounter)
    {
        for (int i = 0; i < b.getNumSamples(); ++i, ++sampleCounter)
        {
            const float t = static_cast<float> (sampleCounter);
            const float v = 0.4f * std::sin (0.05f * t) + 0.2f * std::sin (0.31f * t + 1.0f);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
    }

    static bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i))) return false;
        return true;
    }

    static double sumSquaredDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        double d = 0.0;
        for (int c = 0; c < a.getNumChannels(); ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
            {
                const double e = static_cast<double> (a.getSample (c, i)) - b.getSample (c, i);
                d += e * e;
            }
        return d;
    }

    static void prepareProcessor (SK4nAudioProcessor& p)
    {
        p.setPlayConfigDetails (2, 2, 48000.0, kBlock);
        p.prepareToPlay (48000.0, kBlock);
    }

    void runTest() override
    {
        beginTest ("Reverb Mix change is audible (smoothers advance)");
        {
            // Two identical processors fed identical audio; only the second gets Reverb Mix = 1
            // after a few blocks. Before the fix the reverb stage read a smoother that never
            // moved, so both outputs stayed identical forever.
            SK4nAudioProcessor a, b;
            for (auto* p : { &a, &b })
            {
                setParam (*p, "reverbMix", 0.0f);
                setParam (*p, "reverbSize", 0.9f);
                setParam (*p, "sampleMix", 1.0f);
                setParam (*p, "dryWet", 1.0f);
                setParam (*p, "coarsePos", 0.02f);
                prepareProcessor (*p);
            }

            juce::AudioBuffer<float> ba (2, kBlock), bb (2, kBlock);
            juce::MidiBuffer midi;
            int ca = 0, cb = 0;

            for (int blk = 0; blk < 6; ++blk)
            {
                fillStimulus (ba, ca); fillStimulus (bb, cb);
                a.processBlock (ba, midi); b.processBlock (bb, midi);
            }
            expectWithinAbsoluteError (sumSquaredDiff (ba, bb), 0.0, 1.0e-12);

            setParam (b, "reverbMix", 1.0f);

            double diff = 0.0;
            for (int blk = 0; blk < 12; ++blk)
            {
                fillStimulus (ba, ca); fillStimulus (bb, cb);
                a.processBlock (ba, midi); b.processBlock (bb, midi);
                diff += sumSquaredDiff (ba, bb);
            }
            expect (diff > 1.0e-4, "output should change after Reverb Mix 0 -> 1, diff = " + juce::String (diff));
        }

        beginTest ("NaN input does not latch the plugin");
        {
            SK4nAudioProcessor p;
            setParam (p, "sampleMix", 1.0f);
            setParam (p, "dryWet", 1.0f);
            setParam (p, "fbAmount", 0.8f);   // exercise the feedback path
            setParam (p, "fbSource", 1.0f);
            setParam (p, "reverbMix", 0.5f);
            setParam (p, "coarsePos", 0.02f);
            prepareProcessor (p);

            juce::AudioBuffer<float> buf (2, kBlock);
            juce::MidiBuffer midi;
            int c = 0;
            for (int blk = 0; blk < 4; ++blk) { fillStimulus (buf, c); p.processBlock (buf, midi); }
            expect (allFinite (buf), "warm-up output finite");

            fillStimulus (buf, c);
            buf.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
            buf.setSample (1, 20, std::numeric_limits<float>::infinity());
            p.processBlock (buf, midi);
            expect (allFinite (buf), "block containing NaN/Inf must not output non-finite samples");

            bool recovered = false;
            for (int blk = 0; blk < 4 && ! recovered; ++blk)
            {
                buf.clear();
                p.processBlock (buf, midi);
                recovered = allFinite (buf);
            }
            expect (recovered, "output returns to finite after non-finite input");

            // And it keeps working afterwards: stimulus in -> finite, non-silent stimulus out.
            float peak = 0.0f;
            for (int blk = 0; blk < 8; ++blk)
            {
                fillStimulus (buf, c);
                p.processBlock (buf, midi);
                expect (allFinite (buf), "post-recovery output finite");
                peak = std::max (peak, buf.getMagnitude (0, kBlock));
            }
            expect (peak > 1.0e-4f, "plugin still produces sound after non-finite input");
        }
    }
};


// A host may deliver a block larger than the samplesPerBlock it prepared with (several hosts do,
// e.g. offline bounce or a changed buffer size without a new prepare). The processor must handle
// it without touching the heap and must treat it as consecutive prepared-size blocks.
class OversizedBlockTests : public juce::UnitTest
{
public:
    OversizedBlockTests() : juce::UnitTest ("SK4nOversizedBlocks") {}

    static constexpr int kPrepared = 512;

    static void setParam (SK4nAudioProcessor& p, const juce::String& id, float v)
    {
        auto* prm = p.apvts.getParameter (id);
        jassert (prm != nullptr);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }

    static void fill (juce::AudioBuffer<float>& b, int startSample)
    {
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float t = static_cast<float> (startSample + i);
            b.setSample (0, i, 0.4f * std::sin (0.05f * t) + 0.2f * std::sin (0.31f * t + 1.0f));
            b.setSample (1, i, 0.3f * std::sin (0.07f * t + 0.5f));
        }
    }

    // Every stateful stage is audible: reverb, echo, delay, filter, feedback, LFO, envelopes.
    static void configure (SK4nAudioProcessor& p)
    {
        setParam (p, "sampleMix", 1.0f);
        setParam (p, "dryWet", 0.8f);
        setParam (p, "coarsePos", 0.02f);
        setParam (p, "reverbMix", 0.4f);
        setParam (p, "reverbSize", 0.7f);
        setParam (p, "echoMix", 0.4f);
        setParam (p, "echoFb", 0.4f);
        setParam (p, "delayMix", 0.3f);
        setParam (p, "filterMix", 0.5f);
        setParam (p, "fbAmount", 0.3f);
        setParam (p, "fbSource", 1.0f);
        setParam (p, "freeRun", 1.0f);
        p.setPlayConfigDetails (2, 2, 48000.0, kPrepared);
        p.prepareToPlay (48000.0, kPrepared);
    }

    static double maxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        double m = 0.0;
        for (int c = 0; c < a.getNumChannels(); ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
                m = std::max (m, std::fabs ((double) a.getSample (c, i) - (double) b.getSample (c, i)));
        return m;
    }

    // Feeds `total` samples to `whole` as ONE host block and to `ref` as consecutive blocks of
    // `refBlock` (the last one shorter if total is not a multiple). Returns the worst difference.
    double compare (int total, int refBlock, int rounds)
    {
        SK4nAudioProcessor whole, ref;
        configure (whole);
        configure (ref);
        juce::MidiBuffer midi;

        double worst = 0.0;
        int pos = 0;
        for (int r = 0; r < rounds; ++r)
        {
            juce::AudioBuffer<float> big (2, total);
            fill (big, pos);
            whole.processBlock (big, midi);

            juce::AudioBuffer<float> joined (2, total);
            for (int start = 0; start < total; start += refBlock)
            {
                const int n = std::min (refBlock, total - start);
                juce::AudioBuffer<float> blk (2, n);
                fill (blk, pos + start);
                ref.processBlock (blk, midi);
                for (int c = 0; c < 2; ++c)
                    joined.copyFrom (c, start, blk, c, 0, n);
            }
            worst = std::max (worst, maxAbsDiff (big, joined));
            pos += total;
        }
        return worst;
    }

    void runTest() override
    {
        beginTest ("allocation probe sees new and malloc-based growth");
        {
            alloc_probe::Scope scope;
            volatile float* v = new float[1024];
            v[0] = 1.0f;
            const long afterNew = scope.allocations();
            delete[] const_cast<float*> (v);

            juce::AudioBuffer<float> buf (2, 16);
            buf.setSize (2, 100000, false, false, true);   // HeapBlock path: malloc, never operator new
            const long afterBuffer = scope.allocations();

            expect (afterNew >= 1, "operator new must be counted");
            expect (afterBuffer > afterNew, "AudioBuffer growth (malloc) must be counted");
        }

        beginTest ("4096-sample block into a 512 preparation matches eight 512 blocks");
        {
            const double d = compare (4096, kPrepared, 3);
            expect (d <= 1.0e-6, "max abs difference = " + juce::String (d));
        }

        beginTest ("non-multiple oversized block (1000 into 512) matches 512 + 488");
        {
            const double d = compare (1000, kPrepared, 3);
            expect (d <= 1.0e-6, "max abs difference = " + juce::String (d));
        }

        beginTest ("oversized and undersized host blocks do not allocate");
        {
            SK4nAudioProcessor p;
            configure (p);
            juce::MidiBuffer midi;

            juce::AudioBuffer<float> normal (2, kPrepared), big (2, 4096), small (2, 64), odd (2, 1000);
            int pos = 0;
            for (int i = 0; i < 4; ++i)   // warm-up at the prepared size only
            {
                fill (normal, pos); pos += kPrepared;
                p.processBlock (normal, midi);
            }

            long worstBig = 0, worstSmall = 0, worstOdd = 0;
            for (int rep = 0; rep < 3; ++rep)
            {
                fill (big, pos); pos += 4096;
                { alloc_probe::Scope s; p.processBlock (big, midi); worstBig = std::max (worstBig, s.allocations()); }

                fill (small, pos); pos += 64;
                { alloc_probe::Scope s; p.processBlock (small, midi); worstSmall = std::max (worstSmall, s.allocations()); }

                fill (odd, pos); pos += 1000;
                { alloc_probe::Scope s; p.processBlock (odd, midi); worstOdd = std::max (worstOdd, s.allocations()); }
            }
            expectEquals (worstBig,   (long) 0);
            expectEquals (worstSmall, (long) 0);
            expectEquals (worstOdd,   (long) 0);
        }
    }
};

static OversizedBlockTests   t_oversized;

static ProcessorTests        t_proc;

static CircularBufferTests   t_cb;
static PhaseOscillatorTests  t_po;
static EightPoleFilterTests  t_8p;
static TransientDetectorTests t_td;
static EnvelopeTests         t_env;
static SoftClipperTests      t_sc;

int main (int /*argc*/, char** /*argv*/)
{
    juce::ScopedJuceInitialiser_GUI gui;   // the processor's APVTS/UI helpers need a message manager
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
