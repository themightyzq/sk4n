cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS Universal Binary architectures")
endif()

project(SK4n VERSION 0.2.0 LANGUAGES C CXX)

# Enable CTest for test registration
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.7
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# The ZQ SFX house UI (tokens, LookAndFeel, bound controls, embedded OFL fonts). Must come
# AFTER JUCE. Pinned by tag: bump deliberately, then re-run sk4n_ui_snapshot and compare.
FetchContent_Declare(zqsfx_ui
    GIT_REPOSITORY https://github.com/themightyzq/zqsfx_ui.git
    GIT_TAG        v0.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(zqsfx_ui)

# ---- Shared DSP source list ----
set(SK4N_DSP_SOURCES
    Source/DSP/Smoothers.h
    Source/DSP/SVFFilter.h
    Source/DSP/SoftClipper.h
    Source/DSP/AMSection.h
    Source/DSP/CircularBuffer.cpp
    Source/DSP/CircularBuffer.h
    Source/DSP/PositionEngine.cpp
    Source/DSP/PositionEngine.h
    Source/DSP/PhaseOscillator.cpp
    Source/DSP/PhaseOscillator.h
    Source/DSP/SampleReader.cpp
    Source/DSP/SampleReader.h
    Source/DSP/TunedDelay.cpp
    Source/DSP/TunedDelay.h
    Source/DSP/Cabinet.cpp
    Source/DSP/Cabinet.h
    Source/DSP/EightPoleFilter.cpp
    Source/DSP/EightPoleFilter.h
    Source/DSP/EchoFlanger.cpp
    Source/DSP/EchoFlanger.h
    Source/DSP/ReverbStage.cpp
    Source/DSP/ReverbStage.h
    Source/DSP/ADBDSREnvelope.cpp
    Source/DSP/ADBDSREnvelope.h
    Source/DSP/TransientDetector.cpp
    Source/DSP/TransientDetector.h
    Source/DSP/GlobalLFO.cpp
    Source/DSP/GlobalLFO.h
    Source/DSP/PresetMorpher.cpp
    Source/DSP/PresetMorpher.h
)

# ---- Plugin target ----
juce_add_plugin(SK4n
    COMPANY_NAME              "ZQ SFX"
    COMPANY_WEBSITE           "https://www.zq-sfx.com"
    COMPANY_EMAIL             "connect@zq-sfx.com"
    COMPANY_COPYRIGHT         "Copyright (c) 2026 ZQ SFX"
    BUNDLE_ID                 "com.zqsfx.sk4n"
    PLUGIN_MANUFACTURER_CODE  ZQSF
    PLUGIN_CODE               Sk4n
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "SK4n"
    PLUGIN_NAME               "SK4n"
    DESCRIPTION               "Real-time wave-scanning FX"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
)

set(SK4N_UI_SOURCES
    Source/UI/SK4nLookAndFeel.h
    Source/UI/SK4nLookAndFeel.cpp
    Source/UI/ValueFormatters.h
    Source/UI/ValueFormatters.cpp
    Source/UI/KnobControl.h
    Source/UI/KnobControl.cpp
    Source/UI/ToggleControl.h
    Source/UI/ToggleControl.cpp
    Source/UI/ChoiceControl.h
    Source/UI/ChoiceControl.cpp
    Source/UI/SectionPanel.h
    Source/UI/SectionPanel.cpp
    Source/UI/ModeSwitcher.h
    Source/UI/ModeSwitcher.cpp
    Source/UI/BufferDisplay.h
    Source/UI/BufferDisplay.cpp
    Source/UI/ModMeter.h
    Source/UI/ModMeter.cpp
    Source/UI/EnvelopeMeter.h
    Source/UI/EnvelopeMeter.cpp
    Source/UI/FilterResponseDisplay.h
    Source/UI/FilterResponseDisplay.cpp
    Source/UI/SnapshotRow.h
    Source/UI/SnapshotRow.cpp
    Source/UI/CircularMorpher.h
    Source/UI/CircularMorpher.cpp
    Source/UI/FireDot.h
    Source/UI/FireDot.cpp
    Source/UI/HelpOverlay.h
    Source/UI/HelpOverlay.cpp
    Source/UI/Randomizer.h
    Source/UI/Randomizer.cpp
    Source/UI/DiceButton.h
    Source/UI/DiceButton.cpp
    Source/UI/PerformanceMacro.h
    Source/UI/PerformanceMacro.cpp
    Source/UI/OutputMeter.h
    Source/UI/OutputMeter.cpp
    Source/UI/DisclosureRow.h
    Source/UI/DisclosureRow.cpp
)

target_sources(SK4n PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    ${SK4N_DSP_SOURCES}
    ${SK4N_UI_SOURCES}
)

target_compile_definitions(SK4n PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_JACK=0
    JUCE_ALSA=0
)

target_include_directories(SK4n PRIVATE Source)

target_link_libraries(SK4n
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        zqsfx::ui
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- UI snapshot tool (headless before/after render gate for the zqsfx_ui migration) ----
# Links against the plugin's own shared-code target ("SK4n", produced by juce_add_plugin above)
# instead of recompiling PluginProcessor.cpp/PluginEditor.cpp a second time. SK4n never calls
# juce_generate_juce_header and every SK4n source includes JUCE module headers directly (no
# generated JuceHeader.h aggregator), so -- unlike LFlOw's lflow_ui_snapshot -- no
# JUCE_GENERATED_SOURCES_DIRECTORY propagation is needed here.
juce_add_console_app(sk4n_ui_snapshot PRODUCT_NAME "sk4n_ui_snapshot")
target_sources(sk4n_ui_snapshot PRIVATE tools/ui_snapshot/main.cpp)
target_include_directories(sk4n_ui_snapshot PRIVATE Source)
target_link_libraries(sk4n_ui_snapshot PRIVATE
    SK4n
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    zqsfx::ui
)

# ---- Test console app ----
juce_add_console_app(SK4nTests
    PRODUCT_NAME "SK4nTests"
)

target_sources(SK4nTests PRIVATE
    Tests/Tests.cpp
    ${SK4N_DSP_SOURCES}
)

target_compile_definitions(SK4nTests PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_include_directories(SK4nTests PRIVATE Source)

target_link_libraries(SK4nTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Register with CTest
add_test(NAME SK4nTests COMMAND SK4nTests)
