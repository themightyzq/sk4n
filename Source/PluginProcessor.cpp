#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/AMSection.h"
#include "DSP/SoftClipper.h"

#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>

namespace {
constexpr float kSmoothRamp = 0.01f;

juce::NormalisableRange<float> linRange (float a, float b)             { return { a, b }; }
juce::NormalisableRange<float> logRange (float a, float b)
{
    juce::NormalisableRange<float> r (a, b);
    r.setSkewForCentre (std::sqrt (a * b));
    return r;
}
juce::NormalisableRange<float> centred  (float a, float b, float c)
{
    juce::NormalisableRange<float> r (a, b);
    r.setSkewForCentre (c);
    return r;
}
} // namespace

SK4nAudioProcessor::SK4nAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      randomizer (apvts)
{
    cacheParameterPointers();
    morpher.attach (apvts, "snapshotA", "snapshotB", "morphPosition", "morphSpeed");
    morpher.populateDefaults();

    using Dest = sk4n_ui::PerformanceMacro::Destination;

    macroMovement = std::make_unique<sk4n_ui::PerformanceMacro> (apvts, "perfMacroMovement",
        std::vector<Dest> {
            { "window",    0.05f, 0.60f, false },
            { "lfoToPos",  0.00f, 0.50f, false },
            { "oscAToPos", 0.00f, 0.30f, false },
            { "lfoRate",   0.50f, 4.00f, true  }
        });

    macroPitch = std::make_unique<sk4n_ui::PerformanceMacro> (apvts, "perfMacroPitch",
        std::vector<Dest> {
            { "oscAPitch", 36.0f, 84.0f, false },
            { "oscBPitch", 18.0f, 66.0f, false }
        });

    macroColor = std::make_unique<sk4n_ui::PerformanceMacro> (apvts, "perfMacroColor",
        std::vector<Dest> {
            { "filterCenter", 200.0f, 12000.0f, true  },
            { "filterReson",  0.10f,  0.40f,    false },
            { "filterMix",    0.30f,  0.70f,    false }
        });

    macroDrive = std::make_unique<sk4n_ui::PerformanceMacro> (apvts, "perfMacroDrive",
        std::vector<Dest> {
            { "filterMode", 0.0f,  1.0f,  false },
            { "cabDrive",   0.0f,  0.55f, false },
            { "cabFold",    0.0f,  0.35f, false },
            { "cabLevel",   -3.0f, 0.0f,  false }
        });

    macroSpace = std::make_unique<sk4n_ui::PerformanceMacro> (apvts, "perfMacroSpace",
        std::vector<Dest> {
            { "reverbMix",  0.0f, 0.40f, false },
            { "reverbSize", 0.3f, 0.75f, false },
            { "echoMix",    0.0f, 0.20f, false }
        });

    macroTexture = std::make_unique<sk4n_ui::PerformanceMacro> (apvts, "perfMacroTexture",
        std::vector<Dest> {
            { "oscAFb",    0.0f, 0.40f, false },
            { "oscBFb",    0.0f, 0.40f, false },
            { "oscAShape", 0.0f, 0.50f, false },
            { "oscBShape", 0.0f, 0.50f, false },
            { "amMix",     0.0f, 0.30f, false }
        });

    // Apply macros once to establish a coherent default state.
    macroMovement->applyNow();
    macroPitch   ->applyNow();
    macroColor   ->applyNow();
    macroDrive   ->applyNow();
    macroSpace   ->applyNow();
    macroTexture ->applyNow();
}

