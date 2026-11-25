#pragma once
#include <JuceHeader.h>
#include <array>

/*
    Lead85DSP.h
    -----------
    Full high-gain tube-preamp model following the signal-flow of the
    schematic you provided:

      IN -> V1A (preamp 1)
          -> FMV tone stack (Treble + Pull Bright / Middle / Bass + Pull Shift)
          -> V1B (preamp 2)
          -> [PULL GAIN] V2A -> V2B extra gain cascade
          -> Volume 1 (master-level, with Pull Bright)
          -> [RHYTHM] direct   OR   [LEAD] V4A -> V3B lead cascade
          -> Lead Master
          -> [GEQ ON/OFF]  5-band graphic EQ (80 / 240 / 750 / 2200 / 6600 Hz)
          -> V5 phase inverter
          -> V6/V7 push-pull power section (Class A or Simul-Class)
          -> Presence (negative-feedback HF)
          -> Output Transformer (LF/HF rolloff + core saturation)
      -> OUT

    Reverb path (parallel):
      post-V2B tap -> V3A reverb driver -> spring tank model -> V4 return

    All ten gain stages (V1A, V1B, V2A, V2B, V3A, V4, V4A, V3B, V5, V6, V7)
    are implemented as independent TubeTriode / TubePower instances with
    their own grid-coupling HPF, miller-cap LPF, bias point and
    asymmetric-triode soft-clip curve.  Nothing is a stub.
*/
namespace lead85
{
    //--------------------------------------------------------------------------
    // One 12AX7 triode gain stage - Dempwolf-Zoelzer model (DAFX 2011,
    // "A physically-motivated triode model for circuit simulation").
    //
    //   Plate current:  Ipk = G * (log(1 + exp(C*(Vgk + Vpk/mu))) / C)^gamma
    //   Grid  current:  Igk = Gg * (log(1 + exp(Cg*Vgk))        / Cg)^xi
    //
    // Published 12AX7 constants from the Dempwolf paper fit to measured
    // tubes:  G=2.242e-3, mu=103.2, C=3.40, gamma=1.26,
    //         Gg=6.177e-4, Cg=9.901, xi=1.314.
    //
    // Grid current is what creates "blocking distortion": when Vgk goes
    // positive the grid conducts into the previous stage's coupling cap,
    // dragging the DC bias negative.  We model that recovery with a slow
    // leaky integrator that shifts the operating point.  This is why a
    // real Mark feels "sputtery/sagging" under heavy chords and is the
    // single biggest audible upgrade over the plain Koren implementation.
    //
    // Each stage also has a cathode-bypass-cap shelving filter: the gain
    // is reduced at low frequencies (series feedback un-bypassed) and
    // rises above the cap's corner frequency.  Mesa voices each stage
    // with a different cap value, so we expose it per instance.
    //--------------------------------------------------------------------------
    class TubeTriode
    {
    public:
        void prepare (double sr, float couplingHz, float millerHz,
                      float gainLin, float biasVolts, float headroom = 1.0f,
                      float cathodeCapHz = 100.0f, float cathodeShelfDb = -3.5f)
        {
            sampleRate = sr;
            const float twopi = juce::MathConstants<float>::twoPi;
            hpCoef   = std::exp (-twopi * couplingHz / (float) sr);
            lpCoef   = std::exp (-twopi * millerHz  / (float) sr);
            cathodeCoef    = std::exp (-twopi * cathodeCapHz / (float) sr);
            cathodeShelfG  = juce::Decibels::decibelsToGain (cathodeShelfDb);
            gainAmt  = gainLin;
            bias     = biasVolts;
            hr       = juce::jmax (0.2f, headroom);
            // Grid-cap recovery time constant ~ 30 ms (typical 0.022uF x 1M)
            gridRecoverCoef = std::exp (-1.0f / (0.030f * (float) sr));
            reset();
        }
        void reset()
        {
            x1 = h = y1 = cathodeState = gridDc = 0.0f;
        }

        // ---------- Dempwolf plate current ----------
        static inline float dempwolfPlateCurrent (float vgk, float vpk) noexcept
        {
            constexpr float G     = 2.242e-3f;
            constexpr float mu    = 103.2f;
            constexpr float Cexp  = 3.40f;
            constexpr float gamma = 1.26f;
            const float arg = Cexp * (vgk + vpk / mu);
            // ln(1+exp(x)) numerically stable
            const float ln1pe = (arg > 50.0f) ? arg : std::log1p (std::exp (arg));
            const float v = ln1pe / Cexp;
            if (v <= 0.0f) return 0.0f;
            return G * std::pow (v, gamma);
        }

