#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace sk4n_ui {

// A UI-level controller that maps a single APVTS macro parameter to one or
// more destination parameters via linear or log interpolation between
// (atZero, atFull). The macro parameter is automatable and saved with state.
// The destinations are existing audio parameters whose values get updated when
// the macro changes.
class PerformanceMacro : public juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Destination
    {
        juce::String paramID;
        float        atZero;
        float        atFull;
        bool         logScale;
    };

    PerformanceMacro (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& macroID,
                      std::vector<Destination> destinations);

    ~PerformanceMacro() override;

    void applyNow();

    const juce::String& getMacroID() const { return macroID; }
    const std::vector<Destination>& getDestinations() const { return destinations; }

private:
    void parameterChanged (const juce::String& id, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String macroID;
    std::vector<Destination> destinations;
    bool applying = false;
};

} // namespace sk4n_ui
