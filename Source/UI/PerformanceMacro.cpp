#include "PerformanceMacro.h"

#include <cmath>

namespace sk4n_ui {

PerformanceMacro::PerformanceMacro (juce::AudioProcessorValueTreeState& a,
                                    const juce::String& m,
                                    std::vector<Destination> d)
    : apvts (a), macroID (m), destinations (std::move (d))
{
    apvts.addParameterListener (macroID, this);
}

PerformanceMacro::~PerformanceMacro()
{
    apvts.removeParameterListener (macroID, this);
}

void PerformanceMacro::parameterChanged (const juce::String&, float)
{
    if (applying) return;
    applyNow();
}

void PerformanceMacro::applyNow()
{
    auto* raw = apvts.getRawParameterValue (macroID);
    if (raw == nullptr) return;
    const float t = juce::jlimit (0.0f, 1.0f, raw->load());

    applying = true;
    for (const auto& d : destinations)
    {
        float v;
        if (d.logScale && d.atZero > 0.0f && d.atFull > 0.0f)
            v = std::exp (std::log (d.atZero)
                          + (std::log (d.atFull) - std::log (d.atZero)) * t);
        else
            v = d.atZero + (d.atFull - d.atZero) * t;

        if (auto* p = apvts.getParameter (d.paramID))
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const float norm = ranged->getNormalisableRange().convertTo0to1 (v);
                ranged->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            }
        }
    }
    applying = false;
}

} // namespace sk4n_ui
