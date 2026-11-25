#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
APVTS::ParameterLayout FETLimiter74Processor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using PB = juce::AudioParameterBool;
    using PC = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // VR1 - Input level (47k log pot behaviour -> skew)
    p.push_back (std::make_unique<P>  ("inputDb",   "Input",
                    juce::NormalisableRange<float>(-20.0f, 40.0f, 0.1f, 0.6f), 0.0f));

    p.push_back (std::make_unique<PC> ("micLine",   "Source",
                    juce::StringArray { "MIC", "LINE" }, 1));

    p.push_back (std::make_unique<PB> ("limiterOn", "Limiter",       true));
    p.push_back (std::make_unique<PC> ("attackMode","Attack",
                    juce::StringArray { "FAST", "SLOW" }, 0));
    p.push_back (std::make_unique<PC> ("releaseMode","Release",
                    juce::StringArray { "FAST", "AUTO", "MED", "SLOW" }, 1));

    // VR2 - Internal threshold
    p.push_back (std::make_unique<P>  ("thresholdDb", "Threshold",
                    juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), -6.0f));

    // VR4 - Distortion compensation
    p.push_back (std::make_unique<P>  ("distComp",   "Dist Comp",
                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // VR3 - LED adjust (cosmetic LED threshold reference)
    p.push_back (std::make_unique<P>  ("ledCal",     "LED Cal",
                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    p.push_back (std::make_unique<PB> ("stereoLink", "Stereo Link",  true));

    p.push_back (std::make_unique<P>  ("outputDb",   "Output",
                    juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<PC> ("outputFormat","Output Lvl",
                    juce::StringArray { "0 dBm", "-30 dBm" }, 0));

    p.push_back (std::make_unique<PB> ("psuRipple",  "PSU Ripple", false));

    return { p.begin(), p.end() };
}

//==============================================================================
FETLimiter74Processor::FETLimiter74Processor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pInputDb      = apvts.getRawParameterValue ("inputDb");
    pMicLine      = apvts.getRawParameterValue ("micLine");
    pLimiterOn    = apvts.getRawParameterValue ("limiterOn");
    pAttackMode   = apvts.getRawParameterValue ("attackMode");
    pReleaseMode  = apvts.getRawParameterValue ("releaseMode");
    pThresholdDb  = apvts.getRawParameterValue ("thresholdDb");
    pDistComp     = apvts.getRawParameterValue ("distComp");
    pLedCal       = apvts.getRawParameterValue ("ledCal");
    pStereoLink   = apvts.getRawParameterValue ("stereoLink");
    pOutputDb     = apvts.getRawParameterValue ("outputDb");
    pOutputFormat = apvts.getRawParameterValue ("outputFormat");
    pPsuRipple    = apvts.getRawParameterValue ("psuRipple");
}

//==============================================================================
bool FETLimiter74Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void FETLimiter74Processor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    for (auto& c : ch) { c.prepare (sampleRate); c.reset(); }
    currentGrDb.store (0.0f);
}

//==============================================================================
void FETLimiter74Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    const bool  limOn       = pLimiterOn->load()  > 0.5f;
    const bool  stereoLink  = pStereoLink->load() > 0.5f;
    const bool  psuOn       = pPsuRipple->load()  > 0.5f;
    const float inGain      = juce::Decibels::decibelsToGain (pInputDb->load());
    const float outGain     = juce::Decibels::decibelsToGain (pOutputDb->load());
    const int   micLineIdx  = (int) pMicLine->load();        // 0=MIC, 1=LINE
    // MIC input adds ~20 dB preamp gain through the discrete stage (per schematic)
    const float micBoost    = (micLineIdx == 0) ? juce::Decibels::decibelsToGain (20.0f) : 1.0f;
    const int   outFmt      = (int) pOutputFormat->load();
    // -30 dBm output tap is ~30 dB lower (balanced-to-unbal attenuator in schematic)
    const float outFmtTrim  = (outFmt == 1) ? juce::Decibels::decibelsToGain (-30.0f) : 1.0f;
    const float threshold   = pThresholdDb->load();
    const float distComp    = pDistComp->load();

    const auto atkMode = (fetlim::SidechainDetector::AttackMode)  (int) pAttackMode ->load();
    const auto relMode = (fetlim::SidechainDetector::ReleaseMode) (int) pReleaseMode->load();

    for (auto& c : ch)
    {
        c.detector.setAttackMode  (atkMode);
        c.detector.setReleaseMode (relMode);
        c.detector.setThresholdDb (threshold);
        c.vca.setDistortionComp   (distComp);
        c.inputStage.setDrive     (0.25f);
        c.outputStage.setDrive    (0.20f);
        c.psu.setEnabled          (psuOn);
    }

    float peakGrDb = 0.0f;

    // Process sample-by-sample; stereo-link shares the max GR across channels.
    for (int i = 0; i < n; ++i)
    {
        float samples[2] = { 0.0f, 0.0f };
        float detSig[2]  = { 0.0f, 0.0f };

        // --- Front end: xfmr -> input stage -> input gain ---
        for (int c = 0; c < numCh; ++c)
        {
            float x = buffer.getReadPointer (c)[i] * micBoost;
            x = ch[c].inputXfmr.processSample (x);
            x = ch[c].inputStage.processSample (x * inGain);
            samples[c] = x;
            detSig[c]  = x;   // feed-forward detector tap
        }

        // --- Sidechain detector (per channel or linked) ---
        float grDb[2] = { 0.0f, 0.0f };
        if (limOn)
        {
            float detSum = detSig[0];
            if (numCh > 1) detSum = juce::jmax (std::abs (detSig[0]), std::abs (detSig[1]));

            if (stereoLink && numCh > 1)
            {
                const float g = ch[0].detector.processSample (detSum);
                // Pass through 741 bandwidth-limit so sidechain feels like the hardware
                const float gf = ch[0].sidechainOp.processSample (g);
                grDb[0] = grDb[1] = juce::jlimit (0.0f, 30.0f, gf);
            }
            else
            {
                for (int c = 0; c < numCh; ++c)
                {
                    const float g  = ch[c].detector.processSample (detSig[c]);
                    const float gf = ch[c].sidechainOp.processSample (g);
                    grDb[c] = juce::jlimit (0.0f, 30.0f, gf);
                }
            }
        }

        // --- JFET VCA + output stage + output xfmr + output trim ---
        for (int c = 0; c < numCh; ++c)
        {
            float y = ch[c].vca.processSample (samples[c], grDb[c]);
            y = ch[c].outputStage.processSample (y);
            y = ch[c].outputXfmr.processSample (y);
            y = ch[c].psu.processSample (y);
            y = y * outGain * outFmtTrim;
            buffer.getWritePointer (c)[i] = y;

            peakGrDb = juce::jmax (peakGrDb, grDb[c]);
        }
    }

    // Share current GR with editor (atomic, no mutex)
    currentGrDb.store (peakGrDb);
}

//==============================================================================
juce::AudioProcessorEditor* FETLimiter74Processor::createEditor()
{
    return new FETLimiter74Editor (*this);
}

void FETLimiter74Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FETLimiter74Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FETLimiter74Processor();
}
