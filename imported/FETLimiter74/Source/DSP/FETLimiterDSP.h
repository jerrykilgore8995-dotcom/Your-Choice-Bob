#pragma once
#include <JuceHeader.h>

/*
    FETLimiterDSP.h
    ---------------
    Full-chain emulation of a classic British feed-forward FET limiter.
    Topology modelled (per the service-manual schematic the user shared):

      [IN]-> input transformer / unbal line ->
      INPUT PREAMP (2SC732 / 2SA562 Class-A discrete, VR1 input gain) ->
      VR2 internal threshold tap ->
      JFET VCA (BF244B, voltage-controlled resistor) ->
      OUTPUT PREAMP (2SC732 / 2SA562 discrete) ->
      OUTPUT TRANSFORMER (0 dBm / -30 dBm taps) ->
      [OUT]

    SIDECHAIN (parallel):
      peak-rectified post-VCA signal -> uA741 comparator vs VR3 reference ->
      switchable R/C time constants ->
      JFET gate control voltage ->
      "LIMIT > 3 dB" LED when GR exceeds 3 dB

    All circuit non-linearities (discrete-stage 2nd-harmonic bias,
    JFET VCR distortion, transformer core saturation, 741 slew-limiting
    noise, +/-24V PSU ripple) are modelled as cheap sample-accurate
    functions in C++ — no SPICE, no convolution, just the stuff you can
    actually hear.
*/
namespace fetlim
{
    //--------------------------------------------------------------------------
    // Discrete Class-A transistor stage - adds mild 2nd + 3rd harmonic bias
    //--------------------------------------------------------------------------
    class DiscreteStage
    {
    public:
        void setDrive (float amt01) noexcept    // 0..1 scales distortion
        {
            amount = juce::jlimit (0.0f, 1.0f, amt01);
        }
        inline float processSample (float x) noexcept
        {
            // Slight asymmetric bias then soft-clip - characteristic BJT Class-A
            const float bias  = 0.03f + 0.07f * amount;
            const float drive = 1.0f  + 2.5f  * amount;
            const float y = std::tanh (drive * (x + bias)) - std::tanh (drive * bias);
            return y / drive;      // level-compensate
        }
    private:
        float amount { 0.3f };
    };

    //--------------------------------------------------------------------------
    // Input / output transformer - LF + HF rolloff, soft core saturation
    //--------------------------------------------------------------------------
    class Transformer
    {
    public:
        void prepare (double sr, float lowCutHz, float highCutHz, float satLevel = 1.5f)
        {
            satLvl = satLevel;
            // 1-pole HPF (LF rolloff) and 1-pole LPF (HF rolloff)
            const float wh = std::exp (-juce::MathConstants<float>::twoPi * lowCutHz  / (float) sr);
            const float wl = std::exp (-juce::MathConstants<float>::twoPi * highCutHz / (float) sr);
            hpCoef = wh;
            lpCoef = wl;
            x1 = y1 = y2 = 0.0f;
        }
        void reset() { x1 = y1 = y2 = 0.0f; }

        inline float processSample (float x) noexcept
        {
            // 1-pole HPF
            const float y = hpCoef * (y1 + x - x1);
            x1 = x; y1 = y;
            // 1-pole LPF
            y2 = (1.0f - lpCoef) * y + lpCoef * y2;
            // Core saturation
            return std::tanh (y2 / satLvl) * satLvl;
        }
    private:
        float hpCoef { 0 }, lpCoef { 0 }, x1 { 0 }, y1 { 0 }, y2 { 0 };
        float satLvl { 1.5f };
    };

    //--------------------------------------------------------------------------
    // uA741-style op-amp artefacts - tiny noise + ~20 kHz bandwidth cap
    //--------------------------------------------------------------------------
    class OpAmp741
    {
    public:
        void prepare (double sr)
        {
            const float wl = std::exp (-juce::MathConstants<float>::twoPi * 20000.0f / (float) sr);
            lp = wl; y = 0.0f;
            rng.setSeed (0x741BEEF);
        }
        inline float processSample (float x) noexcept
        {
            // Band-limit (gain-bandwidth product fall-off)
            y = (1.0f - lp) * x + lp * y;
            // Equivalent-input noise ~ -108 dBu-ish (very low)
            const float n = (rng.nextFloat() - 0.5f) * 4.0e-5f;
            return y + n;
        }
    private:
        float lp { 0 }, y { 0 };
        juce::Random rng;
    };

    //--------------------------------------------------------------------------
    // BF244B JFET used as voltage-controlled resistor (the VCA element)
    //
    // Rds(on) ~ 150 ohm; pinch-off Vp = -2V typical.
    // Rds(Vgs) = Rds_on / (1 + Vgs / Vp)  for 0 >= Vgs > Vp
    // In the limiter it forms a shunt to ground after a series R,
    // acting as a voltage divider that attenuates when Vgs rises.
    //
    // DSP model: convert control voltage -> gain, then re-inject the small
    // signal-dependent distortion that a real FET VCA produces (2nd + 3rd
    // harmonics that get worse as GR increases).
    //--------------------------------------------------------------------------
    class JFETVca
    {
    public:
        void setDistortionComp (float amt01) noexcept  // VR4 - 0..1
        {
            distComp = juce::jlimit (0.0f, 1.0f, amt01);
        }