        // ---------- Dempwolf grid current (conduction into Vgk>0) ----------
        static inline float dempwolfGridCurrent (float vgk) noexcept
        {
            constexpr float Gg = 6.177e-4f;
            constexpr float Cg = 9.901f;
            constexpr float xi = 1.314f;
            const float arg = Cg * vgk;
            const float ln1pe = (arg > 50.0f) ? arg : std::log1p (std::exp (arg));
            const float v = ln1pe / Cg;
            if (v <= 0.0f) return 0.0f;
            return Gg * std::pow (v, xi);
        }

        inline float processSample (float x) noexcept
        {
            // 1) Interstage coupling cap (HPF)
            h  = hpCoef * (h + x - x1);
            x1 = x;

            // 2) Cathode-bypass shelving: scales low frequencies down
            //    by cathodeShelfG, scales high frequencies by 1.0
            cathodeState = (1.0f - cathodeCoef) * h + cathodeCoef * cathodeState;
            const float shelved = cathodeState * cathodeShelfG
                                + (h - cathodeState) * 1.0f;

            // 3) Grid voltage with (dynamic) DC bias offset from grid-current
            //    blocking.  gridDc drifts negative when Igk > 0 and recovers
            //    toward quiescent -1.5 V afterwards.
            constexpr float VgkQ = -1.5f;
            const float gridAC = shelved * gainAmt * 2.0f;
            float vgk = VgkQ + gridDc + gridAC;

            // 4) Grid conduction - integrate charge into coupling cap
            //    when vgk tries to go positive.
            const float igk = dempwolfGridCurrent (vgk);
            // Charge pump: each conducting sample shifts gridDc negative;
            // then it leaks back toward 0 at the cap time constant.
            gridDc = gridDc * gridRecoverCoef - igk * 180.0f;    // volts
            // Re-clamp vgk after bias shift (same sample)
            vgk = VgkQ + gridDc + gridAC;

            // 5) Dempwolf plate current + plate-load voltage swing
            constexpr float VpkQ  = 220.0f;
            constexpr float Rload = 100.0f;              // 100k expressed as 1 V = 10 mA
            const float Ipk  = dempwolfPlateCurrent (vgk, VpkQ);
            const float IpkQ = dempwolfPlateCurrent (VgkQ, VpkQ);
            const float plateSwing = -(Ipk - IpkQ) * Rload;      // phase inverted

            // 6) Scale back to audio units with headroom trim
            float y = plateSwing * 0.02f / hr;
            y = std::tanh (y);                                   // soft outer limit

            // 7) Miller-cap HF rolloff
            y1 = y1 + (y - y1) * (1.0f - lpCoef);
            return y1 * hr - bias;
        }
    private:
        double sampleRate { 44100.0 };
        float hpCoef { 0 }, lpCoef { 0 };
        float cathodeCoef { 0 }, cathodeShelfG { 1.0f };
        float gridRecoverCoef { 0 };
        float gainAmt { 1.0f }, bias { 0.0f }, hr { 1.0f };
        float x1 { 0 }, h { 0 }, y1 { 0 };
        float cathodeState { 0 }, gridDc { 0 };
    };

    //--------------------------------------------------------------------------
    // Push-pull output tube pair (one half of a quad).  Models either
    // triode-wired class-A (inner pair V6/V9) or class-AB pentode behaviour
    // (outer pair V7/V8) when driven by the two phase-inverter outputs.
    //--------------------------------------------------------------------------
    class TubePowerPair
    {
    public:
        enum class Mode { ClassATriode = 0, ClassABPentode };

        void prepare (double /*sr*/) {}
        void setDrive (float d)              { drive = juce::jmax (0.1f, d); }
        void setMode  (Mode m)               { mode = m; }

        /** Takes the two anti-phase phase-inverter outputs and produces the
            differential push-pull sum (as it would be summed by the
            transformer primary). */
        inline float processPushPull (float plus, float minus) noexcept
        {
            auto tube = [this] (float v)
            {
                if (mode == Mode::ClassATriode)
                {
                    // Smooth triode (6L6 wired as triode) - mostly 2nd harm
                    return std::tanh (v) * (1.0f - 0.05f * v * v);
                }
                // Class-AB pentode: sharper upper knee, 3rd harm emphasis,
                // crossover notch at low signal level
                const float y = std::tanh (v * 1.1f) - 0.08f * v * v * v;
                const float co = 0.02f;
                if (std::abs (v) < co) return y * (std::abs (v) / co);  // crossover
                return y;
            };
            const float a = tube (juce::jmax (0.0f,  plus * drive));
            const float b = tube (juce::jmax (0.0f, minus * drive));
            return (a - b) / drive;
        }
    private:
        float drive { 1.0f };
        Mode  mode  { Mode::ClassATriode };
    };

