#pragma once

#include <juce_core/juce_core.h>

namespace sk4n_ui::format {

juce::String hz             (float v);
juce::String ms             (float v);
juce::String db             (float v);
juce::String semi           (float v);
juce::String cents          (float v);
juce::String pctBipolar     (float v);   // -1..+1
juce::String pct            (float v);   // 0..1
juce::String number         (float v, int decimals = 2);
juce::String onOff          (float v);
juce::String midiNote       (float semi);
juce::String seconds        (float v);
juce::String beatDivision   (int idx);

// Helper: returns "—" for non-finite, else nullopt-equivalent (empty)
bool isFiniteOrFallback (float v, juce::String& outFallback);

} // namespace sk4n_ui::format
