#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>

#include "DSP/CircularBuffer.h"
#include "DSP/PositionEngine.h"
#include "DSP/PhaseOscillator.h"
#include "DSP/SampleReader.h"
#include "DSP/TunedDelay.h"
#include "DSP/EightPoleFilter.h"
#include "DSP/Cabinet.h"
#include "DSP/EchoFlanger.h"
#include "DSP/ReverbStage.h"
#include "DSP/ADBDSREnvelope.h"
#include "DSP/TransientDetector.h"
#include "DSP/GlobalLFO.h"
#include "DSP/PresetMorpher.h"
#include "DSP/Smoothers.h"
#include "UI/Randomizer.h"
#include "UI/PerformanceMacro.h"

class SK4nAudioProcessor : public juce::AudioProcessor
{
public:
    SK4nAudioProcessor();
    ~SK4nAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SK4n"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    sk4n::PresetMorpher                morpher;
    sk4n_ui::Randomizer                randomizer;

    // Performance macros (constructed in processor ctor after apvts).
    std::unique_ptr<sk4n_ui::PerformanceMacro> macroMovement;
    std::unique_ptr<sk4n_ui::PerformanceMacro> macroPitch;
    std::unique_ptr<sk4n_ui::PerformanceMacro> macroColor;
    std::unique_ptr<sk4n_ui::PerformanceMacro> macroDrive;
    std::unique_ptr<sk4n_ui::PerformanceMacro> macroSpace;
    std::unique_ptr<sk4n_ui::PerformanceMacro> macroTexture;

    // Editor accessors
    int   getWriteIndex()      const { return circBuffer.getWriteIndex(); }
    int   getActiveSize()      const { return circBuffer.getActiveSize(); }
    float getCurrentReadPosA() const { return lastReadPosA.load (std::memory_order_relaxed); }
    float getCurrentReadPosB() const { return lastReadPosB.load (std::memory_order_relaxed); }
    const float* getBufferData() const { return circBuffer.data(); }
    float getEnvAValue()        const { return lastEnvA.load    (std::memory_order_relaxed); }
    float getEnvBValue()        const { return lastEnvB.load    (std::memory_order_relaxed); }
    float getAmpEnvValue()      const { return lastAmpEnv.load  (std::memory_order_relaxed); }
    float getLfoValue()         const { return lastLfo.load     (std::memory_order_relaxed); }

    void  saveSnapshotToSlot (int slot)  { morpher.saveSnapshot (slot); }

    // Read-only diagnostics (atomic loads; safe from any thread).
    float  getOscAFrequencyHz()   const { return oscAFreqHz.load    (std::memory_order_relaxed); }
    float  getOscBFrequencyHz()   const { return oscBFreqHz.load    (std::memory_order_relaxed); }
    float  getTransientFollower() const { return transientFollower.load (std::memory_order_relaxed); }
    bool   consumeTriggerFired()        { return triggerFiredFlag.exchange (false, std::memory_order_relaxed); }
    float  getFeedbackSignal()    const { return feedbackSignalAtom.load (std::memory_order_relaxed); }
    float  getCurrentMorphPos()   const { return morpher.getCurrentPos(); }
    double getCurrentSampleRate() const { return currentSampleRate; }
    int    getCurrentBlockSize()  const { return currentBlockSize; }
    float  getProcessLoad()       const { return processLoadPercent.load (std::memory_order_relaxed); }
    float  getOutputPeakL()       const { return outputPeakL.load (std::memory_order_relaxed); }
    float  getOutputPeakR()       const { return outputPeakR.load (std::memory_order_relaxed); }

