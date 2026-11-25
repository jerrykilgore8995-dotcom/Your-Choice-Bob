#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
APVTS::ParameterLayout Lead85MKIIProcessor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using PB = juce::AudioParameterBool;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto k01 = [](const char* id, const char* name, float def = 0.5f) {
        return std::make_unique<P>(id, name, juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), def);
    };
    auto kDb = [](const char* id, const char* name, float lo, float hi, float def) {
        return std::make_unique<P>(id, name, juce::NormalisableRange<float>(lo, hi, 0.1f), def);
    };

    p.push_back (kDb ("input",    "Input",    -12.0f, 24.0f, 0.0f));

    p.push_back (k01 ("volume1",  "Volume 1", 0.5f));
    p.push_back (std::make_unique<PB>("pullBright1", "V1 Pull Bright", false));

    p.push_back (k01 ("treble",   "Treble",   0.7f));
    p.push_back (std::make_unique<PB>("pullTreble", "Pull Treble Shift", false));

    p.push_back (k01 ("middle",   "Middle",   0.4f));
    p.push_back (k01 ("bass",     "Bass",     0.4f));
    p.push_back (std::make_unique<PB>("pullShift", "Bass Pull Shift", false));

    p.push_back (k01 ("master1",  "Master 1", 0.5f));
    p.push_back (std::make_unique<PB>("pullGain", "Pull Deep", false));

    p.push_back (std::make_unique<PB>("leadMode", "Lead Channel", false));
    p.push_back (k01 ("leadDrive", "Lead Drive",  0.5f));
    p.push_back (std::make_unique<PB>("pullLeadBright", "Lead Pull Bright", false));
    p.push_back (k01 ("leadMaster","Lead Master", 0.5f));

    p.push_back (kDb ("presence", "Presence", -6.0f, 10.0f, 2.0f));
    p.push_back (std::make_unique<PB>("reverbOn", "Reverb On", false));
    p.push_back (k01 ("reverb",   "Reverb",   0.2f));

    // Graphic EQ: ON/OFF and five bands at exactly the schematic frequencies,
    // range +/-12 dB per band (matches the hardware's slider range).
    p.push_back (std::make_unique<PB>("geqOn", "Graphic EQ On", false));
    const char* geqNames[5] = { "GEQ 80 Hz", "GEQ 240 Hz", "GEQ 750 Hz",
                                "GEQ 2200 Hz", "GEQ 6600 Hz" };
    const char* geqIds[5]   = { "geq0", "geq1", "geq2", "geq3", "geq4" };
    for (int i = 0; i < 5; ++i)
        p.push_back (std::make_unique<P>(geqIds[i], geqNames[i],
                                         juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f),
                                         0.0f));

    p.push_back (std::make_unique<PB>("classA", "Class A Mode", false));
    p.push_back (kDb ("output",   "Output",   -24.0f, 12.0f, 0.0f));
    p.push_back (std::make_unique<PB>("powerOn", "Power",       true));

    p.push_back (std::make_unique<PB>(juce::ParameterID { "standby", 2 }, "Standby", false));
    p.push_back (std::make_unique<PB>(juce::ParameterID { "cabEnabled", 2 }, "Cabinet", true));

    return { p.begin(), p.end() };
}

