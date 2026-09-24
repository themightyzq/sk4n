#include "ValueFormatters.h"

#include <cmath>

namespace sk4n_ui::format {

bool isFiniteOrFallback (float v, juce::String& outFallback)
{
    if (! std::isfinite (v))
    {
        outFallback = "--";
        return false;
    }
    return true;
}

juce::String hz (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    if (v < 0.0f) v = 0.0f;
    if (v < 1.0f)
        return juce::String (v, 2) + " Hz";
    if (v < 1000.0f)
        return juce::String (v, 1) + " Hz";
    return juce::String (v / 1000.0f, 2) + " kHz";
}

juce::String ms (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    if (v < 1000.0f) return juce::String (v, 1) + " ms";
    return juce::String (v / 1000.0f, 2) + " s";
}

juce::String db (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    if (v <= -60.0f) return juce::String ("-inf dB");
    juce::String sign = v >= 0.0f ? "+" : "";
    return sign + juce::String (v, 1) + " dB";
}

juce::String semi (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    juce::String sign = v > 0.0f ? "+" : "";
    return sign + juce::String (v, 1) + " st";
}

juce::String cents (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    juce::String sign = v > 0.0f ? "+" : "";
    return sign + juce::String (juce::roundToInt (v)) + " ct";
}

juce::String pctBipolar (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    const int p = juce::roundToInt (v * 100.0f);
    juce::String sign = p > 0 ? "+" : "";
    return sign + juce::String (p) + " %";
}

juce::String pct (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    return juce::String (juce::roundToInt (v * 100.0f)) + " %";
}

juce::String number (float v, int decimals)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    return juce::String (v, decimals);
}

juce::String onOff (float v)
{
    return v >= 0.5f ? juce::String ("On") : juce::String ("Off");
}

juce::String midiNote (float s)
{
    juce::String fb;
    if (! isFiniteOrFallback (s, fb)) return fb;

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int midi = juce::roundToInt (s);
    int noteIdx = ((midi % 12) + 12) % 12;
    int octave  = (midi / 12) - 1;
    return juce::String (names[noteIdx]) + juce::String (octave);
}

juce::String seconds (float v)
{
    juce::String fb;
    if (! isFiniteOrFallback (v, fb)) return fb;
    return juce::String (v, 2) + " s";
}

juce::String beatDivision (int idx)
{
    static const char* divs[] = { "1/64","1/32","1/16","1/8","1/4","1/2","1/1","2/1","4/1" };
    if (idx < 0 || idx >= 9) return "--";
    return divs[idx];
}

} // namespace sk4n_ui::format
