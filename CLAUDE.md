Universal ZQ SFX rules (identity, real-time safety, VCS policy, signing, shared agents, shared docs) live in ../CLAUDE.md and apply here. This file only adds what is specific to SK4n.

## What SK4n is

Real-time wave-scanning FX (CMake `DESCRIPTION`). Continuously buffers incoming
audio, then drives a pair of phase-modulated sine oscillators that scan that
buffer — anything from rhythmic sub-audio scratching to harmonically complex
audio-rate synthesis tones (README.md). 100+ parameters under APVTS; all
per-sample-DSP continuous parameters carry a 10 ms `SmoothedValue` ramp.

## Identity

- `PRODUCT_NAME` / `PLUGIN_NAME`: `SK4n`
- `BUNDLE_ID`: `com.zqsfx.sk4n`
- `PLUGIN_CODE`: `Sk4n` — **frozen, never change** (hosts key saved sessions on
  manufacturer + plugin code; see ../CLAUDE.md §2)
- `PLUGIN_MANUFACTURER_CODE`: `ZQSF`, `COMPANY_NAME`: `ZQ SFX`

## Formats & toolchain

`FORMATS VST3 AU Standalone`. `COPY_PLUGIN_AFTER_BUILD FALSE` — installs are a
manual step (see Build below). JUCE via `FetchContent`, pinned `GIT_TAG 8.0.7`
(`GIT_SHALLOW TRUE`). C++17. macOS Universal Binary
(`CMAKE_OSX_ARCHITECTURES "arm64;x86_64"`), deployment target 10.13.
README.md's verified build uses the Ninja generator (`-G Ninja`); JUCE 7 does
not build against the macOS 15 SDK, hence the JUCE 8 pin.

## Architecture map

- `Source/PluginProcessor.{h,cpp}` — owns APVTS; `createLayout()` builds the
  100+ parameter tree.
- `Source/PluginEditor.{h,cpp}` — editor; `contentHeightFor(Disclosure)`
  drives DisclosureRow's variable-height layout (compact view resizes to
  `compact + gap + contentHeightFor(openDisclosure)`; only one disclosure open
  at a time).
- `Source/DSP/` — engine: `CircularBuffer`, `PositionEngine`,
  `PhaseOscillator`, `SampleReader`, `TunedDelay`, `EightPoleFilter`,
  `Cabinet`, `EchoFlanger`, `ReverbStage`, `ADBDSREnvelope`,
  `TransientDetector`, `GlobalLFO`, `PresetMorpher`, plus header-only
  `Smoothers.h`, `SVFFilter.h`, `SoftClipper.h`, `AMSection.h`.
- `Source/UI/` — 20 modules listed in CMake's `SK4N_UI_SOURCES`:
  `SK4nLookAndFeel`, `ValueFormatters`, `KnobControl`, `ToggleControl`,
  `ChoiceControl`, `SectionPanel`, `ModeSwitcher`, `BufferDisplay`,
  `ModMeter`, `EnvelopeMeter`, `FilterResponseDisplay`, `SnapshotRow`,
  `CircularMorpher`, `FireDot`, `HelpOverlay`, `Randomizer`, `DiceButton`,
  `PerformanceMacro`, `OutputMeter`, `DisclosureRow`.
- `Source/UI/SK4nLookAndFeel.h` — the palette lives in namespace
  `sk4n_ui::pal` (e.g. `pal::bgBase`, `pal::accentPrimary`, `pal::accentOscA`)
  and a `sk4n_ui::font` namespace for shared type styles.

## Meta parameters (binding)

Nine parameters rewrite other host-visible parameters from the processor side and are
therefore declared with `.withMeta (true)` in `createLayout()` (`fpMeta` / `ipMeta` helpers in
`Source/PluginProcessor.cpp`): the six performance macros (`perfMacroMovement`, `perfMacroPitch`,
`perfMacroColor`, `perfMacroDrive`, `perfMacroSpace`, `perfMacroTexture`, applied by
`PerformanceMacro`) and the morph controls `snapshotA`, `snapshotB`, `morphPosition` (applied by
`PresetMorpher`). Any new control that calls `setValueNotifyingHost` on a *different* parameter
must be meta too, or `auval -v aufx Sk4n ZQSF` fails with "Meta Param Flag is NOT set".
Re-run auval after touching the layout, the macros, or the morpher.

