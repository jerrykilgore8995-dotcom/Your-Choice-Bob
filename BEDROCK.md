# BEDROCK 0.1
Split bass console for rock and metal. Mono/stereo VST3 with linked detectors. 24 dB/oct Linkwitz-Riley crossover, independently leveled low and high paths, 4x FIR oversampled upper-path nonlinear color, latency-compensated low path, balance and final glue. Color is original bounded DSP, not a calibrated circuit or bass amp model.

## Quick start
Select one of five embedded editor presets: Rock foundation, Metal demolition, Vintage fat, Pick attack, Clean DI. Set input and output to suit source level. Raise Anchor until low notes remain consistent (meter is commanded reduction). Raise Grind for audible grit; use Balance toward the low path if thin, toward the high path if too dark. Use Growl and Clarity for audibility, Weight for foundation. Match bypass loudness with Output. Presets are parameter snapshots applied by the editor, not host program slots.

## Validation
Windows workflow checks bundle presence. Compilation, host processing, crossover phase/summing, saturation harmonics and aliasing, automation artifacts and subjective listening are not yet verified. Input-dependent gain laws and fixed thresholds mean different recordings need input adjustment. Saturation is normalized to small-signal gain and can attenuate peaks; use balance and output to level match. Rumble is a fourth-order highpass. Upper-path distortion can produce new low-frequency components; clean low path does not imply the summed signal is distortion-free below crossover.
Builds: https://github.com/jerrykilgore8995-dotcom/Your-Choice-Bob/actions/workflows/bedrock.yml
