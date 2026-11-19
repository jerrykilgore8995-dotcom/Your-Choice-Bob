# Bob Audio Console Suite
Four distinct Windows VST3 targets, shared infrastructure in ConsoleSuite.cpp; earlier plugin sources unchanged. Each has four built-in parameter snapshots, independent plugin ID, custom rack editor, mono/stereo linked processing, persistent parameters, 4x FIR oversampled nonlinear color and latency-aligned dry/wet mix.

## WARCRY
Vocal highpass, body/presence/air EQ, slow leveling and fast peak catch, frequency-selective de-essing, asymmetric valve-inspired saturator. Presets: Rock lead, Metal upfront, Ballad warmth, Parallel vocal. Start Level 45%, Catch 20%, Valve 20%; tune input to get useful reduction. De-ess can be set to zero.
## THUNDERHEAD
Drum highpass, independent bipolar transient attack/sustain controls based on fast/slow envelopes, punch compressor, Thump/Box/Crack EQ, asymmetric and symmetric Smash waveshaping. Presets: Kit punch, Snare impact, Room destruction, Vintage kit.
## BLACKIRON
Variable low shelf, parametric mid, high shelf, Color and signed Bias adjust nonlinear transfer symmetry. Presets: Console polish, Warm and thick, Guitar articulation, Parallel color. Not a measured hardware EQ. GR meter remains at zero because there is no compressor.
## CROWN
Stereo-linked glue with detector highpass, variable attack/release, broad Weight/Tone/Air EQ, restrained Iron waveshaping, M/S width. Presets: Mix glue, Rock master bus, Vintage weight, Parallel aggression. No brickwall limiter or true-peak guarantee. Width also affects parallel dry component.

## Validation
Original analog-inspired algorithms, not measured circuit replicas. Fixed detector thresholds require input adjustment. FIR latency compensation aligns bulk delay, but minimum-phase EQ and DC filtering may cause wet/dry phase interactions. No listening, host, compilation, CPU, harmonic or aliasing validation yet. Workflow verifies binary presence only. Block-rate smoothing/coefficient updates and oversized host blocks require testing. Oversized blocks currently clear output rather than reallocating.
Builds: https://github.com/jerrykilgore8995-dotcom/Your-Choice-Bob/actions/workflows/console-suite.yml