//==============================================================================
Lead85MKIIProcessor::Lead85MKIIProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pInput          = apvts.getRawParameterValue ("input");
    pVolume1        = apvts.getRawParameterValue ("volume1");
    pPullBright1    = apvts.getRawParameterValue ("pullBright1");
    pTreble         = apvts.getRawParameterValue ("treble");
    pPullTreble     = apvts.getRawParameterValue ("pullTreble");
    pMiddle         = apvts.getRawParameterValue ("middle");
    pBass           = apvts.getRawParameterValue ("bass");
    pPullShift      = apvts.getRawParameterValue ("pullShift");
    pMaster1        = apvts.getRawParameterValue ("master1");
    pPullGain       = apvts.getRawParameterValue ("pullGain");
    pLeadMode       = apvts.getRawParameterValue ("leadMode");
    pLeadDrive      = apvts.getRawParameterValue ("leadDrive");
    pPullLeadBright = apvts.getRawParameterValue ("pullLeadBright");
    pLeadMaster     = apvts.getRawParameterValue ("leadMaster");
    pPresence       = apvts.getRawParameterValue ("presence");
    pReverbOn       = apvts.getRawParameterValue ("reverbOn");
    pReverb         = apvts.getRawParameterValue ("reverb");
    pGeqOn          = apvts.getRawParameterValue ("geqOn");
    for (int i = 0; i < 5; ++i) pGeq[i] = apvts.getRawParameterValue ("geq" + juce::String(i));
    pClassA         = apvts.getRawParameterValue ("classA");
    pOutput         = apvts.getRawParameterValue ("output");
    pPowerOn        = apvts.getRawParameterValue ("powerOn");
    pCabEnabled     = apvts.getRawParameterValue ("cabEnabled");
    pStandby        = apvts.getRawParameterValue ("standby");
}

bool Lead85MKIIProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void Lead85MKIIProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto channels = getTotalNumOutputChannels();
    processingBlockSize = juce::jmax (1, samplesPerBlock);
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        static_cast<size_t> (channels), 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing (static_cast<size_t> (processingBlockSize));
    const auto ampRate = sampleRate * 2.0;
    for (auto& c : ch) { c.prepare (ampRate, processingBlockSize * 2); c.reset(); }
    tone.prepare (ampRate);
    geq.prepare (ampRate);
    presence.prepare (ampRate);
    cabinet.prepare (sampleRate, processingBlockSize, channels, pCabEnabled->load() > 0.5f);
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));
    currentDriveLedBrightness.store (0.0f);
    wasMuted = false;
}

void Lead85MKIIProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    if (pPowerOn->load() <= 0.5f || pStandby->load() > 0.5f)
    {
        if (!wasMuted)
        {
            for (auto& c : ch)
                c.reset();
            tone.reset();
            geq.reset();
            presence.reset();
            cabinet.reset();
            oversampling->reset();
            wasMuted = true;
        }
        buffer.clear();
        currentDriveLedBrightness.store (0.0f);
        return;
    }

    wasMuted = false;
    auto audio = juce::dsp::AudioBlock<float> (buffer);
    for (size_t start = 0; start < audio.getNumSamples(); start += static_cast<size_t> (processingBlockSize))
    {
        auto n = juce::jmin (static_cast<size_t> (processingBlockSize), audio.getNumSamples() - start);
        auto part = audio.getSubBlock (start, n);
        processAmp (oversampling->processSamplesUp (part));
        oversampling->processSamplesDown (part);
    }

    cabinet.process (buffer, pCabEnabled->load() > 0.5f);
    buffer.applyGain (juce::Decibels::decibelsToGain (pOutput->load()));
}

