# AFTERGLOW 0.1
Stereo vintage studio FX rack, mono/mono, mono/stereo and stereo/stereo. Independent VST3 target; previous processors unchanged.

## Routing
Modulation (Off, Analog chorus, Symphonic, Phaser, Flanger, Rotary, Tremolo, Dimension) feeds dual L/R echo with cross-feedback and analog-inspired damping/flutter/saturation. Reverb is an original eight-line feedback delay network with Plate, Hall, Spring-like, Room and Gated variants. Wet return has HP/LP filters, color and M/S width. Insert mode retains a direct path; Master Mix scales contributions. AUX / 100% WET removes the separate dry path and overrides Master Mix. Phaser/flange/tremolo/rotary inherently may include instantaneous signal; use as inserts for conventional results. Disable modulation for a conventional echo/reverb-only return. Set output to avoid clipping when summing effects. Modulation and reverb type switches can change tails abruptly. No amp/cab simulation.

Tempo Sync scales the displayed delay values (reference 120 BPM) by 120 / host BPM, with 120 fallback. 500 ms = quarter, 250 = eighth, 375 = dotted eighth at 120 BPM. The values are continuously variable, not note-menu choices. Color applies bounded nonlinear shaping and darker, slightly modulated delay feedback, not a calibrated hardware circuit or oversampled nonlinear model.

## Presets
52 preset records across Clean guitar (12), Lead guitar (15), Modulation (9), Delay and verb (4), Vocals (12). Artist/song labels identify subjective starting points, not verified recording signal chains or endorsements. The preset loader changes the FX settings without changing Aux mode or output trim. For vocal plate/slap/echo presets, enable Aux on a send and balance using the DAW return. For modulation-only presets use insert mode. For a huge guitar solo try Dual quarter and eighth, turn on tempo sync, keep the return filtered around 200 Hz to 6.5 kHz.

## Validation
Preset table count and parameter bounds checked. Build workflow verifies presence of the VST3 bundle and binary. Actual compilation, host compatibility, measured reverb decay, modulation character, CPU, alias rejection, mono compatibility, listening and preset authenticity remain unverified. Algorithms are original stylized effects rather than replicas of vintage units. No pitch shifter, cabinet IR, reverse reverb or true mechanical spring simulation in this version.

Builds: https://github.com/jerrykilgore8995-dotcom/Your-Choice-Bob/actions/workflows/afterglow.yml
