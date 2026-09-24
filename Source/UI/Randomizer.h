#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <set>
#include <random>

namespace sk4n_ui {

class Randomizer
{
public:
    enum class Mode { Full, Constrained };

    explicit Randomizer (juce::AudioProcessorValueTreeState& apvts);

    void randomizeAll      (Mode mode);
    void randomizeSection  (const juce::String& sectionTag, Mode mode);

    void setLocked   (const juce::String& paramID, bool locked);
    bool isLocked    (const juce::String& paramID) const;
    void clearLocks();
    juce::StringArray getLockedIDs() const;

    juce::ValueTree saveLocksToValueTree() const;
    void            loadLocksFromValueTree (const juce::ValueTree&);

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void locksChanged() = 0;
    };
    void addListener    (Listener*);
    void removeListener (Listener*);

private:
    struct SafeRange
    {
        float min;
        float max;
        juce::String section;
    };

    void  buildSafeRangeTable();
    float pickValue  (const juce::String& paramID, const SafeRange&, Mode mode, std::mt19937& rng);
    void  applyOne   (const juce::String& paramID, float newValue);

    juce::AudioProcessorValueTreeState& apvts;
    std::map<juce::String, SafeRange>   safeRanges;
    std::set<juce::String>              lockedParams;
    juce::ListenerList<Listener>        listeners;
};

} // namespace sk4n_ui