void Lead85MKIIProcessor::processAmp (juce::dsp::AudioBlock<float> block)
{
    const float inGain    = juce::Decibels::decibelsToGain (pInput ->load());
    const float vol1      = pVolume1   ->load();
    const float trDb      = juce::jmap (pTreble ->load(), 0.0f, 1.0f, -12.0f, 12.0f);
    const float mdDb      = juce::jmap (pMiddle ->load(), 0.0f, 1.0f, -12.0f,  6.0f);
    const float bsDb      = juce::jmap (pBass   ->load(), 0.0f, 1.0f, -12.0f, 12.0f);
    const float master1   = pMaster1   ->load();
    const float leadDrv   = pLeadDrive ->load();
    const float leadMast  = pLeadMaster->load();
    const bool  reverbOn  = pReverbOn  ->load() > 0.5f;
    const float revSend   = pReverb    ->load();
    const bool  pBright1  = pPullBright1->load() > 0.5f;
    const bool  pTreb     = pPullTreble ->load() > 0.5f;
    const bool  pShift    = pPullShift  ->load() > 0.5f;
    const bool  pullDeep  = pPullGain   ->load() > 0.5f;
    const bool  pLBright  = pPullLeadBright->load() > 0.5f;
    const bool  leadMode  = pLeadMode  ->load() > 0.5f;
    const bool  geqOn     = pGeqOn     ->load() > 0.5f;
    const bool  classA    = pClassA    ->load() > 0.5f;

    tone.set (trDb, mdDb, bsDb, pTreb, pBright1, pShift);
    for (int i = 0; i < 5; ++i) geq.setBandDb (i, pGeq[i]->load());
    presence.set (pPresence->load());

    // Presence pot also sets the amount of negative feedback in the power section
    const float nfbAmount = juce::jmap (pPresence->load(), -6.0f, 10.0f, 0.35f, 0.05f);
    for (auto& c : ch)
    {
        c.power.setClassAOnly (classA);
        c.power.setDrive      (1.0f);
        c.power.setNfbAmount  (nfbAmount);
    }

    float peakGr = 0.0f;

    for (size_t cI = 0; cI < block.getNumChannels(); ++cI)
    {
        auto& c   = ch[cI];
        auto* buf = block.getChannelPointer (cI);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            // --- 1) Input gain + V1A (first high-gain triode) ---
            float x = buf[i] * inGain;
            x = c.v1a.processSample (x);

            // --- 2) Passive FMV tone stack w/ pulls ---
            x = tone.processSample (cI, x);

            // --- 3) V1B second triode stage ---
            x = c.v1b.processSample (x);

            // --- 4) Volume 1 pot ---
            x *= vol1;

            // --- 7) Reverb send/return (parallel) - skipped entirely when
            //        REVERB IN switch is off: V3A driver and V4 recovery
            //        tubes are silent, same as pulling the reverb pot on
            //        the hardware. ---
            float rev = 0.0f;
            if (reverbOn && revSend > 0.001f)
            {
                const float send = c.v3a.processSample (x * revSend);
                rev = c.v4rec.processSample (c.reverb.processSample (send));
            }

            // --- 8) [LEAD mode] V4A -> V3B cascade with Lead Drive -> Lead Master ---
            if (leadMode)
            {
                const float driveLin = juce::jmap (leadDrv * leadDrv, 0.0f, 1.0f, 0.3f, 4.0f);
                const float brightMul = pLBright ? 1.2f : 1.0f;
                x = c.v4a.processSample (x * driveLin * brightMul);
                x = c.v3b.processSample (x);
                x *= leadMast;
            }

            // Master 1 sets power-stage drive in both channels.
            x = x * master1 + rev * 0.4f;
            const auto deeper = c.deep.processSample (x);
            if (pullDeep)
                x = deeper;

            // --- 9) GRAPHIC EQ (switchable - between preamp and power amp) ---
            if (geqOn) x = geq.processSample (cI, x);

            // --- 10) V5A/V5B phase inverter + V6-V9 simul-class output pairs
            //         + output transformer + negative feedback ---
            x = c.power.processSample (x * 0.7f);

            // --- 11) Presence (NFB high-shelf) adds post-NFB sparkle ---
            x = presence.processSample (cI, x);

            buf[i] = x;

            peakGr = juce::jmax (peakGr, std::abs (x));
        }
    }

    // Smooth the "overdrive" LED brightness (post-chain level roughly tracks drive)
    const float t = juce::jlimit (0.0f, 1.0f, peakGr * 0.8f);
    const float cur = currentDriveLedBrightness.load();
    currentDriveLedBrightness.store (cur + (t - cur) * 0.15f);
}

//==============================================================================
juce::AudioProcessorEditor* Lead85MKIIProcessor::createEditor() { return new Lead85MKIIEditor (*this); }

void Lead85MKIIProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    cabinet.store (state);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}
void Lead85MKIIProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const auto oldSession = !state.getChildWithProperty ("id", "cabEnabled").isValid();
            cabinet.restore (state);
            state.removeProperty ("cabWav", nullptr);
            state.removeProperty ("cabName", nullptr);
            state.removeProperty ("cabFactory", nullptr);
            apvts.replaceState (state);
            if (oldSession)
                apvts.getParameter ("cabEnabled")->setValueNotifyingHost (0.0f);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Lead85MKIIProcessor(); }