## UI (house look, adopted 2026-09-21)
SK4n uses the ZQ SFX house UI style (zqsfx_ui module), fetched by tag in CMakeLists.txt.
`SK4nLookAndFeel` is a
thin subclass of `zqsfx::ui::LookAndFeel`; `sk4n_ui::pal` stays the single colour source and its
values alias house tokens. Module colours are the colour-blind-safe channels: osc A sky, osc B
yellow, envelope purple, LFO green, feedback white, each also named by its section title. Orange is
"active" only; red is warn only. `KnobControl` / `ToggleControl` / `ChoiceControl` set accessible
title + description centrally. The header mark is the About button. UI gate:
`sk4n_ui_snapshot <out.png> [scale] [w h] [disclosure] [param=value ...]` renders the editor
headlessly; render before and after any UI change (`docs/ui_before.png` / `ui_after.png`).

**Every disclosure starts collapsed, so a bare render shows none of the section panels** — which is
how the Filter/FX overlapping-rows bug survived the entire house-UI migration. Pass a disclosure
(`oscillators|filter|fx|modulation|advanced`) to expand one, and `param=value` to render a
non-default state. A mode-switched panel needs **both** of its modes rendered to be considered
checked, e.g.:

```sh
sk4n_ui_snapshot out.png 2 "" "" filter                   # 8-Pole
sk4n_ui_snapshot out.png 2 "" "" filter filterMode=1      # Cabinet
sk4n_ui_snapshot out.png 2 "" "" fx echoFlangerMode=1     # Flanger
```

**Mode-switched panels (Filter, FX).** Each lays two control sets into one rectangle, so exactly
one set must be visible or their labels and readouts overlap into unreadable text.
`applyFilterModeVisibility()` / `applyEchoModeVisibility()` do that, driven by
`ModeSwitcher::onModeChanged` (fired from its timer, so it is message-thread safe even though
`parameterChanged` can arrive on the audio thread). Two rules if you touch either panel:

- **Apply visibility before laying the row out**, not after. `layoutRowCentered` skips controls
  that are not visible, so it must see the final state.
- **A control shared by both modes (Filter's `Mix`) must appear in exactly one layout call.** The
  Filter row therefore picks its item list by mode instead of laying out both rows. Laying out
  both lets the second call re-centre the shared knob on its own, since every other item in that
  list is hidden, dropping it into the middle of the row on top of its neighbours.

(Both were empty stubs until 2026-09-22, and the shared-`Mix` misplacement was underneath them.)

## Build & test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Plugin artefacts: `build/SK4n_artefacts/Release/{VST3,AU,Standalone}/`.
Tests: `SK4nTests` is a `juce_add_console_app` running `juce::UnitTest` cases
over the DSP sources; run `./build/SK4nTests_artefacts/Release/SK4nTests`
after building.

`COPY_PLUGIN_AFTER_BUILD` is `FALSE`; install manually:

```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
cp -R build/SK4n_artefacts/Release/VST3/SK4n.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R build/SK4n_artefacts/Release/AU/SK4n.component ~/Library/Audio/Plug-Ins/Components/
```

Soundminer requires the system path: `sudo cp -R build/SK4n_artefacts/Release/VST3/SK4n.vst3 /Library/Audio/Plug-Ins/VST3/`.

## Version control

Diversion is the working VCS; this git repository is the curated public mirror
(github.com/themightyzq/sk4n), updated with finished work only.

## Project-specific rules

- Colour tokens come only from `sk4n_ui::pal`; no raw `juce::Colours::` in
  component/painting code (the only literal-colour uses in the tree are
  `juce::Colours::transparentBlack` inside `SK4nLookAndFeel`'s constructor,
  which is not a themed colour).
- Paint a visible focus outline on every focusable control when
  `hasKeyboardFocus(true)` is true (see `DisclosureRow::paint`,
  `KnobControl`, `ToggleControl`, `ChoiceControl`).
- Respect the house 22 px minimum hit target (see ../CLAUDE.md §6); SK4n's
  section dice (`kDiceSize`) are already sized to it — don't shrink below it.
- `DisclosureRow` reserves 24 px on the right for the chevron; never let a
  title run under it.

## Open items (see SESSION_HANDOFF.md)

- Pass 10 visual verification not yet done: walk every disclosure and the
  compact view in a DAW against the Pass 10 layout expectations.
- Priority-2 ear tuning never executed: the 6 performance-macro destination
  ranges and the 8 morpher snapshots are educated guesses, not ear-tuned.
- Disclosure open/closed state does not survive a plugin reload.
- Notarization / Developer ID signing for distribution is not done (ad-hoc
  signed only).
- Long-tail deferred UI items: A/B compare toggle, reduce-motion preference,
  UI scale preference, user-remappable macro destinations.
