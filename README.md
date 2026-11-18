# IRONFIST
Original parallel analog-inspired dynamics processor for rock and metal. Windows x64 VST3, JUCE 8. Build artifacts: https://github.com/jerrykilgore8995-dotcom/Your-Choice-Bob/actions

The RMS and peak gain laws run simultaneously, with Density blending their linear gains. A 4x oversampled asymmetric tanh stage and symmetric atan stage supply color. These are original bounded DSP approximations, not calibrated circuit copies. Inspiration: 160 RMS punch, 609 bridge density, 525 discrete color. The 525 repository reports local slopes from 2.04:1 to 11.73:1 and level-dependent gain; no measurement matching is claimed here. The detector HPF affects only detection. Dry latency is compensated by integer delay, but IIR oversampling has frequency-dependent phase, so parallel crossover cancellation should be measured. No hardware matching or listening certification has been performed.

## Starting points
Adjust threshold for the desired GR and output for equal loudness. Values are Input, Threshold, Attack, Release, Ratio, Density, Drive, Edge, HPF, Blend, Output.
- Drum demolition: +6, -24, 15, 90, 8, 65, 55, 45, 80, 30, 0.
- Kit punch: +3, -22, 25, 100, 4, 15, 25, 10, 80, 40, 0.
- Bass weight: +3, -24, 8, 140, 4, 35, 40, 10, 35, 55, 0.
- Vocal parallel: +6, -26, 3, 120, 8, 60, 30, 10, 120, 20, 0.
- Rhythm guitar density: +3, -22, 20, 100, 3, 30, 25, 15, 100, 25, 0.

These are manual starting settings, not embedded presets. Build status and host compatibility must be checked before use. The workflow verifies bundle existence, not audio behavior.