    // Persisted editor window size (0 means "not yet known / use default"). The editor writes
    // these from resized(); getStateInformation/setStateInformation carry them across save/load.
    int  getEditorWidth()  const { return editorWidth.load  (std::memory_order_relaxed); }
    int  getEditorHeight() const { return editorHeight.load (std::memory_order_relaxed); }
    void setEditorSize (int w, int h)
    {
        editorWidth.store  (w, std::memory_order_relaxed);
        editorHeight.store (h, std::memory_order_relaxed);
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void cacheParameterPointers();
    float effectiveLfoRateHz();

    // -------- DSP --------
    sk4n::CircularBuffer    circBuffer;
    sk4n::PositionEngine    positionEngine;
    sk4n::PhaseOscillator   oscA, oscB;
    sk4n::SampleReader      readerA, readerB;
    sk4n::TunedDelay        tunedDelay;
    sk4n::EightPoleFilter   filter8p;
    sk4n::Cabinet           cabinetL, cabinetR;
    sk4n::EchoFlanger       echoFlanger;
    sk4n::ReverbStage       reverbStage;
    sk4n::ADBDSREnvelope    envA, envB, ampEnv;
    sk4n::TransientDetector transientDet;
    sk4n::GlobalLFO         lfo;

    // Wet temp buffers
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> drySnapshot;

    // Smoothed per-sample params
    juce::SmoothedValue<float> smCoarsePos, smFinePos, smFineRange, smSpeed, smWindow;
    juce::SmoothedValue<float> smOscAToPos, smOscBToPos, smFbToPos, smEnvToPos, smLfoToPos;
    juce::SmoothedValue<float> smOscAPitch, smOscAFine, smOscAShape, smOscAFb;
    juce::SmoothedValue<float> smOscBPitch, smOscBFine, smOscBShape, smOscBFb;
    juce::SmoothedValue<float> smOscAEnvPitch, smOscALfoPitch, smOscAEnvQuant, smOscALfoQuant;
    juce::SmoothedValue<float> smOscBEnvPitch, smOscBLfoPitch, smOscBEnvQuant, smOscBLfoQuant;
    juce::SmoothedValue<float> smAmBlend, smAmMix;
    juce::SmoothedValue<float> smDelayTune, smDelayFb, smDelayLfo, smDelayEnv, smDelayMix, smDelayLoCut;
    juce::SmoothedValue<float> smFilterCenter, smFilterGap, smFilterReson, smFilterBalance, smFilterLrOffset, smFilterMix;
    juce::SmoothedValue<float> smCabDrive, smCabFold, smCabTilt, smCabHiCut, smCabLevel;
    juce::SmoothedValue<float> smSampleMix;
    juce::SmoothedValue<float> smEchoTime, smEchoLrOff, smEchoFb, smEchoLoCut, smEchoHiCut, smEchoMix;
    juce::SmoothedValue<float> smFlgTime, smFlgDepth, smFlgRate, smFlgFb, smFlgMix;
    juce::SmoothedValue<float> smReverbSize, smReverbLoCut, smReverbHiCut, smReverbMix;
    juce::SmoothedValue<float> smFbAmount, smFbSource;
    juce::SmoothedValue<float> smMasterGainDb, smDryWet, smAmpEnvDepth;

    // -------- Parameter pointers --------
    std::atomic<float> *pBufferLen=nullptr, *pFreeze=nullptr;
    std::atomic<float> *pCoarsePos=nullptr, *pFinePos=nullptr, *pFineRange=nullptr, *pSpeed=nullptr, *pRangeMode=nullptr, *pWindow=nullptr;
    std::atomic<float> *pOscAToPos=nullptr, *pOscBToPos=nullptr, *pFbToPos=nullptr, *pEnvToPos=nullptr, *pLfoToPos=nullptr;

    std::atomic<float> *pOscAPitch=nullptr, *pOscAFine=nullptr, *pOscAShape=nullptr, *pOscAFb=nullptr;
    std::atomic<float> *pOscAEnvPitch=nullptr, *pOscALfoPitch=nullptr, *pOscAEnvQuant=nullptr, *pOscALfoQuant=nullptr;
    std::atomic<float> *pOscBPitch=nullptr, *pOscBFine=nullptr, *pOscBShape=nullptr, *pOscBFb=nullptr;
    std::atomic<float> *pOscBEnvPitch=nullptr, *pOscBLfoPitch=nullptr, *pOscBEnvQuant=nullptr, *pOscBLfoQuant=nullptr;

    std::atomic<float> *pAmBlend=nullptr, *pAmSquare=nullptr, *pAmMix=nullptr;
    std::atomic<float> *pDelayTune=nullptr, *pDelayFb=nullptr, *pDelayLfo=nullptr, *pDelayEnv=nullptr, *pDelayMix=nullptr, *pDelayLoCut=nullptr;

    std::atomic<float> *pFilterMode=nullptr;
    std::atomic<float> *pFilterCenter=nullptr, *pFilterGap=nullptr, *pFilterReson=nullptr, *pFilterBalance=nullptr, *pFilterLrOffset=nullptr, *pFilterMix=nullptr;
    std::atomic<float> *pCabDrive=nullptr, *pCabFold=nullptr, *pCabTilt=nullptr, *pCabHiCut=nullptr, *pCabLevel=nullptr;

    std::atomic<float> *pSampleMix=nullptr;

    std::atomic<float> *pEchoFlgMode=nullptr;
    std::atomic<float> *pEchoTime=nullptr, *pEchoLrOff=nullptr, *pEchoFb=nullptr, *pEchoLoCut=nullptr, *pEchoHiCut=nullptr, *pEchoSync=nullptr, *pEchoMix=nullptr;
    std::atomic<float> *pFlgTime=nullptr, *pFlgDepth=nullptr, *pFlgRate=nullptr, *pFlgFb=nullptr, *pFlgSync=nullptr, *pFlgMix=nullptr;

    std::atomic<float> *pReverbSize=nullptr, *pReverbLoCut=nullptr, *pReverbHiCut=nullptr, *pReverbMix=nullptr;

    std::atomic<float> *pEnvAA=nullptr, *pEnvAD1=nullptr, *pEnvABreak=nullptr, *pEnvAD2=nullptr, *pEnvAS=nullptr, *pEnvAR=nullptr, *pEnvAVel=nullptr;
    std::atomic<float> *pEnvBA=nullptr, *pEnvBD1=nullptr, *pEnvBBreak=nullptr, *pEnvBD2=nullptr, *pEnvBS=nullptr, *pEnvBR=nullptr, *pEnvBVel=nullptr;
    std::atomic<float> *pAmpA=nullptr, *pAmpD1=nullptr, *pAmpBreak=nullptr, *pAmpD2=nullptr, *pAmpS=nullptr, *pAmpR=nullptr, *pAmpVel=nullptr;
    std::atomic<float> *pTrigThresh=nullptr, *pFreeRun=nullptr, *pFreeRunRate=nullptr;

    std::atomic<float> *pLfoRate=nullptr, *pLfoShape=nullptr, *pLfoSync=nullptr, *pLfoSyncDiv=nullptr;
    std::atomic<float> *pLfoSymmetry=nullptr, *pLfoPhase=nullptr, *pLfoFade=nullptr, *pLfoKeySync=nullptr;

    std::atomic<float> *pFbSource=nullptr, *pFbAmount=nullptr;

    std::atomic<float> *pMasterGain=nullptr, *pDryWet=nullptr, *pAmpEnvDepth=nullptr;

    std::atomic<float> *pSnapshotA=nullptr, *pSnapshotB=nullptr, *pMorphPos=nullptr, *pMorphSpeed=nullptr;

    // Free-run timer for envelopes
    int   freeRunSamplesLeft = 0;
    bool  envsReleased = true;

    // Internal feedback: delayed master output
    float lastMasterMono = 0.0f;

    // Visualization
    std::atomic<float> lastReadPosA { 0.0f };
    std::atomic<float> lastReadPosB { 0.0f };
    std::atomic<float> lastEnvA { 0.0f };
    std::atomic<float> lastEnvB { 0.0f };
    std::atomic<float> lastAmpEnv { 0.0f };
    std::atomic<float> lastLfo { 0.0f };

    // Diagnostic read-only state
    std::atomic<float> oscAFreqHz         { 0.0f };
    std::atomic<float> oscBFreqHz         { 0.0f };
    std::atomic<float> transientFollower  { 0.0f };
    std::atomic<float> feedbackSignalAtom { 0.0f };
    std::atomic<bool>  triggerFiredFlag   { false };
    double             currentSampleRate  = 44100.0;
    int                currentBlockSize   = 512;
    std::atomic<float> processLoadPercent { 0.0f };
    std::atomic<float> outputPeakL { 0.0f };
    std::atomic<float> outputPeakR { 0.0f };

    // Persisted editor size (see getEditorWidth/getEditorHeight/setEditorSize above).
    std::atomic<int> editorWidth  { 0 };
    std::atomic<int> editorHeight { 0 };

    // Mode bookkeeping
    int lastFilterMode = 0;
    int lastEchoFlgMode = 0;
    int lastLfoShape = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SK4nAudioProcessor)
};
