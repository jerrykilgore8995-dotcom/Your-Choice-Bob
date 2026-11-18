# WARWALL 0.1
Original guitar bus console for recorded amplified rock/metal guitars. Not an amp simulator or circuit replica. Independent VST3 target; IRONFIST source unchanged.

Signal order: input, 12 dB/oct high-pass, 230 Hz body bell, 420 Hz mud bell, 1.65 kHz bite bell, linked dynamic band subtraction around 145 Hz and 4.2 kHz, gentle broadband glue, 4x FIR oversampled parallel asymmetric tanh/atan color, low-pass, DC blocker, M/S width, output. Color Mix affects only nonlinear stages. Tight and Fizz are dynamic band controls, not conventional static EQ. Iron is a bounded saturation approximation, not a measured transformer model. Meter values indicate commanded band attenuation, not measured broadband GR. Stereo width above 100% can reduce mono compatibility.

Manual starting settings (low cut Hz, tight %, body dB, mud dB, bite dB, fizz %, high cut Hz, drive %, iron %, glue %, width %, color mix %):
- Thrash wall: 85,65,1,-2,1.5,45,10500,20,30,15,100,70
- Modern metal: 95,80,0,-3,2,55,11000,15,20,15,100,65
- Classic hard rock: 70,25,2,-1,1,15,14000,35,45,30,100,80
- Lead sustain: 80,20,1,-1,2,35,12000,30,35,50,100,75
- Quad track cleanup: 85,55,0,-2.5,0.5,40,10500,15,20,10,100,60

Use input and output trims to level match. Dynamics have fixed internal thresholds so input level changes action. Starting points are not embedded presets. No measured hardware matching, subjective listening, plugin host validation, CPU benchmark or harmonic/alias testing has been performed. Block-scale filter coefficient updates should be checked for automation artifacts. Windows workflow validates bundle presence only.
Builds: https://github.com/jerrykilgore8995-dotcom/Your-Choice-Bob/actions/workflows/warwall.yml
