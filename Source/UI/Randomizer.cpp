#include "Randomizer.h"

#include <cmath>

namespace sk4n_ui {

Randomizer::Randomizer (juce::AudioProcessorValueTreeState& a) : apvts (a)
{
    buildSafeRangeTable();
}

void Randomizer::buildSafeRangeTable()
{
    // ---- Performance macros (Pass 7) ----
    safeRanges["perfMacroMovement"] = { 0.0f, 0.7f, "performance" };
    safeRanges["perfMacroPitch"]    = { 0.2f, 0.8f, "performance" };
    safeRanges["perfMacroColor"]    = { 0.2f, 0.8f, "performance" };
    safeRanges["perfMacroDrive"]    = { 0.0f, 0.6f, "performance" };
    safeRanges["perfMacroSpace"]    = { 0.0f, 0.5f, "performance" };
    safeRanges["perfMacroTexture"]  = { 0.0f, 0.6f, "performance" };

    // ---- Position (window / lfoToPos / oscAToPos managed by macros) ----
    safeRanges["bufferLen"]     = { 1.0f,    4.0f,    "position" };
    safeRanges["coarsePos"]     = { 0.05f,   0.6f,    "position" };
    safeRanges["finePos"]       = { -0.5f,   0.5f,    "position" };
    safeRanges["fineRange"]     = { 0.1f,    0.6f,    "position" };
    safeRanges["speed"]         = { 0.5f,    50.0f,   "position" };
    safeRanges["oscBToPos"]     = { -0.4f,   0.4f,    "position" };
    safeRanges["fbToPos"]       = { -0.3f,   0.3f,    "position" };
    safeRanges["envToPos"]      = { -0.5f,   0.5f,    "position" };

    // ---- Osc A (pitch / shape / fb managed by macros) ----
    safeRanges["oscAFine"]      = { -25.0f,  25.0f,   "oscA" };
    safeRanges["oscAEnvPitch"]  = { -0.4f,   0.4f,    "oscA" };
    safeRanges["oscALfoPitch"]  = { -0.4f,   0.4f,    "oscA" };
    safeRanges["oscAEnvQuant"]  = { 0.001f,  0.1f,    "oscA" };
    safeRanges["oscALfoQuant"]  = { 0.001f,  0.1f,    "oscA" };

    // ---- Osc B (pitch / shape / fb managed by macros) ----
    safeRanges["oscBFine"]      = { -25.0f,  25.0f,   "oscB" };
    safeRanges["oscBEnvPitch"]  = { -0.4f,   0.4f,    "oscB" };
    safeRanges["oscBLfoPitch"]  = { -0.4f,   0.4f,    "oscB" };
    safeRanges["oscBEnvQuant"]  = { 0.001f,  0.1f,    "oscB" };
    safeRanges["oscBLfoQuant"]  = { 0.001f,  0.1f,    "oscB" };

    // ---- AM (amMix managed by macro) ----
    safeRanges["amBlend"]       = { 0.0f,    1.0f,    "am" };

    // ---- Delay ----
    safeRanges["delayTune"]     = { 24.0f,   96.0f,   "delay" };
    safeRanges["delayFeedback"] = { 0.0f,    0.65f,   "delay" };
    safeRanges["delayLfo"]      = { 0.0f,    0.5f,    "delay" };
    safeRanges["delayEnv"]      = { 0.0f,    0.5f,    "delay" };
    safeRanges["delayMix"]      = { -0.6f,   0.6f,    "delay" };
    safeRanges["delayLoCut"]    = { 40.0f,   400.0f,  "delay" };

    // ---- Filter (center / reson / mix managed by macros) ----
    safeRanges["filterGap"]      = { -0.5f,  0.5f,     "filter" };
    safeRanges["filterBalance"]  = { -1.0f,  1.0f,     "filter" };
    safeRanges["filterLrOffset"] = { 0.0f,   0.5f,     "filter" };

    // ---- Cabinet (drive / fold / level managed by macro) ----
    safeRanges["cabTilt"]       = { -0.5f,   0.5f,     "cabinet" };
    safeRanges["cabHiCut"]      = { 2000.0f, 14000.0f, "cabinet" };

    // ---- Mixer ----
    safeRanges["sampleMix"]     = { 0.3f,    1.0f,     "mixer" };

    // ---- FX (echoMix managed by macro) ----
    safeRanges["echoTime"]      = { 50.0f,   800.0f,   "fx" };
    safeRanges["echoLrOff"]     = { -0.5f,   0.5f,     "fx" };
    safeRanges["echoFb"]        = { 0.0f,    0.6f,     "fx" };
    safeRanges["echoLoCut"]     = { 40.0f,   400.0f,   "fx" };
    safeRanges["echoHiCut"]     = { 3000.0f, 12000.0f, "fx" };
    safeRanges["flgTime"]       = { 0.5f,    15.0f,    "fx" };
    safeRanges["flgDepth"]      = { 0.2f,    0.8f,     "fx" };
    safeRanges["flgRate"]       = { 0.05f,   5.0f,     "fx" };
    safeRanges["flgFb"]         = { 0.0f,    0.6f,     "fx" };
    safeRanges["flgMix"]        = { 0.0f,    0.5f,     "fx" };

    // ---- Reverb (size / mix managed by macro) ----
    safeRanges["reverbLoCut"]   = { 80.0f,   400.0f,   "reverb" };
    safeRanges["reverbHiCut"]   = { 4000.0f, 12000.0f, "reverb" };

    // ---- Envelopes ----
    auto addEnvSection = [&] (const juce::String& prefix, const juce::String& tag)
    {
        safeRanges[prefix + "A"]     = { 5.0f,    500.0f,  tag };
        safeRanges[prefix + "D1"]    = { 20.0f,   1500.0f, tag };
        safeRanges[prefix + "Break"] = { 0.3f,    0.9f,    tag };
        safeRanges[prefix + "D2"]    = { 50.0f,   2000.0f, tag };
        safeRanges[prefix + "S"]     = { 0.2f,    0.9f,    tag };
        safeRanges[prefix + "R"]     = { 50.0f,   3000.0f, tag };
        safeRanges[prefix + "Vel"]   = { 0.2f,    0.8f,    tag };
    };
    addEnvSection ("envA",   "envA");
    addEnvSection ("envB",   "envB");
    addEnvSection ("ampEnv", "ampEnv");

    // ---- Trigger ----
    safeRanges["trigThresh"]    = { 0.05f,   0.4f,     "trigger" };
    safeRanges["freeRunRate"]   = { 0.2f,    4.0f,     "trigger" };

    // ---- LFO (lfoRate managed by macro) ----
    safeRanges["lfoSymmetry"]   = { -0.4f,   0.4f,     "lfo" };
    safeRanges["lfoPhase"]      = { -0.25f,  0.25f,    "lfo" };
    safeRanges["lfoFade"]       = { 0.0f,    0.5f,     "lfo" };

    // ---- Feedback ----
    safeRanges["fbSource"]      = { 0.3f,    0.8f,     "feedback" };
    safeRanges["fbAmount"]      = { -0.4f,   0.4f,     "feedback" };

    // ---- Master ----
    safeRanges["masterGain"]    = { -6.0f,   3.0f,     "master" };
    // dryWet intentionally NOT in the safe-range table -- randomization should
    // not move the user's chosen dry/wet level.
    safeRanges["ampEnvDepth"]   = { 0.0f,    0.6f,     "master" };
}

float Randomizer::pickValue (const juce::String& paramID, const SafeRange& sr,
                              Mode mode, std::mt19937& rng)
{
    auto* param  = apvts.getParameter (paramID);
    if (param == nullptr) return 0.0f;

    if (mode == Mode::Full)
    {
        const bool isLogScale = (sr.min > 0.0f) && (sr.max / juce::jmax (sr.min, 0.001f) > 50.0f);
        if (isLogScale)
        {
            std::uniform_real_distribution<float> d (std::log (sr.min), std::log (sr.max));
            return std::exp (d (rng));
        }
        std::uniform_real_distribution<float> d (sr.min, sr.max);
        return d (rng);
    }

    if (auto* raw = apvts.getRawParameterValue (paramID))
    {
        const float current = raw->load();
        const float window  = (sr.max - sr.min) * 0.2f;
        std::uniform_real_distribution<float> d (-window, window);
        return juce::jlimit (sr.min, sr.max, current + d (rng));
    }
    return sr.min + (sr.max - sr.min) * 0.5f;
}

void Randomizer::applyOne (const juce::String& paramID, float newValue)
{
    auto* param = apvts.getParameter (paramID);
    if (param == nullptr) return;
    auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
    if (ranged == nullptr) return;
    const float normalised = ranged->getNormalisableRange().convertTo0to1 (newValue);
    ranged->beginChangeGesture();
    ranged->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
    ranged->endChangeGesture();
}

void Randomizer::randomizeAll (Mode mode)
{
    std::mt19937 rng (std::random_device{}());
    for (const auto& [id, sr] : safeRanges)
    {
        if (lockedParams.count (id)) continue;
        applyOne (id, pickValue (id, sr, mode, rng));
    }
}

void Randomizer::randomizeSection (const juce::String& sectionTag, Mode mode)
{
    std::mt19937 rng (std::random_device{}());
    for (const auto& [id, sr] : safeRanges)
    {
        if (sr.section != sectionTag) continue;
        if (lockedParams.count (id))   continue;
        applyOne (id, pickValue (id, sr, mode, rng));
    }
}

void Randomizer::setLocked (const juce::String& paramID, bool locked)
{
    if (locked) lockedParams.insert (paramID);
    else        lockedParams.erase  (paramID);
    listeners.call (&Listener::locksChanged);
}

bool Randomizer::isLocked (const juce::String& paramID) const
{
    return lockedParams.count (paramID) != 0;
}

void Randomizer::clearLocks()
{
    if (lockedParams.empty()) return;
    lockedParams.clear();
    listeners.call (&Listener::locksChanged);
}

juce::StringArray Randomizer::getLockedIDs() const
{
    juce::StringArray a;
    for (const auto& id : lockedParams) a.add (id);
    return a;
}

juce::ValueTree Randomizer::saveLocksToValueTree() const
{
    juce::ValueTree v ("RandomizerLocks");
    for (const auto& id : lockedParams)
    {
        juce::ValueTree p ("Lock");
        p.setProperty ("id", id, nullptr);
        v.addChild (p, -1, nullptr);
    }
    return v;
}

void Randomizer::loadLocksFromValueTree (const juce::ValueTree& v)
{
    if (! v.hasType ("RandomizerLocks")) return;
    lockedParams.clear();
    for (int i = 0; i < v.getNumChildren(); ++i)
        lockedParams.insert (v.getChild (i).getProperty ("id").toString());
    listeners.call (&Listener::locksChanged);
}

void Randomizer::addListener    (Listener* l) { listeners.add    (l); }
void Randomizer::removeListener (Listener* l) { listeners.remove (l); }

} // namespace sk4n_ui
