#pragma once
#include <JuceHeader.h>
#include "DSP/FETLimiterDSP.h"

class FETLimiter74Processor  : public juce::AudioProcessor
{
public:
    FETLimiter74Processor();
    ~FETLimiter74Processor() override = default;

    //==============================================================================
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override          { return 1; }
    int getCurrentProgram() override       { return 0; }
    void setCurrentProgram (int) override  {}
    const juce::String getProgramName (int) override           { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Shared with the editor for the LIMIT >3dB LED
    std::atomic<float> currentGrDb { 0.0f };

private:
    //==============================================================================
    struct Channel
    {
        fetlim::Transformer       inputXfmr;
        fetlim::DiscreteStage     inputStage;
        fetlim::JFETVca           vca;
        fetlim::SidechainDetector detector;
        fetlim::OpAmp741          sidechainOp;
        fetlim::DiscreteStage     outputStage;
        fetlim::Transformer       outputXfmr;
        fetlim::PsuRipple         psu;

        void prepare (double sr)
        {
            // Classic British limiter xfmr specs: ~20 Hz / 25 kHz
            inputXfmr .prepare (sr, 20.0f, 25000.0f, 1.6f);
            outputXfmr.prepare (sr, 25.0f, 22000.0f, 1.4f);
            sidechainOp.prepare (sr);
            detector.prepare (sr);
            psu.prepare (sr);
        }
        void reset()
        {
            inputXfmr.reset(); outputXfmr.reset(); detector.reset();
        }
    };

    Channel ch[2];

    // Parameter pointers (lock-free reads)
    std::atomic<float>* pInputDb        { nullptr };
    std::atomic<float>* pMicLine        { nullptr };
    std::atomic<float>* pLimiterOn      { nullptr };
    std::atomic<float>* pAttackMode     { nullptr };
    std::atomic<float>* pReleaseMode    { nullptr };
    std::atomic<float>* pThresholdDb    { nullptr };
    std::atomic<float>* pDistComp       { nullptr };
    std::atomic<float>* pLedCal         { nullptr };
    std::atomic<float>* pStereoLink     { nullptr };
    std::atomic<float>* pOutputDb       { nullptr };
    std::atomic<float>* pOutputFormat   { nullptr };
    std::atomic<float>* pPsuRipple      { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FETLimiter74Processor)
};