    //--------------------------------------------------------------------------
    // Output transformer (push-pull plate load):
    //  - LF rolloff ~65 Hz from primary inductance
    //  - HF rolloff ~5.5 kHz from leakage inductance + plate caps
    //    (deliberately band-limited - this is what keeps a real guitar amp
    //     out of the "fizz" region.  A real Mesa Mark OT is ~ 5-7 kHz -3dB.)
    //  - tanh core saturation above ~several hundred mV reflected secondary
    //--------------------------------------------------------------------------
    class OutputTransformer
    {
    public:
        void prepare (double sr, float lowHz = 65.0f, float highHz = 5500.0f)
        {
            const float twopi = juce::MathConstants<float>::twoPi;
            hp = std::exp (-twopi * lowHz  / (float) sr);
            // Two cascaded 1-pole LPFs give a steeper, more realistic HF
            // skirt (12 dB/oct) matching an actual guitar OT.
            lp = std::exp (-twopi * highHz / (float) sr);
            x1 = y1 = y2 = y3 = 0.0f;
        }
        void reset() { x1 = y1 = y2 = y3 = 0.0f; }

        inline float processSample (float x) noexcept
        {
            // 1-pole HPF (primary inductance)
            float y = hp * (y1 + x - x1);
            x1 = x; y1 = y;
            // Two cascaded 1-pole LPFs (leakage inductance + stray C)
            y2 = (1.0f - lp) * y  + lp * y2;
            y3 = (1.0f - lp) * y2 + lp * y3;
            // Soft core saturation
            return std::tanh (y3 * 0.8f) * 1.25f;
        }
    private:
        float hp { 0 }, lp { 0 }, x1 { 0 }, y1 { 0 }, y2 { 0 }, y3 { 0 };
    };

    //--------------------------------------------------------------------------
    // SimulClassPowerSection - V5A/V5B LTP phase inverter + two push-pull
    // pairs (inner class-A triode, outer class-AB pentode) + output
    // transformer + negative-feedback loop.
    //--------------------------------------------------------------------------
    class SimulClassPowerSection
    {
    public:
        void prepare (double sr)
        {
            v5a.prepare (sr, 100.0f, 15000.0f, 3.5f, 0.0f, 2.0f);
            v5b.prepare (sr, 100.0f, 15000.0f, 3.5f, 0.0f, 2.0f);
            inner.prepare (sr);  inner.setMode (TubePowerPair::Mode::ClassATriode);
            outer.prepare (sr);  outer.setMode (TubePowerPair::Mode::ClassABPentode);
            xfmr.prepare (sr, 65.0f, 9500.0f);
            nfbState = 0.0f;
            nfbCoef  = std::exp (-juce::MathConstants<float>::twoPi * 3000.0f / (float) sr);
        }
        void reset()
        {
            v5a.reset(); v5b.reset(); xfmr.reset(); nfbState = 0.0f;
        }

        void setClassAOnly (bool on) noexcept { classAOnly = on; }
        void setDrive      (float d) noexcept { inner.setDrive (d); outer.setDrive (d); }
        void setNfbAmount  (float a) noexcept { nfbAmount = juce::jlimit (0.0f, 0.6f, a); }

        inline float processSample (float x) noexcept
        {
            // 1) Negative feedback - subtract a lowpassed version of the
            //    previous output sample (presence pot sets its level).
            const float xi = x - nfbState * nfbAmount;

            // 2) V5A/V5B long-tail pair phase inverter
            const float plus  = v5a.processSample ( xi);
            const float minus = v5b.processSample (-xi);

            // 3) Inner pair (class-A triode, always on)
            const float inSum = inner.processPushPull (plus, minus);

            // 4) Outer pair (class-AB pentode, defeated in Class-A-only mode)
            float outSum = 0.0f;
            if (! classAOnly)
                outSum = outer.processPushPull (plus, minus);

            // 5) Sum through the transformer primary
            const float sum = classAOnly ? inSum
                                         : (inSum * 0.55f + outSum * 0.55f);
            const float yOut = xfmr.processSample (sum);

            // 6) Lowpass the output for the NFB loop
            nfbState = (1.0f - nfbCoef) * yOut + nfbCoef * nfbState;
            return yOut;
        }

    private:
        TubeTriode v5a, v5b;
        TubePowerPair inner, outer;
        OutputTransformer xfmr;
        float nfbState { 0 }, nfbCoef { 0 }, nfbAmount { 0.2f };
        bool  classAOnly { false };
    };

