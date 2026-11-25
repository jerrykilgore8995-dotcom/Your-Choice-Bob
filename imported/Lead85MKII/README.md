# LEAD-85 — High-Gain Tube Lead Preamplifier (JUCE VST3 / AU)

Full-circuit DSP emulation of a classic '80s American boutique
cascading-gain tube lead preamp with simul-class power section and
graphic EQ. Built for **Xcode 12.4 / macOS Catalina 10.15.7** with
Projucer.

## Signal path (as modelled, matching the schematic stage-for-stage)

```
IN ─► V1A ─► FMV tone stack (Treble / Middle / Bass + Pull switches)
      │                                       │
      │   (Pull Bright on V1 Volume - bright cap)│
      │   (Pull Treble Shift)                  │
      │   (Pull Bass Shift)                    │
      │                                       │
      ▼                                       ▼
   V1B ─► Volume 1 ─► [Pull Gain: V2A ─► V2B cascade]
                                              │
                                              ├──► V3A reverb drive
                                              │        │
                                              │     spring tank
                                              │        │
                                              │    V4 recovery
                                              │        │
                                              ▼        │
   Master 1 ─► [LEAD mode: V4A ─► V3B ─► Lead Master] ◄┘
                                              │
                                              ▼
                           [GEQ ON]  5-band Graphic EQ
                                80 / 240 / 750 / 2200 / 6600 Hz
                                ±12 dB per band
                                              │
                                              ▼
                            V5A/V5B long-tail pair phase inverter
                                              │
                   ┌──────────────────────────┼──────────────────────────┐
                   ▼                                                     ▼
       Inner pair V6/V9  (Class-A triode-wired)      Outer pair V7/V8  (Class-AB pentode)
                   │                                                     │
                   └─────────────── Output transformer ───────────────────┘
                                      (summing + LF/HF rolloff + core sat)
                                              │
                                              ▼
                                 Presence (post-NFB high-shelf)
                                              │
                                              ▼
                                       Output trim ─► OUT

   Class-A switch defeats the outer pentode pair entirely (only V6/V9 run).
   Presence pot also sets the amount of negative feedback around V5-V9.
```

## Every tube modelled

| ID       | Role                                   | Mode                         |
| -------- | -------------------------------------- | ---------------------------- |
| V1A      | Preamp stage 1 (high-gain input)       | 12AX7-style triode           |
| V1B      | Preamp stage 2 (post tone-stack)       | 12AX7-style triode           |
| V2A, V2B | Extra gain cascade (Pull Gain only)    | 12AX7-style triode           |
| V3A      | Reverb driver                          | 12AX7-style triode           |
| V4       | Reverb recovery                        | 12AX7-style triode           |
| V4A      | Lead-channel gain                      | 12AX7-style triode           |
| V3B      | Lead-channel master driver             | 12AX7-style triode           |
| V5A/V5B  | Long-tail-pair phase inverter          | Differential triode pair     |
| V6, V9   | Inner output pair                      | 6L6 class-A triode-wired     |
| V7, V8   | Outer output pair                      | 6L6 class-AB pentode         |

Each stage has its own grid-coupling HPF, Miller-cap LPF, bias point and
asymmetric soft-clip triode curve. The two power-amp pairs have different
clipping behaviour (triode = smooth, mostly 2nd-harmonic; pentode =
sharper upper knee with crossover notch).

## Front-panel controls

**Top row (chicken-head knobs, chrome bezel, white tip stripe)**

| Knob         | Pull switch   | Function                                    |
| ------------ | ------------- | ------------------------------------------- |
| INPUT        | —             | Pre-V1A input trim                          |
| VOLUME 1     | PULL BRIGHT   | V1 bright cap engages                       |
| TREBLE       | PULL SHIFT    | Treble corner shifts up                     |
| MIDDLE       | —             | Mid peak                                    |
| BASS         | PULL SHIFT    | Low-shelf extension                         |
| MASTER 1     | PULL GAIN     | Engages the extra V2A/V2B gain cascade      |
| LEAD DRIVE   | PULL BRIGHT   | V4A input + optional bright cap             |
| LEAD MASTER  | —             | Post-V3B gain                               |
| PRESENCE     | —             | NFB high-shelf + feedback amount            |
| REVERB       | —             | Spring send                                 |
| OUTPUT       | —             | Final trim                                  |

**Mode switches (chrome latching, amber lamp)**

| Switch       | Function                                           |
| ------------ | -------------------------------------------------- |
| CHANNEL      | Rhythm / Lead (routes through V4A -> V3B cascade)  |
| POWER MODE   | Simul-Class / Class-A (defeats outer pentode pair) |
| EQ IN        | Graphic EQ on/off (sits between preamp & power amp)|
| MAIN POWER   | Entire chain bypass                                |

**Graphic EQ** — 5 vertical sliders at the schematic's tank frequencies:
80 Hz / 240 Hz / 750 Hz / 2.2 kHz / 6.6 kHz, range exactly **±12 dB** per
band (matches the hardware). Centre detent at 0 dB.

## Build on your Mac

```
parent/
├── JUCE/               # git clone https://github.com/juce-framework/JUCE.git
│                       # (git checkout 7.0.5  or 6.1.6 fallback for Xcode 12.4)
└── Lead85/
    ├── Lead85.jucer
    └── Source/
```

Open `Lead85.jucer` → **Save Project and Open in IDE** → Xcode 12.4 →
build **AU** and **VST3** in Release.

Plugins land in:
- AU:  `~/Library/Audio/Plug-Ins/Components/Lead85.component`
- VST3:`~/Library/Audio/Plug-Ins/VST3/Lead85.vst3`

## Files

```
Lead85/
├── Lead85.jucer
├── README.md
└── Source/
    ├── PluginProcessor.h/.cpp    Chain assembly + 23 APVTS params
    ├── PluginEditor.h/.cpp       Black-tolex panel, chrome, chicken-heads,
    │                             amber pull lamps, 5-band ±12 dB GEQ
    └── DSP/
        └── Lead85DSP.h           All circuit models in one header
                                  (TubeTriode, ToneStack, GraphicEQ5,
                                   TubePowerPair, SimulClassPowerSection,
                                   OutputTransformer, Presence, SpringReverb)
```