juce::AudioProcessorValueTreeState::ParameterLayout SK4nAudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using BP = juce::AudioParameterBool;
    using CP = juce::AudioParameterChoice;
    using IP = juce::AudioParameterInt;
    using PID = juce::ParameterID;
    using NR = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto fp = [&] (const char* id, const char* name, NR range, float def)
    { p.push_back (std::make_unique<FP> (PID { id, 1 }, name, range, def)); };
    auto bp = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<BP> (PID { id, 1 }, name, def)); };
    auto cp = [&] (const char* id, const char* name, juce::StringArray choices, int def)
    { p.push_back (std::make_unique<CP> (PID { id, 1 }, name, choices, def)); };
    auto ip = [&] (const char* id, const char* name, int min, int max, int def)
    { p.push_back (std::make_unique<IP> (PID { id, 1 }, name, min, max, def)); };

    // Meta parameters: setting one rewrites other host-visible parameters (the performance
    // macros via PerformanceMacro, the snapshot/morph controls via PresetMorpher). Hosts must be
    // told, or AU validation fails ("Meta Param Flag is NOT set") and automation of the
    // destinations can fight the macro. IDs and ranges are unchanged, so sessions still load.
    auto fpMeta = [&] (const char* id, const char* name, NR range, float def)
    { p.push_back (std::make_unique<FP> (PID { id, 1 }, name, range, def,
                                          juce::AudioParameterFloatAttributes().withMeta (true))); };
    auto ipMeta = [&] (const char* id, const char* name, int min, int max, int def)
    { p.push_back (std::make_unique<IP> (PID { id, 1 }, name, min, max, def,
                                          juce::AudioParameterIntAttributes().withMeta (true))); };

    // Buffer / Position
    fp ("bufferLen", "Buffer Length", logRange (0.5f, 8.0f), 2.0f);
    bp ("freeze",    "Freeze", false);
    fp ("coarsePos", "Coarse Pos", linRange (0.0f, 1.0f), 0.1f);
    fp ("finePos",   "Fine Pos",   linRange (-1.0f, 1.0f), 0.0f);
    fp ("fineRange", "Fine Range", centred (0.0f, 1.0f, 0.3f), 0.3f);
    fp ("speed",     "Speed", centred (0.01f, 1000.0f, 1.0f), 1.0f);
    cp ("rangeMode", "Range Mode", { "ms", "%" }, 0);
    fp ("window",    "Window", centred (0.0f, 1.0f, 0.25f), 0.25f);
    fp ("oscAToPos", "Osc A -> Pos",  linRange (-1.0f, 1.0f), 0.0f);
    fp ("oscBToPos", "Osc B -> Pos",  linRange (-1.0f, 1.0f), 0.0f);
    fp ("fbToPos",   "FB -> Pos",     linRange (-1.0f, 1.0f), 0.0f);
    fp ("envToPos",  "Env -> Pos",    linRange (-1.0f, 1.0f), 0.0f);
    fp ("lfoToPos",  "LFO -> Pos",    linRange (-1.0f, 1.0f), 0.0f);

    // Osc A
    fp ("oscAPitch",     "Osc A Pitch",   linRange (-60.0f, 96.0f), 60.0f);
    fp ("oscAFine",      "Osc A Fine",    linRange (-50.0f, 50.0f), 0.0f);
    fp ("oscAShape",     "Osc A Shape",   linRange (0.0f, 1.0f), 0.0f);
    fp ("oscAFb",        "Osc A FB",      linRange (0.0f, 1.0f), 0.0f);
    fp ("oscAEnvPitch",  "Osc A Env->P",  linRange (-1.0f, 1.0f), 0.0f);
    fp ("oscALfoPitch",  "Osc A LFO->P",  linRange (-1.0f, 1.0f), 0.0f);
    fp ("oscAEnvQuant",  "Osc A Env Q",   logRange (0.001f, 1.0f), 0.001f);
    fp ("oscALfoQuant",  "Osc A LFO Q",   logRange (0.001f, 1.0f), 0.001f);
    // Osc B
    fp ("oscBPitch",     "Osc B Pitch",   linRange (-60.0f, 96.0f), 42.0f);
    fp ("oscBFine",      "Osc B Fine",    linRange (-50.0f, 50.0f), 0.0f);
    fp ("oscBShape",     "Osc B Shape",   linRange (0.0f, 1.0f), 0.0f);
    fp ("oscBFb",        "Osc B FB",      linRange (0.0f, 1.0f), 0.0f);
    fp ("oscBEnvPitch",  "Osc B Env->P",  linRange (-1.0f, 1.0f), 0.0f);
    fp ("oscBLfoPitch",  "Osc B LFO->P",  linRange (-1.0f, 1.0f), 0.0f);
    fp ("oscBEnvQuant",  "Osc B Env Q",   logRange (0.001f, 1.0f), 0.001f);
    fp ("oscBLfoQuant",  "Osc B LFO Q",   logRange (0.001f, 1.0f), 0.001f);

    // AM
    fp ("amBlend",  "AM Blend",  linRange (0.0f, 1.0f), 0.5f);
    bp ("amSquare", "AM X^2",    false);
    fp ("amMix",    "AM Mix",    linRange (-1.0f, 1.0f), 0.0f);

    // Delay
    fp ("delayTune",     "Delay Tune",   linRange (0.0f, 200.0f), 60.0f);
    fp ("delayFeedback", "Delay FB",     linRange (0.0f, 0.95f), 0.3f);
    fp ("delayLfo",      "Delay LFO",    linRange (0.0f, 1.0f), 0.0f);
    fp ("delayEnv",      "Delay Env",    linRange (0.0f, 1.0f), 0.0f);
    fp ("delayMix",      "Delay Mix",    linRange (-1.0f, 1.0f), 0.0f);
    fp ("delayLoCut",    "Delay LoCut",  logRange (10.0f, 5000.0f), 80.0f);

    // Filter mode
    cp ("filterMode", "Filter Mode", { "8-Pole", "Cabinet" }, 0);
    // 8P
    fp ("filterCenter",   "Filt Center",   logRange (20.0f, 20000.0f), 8000.0f);
    fp ("filterGap",      "Filt Gap",      linRange (-1.0f, 1.0f), 0.0f);
    fp ("filterReson",    "Filt Reson",    linRange (0.0f, 1.0f), 0.2f);
    fp ("filterBalance",  "Filt Balance",  linRange (-1.0f, 1.0f), -1.0f);
    fp ("filterLrOffset", "Filt L/R",      linRange (0.0f, 1.0f), 0.0f);
    fp ("filterMix",      "Filt Mix",      linRange (0.0f, 1.0f), 0.0f);
    // Cabinet
    fp ("cabDrive",  "Cab Drive",  linRange (0.0f, 1.0f), 0.3f);
    fp ("cabFold",   "Cab Fold",   linRange (0.0f, 1.0f), 0.0f);
    fp ("cabTilt",   "Cab Tilt",   linRange (-1.0f, 1.0f), 0.0f);
    fp ("cabHiCut",  "Cab HiCut",  logRange (100.0f, 20000.0f), 12000.0f);
    fp ("cabLevel",  "Cab Level",  linRange (-24.0f, 12.0f), 0.0f);

    // Mixer
    fp ("sampleMix", "Sample Mix", linRange (-1.0f, 1.0f), 1.0f);

    // Echo / Flanger
    cp ("echoFlangerMode", "Echo/Flg Mode", { "Echo", "Flanger" }, 0);
    fp ("echoTime",     "Echo Time",   logRange (1.0f, 1500.0f), 250.0f);
    fp ("echoLrOff",    "Echo L/R",    linRange (-1.0f, 1.0f), 0.0f);
    fp ("echoFb",       "Echo FB",     linRange (0.0f, 0.95f), 0.4f);
    fp ("echoLoCut",    "Echo LoCut",  logRange (10.0f, 5000.0f), 80.0f);
    fp ("echoHiCut",    "Echo HiCut",  logRange (100.0f, 25000.0f), 8000.0f);
    bp ("echoSync",     "Echo Sync",   false);
    fp ("echoMix",      "Echo Mix",    linRange (0.0f, 1.0f), 0.0f);
    fp ("flgTime",      "Flg Time",    logRange (0.1f, 20.0f), 5.0f);
    fp ("flgDepth",     "Flg Depth",   linRange (0.0f, 1.0f), 0.5f);
    fp ("flgRate",      "Flg Rate",    logRange (0.01f, 20.0f), 0.3f);
    fp ("flgFb",        "Flg FB",      linRange (0.0f, 0.95f), 0.3f);
    bp ("flgSync",      "Flg Sync",    false);
    fp ("flgMix",       "Flg Mix",     linRange (0.0f, 1.0f), 0.0f);

    // Reverb
    fp ("reverbSize",  "Rev Size",   linRange (0.0f, 1.0f), 0.5f);
    fp ("reverbLoCut", "Rev LoCut",  logRange (10.0f, 5000.0f), 100.0f);
    fp ("reverbHiCut", "Rev HiCut",  logRange (100.0f, 20000.0f), 8000.0f);
    fp ("reverbMix",   "Rev Mix",    linRange (0.0f, 1.0f), 0.15f);

    // Envelopes
    auto addEnv = [&] (const juce::String& prefix, const juce::String& display, float sustainDef)
    {
        fp ((prefix + "A")     .toRawUTF8(), (display + " A")     .toRawUTF8(), logRange (1.0f, 5000.0f), 10.0f);
        fp ((prefix + "D1")    .toRawUTF8(), (display + " D1")    .toRawUTF8(), logRange (1.0f, 5000.0f), 100.0f);
        fp ((prefix + "Break") .toRawUTF8(), (display + " Break") .toRawUTF8(), linRange (0.0f, 1.0f), 0.7f);
        fp ((prefix + "D2")    .toRawUTF8(), (display + " D2")    .toRawUTF8(), logRange (1.0f, 5000.0f), 300.0f);
        fp ((prefix + "S")     .toRawUTF8(), (display + " S")     .toRawUTF8(), linRange (0.0f, 1.0f), sustainDef);
        fp ((prefix + "R")     .toRawUTF8(), (display + " R")     .toRawUTF8(), logRange (1.0f, 10000.0f), 500.0f);
        fp ((prefix + "Vel")   .toRawUTF8(), (display + " Vel")   .toRawUTF8(), linRange (0.0f, 1.0f), 0.5f);
    };
    addEnv ("envA",    "Env A",   0.5f);
    addEnv ("envB",    "Env B",   0.5f);
    addEnv ("ampEnv",  "Amp Env", 1.0f);

    fp ("trigThresh",  "Trig Thresh", linRange (0.0f, 1.0f), 0.2f);
    bp ("freeRun",     "Free Run", false);
    fp ("freeRunRate", "Free Run Rate", logRange (0.05f, 10.0f), 1.0f);

    // LFO
    fp ("lfoRate",     "LFO Rate",  logRange (0.01f, 20.0f), 1.0f);
    cp ("lfoShape",    "LFO Shape", { "Sine", "Triangle", "S&H" }, 0);
    bp ("lfoSync",     "LFO Sync", false);
    cp ("lfoSyncDiv",  "LFO Div", { "1/64","1/32","1/16","1/8","1/4","1/2","1/1","2/1","4/1" }, 4);
    fp ("lfoSymmetry", "LFO Sym",  linRange (-1.0f, 1.0f), 0.0f);
    fp ("lfoPhase",    "LFO Phase",linRange (-0.5f, 0.5f), 0.0f);
    fp ("lfoFade",     "LFO Fade", linRange (0.0f, 1.0f), 0.0f);
    bp ("lfoKeySync",  "LFO Key Sync", false);

    // Feedback
    fp ("fbSource",  "FB Source", linRange (0.0f, 1.0f), 0.5f);
    fp ("fbAmount",  "FB Amount", linRange (-1.0f, 1.0f), 0.0f);
    // (fbLfoAmt / fbEnvAmt removed in Pass 4 -- never read by DSP.)

    // Performance macros (Pass 7) -- UI-level controls that drive several
    // underlying parameters at once. Stored in APVTS so they are automatable.
    fpMeta ("perfMacroMovement", "Movement", linRange (0.0f, 1.0f), 0.30f);
    fpMeta ("perfMacroPitch",    "Pitch",    linRange (0.0f, 1.0f), 0.50f);
    fpMeta ("perfMacroColor",    "Color",    linRange (0.0f, 1.0f), 0.50f);
    fpMeta ("perfMacroDrive",    "Drive",    linRange (0.0f, 1.0f), 0.20f);
    fpMeta ("perfMacroSpace",    "Space",    linRange (0.0f, 1.0f), 0.30f);
    fpMeta ("perfMacroTexture",  "Texture",  linRange (0.0f, 1.0f), 0.30f);

    // Master
    fp ("masterGain",  "Master Gain", linRange (-24.0f, 12.0f), 0.0f);
    fp ("dryWet",      "Dry/Wet",     linRange (0.0f, 1.0f), 1.0f);
    fp ("ampEnvDepth", "Amp Env Depth", linRange (0.0f, 1.0f), 0.0f);

    // Morpher
    ipMeta ("snapshotA",     "Snapshot A", 0, 7, 0);
    ipMeta ("snapshotB",     "Snapshot B", 0, 7, 1);
    fpMeta ("morphPosition", "Morph Pos",   linRange (0.0f, 1.0f), 0.0f);
    fp ("morphSpeed",    "Morph Speed", linRange (0.0f, 1.0f), 0.5f);
    // (morphLfoAmt removed in Pass 4 -- never read by DSP.)

    return { p.begin(), p.end() };
}

