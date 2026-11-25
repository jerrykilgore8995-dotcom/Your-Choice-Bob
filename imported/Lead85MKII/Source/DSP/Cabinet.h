#pragma once

#include <JuceHeader.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <complex>
#include <Lead85IRData.h>

class Lead85Cabinet
{
public:
    juce::Result loadFile (const juce::File& file)
    {
        juce::MemoryBlock bytes;
        if (!file.existsAsFile() || file.getSize() > maxFileBytes || !file.loadFileAsData (bytes))
            return juce::Result::fail ("Choose a WAV cabinet IR under 2 MB.");

        return load (std::move (bytes), file.getFileName());
    }

    juce::Result load (juce::MemoryBlock bytes, const juce::String& name, int builtIn = -1)
    {
        if (bytes.getSize() == 0 || bytes.getSize() > maxFileBytes)
            return juce::Result::fail ("Choose a WAV cabinet IR under 2 MB.");

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (
            new juce::MemoryInputStream (bytes, false), true));
        if (reader == nullptr || reader->numChannels < 1 || reader->numChannels > 2
            || reader->sampleRate < 8000.0 || reader->sampleRate > 384000.0
            || reader->lengthInSamples < 1 || reader->lengthInSamples > reader->sampleRate)
            return juce::Result::fail ("The cabinet IR must be a mono or stereo WAV up to one second.");

        Impulse next;
        next.rate = reader->sampleRate;
        next.samples.setSize (static_cast<int> (reader->numChannels),
                               static_cast<int> (reader->lengthInSamples));
        if (!reader->read (&next.samples, 0, next.samples.getNumSamples(), 0, true, true))
            return juce::Result::fail ("The WAV could not be read.");

        // Gain-match in the guitar midrange while preserving stereo balance and IR phase.
        auto power = 0.0;
        for (auto frequency : { 250.0, 380.0, 570.0, 850.0, 1250.0, 1850.0, 2700.0 })
        {
            auto step = juce::MathConstants<double>::twoPi * frequency / next.rate;
            auto phase = std::polar (1.0, step);
            for (int ch = 0; ch < next.samples.getNumChannels(); ++ch)
            {
                std::complex<double> response {}, oscillator { 1.0, 0.0 };
                for (int i = 0; i < next.samples.getNumSamples(); ++i)
                {
                    response += static_cast<double> (next.samples.getSample (ch, i)) * oscillator;
                    oscillator *= phase;
                }
                power += std::norm (response);
            }
        }

        if (!std::isfinite (power) || power < 1.0e-12)
            return juce::Result::fail ("The WAV has no usable cabinet response.");

        next.samples.applyGain (static_cast<float> (juce::jlimit (
            0.01, 32.0, std::sqrt (7.0 * next.samples.getNumChannels() / power))));

