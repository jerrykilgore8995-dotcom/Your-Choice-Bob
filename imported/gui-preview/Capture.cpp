#include <JuceHeader.h>
#include <cstdio>

#define JucePlugin_Name "Lead85MKII"
#include "../Lead85MKII/Source/PluginProcessor.h"
#undef JucePlugin_Name

juce::AudioProcessor* JUCE_CALLTYPE createFETLimiter74();
juce::AudioProcessor* JUCE_CALLTYPE createLead85MKII();

static bool capture(std::unique_ptr<juce::AudioProcessor> processor, const juce::File& output)
{
    const std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    if (editor == nullptr)
        return false;

    editor->setVisible(true);
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    juce::FileOutputStream stream(output);
    return image.isValid() && stream.openedOk() && juce::PNGImageFormat().writeImageToStream(image, stream);
}

struct AmpMeasurement
{
    float rms;
    float peak;
    double meanMilliseconds;
    double worstMilliseconds;
};

static AmpMeasurement measure (Lead85MKIIProcessor& amp, float inputDb)
{
    constexpr auto sampleRate = 44100.0;
    constexpr auto blockSize = 512;
    constexpr auto blocks = 115;
    const auto level = juce::Decibels::decibelsToGain (inputDb);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    auto energy = 0.0;
    auto peak = 0.0f;
    auto elapsed = 0.0;
    auto worst = 0.0;

    amp.prepareToPlay (sampleRate, blockSize);
    for (int block = 0; block < blocks; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const auto t = (block * blockSize + i) / sampleRate;
            const auto sine = [] (double frequency, double time)
            {
                return std::sin (juce::MathConstants<double>::twoPi * frequency * time);
            };
            const auto chord = 0.50 * sine (82.407, t) + 0.30 * sine (123.47, t)
                             + 0.12 * sine (164.81, t) + 0.08 * sine (246.94, t);
            buffer.setSample (0, i, level * static_cast<float> (chord));
            buffer.setSample (1, i, level * static_cast<float> (chord));
        }

        const auto start = juce::Time::getMillisecondCounterHiRes();
        amp.processBlock (buffer, midi);
        const auto duration = juce::Time::getMillisecondCounterHiRes() - start;
        if (block >= 10)
        {
            elapsed += duration;
            worst = juce::jmax (worst, duration);
            for (int i = 0; i < blockSize; ++i)
            {
                const auto out = buffer.getSample (0, i);
                if (!std::isfinite (out))
                    return { -200.0f, 2.0f, elapsed, worst };
                energy += static_cast<double> (out) * out;
                peak = juce::jmax (peak, std::abs (out));
            }
        }
    }
    amp.releaseResources();
    return { juce::Decibels::gainToDecibels (
                 static_cast<float> (std::sqrt (energy / ((blocks - 10) * blockSize))), -200.0f),
             peak, elapsed / (blocks - 10), worst };
}

static bool verifyLead (const juce::File& externalIR)
{
    auto amp = std::make_unique<Lead85MKIIProcessor>();
    const auto quiet = measure (*amp, -36.0f);
    const auto loud = measure (*amp, -18.0f);
    amp->apvts.getParameter ("leadMode")->setValueNotifyingHost (1.0f);
    const auto lead = measure (*amp, -18.0f);
    amp->useFactoryCabinet (2);
    const auto greenback = measure (*amp, -18.0f);
    std::printf ("V30 Rhythm -36 dBFS: %.2f dBFS RMS\nV30 Rhythm -18 dBFS: %.2f dBFS RMS\n"
                 "V30 Lead: %.2f dBFS RMS, %.3f ms mean, %.3f ms worst per 512\n"
                 "Greenback Lead: %.2f dBFS RMS\n", quiet.rms, loud.rms, lead.rms,
                 lead.meanMilliseconds, lead.worstMilliseconds, greenback.rms);

    if (!externalIR.existsAsFile() || amp->loadCabinet (externalIR).failed())
        return false;
    const auto external = measure (*amp, -18.0f);
    juce::MemoryBlock state;
    amp->getStateInformation (state);
    auto restored = std::make_unique<Lead85MKIIProcessor>();
    restored->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    const auto reloaded = measure (*restored, -18.0f);
    std::printf ("External WAV: %.2f dBFS RMS, restored: %.2f dBFS RMS\n",
                 external.rms, reloaded.rms);

    return quiet.rms > -95.0f && loud.rms > -60.0f && loud.rms - quiet.rms >= 3.0f
        && lead.rms > -60.0f && greenback.rms > -60.0f && external.rms > -60.0f
        && lead.peak < 1.0f && std::abs (greenback.rms - lead.rms) > 0.05f
        && restored->getCabinetName() == externalIR.getFileName()
        && std::abs (reloaded.rms - external.rms) < 0.05f;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI initialise;
    const auto directory = juce::File::getCurrentWorkingDirectory();
    if (argc == 2)
        return capture (std::unique_ptr<juce::AudioProcessor> (createLead85MKII()),
                        directory.getChildFile ("Lead85MKII-GUI.png"))
            && verifyLead (directory.getChildFile (juce::String::fromUTF8 (argv[1]))) ? 0 : 1;

    return capture (std::unique_ptr<juce::AudioProcessor> (createFETLimiter74()),
                    directory.getChildFile ("FETLimiter74-GUI.png"))
        && capture (std::unique_ptr<juce::AudioProcessor> (createLead85MKII()),
                    directory.getChildFile ("Lead85MKII-GUI.png")) ? 0 : 1;
}