void SK4nAudioProcessor::cacheParameterPointers()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    pBufferLen = get ("bufferLen"); pFreeze = get ("freeze");
    pCoarsePos = get ("coarsePos"); pFinePos = get ("finePos"); pFineRange = get ("fineRange");
    pSpeed = get ("speed"); pRangeMode = get ("rangeMode"); pWindow = get ("window");
    pOscAToPos = get ("oscAToPos"); pOscBToPos = get ("oscBToPos");
    pFbToPos = get ("fbToPos"); pEnvToPos = get ("envToPos"); pLfoToPos = get ("lfoToPos");

    pOscAPitch = get ("oscAPitch"); pOscAFine = get ("oscAFine");
    pOscAShape = get ("oscAShape"); pOscAFb   = get ("oscAFb");
    pOscAEnvPitch = get ("oscAEnvPitch"); pOscALfoPitch = get ("oscALfoPitch");
    pOscAEnvQuant = get ("oscAEnvQuant"); pOscALfoQuant = get ("oscALfoQuant");

    pOscBPitch = get ("oscBPitch"); pOscBFine = get ("oscBFine");
    pOscBShape = get ("oscBShape"); pOscBFb   = get ("oscBFb");
    pOscBEnvPitch = get ("oscBEnvPitch"); pOscBLfoPitch = get ("oscBLfoPitch");
    pOscBEnvQuant = get ("oscBEnvQuant"); pOscBLfoQuant = get ("oscBLfoQuant");

    pAmBlend = get ("amBlend"); pAmSquare = get ("amSquare"); pAmMix = get ("amMix");

    pDelayTune = get ("delayTune"); pDelayFb = get ("delayFeedback");
    pDelayLfo = get ("delayLfo"); pDelayEnv = get ("delayEnv");
    pDelayMix = get ("delayMix"); pDelayLoCut = get ("delayLoCut");

    pFilterMode = get ("filterMode");
    pFilterCenter = get ("filterCenter"); pFilterGap = get ("filterGap");
    pFilterReson = get ("filterReson"); pFilterBalance = get ("filterBalance");
    pFilterLrOffset = get ("filterLrOffset"); pFilterMix = get ("filterMix");
    pCabDrive = get ("cabDrive"); pCabFold = get ("cabFold");
    pCabTilt = get ("cabTilt"); pCabHiCut = get ("cabHiCut"); pCabLevel = get ("cabLevel");

    pSampleMix = get ("sampleMix");

    pEchoFlgMode = get ("echoFlangerMode");
    pEchoTime = get ("echoTime"); pEchoLrOff = get ("echoLrOff");
    pEchoFb = get ("echoFb"); pEchoLoCut = get ("echoLoCut"); pEchoHiCut = get ("echoHiCut");
    pEchoSync = get ("echoSync"); pEchoMix = get ("echoMix");
    pFlgTime = get ("flgTime"); pFlgDepth = get ("flgDepth");
    pFlgRate = get ("flgRate"); pFlgFb = get ("flgFb"); pFlgSync = get ("flgSync"); pFlgMix = get ("flgMix");

    pReverbSize = get ("reverbSize"); pReverbLoCut = get ("reverbLoCut");
    pReverbHiCut = get ("reverbHiCut"); pReverbMix = get ("reverbMix");

    pEnvAA = get ("envAA"); pEnvAD1 = get ("envAD1"); pEnvABreak = get ("envABreak");
    pEnvAD2 = get ("envAD2"); pEnvAS = get ("envAS"); pEnvAR = get ("envAR"); pEnvAVel = get ("envAVel");
    pEnvBA = get ("envBA"); pEnvBD1 = get ("envBD1"); pEnvBBreak = get ("envBBreak");
    pEnvBD2 = get ("envBD2"); pEnvBS = get ("envBS"); pEnvBR = get ("envBR"); pEnvBVel = get ("envBVel");
    pAmpA  = get ("ampEnvA"); pAmpD1 = get ("ampEnvD1"); pAmpBreak = get ("ampEnvBreak");
    pAmpD2 = get ("ampEnvD2"); pAmpS = get ("ampEnvS"); pAmpR = get ("ampEnvR"); pAmpVel = get ("ampEnvVel");
    pTrigThresh = get ("trigThresh"); pFreeRun = get ("freeRun"); pFreeRunRate = get ("freeRunRate");

    pLfoRate = get ("lfoRate"); pLfoShape = get ("lfoShape");
    pLfoSync = get ("lfoSync"); pLfoSyncDiv = get ("lfoSyncDiv");
    pLfoSymmetry = get ("lfoSymmetry"); pLfoPhase = get ("lfoPhase");
    pLfoFade = get ("lfoFade"); pLfoKeySync = get ("lfoKeySync");

    pFbSource = get ("fbSource"); pFbAmount = get ("fbAmount");

    pMasterGain = get ("masterGain"); pDryWet = get ("dryWet"); pAmpEnvDepth = get ("ampEnvDepth");

    pSnapshotA = get ("snapshotA"); pSnapshotB = get ("snapshotB");
    pMorphPos = get ("morphPosition"); pMorphSpeed = get ("morphSpeed");
}