        /** gainReductionDb is 0 .. +30 (positive = attenuating). */
        inline float processSample (float x, float gainReductionDb) noexcept
        {
            const float g = juce::Decibels::decibelsToGain (-gainReductionDb);

            // Signal-dependent harmonic distortion that scales with GR
            // (real FET-VCR THD grows ~ linearly with 1 - g)
            const float gr01   = juce::jlimit (0.0f, 1.0f, 1.0f - g);
            const float thdAmt = gr01 * (1.0f - 0.85f * distComp);   // VR4 reduces it

            // VR4 "distortion compensation" bleeds a counter-phased 2nd
            // harmonic back in - this is what the trimmer actually does on
            // the hardware. Positive = subtracts 2nd harmonic.
            const float x2 = x * x * (x >= 0 ? 1.0f : -1.0f);     // |x|*x -> 2nd harmonic
            const float dist = thdAmt * (0.6f * x2 + 0.25f * x * x * x);

            return (x - dist) * g;
        }
    private:
        float distComp { 0.5f };
    };

    //--------------------------------------------------------------------------
    // Sidechain detector - rectifier -> log -> threshold -> switchable R/C
    //--------------------------------------------------------------------------
    class SidechainDetector
    {
    public:
        enum class AttackMode  { Fast = 0, Slow };
        enum class ReleaseMode { Fast = 0, Auto, Med, Slow };

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            envDb = -100.0f;
            releaseEnv = 0.0f;
            applyTimeConstants();
        }
        void reset() { envDb = -100.0f; releaseEnv = 0.0f; }

        void setAttackMode  (AttackMode m)  { atk = m; applyTimeConstants(); }
        void setReleaseMode (ReleaseMode m) { rel = m; applyTimeConstants(); }
        void setThresholdDb (float t)       { thresholdDb = t; }

        /** Returns gain reduction in dB (positive = attenuating). */
        inline float processSample (float detectorInput) noexcept
        {
            // Full-wave rectifier -> log domain
            const float inDb = juce::Decibels::gainToDecibels (std::abs (detectorInput) + 1.0e-9f);

            // AUTO release = program-dependent dual timeconstant.
            float relCoef = relCoefStatic;
            if (rel == ReleaseMode::Auto)
            {
                // Fast release at first, then slow if hold is sustained
                releaseEnv = releaseEnv * autoDecayCoef + (inDb > thresholdDb ? 1.0f : 0.0f) * (1.0f - autoDecayCoef);
                const float t = juce::jlimit (0.0f, 1.0f, releaseEnv);
                relCoef = autoFastCoef * (1.0f - t) + autoSlowCoef * t;
            }

            if (inDb > envDb) envDb = atkCoef * envDb + (1.0f - atkCoef) * inDb;
            else              envDb = relCoef * envDb + (1.0f - relCoef) * inDb;

            // Hard-knee limiter: everything above threshold reduced to threshold
            // (brick-wall with internal thresholdDb, which VR2 sets).
            const float over = envDb - thresholdDb;
            return (over > 0.0f) ? over : 0.0f;
        }

    private:
        void applyTimeConstants()
        {
            // Attack: Fast ~ 150 us, Slow ~ 2 ms
            const float atkMs = (atk == AttackMode::Fast) ? 0.15f : 2.0f;
            atkCoef = std::exp (-1.0f / (atkMs * 0.001f * (float) sr));

            // Release: Fast ~ 50 ms, Med ~ 200 ms, Slow ~ 800 ms
            float relMs = 50.0f;
            switch (rel)
            {
                case ReleaseMode::Fast: relMs =  50.0f; break;
                case ReleaseMode::Med:  relMs = 200.0f; break;
                case ReleaseMode::Slow: relMs = 800.0f; break;
                case ReleaseMode::Auto: relMs = 150.0f; break; // overridden per sample
            }
            relCoefStatic = std::exp (-1.0f / (relMs * 0.001f * (float) sr));

            // AUTO dual-stage coefficients
            autoFastCoef = std::exp (-1.0f / ( 30.0f * 0.001f * (float) sr));
            autoSlowCoef = std::exp (-1.0f / (900.0f * 0.001f * (float) sr));
            autoDecayCoef= std::exp (-1.0f / (400.0f * 0.001f * (float) sr));
        }

        double sr { 44100.0 };
        AttackMode  atk { AttackMode::Fast };
        ReleaseMode rel { ReleaseMode::Auto };
        float thresholdDb { -6.0f };
        float envDb { -100.0f };
        float releaseEnv { 0.0f };
        float atkCoef { 0 }, relCoefStatic { 0 };
        float autoFastCoef { 0 }, autoSlowCoef { 0 }, autoDecayCoef { 0 };
    };

    //--------------------------------------------------------------------------
    // +/- 24V PSU ripple - optional 100 Hz hum coupling (vintage mode)
    //--------------------------------------------------------------------------
    class PsuRipple
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; phase = 0.0f; }
        void setEnabled (bool on) noexcept { enabled = on; }
        inline float processSample (float x) noexcept
        {
            if (! enabled) return x;
            phase += 100.0f / (float) sr;
            if (phase >= 1.0f) phase -= 1.0f;
            const float hum = std::sin (phase * juce::MathConstants<float>::twoPi) * 1.5e-4f
                            + std::sin (phase * juce::MathConstants<float>::twoPi * 2.0f) * 7.0e-5f;
            return x + hum;
        }
    private:
        double sr { 44100.0 };
        float  phase { 0.0f };
        bool   enabled { false };
    };
} // namespace fetlim
