#pragma once
#include <JuceHeader.h>
#include "DSP/Lead85DSP.h"

class Lead85MKIIProcessor  : public juce::AudioProcessor
{
public:
    Lead85MKIIProcessor();
    ~Lead85MKIIProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override          { return 1; }
    int getCurrentProgram() override       { return 0; }
    void setCurrentProgram (int) override  {}
    const juce::String getProgramName (int) override           { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> currentDriveLedBrightness { 0.0f };

private:
    //-------- per-channel full chain --------------------------------------
    struct Channel
    {
        // Preamp tube stages (V1A, V1B, V2A, V2B, V3A reverb drv, V4 recovery,
        //                     V4A lead-1, V3B lead-2)
        lead85::TubeTriode v1a, v1b, v2a, v2b, v3a, v4rec, v4a, v3b;

        // Full simul-class power section (V5A/V5B + V6/V9 + V7/V8 + xfmr + NFB)
        lead85::SimulClassPowerSection power;

        lead85::SpringReverb reverb;

        void prepare (double sr, int block)
        {
            // Preamp stages tuned per schematic coupling/Miller values + per-stage
            // cathode-cap shelving corner from the circuit (Mesa-style voicing).
            //                                      coupHz  millHz  gain  bias  hdrm  CkHz  CkDb
            v1a .prepare (sr,   50.0f, 10000.0f, 60.0f, 0.00f, 1.2f,  22.0f, -4.0f);  // high-gain input
            v1b .prepare (sr,   60.0f,  9000.0f, 25.0f, 0.00f, 1.3f,  70.0f, -3.0f);
            v2a .prepare (sr,   80.0f,  9000.0f, 15.0f, 0.00f, 1.4f, 150.0f, -2.5f);
            v2b .prepare (sr,   80.0f,  9000.0f, 15.0f, 0.00f, 1.4f, 150.0f, -2.5f);
            v4a .prepare (sr,   80.0f,  8000.0f, 50.0f, 0.00f, 1.1f,  32.0f, -3.5f);  // Lead gain
            v3b .prepare (sr,   60.0f,  8000.0f, 30.0f, 0.00f, 1.2f, 100.0f, -3.0f);  // Lead master
            v3a .prepare (sr,   40.0f, 10000.0f,  8.0f, 0.00f, 1.4f, 200.0f, -2.0f);  // Reverb drive
            v4rec.prepare(sr,   60.0f,  8000.0f, 20.0f, 0.00f, 1.3f, 150.0f, -2.5f);  // Reverb recovery

            power.prepare (sr);
            reverb.prepare (sr, block);
        }
        void reset()
        {
            for (auto* t : { &v1a, &v1b, &v2a, &v2b, &v3a, &v4rec, &v4a, &v3b })
                t->reset();
            power.reset();
            reverb.reset();
        }
    };

    Channel ch[2];

    lead85::ToneStack  tone;
    lead85::GraphicEQ5 geq;
    lead85::Presence   presence;

    // Parameter pointers
    std::atomic<float>* pInput         { nullptr };
    std::atomic<float>* pVolume1       { nullptr };
    std::atomic<float>* pPullBright1   { nullptr };
    std::atomic<float>* pTreble        { nullptr };
    std::atomic<float>* pPullTreble    { nullptr };
    std::atomic<float>* pMiddle        { nullptr };
    std::atomic<float>* pBass          { nullptr };
    std::atomic<float>* pPullShift     { nullptr };
    std::atomic<float>* pMaster1       { nullptr };
    std::atomic<float>* pPullGain      { nullptr };
    std::atomic<float>* pLeadMode      { nullptr };
    std::atomic<float>* pLeadDrive     { nullptr };
    std::atomic<float>* pPullLeadBright{ nullptr };
    std::atomic<float>* pLeadMaster    { nullptr };
    std::atomic<float>* pPresence      { nullptr };
    std::atomic<float>* pReverbOn      { nullptr };
    std::atomic<float>* pReverb        { nullptr };
    std::atomic<float>* pGeqOn         { nullptr };
    std::atomic<float>* pGeq[5]        {};
    std::atomic<float>* pClassA        { nullptr };
    std::atomic<float>* pOutput        { nullptr };
    std::atomic<float>* pPowerOn       { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Lead85MKIIProcessor)
};