bool SK4nAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    return in == out
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void SK4nAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    circBuffer.prepare (sampleRate, 8);
    circBuffer.setActiveSeconds (pBufferLen != nullptr ? pBufferLen->load() : 2.0f);

    positionEngine.prepare (sampleRate, samplesPerBlock);
    positionEngine.setActiveSize (circBuffer.getActiveSize());
    positionEngine.reset (pCoarsePos != nullptr
                              ? pCoarsePos->load() * static_cast<float> (circBuffer.getActiveSize())
                              : 0.0f);

    oscA.prepare (sampleRate);
    oscB.prepare (sampleRate);
    readerA.prepare (sampleRate);
    readerB.prepare (sampleRate);

    // Tuned delay max ~120 ms (low pitches)
    tunedDelay.prepare (sampleRate, static_cast<float> (sampleRate) * 0.15f + 64.0f);

    filter8p.prepare (sampleRate);
    cabinetL.prepare (sampleRate);
    cabinetR.prepare (sampleRate);

    echoFlanger.prepare (sampleRate, samplesPerBlock);
    reverbStage.prepare (sampleRate);

    envA.prepare (sampleRate);   envB.prepare (sampleRate);   ampEnv.prepare (sampleRate);
    transientDet.prepare (sampleRate);
    lfo.prepare (sampleRate);

    wetBuffer.setSize    (2, samplesPerBlock, false, false, true);
    drySnapshot.setSize  (2, samplesPerBlock, false, false, true);

    auto initSm = [sampleRate] (juce::SmoothedValue<float>& s, float v)
    { s.reset (sampleRate, kSmoothRamp); s.setCurrentAndTargetValue (v); };

    initSm (smCoarsePos,  pCoarsePos  ? pCoarsePos ->load() : 0.1f);
    initSm (smFinePos,    pFinePos    ? pFinePos   ->load() : 0.0f);
    initSm (smFineRange,  pFineRange  ? pFineRange ->load() : 0.3f);
    initSm (smSpeed,      pSpeed      ? pSpeed     ->load() : 1.0f);
    initSm (smWindow,     pWindow     ? pWindow    ->load() : 0.25f);
    initSm (smOscAToPos,  pOscAToPos  ? pOscAToPos ->load() : 0.0f);
    initSm (smOscBToPos,  pOscBToPos  ? pOscBToPos ->load() : 0.0f);
    initSm (smFbToPos,    pFbToPos    ? pFbToPos   ->load() : 0.0f);
    initSm (smEnvToPos,   pEnvToPos   ? pEnvToPos  ->load() : 0.0f);
    initSm (smLfoToPos,   pLfoToPos   ? pLfoToPos  ->load() : 0.0f);

    initSm (smOscAPitch,  pOscAPitch ? pOscAPitch->load() : 60.0f);
    initSm (smOscAFine,   pOscAFine  ? pOscAFine ->load() : 0.0f);
    initSm (smOscAShape,  pOscAShape ? pOscAShape->load() : 0.0f);
    initSm (smOscAFb,     pOscAFb    ? pOscAFb   ->load() : 0.0f);
    initSm (smOscBPitch,  pOscBPitch ? pOscBPitch->load() : 42.0f);
    initSm (smOscBFine,   pOscBFine  ? pOscBFine ->load() : 0.0f);
    initSm (smOscBShape,  pOscBShape ? pOscBShape->load() : 0.0f);
    initSm (smOscBFb,     pOscBFb    ? pOscBFb   ->load() : 0.0f);
    initSm (smOscAEnvPitch, pOscAEnvPitch ? pOscAEnvPitch->load() : 0.0f);
    initSm (smOscALfoPitch, pOscALfoPitch ? pOscALfoPitch->load() : 0.0f);
    initSm (smOscAEnvQuant, pOscAEnvQuant ? pOscAEnvQuant->load() : 0.001f);
    initSm (smOscALfoQuant, pOscALfoQuant ? pOscALfoQuant->load() : 0.001f);
    initSm (smOscBEnvPitch, pOscBEnvPitch ? pOscBEnvPitch->load() : 0.0f);
    initSm (smOscBLfoPitch, pOscBLfoPitch ? pOscBLfoPitch->load() : 0.0f);
    initSm (smOscBEnvQuant, pOscBEnvQuant ? pOscBEnvQuant->load() : 0.001f);
    initSm (smOscBLfoQuant, pOscBLfoQuant ? pOscBLfoQuant->load() : 0.001f);

    initSm (smAmBlend, pAmBlend ? pAmBlend->load() : 0.5f);
    initSm (smAmMix,   pAmMix   ? pAmMix  ->load() : 0.0f);

    initSm (smDelayTune,  pDelayTune  ? pDelayTune ->load() : 60.0f);
    initSm (smDelayFb,    pDelayFb    ? pDelayFb   ->load() : 0.3f);
    initSm (smDelayLfo,   pDelayLfo   ? pDelayLfo  ->load() : 0.0f);
    initSm (smDelayEnv,   pDelayEnv   ? pDelayEnv  ->load() : 0.0f);
    initSm (smDelayMix,   pDelayMix   ? pDelayMix  ->load() : 0.0f);
    initSm (smDelayLoCut, pDelayLoCut ? pDelayLoCut->load() : 80.0f);

    initSm (smFilterCenter,   pFilterCenter   ? pFilterCenter  ->load() : 8000.0f);
    initSm (smFilterGap,      pFilterGap      ? pFilterGap     ->load() : 0.0f);
    initSm (smFilterReson,    pFilterReson    ? pFilterReson   ->load() : 0.2f);
    initSm (smFilterBalance,  pFilterBalance  ? pFilterBalance ->load() : -1.0f);
    initSm (smFilterLrOffset, pFilterLrOffset ? pFilterLrOffset->load() : 0.0f);
    initSm (smFilterMix,      pFilterMix      ? pFilterMix     ->load() : 0.0f);
    initSm (smCabDrive,  pCabDrive ? pCabDrive->load() : 0.3f);
    initSm (smCabFold,   pCabFold  ? pCabFold ->load() : 0.0f);
    initSm (smCabTilt,   pCabTilt  ? pCabTilt ->load() : 0.0f);
    initSm (smCabHiCut,  pCabHiCut ? pCabHiCut->load() : 12000.0f);
    initSm (smCabLevel,  pCabLevel ? pCabLevel->load() : 0.0f);
    initSm (smSampleMix, pSampleMix ? pSampleMix->load() : 1.0f);

    initSm (smEchoTime, pEchoTime ? pEchoTime->load() : 250.0f);
    initSm (smEchoLrOff, pEchoLrOff ? pEchoLrOff->load() : 0.0f);
    initSm (smEchoFb,   pEchoFb   ? pEchoFb  ->load() : 0.4f);
    initSm (smEchoLoCut, pEchoLoCut ? pEchoLoCut->load() : 80.0f);
    initSm (smEchoHiCut, pEchoHiCut ? pEchoHiCut->load() : 8000.0f);
    initSm (smEchoMix,  pEchoMix  ? pEchoMix ->load() : 0.0f);
    initSm (smFlgTime,  pFlgTime  ? pFlgTime ->load() : 5.0f);
    initSm (smFlgDepth, pFlgDepth ? pFlgDepth->load() : 0.5f);
    initSm (smFlgRate,  pFlgRate  ? pFlgRate ->load() : 0.3f);
    initSm (smFlgFb,    pFlgFb    ? pFlgFb   ->load() : 0.3f);
    initSm (smFlgMix,   pFlgMix   ? pFlgMix  ->load() : 0.0f);

    initSm (smReverbSize,  pReverbSize  ? pReverbSize ->load() : 0.5f);
    initSm (smReverbLoCut, pReverbLoCut ? pReverbLoCut->load() : 100.0f);
    initSm (smReverbHiCut, pReverbHiCut ? pReverbHiCut->load() : 8000.0f);
    initSm (smReverbMix,   pReverbMix   ? pReverbMix  ->load() : 0.15f);

    initSm (smFbAmount, pFbAmount ? pFbAmount->load() : 0.0f);
    initSm (smFbSource, pFbSource ? pFbSource->load() : 0.5f);

    initSm (smMasterGainDb, pMasterGain ? pMasterGain->load() : 0.0f);
    initSm (smDryWet,       pDryWet      ? pDryWet     ->load() : 1.0f);
    initSm (smAmpEnvDepth,  pAmpEnvDepth ? pAmpEnvDepth->load() : 0.0f);

    freeRunSamplesLeft = 0;
    envsReleased = true;
    lastMasterMono = 0.0f;

    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;
    oscAFreqHz.store        (0.0f, std::memory_order_relaxed);
    oscBFreqHz.store        (0.0f, std::memory_order_relaxed);
    transientFollower.store (0.0f, std::memory_order_relaxed);
    feedbackSignalAtom.store(0.0f, std::memory_order_relaxed);
    triggerFiredFlag.store  (false, std::memory_order_relaxed);
}

