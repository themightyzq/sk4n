#include "PresetMorpher.h"
#include <algorithm>
#include <cmath>

namespace sk4n {

void PresetMorpher::attach (juce::AudioProcessorValueTreeState& a,
                            const juce::String& snapshotAID,
                            const juce::String& snapshotBID,
                            const juce::String& morphPositionID,
                            const juce::String& morphSpeedID)
{
    apvts   = &a;
    idA     = snapshotAID;
    idB     = snapshotBID;
    idPos   = morphPositionID;
    idSpeed = morphSpeedID;

    // Parameters that are driven by Pass 7 performance macros. Morphing should
    // ONLY move the macros; the destinations follow automatically via the macro
    // listeners. If we morph both, the destination values briefly "win" and then
    // get overwritten by the macro listener -- noisy and incorrect.
    static const juce::StringArray macroDestinations {
        "window", "lfoToPos", "oscAToPos", "lfoRate",
        "oscAPitch", "oscBPitch",
        "filterCenter", "filterReson", "filterMix",
        "filterMode", "cabDrive", "cabFold", "cabLevel",
        "reverbMix", "reverbSize", "echoMix",
        "oscAFb", "oscBFb", "oscAShape", "oscBShape", "amMix"
    };

    morphableIDs.clear();
    for (auto* param : apvts->processor.getParameters())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto pid = p->getParameterID();
            if (pid == idA || pid == idB || pid == idPos || pid == idSpeed) continue;
            if (macroDestinations.contains (pid)) continue;
            morphableIDs.push_back (pid);
        }
    }

    if (auto* rawPos = apvts->getRawParameterValue (idPos))
    {
        const float p = juce::jlimit (0.0f, 1.0f, rawPos->load());
        currentPos = targetPos = p;
    }

    apvts->addParameterListener (idA,    this);
    apvts->addParameterListener (idB,    this);
    apvts->addParameterListener (idPos,  this);

    startTimerHz (60);
}

PresetMorpher::~PresetMorpher()
{
    stopTimer();
    if (apvts != nullptr)
    {
        apvts->removeParameterListener (idA,   this);
        apvts->removeParameterListener (idB,   this);
        apvts->removeParameterListener (idPos, this);
    }
    cancelPendingUpdate();
}

void PresetMorpher::saveSnapshot (int slot)
{
    if (slot < 0 || slot >= kNumSnapshots || apvts == nullptr) return;
    auto& m = snapshots[static_cast<size_t> (slot)];
    m.clear();
    for (const auto& id : morphableIDs)
    {
        if (auto* raw = apvts->getRawParameterValue (id))
            m[id] = raw->load();
    }
    validSlot[static_cast<size_t> (slot)] = true;
}

bool PresetMorpher::isSnapshotValid (int slot) const
{
    if (slot < 0 || slot >= kNumSnapshots) return false;
    return validSlot[static_cast<size_t> (slot)];
}

juce::ValueTree PresetMorpher::saveAllToValueTree() const
{
    juce::ValueTree v ("MorpherSnapshots");
    for (int i = 0; i < kNumSnapshots; ++i)
    {
        if (! validSlot[static_cast<size_t> (i)]) continue;
        juce::ValueTree s ("Snapshot");
        s.setProperty ("slot", i, nullptr);
        for (const auto& [id, val] : snapshots[static_cast<size_t> (i)])
        {
            juce::ValueTree p ("P");
            p.setProperty ("id",  id,  nullptr);
            p.setProperty ("val", val, nullptr);
            s.addChild (p, -1, nullptr);
        }
        v.addChild (s, -1, nullptr);
    }
    return v;
}

void PresetMorpher::loadAllFromValueTree (const juce::ValueTree& v)
{
    if (! v.hasType ("MorpherSnapshots")) return;
    for (auto& s : validSlot) s = false;
    for (int i = 0; i < v.getNumChildren(); ++i)
    {
        auto sn = v.getChild (i);
        const int slot = static_cast<int> (sn.getProperty ("slot"));
        if (slot < 0 || slot >= kNumSnapshots) continue;
        snapshots[static_cast<size_t> (slot)].clear();
        for (int j = 0; j < sn.getNumChildren(); ++j)
        {
            auto p = sn.getChild (j);
            const auto id  = p.getProperty ("id").toString();
            const float val = static_cast<float> (static_cast<double> (p.getProperty ("val")));
            snapshots[static_cast<size_t> (slot)][id] = val;
        }
        validSlot[static_cast<size_t> (slot)] = true;
    }
}