        const juce::SpinLock::ScopedLockType lock (exchange);
        if (builtIn < 0)
            savedWav = std::move (bytes);
        else
            savedWav.reset();
        savedName = name;
        factoryIndex = builtIn;
        pending = std::move (next);
        return juce::Result::ok();
    }

    static const std::array<const char*, 4>& factoryNames()
    {
        static const std::array<const char*, 4> names {
            "V30 / SM57", "V30 / SM57 alternate", "Greenback / SM57", "Greenback / e606"
        };
        return names;
    }

    void useFactory (int index = 0)
    {
        const auto& names = factoryNames();
        index = juce::jlimit (0, static_cast<int> (names.size()) - 1, index);
        using namespace Lead85IRData;
        const std::array<const char*, 4> data {
            V30_SM57_wav, V30_SM57_Alt_wav, Greenback_SM57_wav, Greenback_e606_wav
        };
        const std::array<int, 4> sizes {
            V30_SM57_wavSize, V30_SM57_Alt_wavSize, Greenback_SM57_wavSize, Greenback_e606_wavSize
        };
        const auto slot = static_cast<size_t> (index);
        load (juce::MemoryBlock (data[slot], static_cast<size_t> (sizes[slot])), names[slot], index);
    }

    juce::String getName() const
    {
        const juce::SpinLock::ScopedLockType lock (exchange);
        return savedName.isEmpty() ? factoryNames().front() : savedName;
    }

    int getFactoryIndex() const
    {
        const juce::SpinLock::ScopedLockType lock (exchange);
        return factoryIndex;
    }

    void store (juce::ValueTree& tree) const
    {
        juce::MemoryBlock bytes;
        juce::String name;
        {
            const juce::SpinLock::ScopedLockType lock (exchange);
            bytes = savedWav;
            name = savedName;
            tree.setProperty ("cabFactory", juce::jmax (0, factoryIndex), nullptr);
        }
        if (!bytes.isEmpty())
        {
            tree.setProperty ("cabWav", bytes.toBase64Encoding(), nullptr);
            tree.setProperty ("cabName", name, nullptr);
        }
    }

    void restore (const juce::ValueTree& tree)
    {
        juce::MemoryBlock bytes;
        auto encoded = tree.getProperty ("cabWav").toString();
        if (encoded.isNotEmpty() && encoded.length() <= 3 * maxFileBytes
            && bytes.fromBase64Encoding (encoded)
            && load (std::move (bytes), tree.getProperty ("cabName", "User cabinet").toString()).wasOk())
            return;

        useFactory (static_cast<int> (tree.getProperty ("cabFactory", 0)));
    }

    void prepare (double rate, int size, int channels, bool enabled)
    {
        juce::MemoryBlock bytes;
        juce::String name;
        int selection = 0;
        {
            const juce::SpinLock::ScopedLockType lock (exchange);
            bytes = savedWav;
            name = savedName;
            selection = factoryIndex;
        }
        if (bytes.isEmpty())
            useFactory (selection);
        else
            load (std::move (bytes), name);
        submitPending();
        convolution.prepare ({ rate, static_cast<juce::uint32> (juce::jmax (1, size)),
                               static_cast<juce::uint32> (channels) });
        dry.setSize (channels, juce::jmax (1, size));
        wet.reset (rate, 0.010);
        wet.setCurrentAndTargetValue (enabled ? 1.0f : 0.0f);
        maxBlock = juce::jmax (1, size);
    }

    void reset() { convolution.reset(); }

    void process (juce::AudioBuffer<float>& buffer, bool enabled)
    {
        submitPending();
        wet.setTargetValue (enabled ? 1.0f : 0.0f);
        if (!enabled && !wet.isSmoothing())
            return;

        auto block = juce::dsp::AudioBlock<float> (buffer);
        for (size_t start = 0; start < block.getNumSamples(); start += static_cast<size_t> (maxBlock))
        {
            auto n = juce::jmin (static_cast<size_t> (maxBlock), block.getNumSamples() - start);
            auto part = block.getSubBlock (start, n);
            if (!wet.isSmoothing())
            {
                if (enabled)
                    convolution.process (juce::dsp::ProcessContextReplacing<float> (part));
                continue;
            }

            auto wetPart = juce::dsp::AudioBlock<float> (dry)
                .getSubsetChannelBlock (0, part.getNumChannels()).getSubBlock (0, n);
            wetPart.copyFrom (part);
            convolution.process (juce::dsp::ProcessContextReplacing<float> (wetPart));
            for (size_t i = 0; i < n; ++i)
            {
                const auto mix = wet.getNextValue();
                for (size_t ch = 0; ch < part.getNumChannels(); ++ch)
                {
                    auto* out = part.getChannelPointer (ch);
                    out[i] += mix * (wetPart.getChannelPointer (ch)[i] - out[i]);
                }
            }
        }
    }

private:
    static constexpr int maxFileBytes = 2 * 1024 * 1024;

    struct Impulse
    {
        juce::AudioBuffer<float> samples;
        double rate = 44100.0;
    };

    void submitPending()
    {
        const juce::SpinLock::ScopedTryLockType lock (exchange);
        if (!lock.isLocked() || pending.samples.getNumSamples() == 0)
            return;

        const bool stereo = pending.samples.getNumChannels() == 2;
        convolution.loadImpulseResponse (std::move (pending.samples), pending.rate,
            stereo ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    }

    mutable juce::SpinLock exchange;
    Impulse pending;
    juce::MemoryBlock savedWav;
    juce::String savedName;
    int factoryIndex = 0;
    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 256 } };
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> wet;
    int maxBlock = 1;
};