float SK4nAudioProcessor::effectiveLfoRateHz()
{
    if (pLfoSync != nullptr && pLfoSync->load() >= 0.5f)
    {
        // Sync to host tempo
        if (auto* ph = getPlayHead())
        {
            if (auto info = ph->getPosition())
            {
                if (auto bpm = info->getBpm())
                {
                    static const float divs[9] = {
                        1.0f/64, 1.0f/32, 1.0f/16, 1.0f/8, 1.0f/4, 1.0f/2, 1.0f, 2.0f, 4.0f
                    };
                    int idx = pLfoSyncDiv != nullptr ? juce::jlimit (0, 8, (int) pLfoSyncDiv->load()) : 4;
                    const float beats = divs[idx];
                    return static_cast<float> (*bpm / 60.0 / beats);
                }
            }
        }
    }
    return pLfoRate != nullptr ? pLfoRate->load() : 1.0f;
}

void SK4nAudioProcessor::processBlock (juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto blockStartTicks = juce::Time::getHighResolutionTicks();
    const int numSamples = audioBuffer.getNumSamples();
    const int numChans   = audioBuffer.getNumChannels();
    if (numSamples <= 0) return;

    // ---- Block-rate updates ----
    circBuffer.setActiveSeconds (pBufferLen->load());
    circBuffer.setFrozen        (pFreeze->load() >= 0.5f);
    positionEngine.setActiveSize (circBuffer.getActiveSize());
    positionEngine.setBlockSize  (numSamples);

    // Mode switches
    const int filterMode  = pFilterMode  ? juce::jlimit (0, 1, (int) pFilterMode ->load()) : 0;
    const int echoFlgMode = pEchoFlgMode ? juce::jlimit (0, 1, (int) pEchoFlgMode->load()) : 0;
    if (echoFlgMode != lastEchoFlgMode)
    {
        echoFlanger.setMode (echoFlgMode == 0 ? sk4n::EchoFlanger::Mode::Echo
                                              : sk4n::EchoFlanger::Mode::Flanger);
        lastEchoFlgMode = echoFlgMode;
    }
    const int lfoShape = pLfoShape ? juce::jlimit (0, 2, (int) pLfoShape->load()) : 0;
    if (lfoShape != lastLfoShape)
    {
        lfo.setShape (static_cast<sk4n::GlobalLFO::Shape> (lfoShape));
        lastLfoShape = lfoShape;
    }
    lfo.setSymmetry (pLfoSymmetry ? pLfoSymmetry->load() : 0.0f);
    lfo.setPhase    (pLfoPhase    ? pLfoPhase   ->load() : 0.0f);
    lfo.setFade     (pLfoFade     ? pLfoFade    ->load() * 10.0f : 0.0f);

    envA  .setTimes  (pEnvAA->load(),  pEnvAD1->load(),  pEnvAD2->load(),  pEnvAR->load());
    envA  .setLevels (pEnvABreak->load(), pEnvAS->load());
    envB  .setTimes  (pEnvBA->load(),  pEnvBD1->load(),  pEnvBD2->load(),  pEnvBR->load());
    envB  .setLevels (pEnvBBreak->load(), pEnvBS->load());
    ampEnv.setTimes  (pAmpA ->load(),  pAmpD1 ->load(),  pAmpD2 ->load(),  pAmpR ->load());
    ampEnv.setLevels (pAmpBreak->load(), pAmpS ->load());

    // Smoothed param targets
    smCoarsePos.setTargetValue  (pCoarsePos->load());
    smFinePos  .setTargetValue  (pFinePos  ->load());
    smFineRange.setTargetValue  (pFineRange->load());
    smSpeed    .setTargetValue  (pSpeed    ->load());
    smWindow   .setTargetValue  (pWindow   ->load());
    smOscAToPos.setTargetValue  (pOscAToPos->load());
    smOscBToPos.setTargetValue  (pOscBToPos->load());
    smFbToPos  .setTargetValue  (pFbToPos  ->load());
    smEnvToPos .setTargetValue  (pEnvToPos ->load());
    smLfoToPos .setTargetValue  (pLfoToPos ->load());
    smOscAPitch.setTargetValue  (pOscAPitch->load());
    smOscAFine .setTargetValue  (pOscAFine ->load());
    smOscAShape.setTargetValue  (pOscAShape->load());
    smOscAFb   .setTargetValue  (pOscAFb   ->load());
    smOscAEnvPitch.setTargetValue (pOscAEnvPitch->load());
    smOscALfoPitch.setTargetValue (pOscALfoPitch->load());
    smOscAEnvQuant.setTargetValue (pOscAEnvQuant->load());
    smOscALfoQuant.setTargetValue (pOscALfoQuant->load());
    smOscBPitch.setTargetValue  (pOscBPitch->load());
    smOscBFine .setTargetValue  (pOscBFine ->load());
    smOscBShape.setTargetValue  (pOscBShape->load());
    smOscBFb   .setTargetValue  (pOscBFb   ->load());
    smOscBEnvPitch.setTargetValue (pOscBEnvPitch->load());
    smOscBLfoPitch.setTargetValue (pOscBLfoPitch->load());
    smOscBEnvQuant.setTargetValue (pOscBEnvQuant->load());
    smOscBLfoQuant.setTargetValue (pOscBLfoQuant->load());
    smAmBlend  .setTargetValue  (pAmBlend  ->load());
    smAmMix    .setTargetValue  (pAmMix    ->load());
    smDelayTune.setTargetValue  (pDelayTune->load());
    smDelayFb  .setTargetValue  (pDelayFb  ->load());
    smDelayLfo .setTargetValue  (pDelayLfo ->load());
    smDelayEnv .setTargetValue  (pDelayEnv ->load());
    smDelayMix .setTargetValue  (pDelayMix ->load());
    smDelayLoCut.setTargetValue (pDelayLoCut->load());
    smFilterCenter  .setTargetValue (pFilterCenter ->load());
    smFilterGap     .setTargetValue (pFilterGap    ->load());
    smFilterReson   .setTargetValue (pFilterReson  ->load());
    smFilterBalance .setTargetValue (pFilterBalance->load());
    smFilterLrOffset.setTargetValue (pFilterLrOffset->load());
    smFilterMix     .setTargetValue (pFilterMix    ->load());
    smCabDrive .setTargetValue (pCabDrive->load());
    smCabFold  .setTargetValue (pCabFold ->load());
    smCabTilt  .setTargetValue (pCabTilt ->load());
    smCabHiCut .setTargetValue (pCabHiCut->load());
    smCabLevel .setTargetValue (pCabLevel->load());
    smSampleMix.setTargetValue (pSampleMix->load());
    smEchoTime .setTargetValue (pEchoTime->load());
    smEchoLrOff.setTargetValue (pEchoLrOff->load());
    smEchoFb   .setTargetValue (pEchoFb  ->load());
    smEchoLoCut.setTargetValue (pEchoLoCut->load());
    smEchoHiCut.setTargetValue (pEchoHiCut->load());
    smEchoMix  .setTargetValue (pEchoMix ->load());
    smFlgTime  .setTargetValue (pFlgTime ->load());
    smFlgDepth .setTargetValue (pFlgDepth->load());
    smFlgRate  .setTargetValue (pFlgRate ->load());
    smFlgFb    .setTargetValue (pFlgFb   ->load());
    smFlgMix   .setTargetValue (pFlgMix  ->load());
    smReverbSize .setTargetValue (pReverbSize ->load());
    smReverbLoCut.setTargetValue (pReverbLoCut->load());
    smReverbHiCut.setTargetValue (pReverbHiCut->load());
    smReverbMix  .setTargetValue (pReverbMix  ->load());
    smFbAmount.setTargetValue (pFbAmount->load());
    smFbSource.setTargetValue (pFbSource->load());
    smMasterGainDb.setTargetValue (pMasterGain->load());
    smDryWet      .setTargetValue (pDryWet    ->load());
    smAmpEnvDepth .setTargetValue (pAmpEnvDepth->load());

    auto* L = audioBuffer.getWritePointer (0);
    auto* R = numChans > 1 ? audioBuffer.getWritePointer (1) : L;

    // Snapshot dry stereo
    drySnapshot.setSize (2, numSamples, false, false, true);
    drySnapshot.copyFrom (0, 0, L, numSamples);
    drySnapshot.copyFrom (1, 0, R, numSamples);

    wetBuffer.setSize (2, numSamples, false, false, true);
    auto* wL = wetBuffer.getWritePointer (0);
    auto* wR = wetBuffer.getWritePointer (1);

    const sk4n::PositionEngine::RangeMode rm =
        (pRangeMode->load() < 0.5f) ? sk4n::PositionEngine::RangeMode::Ms
                                    : sk4n::PositionEngine::RangeMode::Pct;

    const float lfoRateHz = effectiveLfoRateHz();
    const bool  freeRun   = pFreeRun->load() >= 0.5f;
    const float trigThresh = pTrigThresh->load();

    float lastEnvAVal = 0.0f, lastEnvBVal = 0.0f, lastAmpEnvVal = 0.0f, lastLfoVal = 0.0f;
    float lastPosA = 0.0f, lastPosB = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = L[i];
        const float dryR = R[i];
        const float monoIn = 0.5f * (dryL + dryR);

        circBuffer.writeSample (monoIn);

        // Trigger detection
        bool fired = false;
        if (! freeRun)
        {
            fired = transientDet.process (monoIn, trigThresh);
            if (fired) envsReleased = false;
            if (! envsReleased && transientDet.isBelow (trigThresh))
            {
                envA.release(); envB.release(); ampEnv.release();
                envsReleased = true;
            }
        }
        else
        {
            transientDet.process (monoIn, trigThresh);  // keep follower updated
            if (freeRunSamplesLeft <= 0)
            {
                fired = true;
                const float r = std::clamp (pFreeRunRate->load(), 0.05f, 10.0f);
                freeRunSamplesLeft = static_cast<int> (getSampleRate() / r);
            }
            else
            {
                --freeRunSamplesLeft;
            }
        }

        if (fired)
        {
            envA  .trigger (pEnvAVel->load());
            envB  .trigger (pEnvBVel->load());
            ampEnv.trigger (pAmpVel ->load());
            if (pLfoKeySync != nullptr && pLfoKeySync->load() >= 0.5f)
                lfo.retriggerWithFade();
            triggerFiredFlag.store (true, std::memory_order_relaxed);
        }

        const float envAVal   = envA.process();
        const float envBVal   = envB.process();
        const float ampEnvVal = ampEnv.process();
        const float lfoVal    = lfo.process (lfoRateHz);

        // Smoothed values
        const float coarse  = smCoarsePos.getNextValue();
        const float fineP   = smFinePos  .getNextValue();
        const float fineR   = smFineRange.getNextValue();
        const float speed   = smSpeed    .getNextValue();
        const float window  = smWindow   .getNextValue();

        const float oscAToPos = smOscAToPos.getNextValue();
        const float oscBToPos = smOscBToPos.getNextValue();
        const float fbToPos   = smFbToPos  .getNextValue();
        const float envToPos  = smEnvToPos .getNextValue();
        const float lfoToPos  = smLfoToPos .getNextValue();

        // Osc A pitch (with Env, LFO modulation, quantized)
        const float oscAPitchTotal =
            smOscAPitch.getNextValue()
          + smOscAFine.getNextValue() / 100.0f
          + envAVal * smOscAEnvPitch.getNextValue() * 12.0f * smOscAEnvQuant.getNextValue()
          + lfoVal  * smOscALfoPitch.getNextValue() * 12.0f * smOscALfoQuant.getNextValue();
        const float oscAOut = oscA.process (oscAPitchTotal,
                                            smOscAFb.getNextValue(),
                                            smOscAShape.getNextValue());
        oscAFreqHz.store (sk4n::PhaseOscillator::negPCurve (oscAPitchTotal),
                          std::memory_order_relaxed);

        const float oscBPitchTotal =
            smOscBPitch.getNextValue()
          + smOscBFine.getNextValue() / 100.0f
          + envBVal * smOscBEnvPitch.getNextValue() * 12.0f * smOscBEnvQuant.getNextValue()
          + lfoVal  * smOscBLfoPitch.getNextValue() * 12.0f * smOscBLfoQuant.getNextValue();
        const float oscBOut = oscB.process (oscBPitchTotal,
                                            smOscBFb.getNextValue(),
                                            smOscBShape.getNextValue());
        oscBFreqHz.store (sk4n::PhaseOscillator::negPCurve (oscBPitchTotal),
                          std::memory_order_relaxed);

        // Internal feedback signal (delayed master)
        const float fbBlend = smFbSource.getNextValue();
        const float fbAmt   = smFbAmount.getNextValue();
        const float fbSignal = lastMasterMono * fbBlend * fbAmt;
        feedbackSignalAtom.store (fbSignal, std::memory_order_relaxed);

        // Position
        const float position = positionEngine.process (
            coarse, fineP, fineR, speed, rm,
            oscAOut, oscAToPos,
            oscBOut, oscBToPos,
            fbSignal, fbToPos,
            envAVal, envToPos,
            lfoVal, lfoToPos);

        // Sample reads (both oscillators read at the same Position with their own phase windows)
        const float sampleA = readerA.read (circBuffer, circBuffer.getWriteIndex(),
                                            position, oscAOut, window, oscA.currentAmpWindow());
        const float sampleB = readerB.read (circBuffer, circBuffer.getWriteIndex(),
                                            position, oscBOut, window, oscB.currentAmpWindow());
        const float sampleOut = (sampleA + sampleB) * 0.5f;

        // AM section
        const float amBlend  = smAmBlend.getNextValue();
        const bool  amSquare = pAmSquare->load() >= 0.5f;
        const float amOut    = sk4n::AMSection::process (sampleOut, oscAOut, oscBOut, amBlend, amSquare);

        // Tuned delay (input = AM section output)
        const float delayTune = smDelayTune.getNextValue()
                              + lfoVal  * smDelayLfo.getNextValue() * 24.0f
                              + envAVal * smDelayEnv.getNextValue() * 24.0f;
        const float delayOut = tunedDelay.process (amOut,
                                                    delayTune,
                                                    smDelayFb.getNextValue(),
                                                    smDelayLoCut.getNextValue());

        // Mixer channels: Sample (sampleOut), AM (amOut), Delay (delayOut), Filter (filterL/R).
        // Each channel is independently mixable. Filter takes the clean sampleOut as input
        // so its contribution is a colored version of the sample reader, not a re-coloring
        // of the already-mixed signal.
        const float sampleMix = smSampleMix.getNextValue();
        const float amMix     = smAmMix    .getNextValue();
        const float delayMix  = smDelayMix .getNextValue();
        const float filterMix = smFilterMix.getNextValue();

        float filterL = sampleOut;
        float filterR = sampleOut;
        if (filterMode == 0)
        {
            filter8p.process (filterL, filterR,
                              std::clamp (smFilterCenter.getNextValue(), 20.0f, 20000.0f),
                              smFilterGap.getNextValue(),
                              std::clamp (smFilterReson.getNextValue(), 0.0f, 1.0f),
                              smFilterBalance.getNextValue(),
                              smFilterLrOffset.getNextValue());
            (void) smCabDrive.getNextValue(); (void) smCabFold.getNextValue();
            (void) smCabTilt .getNextValue(); (void) smCabHiCut.getNextValue();
            (void) smCabLevel.getNextValue();
        }
        else
        {
            const float drive = smCabDrive.getNextValue();
            const float fold  = smCabFold .getNextValue();
            const float tilt  = smCabTilt .getNextValue();
            const float hiCut = smCabHiCut.getNextValue();
            const float level = smCabLevel.getNextValue();
            filterL = cabinetL.process (sampleOut, drive, fold, tilt, hiCut, level);
            filterR = cabinetR.process (sampleOut, drive, fold, tilt, hiCut, level);
            (void) smFilterCenter.getNextValue(); (void) smFilterGap.getNextValue();
            (void) smFilterReson.getNextValue();  (void) smFilterBalance.getNextValue();
            (void) smFilterLrOffset.getNextValue();
        }

        const float commonMix = sampleMix * sampleOut + amMix * amOut + delayMix * delayOut;
        float wetL = commonMix + filterMix * filterL;
        float wetR = commonMix + filterMix * filterR;

        // Echo / Flanger (mode crossfade handled internally)
        echoFlanger.process (wetL, wetR,
                             smEchoTime.getNextValue(), smEchoLrOff.getNextValue(),
                             smEchoFb.getNextValue(), smEchoLoCut.getNextValue(),
                             smEchoHiCut.getNextValue(), smEchoMix.getNextValue(),
                             smFlgTime.getNextValue(), smFlgDepth.getNextValue(),
                             smFlgRate.getNextValue(), smFlgFb.getNextValue(),
                             smFlgMix.getNextValue());

        // Amp Env Depth: 0 = no effect, 1 = full envelope shaping.
        // The 1 - depth*(1 - env) form leaves steady-state output at unity when
        // sustain = 1 regardless of depth, so dialing in transient shaping does
        // not lose average level.
        const float ampDepth = smAmpEnvDepth.getNextValue();
        const float ampGain  = 1.0f - ampDepth * (1.0f - ampEnvVal);
        wetL *= ampGain;
        wetR *= ampGain;

        wL[i] = wetL;
        wR[i] = wetR;

        lastMasterMono = (wetL + wetR) * 0.5f;

        lastEnvAVal = envAVal; lastEnvBVal = envBVal; lastAmpEnvVal = ampEnvVal; lastLfoVal = lfoVal;
        const float windowSamples = window * static_cast<float> (circBuffer.getActiveSize()) * 0.25f;
        lastPosA = position - windowSamples * oscAOut;
        lastPosB = position - windowSamples * oscBOut;
    }

    // ---- Block-rate Reverb ----
    reverbStage.processBlock (wL, wR, numSamples,
                              smReverbSize.getCurrentValue(),
                              smReverbLoCut.getCurrentValue(),
                              smReverbHiCut.getCurrentValue(),
                              smReverbMix.getCurrentValue());

    // ---- Final pass: Master gain -> Soft clip -> Dry/Wet vs original dry ----
    const float* dL = drySnapshot.getReadPointer (0);
    const float* dR = drySnapshot.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float gainDb  = smMasterGainDb.getNextValue();
        const float gainLin = std::pow (10.0f, std::clamp (gainDb, -24.0f, 12.0f) / 20.0f);
        const float dryWet  = smDryWet.getNextValue();

        float w_l = sk4n::SoftClipper::process (wL[i] * gainLin);
        float w_r = sk4n::SoftClipper::process (wR[i] * gainLin);

        L[i] = dL[i] + dryWet * (w_l - dL[i]);
        R[i] = dR[i] + dryWet * (w_r - dR[i]);
    }

    lastReadPosA.store (lastPosA, std::memory_order_relaxed);
    lastReadPosB.store (lastPosB, std::memory_order_relaxed);
    lastEnvA   .store (lastEnvAVal,   std::memory_order_relaxed);
    lastEnvB   .store (lastEnvBVal,   std::memory_order_relaxed);
    lastAmpEnv .store (lastAmpEnvVal, std::memory_order_relaxed);
    lastLfo    .store (lastLfoVal,    std::memory_order_relaxed);
    transientFollower.store (transientDet.currentFollower(), std::memory_order_relaxed);
    currentBlockSize  = numSamples;
    currentSampleRate = getSampleRate() > 0.0 ? getSampleRate() : currentSampleRate;

    // Output peak meters (decaying)
    {
        float peakL = 0.0f, peakR = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            peakL = juce::jmax (peakL, std::fabs (L[i]));
            peakR = juce::jmax (peakR, std::fabs (R[i]));
        }
        const float prevL = outputPeakL.load (std::memory_order_relaxed);
        const float prevR = outputPeakR.load (std::memory_order_relaxed);
        outputPeakL.store (juce::jmax (peakL, prevL * 0.85f), std::memory_order_relaxed);
        outputPeakR.store (juce::jmax (peakR, prevR * 0.85f), std::memory_order_relaxed);
    }

    // CPU process load: how long this block took versus how long it had to be done in.
    const auto blockEndTicks = juce::Time::getHighResolutionTicks();
    const double elapsedSec  = juce::Time::highResolutionTicksToSeconds (blockEndTicks - blockStartTicks);
    const double allowedSec  = (double) numSamples / juce::jmax (1.0, currentSampleRate);
    const float  pct  = juce::jlimit (0.0f, 100.0f,
                                       (float) (elapsedSec / juce::jmax (1.0e-9, allowedSec) * 100.0));
    const float  prev = processLoadPercent.load (std::memory_order_relaxed);
    processLoadPercent.store (prev * 0.9f + pct * 0.1f, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* SK4nAudioProcessor::createEditor()
{
    return new SK4nAudioProcessorEditor (*this);
}

void SK4nAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("SK4nState");
    // Editor window size (0 means "not yet known / use default"); plain properties on the root
    // tree, not parameters -- the disclosure-open state lives as a property on apvts.state
    // instead since that tree is already round-tripped via copyState()/replaceState() below.
    root.setProperty ("editor_width",  editorWidth.load  (std::memory_order_relaxed), nullptr);
    root.setProperty ("editor_height", editorHeight.load (std::memory_order_relaxed), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (morpher.saveAllToValueTree(), nullptr);
    root.appendChild (randomizer.saveLocksToValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void SK4nAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("SK4nState"))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            editorWidth.store  ((int) vt.getProperty ("editor_width",  0), std::memory_order_relaxed);
            editorHeight.store ((int) vt.getProperty ("editor_height", 0), std::memory_order_relaxed);
            for (int i = 0; i < vt.getNumChildren(); ++i)
            {
                auto child = vt.getChild (i);
                if (child.hasType (apvts.state.getType()))
                    apvts.replaceState (child);
                else if (child.hasType ("MorpherSnapshots"))
                    morpher.loadAllFromValueTree (child);
                else if (child.hasType ("RandomizerLocks"))
                    randomizer.loadLocksFromValueTree (child);
            }
        }
        else if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SK4nAudioProcessor();
}