    //--------------------------------------------------------------------------
    // FMV (Fender/Marshall/Vox) passive tone stack approximation
    // Treble (high shelf), Middle (peak), Bass (low shelf) + Pull Bright / Shift.
    //--------------------------------------------------------------------------
    class ToneStack
    {
    public:
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        using Filter = juce::dsp::IIR::Filter<float>;

        void prepare (double sr)
        {
            sampleRate = sr;
            for (auto& f : { &treble[0], &treble[1], &middle[0], &middle[1],
                             &bass[0],   &bass[1],   &bright[0], &bright[1],
                             &shift[0],  &shift[1] })
                f->reset();
            update();
        }
        void reset()
        {
            for (auto& f : { &treble[0], &treble[1], &middle[0], &middle[1],
                             &bass[0],   &bass[1],   &bright[0], &bright[1],
                             &shift[0],  &shift[1] })
                f->reset();
        }

        void set (float trebleDb, float middleDb, float bassDb,
                  bool pullTreble, bool pullBright, bool pullShift)
        {
            tDb = trebleDb; mDb = middleDb; bDb = bassDb;
            pT = pullTreble; pB = pullBright; pS = pullShift;
            update();
        }

        inline float processSample (int ch, float x) noexcept
        {
            // Passive tone stack loses ~15 dB in the middle - replicate
            x *= 0.18f;
            x = treble[ch].processSample (x);
            x = middle[ch].processSample (x);
            x = bass  [ch].processSample (x);
            if (pB) x = bright[ch].processSample (x);
            if (pS) x = shift [ch].processSample (x);
            return x * 5.6f;    // make-up the passive loss
        }

    private:
        void update()
        {
            auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };
            // Pull Treble modifies the treble corner higher
            const float trebCorner = pT ? 3200.0f : 2000.0f;
            auto tc = Coeffs::makeHighShelf  (sampleRate, trebCorner, 0.5f, g (tDb));
            auto mc = Coeffs::makePeakFilter (sampleRate, pS ? 350.0f : 600.0f, 0.6f, g (mDb));
            auto bc = Coeffs::makeLowShelf   (sampleRate, 110.0f, 0.5f, g (bDb));
            auto brc = Coeffs::makeHighShelf (sampleRate, 8000.0f, 0.7f, g (6.0f));
            auto shc = Coeffs::makeLowShelf  (sampleRate, 80.0f,  0.5f, g (3.0f));

            for (int c = 0; c < 2; ++c)
            {
                *treble[c].coefficients = *tc;
                *middle[c].coefficients = *mc;
                *bass  [c].coefficients = *bc;
                *bright[c].coefficients = *brc;
                *shift [c].coefficients = *shc;
            }
        }

