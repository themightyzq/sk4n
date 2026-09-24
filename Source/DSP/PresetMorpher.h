#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <unordered_map>
#include <vector>

namespace sk4n {

// Holds 8 snapshots of all APVTS parameters (excluding the morpher's own parameters).
// When snapshotA/snapshotB change, the snapshot endpoints update; when morphPosition
// changes, an internal currentPos slews toward it at a rate set by morphSpeed.
// Self-recursion is prevented via the applying flag.
class PresetMorpher : public juce::AudioProcessorValueTreeState::Listener,
                     public juce::AsyncUpdater,
                     private juce::Timer
{
public:
    static constexpr int kNumSnapshots = 8;

    void attach (juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& snapshotAID,
                 const juce::String& snapshotBID,
                 const juce::String& morphPositionID,
                 const juce::String& morphSpeedID);

    ~PresetMorpher() override;

    // Captures the current parameter state into the given slot.
    void saveSnapshot (int slot);

    // Returns true if slot has been populated.
    bool isSnapshotValid (int slot) const;

    // Save / restore for plugin state.
    juce::ValueTree saveAllToValueTree() const;
    void            loadAllFromValueTree (const juce::ValueTree&);

    // Pre-populate 8 demo snapshots (called once if no saved state).
    void populateDefaults();

    // Slewed morph position (read-only; for visualisation).
    float getCurrentPos() const noexcept { return currentPos; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void applyMorphAt (float pos);

    juce::AudioProcessorValueTreeState* apvts = nullptr;
    juce::String idA, idB, idPos, idSpeed;

    std::array<std::unordered_map<juce::String, float>, kNumSnapshots> snapshots;
    std::array<bool, kNumSnapshots> validSlot { { false, false, false, false, false, false, false, false } };

    std::vector<juce::String> morphableIDs;
    bool  applying       = false;

    float currentPos     = 0.0f;
    float targetPos      = 0.0f;
    float lastAppliedPos = -1.0f;
};

} // namespace sk4n