void PresetMorpher::populateDefaults()
{
    if (apvts == nullptr) return;

    auto setIf = [this] (int slot, const juce::String& id, float val)
    {
        if (auto* raw = apvts->getRawParameterValue (id))
            snapshots[static_cast<size_t> (slot)][id] = val;
    };

    saveSnapshot (0);

    for (int s = 1; s < kNumSnapshots; ++s)
        snapshots[static_cast<size_t> (s)] = snapshots[0];

    // 1: Ambient resonator
    setIf (1, "coarsePos", 0.10f);
    setIf (1, "speed",     0.5f);
    setIf (1, "window",    0.6f);
    setIf (1, "oscAPitch", 39.0f);
    setIf (1, "oscBPitch", 33.0f);
    setIf (1, "oscAFb",    0.3f);
    setIf (1, "filterCenter", 4000.0f);
    setIf (1, "reverbMix", 0.45f);
    setIf (1, "echoMix",   0.10f);
    setIf (1, "dryWet",    0.5f);

    // 2: Aggressive scratch — sub-audio
    setIf (2, "coarsePos", 0.5f);
    setIf (2, "fineRange", 0.8f);
    setIf (2, "speed",     50.0f);
    setIf (2, "window",    0.05f);
    setIf (2, "oscAPitch", -21.0f);
    setIf (2, "oscBPitch", -15.0f);
    setIf (2, "envToPos",  0.6f);
    setIf (2, "amMix",     0.3f);
    setIf (2, "delayMix",  0.0f);
    setIf (2, "dryWet",    0.85f);

    // 3: Deep resonator
    setIf (3, "delayTune",     36.0f);
    setIf (3, "delayFeedback", 0.7f);
    setIf (3, "delayMix",      0.7f);
    setIf (3, "filterCenter",  600.0f);
    setIf (3, "filterReson",   0.5f);
    setIf (3, "dryWet",        0.6f);

    // 4: Cabinet drive
    setIf (4, "filterMode",    1.0f);
    setIf (4, "cabDrive",      0.7f);
    setIf (4, "cabFold",       0.4f);
    setIf (4, "cabTilt",       0.2f);
    setIf (4, "filterMix",     0.6f);
    setIf (4, "oscAPitch",     59.0f);
    setIf (4, "dryWet",        0.7f);

    // 5: AM tones
    setIf (5, "oscAPitch", 93.0f);
    setIf (5, "oscBPitch", 76.0f);
    setIf (5, "amBlend",   0.5f);
    setIf (5, "amSquare",  1.0f);
    setIf (5, "amMix",     0.7f);
    setIf (5, "sampleMix", 0.3f);
    setIf (5, "dryWet",    0.8f);

    // 6: Wide flanger
    setIf (6, "echoFlangerMode", 1.0f);
    setIf (6, "flgTime",  3.0f);
    setIf (6, "flgDepth", 0.7f);
    setIf (6, "flgRate",  0.4f);
    setIf (6, "flgFb",    0.5f);
    setIf (6, "flgMix",   0.6f);
    setIf (6, "echoMix",  0.0f);
    setIf (6, "dryWet",   0.5f);

    // 7: Granular shimmer
    setIf (7, "bufferLen",     4.0f);
    setIf (7, "coarsePos",     0.4f);
    setIf (7, "window",        0.2f);
    setIf (7, "oscAPitch",     81.0f);
    setIf (7, "oscBPitch",     88.0f);
    setIf (7, "oscAShape",     0.4f);
    setIf (7, "lfoToPos",      0.3f);
    setIf (7, "reverbSize",    0.8f);
    setIf (7, "reverbMix",     0.4f);
    setIf (7, "dryWet",        0.7f);

    for (int s = 0; s < kNumSnapshots; ++s)
        validSlot[static_cast<size_t> (s)] = true;
}

void PresetMorpher::parameterChanged (const juce::String& paramID, float newValue)
{
    if (applying) return;

    if (paramID == idPos)
    {
        targetPos = juce::jlimit (0.0f, 1.0f, newValue);
        // The timer slews currentPos toward targetPos.
    }
    else
    {
        // Snapshot A or B changed: re-apply at the current position with new endpoints.
        triggerAsyncUpdate();
    }
}

void PresetMorpher::handleAsyncUpdate()
{
    applyMorphAt (currentPos);
    lastAppliedPos = currentPos;
}

void PresetMorpher::timerCallback()
{
    if (apvts == nullptr) return;
    auto* rawSpeed = apvts->getRawParameterValue (idSpeed);
    if (rawSpeed == nullptr) return;

    // morphSpeed 0..1 maps to glide time 5 sec down to ~16 ms.
    const float speed01  = juce::jlimit (0.0f, 1.0f, rawSpeed->load());
    const float glideSec = juce::jmap (speed01, 5.0f, 0.016f);
    const float dt       = 1.0f / 60.0f;
    const float maxStep  = dt / glideSec;

    const float diff = targetPos - currentPos;
    if (std::fabs (diff) <= maxStep) currentPos = targetPos;
    else                              currentPos += (diff > 0.0f ? maxStep : -maxStep);

    if (std::fabs (targetPos - currentPos) > 1.0e-6f
     || lastAppliedPos != currentPos)
    {
        applyMorphAt (currentPos);
        lastAppliedPos = currentPos;
    }
}

void PresetMorpher::applyMorphAt (float pos)
{
    if (apvts == nullptr) return;
    auto* rawA = apvts->getRawParameterValue (idA);
    auto* rawB = apvts->getRawParameterValue (idB);
    if (rawA == nullptr || rawB == nullptr) return;

    const int slotA = juce::jlimit (0, kNumSnapshots - 1, static_cast<int> (rawA->load()));
    const int slotB = juce::jlimit (0, kNumSnapshots - 1, static_cast<int> (rawB->load()));
    pos = juce::jlimit (0.0f, 1.0f, pos);

    if (! validSlot[static_cast<size_t> (slotA)] || ! validSlot[static_cast<size_t> (slotB)])
        return;

    applying = true;

    const auto& mA = snapshots[static_cast<size_t> (slotA)];
    const auto& mB = snapshots[static_cast<size_t> (slotB)];

    for (const auto& id : morphableIDs)
    {
        auto itA = mA.find (id);
        auto itB = mB.find (id);
        if (itA == mA.end() || itB == mB.end()) continue;
        const float blended = itA->second + (itB->second - itA->second) * pos;

        if (auto* p = apvts->getParameter (id))
        {
            const float norm = p->convertTo0to1 (blended);
            p->setValueNotifyingHost (norm);
        }
    }

    applying = false;
}

} // namespace sk4n
