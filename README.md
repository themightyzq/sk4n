# SK4n

SK4n is a wave-scanning effect. It continuously records incoming audio
into a buffer, then scans back through that buffer with a pair of
phase-modulated oscillators, producing anything from rhythmic scratching
to synth-like tones depending on how fast you scan. VST3, AU, and
Standalone.

## Install

There are no packaged releases yet; build from source (below), then copy
the bundles into place. They are ad-hoc signed, which most hosts accept on
your own machine.

User level (most hosts):

```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
cp -R build/SK4n_artefacts/Release/VST3/SK4n.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R build/SK4n_artefacts/Release/AU/SK4n.component ~/Library/Audio/Plug-Ins/Components/
```

System level (Soundminer requires this path):

```sh
sudo cp -R build/SK4n_artefacts/Release/VST3/SK4n.vst3 /Library/Audio/Plug-Ins/VST3/
```

## Use

Feed SK4n audio and it starts filling its buffer immediately; Freeze holds
the buffer so you can scan a fixed snippet. Two oscillators, Osc A and Osc
B, drive the read position through the buffer: at sub-audio rates you get
rhythmic scratching, at audio rates you get synthesis-style tones. From
there the signal passes through AM, a tuned delay, and a filter/cabinet
stage, gets mixed together, then goes through echo or flanger and a
reverb before output.

### Parameters

100+ parameters cover the oscillators, position engine, filter, cabinet,
echo/flanger, reverb, envelopes, macros, and preset morpher. Continuous
parameters that touch the audio are smoothed with a 10 ms ramp.

### Presets

Snapshot 0 is "init", the state at instantiation. Snapshots 1 to 7 are
pre-populated:

1. Ambient resonator: small position, mid feedback, reverb up
2. Aggressive scratch: sub-audio oscs, large position swings, envelope to
   position
3. Deep resonator: pitched comb at low Hz, high delay feedback
4. Cabinet drive: Cabinet mode active, strong drive and fold
5. AM tones: high-rate oscs, squared blend, AM channel dominant
6. Wide flanger: Flanger mode, deep modulation
7. Granular shimmer: long buffer, scattering LFO into position, big
   reverb

Use the snapshot buttons (S1-S8) to set Snapshot A, then combine with B
and Morph Position to morph between them.

## Build from source

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Toolchain: CMake 3.22+, Ninja, a C++ compiler, JUCE 8.0.7 (fetched
automatically). macOS builds are Universal Binary (arm64 + x86_64); JUCE 8
is required since JUCE 7 does not build against the macOS 15 SDK.

Artefacts land in `build/SK4n_artefacts/Release/{VST3,AU,Standalone}/`.
Run the unit tests with:

```sh
build/SK4nTests_artefacts/Release/SK4nTests
```

## Licence

GPL-3.0-or-later. See LICENSE. Built with JUCE.

ZQ SFX, https://www.zq-sfx.com, connect@zq-sfx.com.
