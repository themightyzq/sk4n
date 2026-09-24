#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <zqsfx_ui/zqsfx_ui.h>
#include <memory>
#include <vector>
#include <array>

#include "PluginProcessor.h"
#include "UI/SK4nLookAndFeel.h"
#include "UI/SectionPanel.h"
#include "UI/BufferDisplay.h"
#include "UI/CircularMorpher.h"
#include "UI/ModeSwitcher.h"
#include "UI/FilterResponseDisplay.h"
#include "UI/ModMeter.h"
#include "UI/EnvelopeMeter.h"
#include "UI/KnobControl.h"
#include "UI/ToggleControl.h"
#include "UI/ChoiceControl.h"
#include "UI/FireDot.h"
#include "UI/HelpOverlay.h"
#include "UI/DiceButton.h"
#include "UI/Randomizer.h"
#include "UI/DisclosureRow.h"
#include "UI/OutputMeter.h"

class SK4nAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit SK4nAudioProcessorEditor (SK4nAudioProcessor&);
    ~SK4nAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    enum class Disclosure { None, Oscillators, Filter, FX, Modulation, Advanced };

    struct EditorSection
    {
        std::unique_ptr<sk4n_ui::SectionPanel> panel;
        std::vector<std::unique_ptr<juce::Component>> children;
    };

    // Public so the headless UI gate (tools/ui_snapshot) can render an expanded disclosure. Every
    // disclosure starts collapsed, so a default render shows none of the section panels -- which
    // is how the Filter/FX overlapping-control-rows bug survived the whole house-UI migration.
    void setDisclosure (Disclosure d);

private:
    void timerCallback() override;

    // Polls a std::function at its own Hz, independent of the editor's main 15 Hz UI timer
    // (used for the CPU-load readout, which the spec calls out as a 4 Hz update).
    class RateTimer : private juce::Timer
    {
    public:
        RateTimer (std::function<void()> cb, int hz) : callback (std::move (cb))
        { startTimerHz (hz); }
    private:
        void timerCallback() override { if (callback) callback(); }
        std::function<void()> callback;
    };

    void applyDisclosureExpansion (Disclosure d);

    template <typename T, typename... Args>
    T* addChild (EditorSection& s, Args&&... args);

    sk4n_ui::KnobControl* addKnob (EditorSection& s,
                                    const juce::String& paramID,
                                    const juce::String& displayName,
                                    std::function<juce::String (float)> formatter,
                                    sk4n_ui::KnobSize size = sk4n_ui::KnobSize::Medium);

    void buildAllSections();
    void buildPosition();
    void buildOscA();
    void buildOscB();
    void buildMixer();
    void buildAM();
    void buildDelay();
    void buildFilter();
    void buildEchoFlanger();
    void buildReverb();
    void buildEnv (EditorSection& s, const juce::String& title, juce::Colour accent,
                   const juce::String& prefix, const juce::String& letterPrefix, bool showDepth);
    void buildLfo();
    void buildTrigger();
    void buildMaster();

    void buildPerformanceRow();
    void buildDisclosureRows();
    void layoutDisclosureContent (juce::Rectangle<int> area);
    void hideAllSectionPanels();

    int  getCompactHeight() const;
    int  getExpandedHeight() const;
    int  contentHeightFor (Disclosure) const;

    void toggleHelp();
    void layoutHelpOverlay();
    void showAboutBox();

    SK4nAudioProcessor& processorRef;
    std::unique_ptr<sk4n_ui::SK4nLookAndFeel> laf;

    juce::TooltipWindow tooltipWindow { this, 600 };

    // Header
    juce::Label  titleLabel, subtitleLabel, presetLabel;
    juce::TextButton helpButton { "?" };
    std::unique_ptr<sk4n_ui::DiceButton> masterDice;
    zqsfx::ui::LogoMark logo { "SK4n" };

    // Read-only audio-thread CPU load readout (house LCD styling, like presetLabel). Updated at
    // 4 Hz from processorRef.getProcessLoad(); never bound to a parameter.
    juce::Label cpuLoadLabel;
    std::unique_ptr<RateTimer> cpuLoadTimer;

    // Centerpiece
    std::unique_ptr<sk4n_ui::BufferDisplay>   bufferDisplay;
    std::unique_ptr<sk4n_ui::CircularMorpher> circularMorpher;

    // Performance row
    std::unique_ptr<sk4n_ui::KnobControl> kMovement, kPitch, kColor, kDrive, kSpace, kTexture;
    std::unique_ptr<sk4n_ui::KnobControl> kDryWet, kMasterGain;
    std::unique_ptr<sk4n_ui::OutputMeter> outputMeter;

    // Disclosures
    std::array<std::unique_ptr<sk4n_ui::DisclosureRow>, 5> disclosures;
    Disclosure openDisclosure = Disclosure::None;

    // All section panels (built once; visibility toggled per disclosure)
    EditorSection position, oscA, oscB, mixer, am, delay, filter, echoFlg, reverb;
    EditorSection envA, envB, ampEnv, lfo, trigger, master;

    sk4n_ui::ModeSwitcher* filterModeSwitch = nullptr;
    sk4n_ui::ModeSwitcher* echoModeSwitch   = nullptr;
    sk4n_ui::FilterResponseDisplay* filterResp = nullptr;
    std::vector<juce::Component*> filter8PControls;
    std::vector<juce::Component*> cabinetControls;
    std::vector<juce::Component*> echoControls;
    std::vector<juce::Component*> flangerControls;
    sk4n_ui::ModMeter*    triggerMeter = nullptr;
    sk4n_ui::FireDot*     fireDot      = nullptr;
    sk4n_ui::KnobControl* oscAPitchKnob = nullptr;
    sk4n_ui::KnobControl* oscBPitchKnob = nullptr;

    std::unique_ptr<sk4n_ui::HelpOverlay> helpOverlay;

    // For visibility tracking
    std::vector<sk4n_ui::SectionPanel*> allSectionPanels;

    void applyFilterModeVisibility();
    void applyEchoModeVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SK4nAudioProcessorEditor)
};