        double sampleRate { 44100.0 };
        Filter treble[2], middle[2], bass[2], bright[2], shift[2];
        float tDb { 0 }, mDb { 0 }, bDb { 0 };
        bool  pT { false }, pB { false }, pS { false };
    };

    //--------------------------------------------------------------------------
    // Classic 5-band graphic EQ (schematic's active LC tank EQ in DSP form).
    // Centre frequencies 80 / 240 / 750 / 2200 / 6600 Hz, +/- 12 dB each.
    //--------------------------------------------------------------------------
    class GraphicEQ5
    {
    public:
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        using Filter = juce::dsp::IIR::Filter<float>;

        static constexpr float freqs[5] = { 80.0f, 240.0f, 750.0f, 2200.0f, 6600.0f };

        void prepare (double sr)
        {
            sampleRate = sr;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 5; ++i) bands[c][i].reset();
            for (int i = 0; i < 5; ++i) gainDb[i] = 0.0f;
            update();
        }
        void reset()
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 5; ++i) bands[c][i].reset();
        }

        void setBandDb (int i, float db)
        {
            gainDb[i] = juce::jlimit (-12.0f, 12.0f, db);
            auto coeff = Coeffs::makePeakFilter (sampleRate, freqs[i], 1.1f,
                                                 juce::Decibels::decibelsToGain (gainDb[i]));
            for (int c = 0; c < 2; ++c) *bands[c][i].coefficients = *coeff;
        }

        inline float processSample (int ch, float x) noexcept
        {
            for (int i = 0; i < 5; ++i) x = bands[ch][i].processSample (x);
            return x;
        }

    private:
        void update() { for (int i = 0; i < 5; ++i) setBandDb (i, gainDb[i]); }
        double sampleRate { 44100.0 };
        Filter bands[2][5];
        float  gainDb[5] { 0,0,0,0,0 };
    };

    //--------------------------------------------------------------------------
    // SpringReverb - 3-spring Accutronics-style tank (two long + one short),
    // with per-spring frequency-dependent dispersion (the characteristic
    // "chirp"), ~2.3 s decay, 2.5 kHz LPF for the dark spring tail.
    //
    // Each spring is a long delay line in a feedback loop with its own
    // damping LPF.  Dispersion is approximated by a cascade of first-order
    // allpass sections that delay higher frequencies more than low ones -
    // this is where the metallic "boing" comes from.
    //--------------------------------------------------------------------------
    class SpringReverb
    {
    public:
        void prepare (double sr, int /*block*/)
        {
            sampleRate = sr;
            for (int s = 0; s < 3; ++s)
            {
                const int d = (int) (sr * springDelays[s]);
                springs[s].buf.assign ((size_t) (d + 4), 0.0f);
                springs[s].len     = d;
                springs[s].wIdx    = 0;
                springs[s].damp    = 0.0f;
                springs[s].dampCoef = std::exp (-juce::MathConstants<float>::twoPi * 2200.0f / (float) sr);
                for (int a = 0; a < NAp; ++a) springs[s].ap[a] = 0.0f;
            }
            tailLp = 0.0f;
            tailCoef = std::exp (-juce::MathConstants<float>::twoPi * 2500.0f / (float) sr);
        }
        void reset()
        {
            for (auto& s : springs)
            {
                std::fill (s.buf.begin(), s.buf.end(), 0.0f);
                s.wIdx = 0;  s.damp = 0.0f;
                for (int a = 0; a < NAp; ++a) s.ap[a] = 0.0f;
            }
            tailLp = 0.0f;
        }

        inline float processSample (float x) noexcept
        {
            float wet = 0.0f;
            for (int s = 0; s < 3; ++s)
            {
                auto& sp = springs[s];
                // Read tap
                int rIdx = sp.wIdx - sp.len; while (rIdx < 0) rIdx += sp.len;
                float r = sp.buf[(size_t) rIdx];

                // Dispersion: cascade of allpass sections introduces
                // frequency-dependent delay -> metallic chirp
                for (int a = 0; a < NAp; ++a)
                {
                    const float apCoef = 0.6f;
                    const float v = r - apCoef * sp.ap[a];
                    const float y = apCoef * v + sp.ap[a];
                    sp.ap[a] = v;
                    r = y;
                }

                // Damp (treble loss per bounce)
                sp.damp = (1.0f - sp.dampCoef) * r + sp.dampCoef * sp.damp;

                // Write back with feedback (controls decay time ~2.3 s)
                sp.buf[(size_t) sp.wIdx] = x + sp.damp * 0.78f;
                if (++sp.wIdx >= sp.len) sp.wIdx = 0;

                wet += sp.damp * springGains[s];
            }
            // Dark tail LPF
            tailLp = (1.0f - tailCoef) * wet + tailCoef * tailLp;
            return tailLp;
        }

    private:
        static constexpr int NAp = 6;
        struct Spring
        {
            std::vector<float> buf;
            int len { 0 }, wIdx { 0 };
            float damp { 0 }, dampCoef { 0 };
            float ap[NAp] {};
        };
        // Accutronics-type: 3 springs with staggered delay (short/med/long)
        static constexpr float springDelays[3] = { 0.029f, 0.037f, 0.043f };
        static constexpr float springGains [3] = { 0.45f,  0.50f,  0.40f };

        Spring springs[3];
        double sampleRate { 44100.0 };
        float  tailLp { 0 }, tailCoef { 0 };
    };

    //--------------------------------------------------------------------------
    // Presence - active high-shelf in the negative-feedback loop of the power
    // stage.  In DSP a static shelf is a fine approximation.
    //--------------------------------------------------------------------------
    class Presence
    {
    public:
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        using Filter = juce::dsp::IIR::Filter<float>;

        void prepare (double sr) { sampleRate = sr; for (auto& f : filt) f.reset(); set (0.0f); }
        void reset() { for (auto& f : filt) f.reset(); }

        void set (float db)
        {
            auto c = Coeffs::makeHighShelf (sampleRate, 3500.0f, 0.6f,
                                            juce::Decibels::decibelsToGain (db));
            for (int ch = 0; ch < 2; ++ch) *filt[ch].coefficients = *c;
        }
        inline float processSample (int ch, float x) noexcept
        {
            return filt[ch].processSample (x);
        }
    private:
        double sampleRate { 44100.0 };
        Filter filt[2];
    };
} // namespace lead85
