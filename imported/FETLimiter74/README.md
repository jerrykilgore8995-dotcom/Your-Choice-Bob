# FET LIMITER '74 — JUCE VST3 / AU Plugin

A full circuit-level emulation of a classic British feed-forward FET
limiter, built from a vintage service-manual schematic.

Designed for **Xcode 12.4 / macOS Catalina 10.15.7** with **Projucer**.

## Signal chain modelled

```
IN  ─► Input transformer (20 Hz / 25 kHz, soft core sat)
     ─► Discrete Class-A pre (2SC732 / 2SA562 style, 2nd+3rd harmonic bias)
     ─► VR1 Input level (47 kΩ log pot)
     ─► JFET VCA (BF244B as VCR, GR-dependent harmonic distortion,
                  VR4 distortion-comp bleed)
     ─► Discrete Class-A output stage
     ─► Output transformer (25 Hz / 22 kHz, soft core sat)
     ─► +/-24 V PSU ripple injection (switchable, vintage mode)
     ─► 0 dBm or -30 dBm output tap
OUT ◄─

SIDECHAIN (feed-forward):
     Post-pre signal
     ─► full-wave rectify -> log domain
     ─► threshold compare vs VR2
     ─► uA741-style bandwidth / noise
     ─► switchable R/C:
          Attack  = FAST (~150 us) / SLOW (~2 ms)
          Release = FAST (50 ms) / AUTO (dual TC) / MED (200 ms) / SLOW (800 ms)
     ─► Drives JFET gate
     ─► LIMIT > 3 dB lamp
```

## Front-panel controls

| Control            | Type               | Schematic ref |
| ------------------ | ------------------ | ------------- |
| INPUT              | 47k log vertical fader | VR1       |
| SOURCE             | MIC / LINE pushbuttons | mic/line sw |
| LIMITER            | IN latching pushbutton | IN/OUT sw   |
| ATTACK             | FAST / SLOW pushbuttons | ATTACK sw  |
| RELEASE            | FAST / AUTO / MED / SLOW pushbuttons | RELEASE sw |
| THRESHOLD          | Chicken-head trim pot | VR2        |
| LED CAL            | Chicken-head trim pot | VR3        |
| DIST COMP          | Chicken-head trim pot | VR4        |
| LIMIT > 3 dB       | Round red lamp     | sidechain LED |
| STEREO LINK        | Latching pushbutton | stereo link jack |
| OUTPUT             | Trim knob          | output trim  |
| OUTPUT LEVEL       | 0 dBm / -30 dBm pushbuttons | xfmr taps |
| PSU RIPPLE         | Latching pushbutton (vintage mode on/off) | PSU hum |

## Build

Place a JUCE clone next to the project folder:

```
parent/
├── JUCE/              <-- git checkout 7.0.5 (or 6.1.6 for Xcode 12.4)
└── FETLimiter74/
    ├── FETLimiter74.jucer
    └── Source/
```

Open `.jucer` in Projucer → Save Project and Open in IDE → Xcode 12.4 →
build Release AU + VST3.

Plugin lands in:
- AU : `~/Library/Audio/Plug-Ins/Components/FETLimiter74.component`
- VST3: `~/Library/Audio/Plug-Ins/VST3/FETLimiter74.vst3`

## Files

```
FETLimiter74/
├── FETLimiter74.jucer         Projucer project
├── README.md
└── Source/
    ├── PluginProcessor.h/.cpp Main AudioProcessor + full chain assembly
    ├── PluginEditor.h/.cpp    Vintage-rack GUI, LIMIT lamp, choice-button groups
    └── DSP/
        └── FETLimiterDSP.h    All circuit emulation (transformers, discrete
                               stages, JFET VCA, 741 sidechain, PSU ripple)
```

## Re-skinning for personal use

If you want the panel to look like a specific vintage unit you own, swap
these on your own copy:

1. In `PluginEditor.cpp`, change `brandLbl` / `modelLbl` / `originLbl` text.
2. In `VintageLNF::VintageLNF()`, change the background and knob colours.
3. In `buildFaceplate()`, replace the painted gradient with a
   `juce::Image::loadFrom (BinaryData::yourFaceplate_png,
   BinaryData::yourFaceplate_pngSize)` draw.
