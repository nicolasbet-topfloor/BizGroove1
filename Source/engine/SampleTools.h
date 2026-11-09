#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

namespace bizgroove
{

/** Sample editing done on the message thread: encoding, trimming, finding
    the hits for Threshold chop, and the destructive processes of the editor. */
namespace SampleTools
{
    /** A 24-bit WAV file of the audio, as the sampler keeps it for saving. */
    inline juce::MemoryBlock encodeWav (const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        juce::MemoryBlock block;
        {
            auto* stream = new juce::MemoryOutputStream (block, false);
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wav.createWriterFor (stream, sampleRate, (unsigned int) juce::jmax (1, audio.getNumChannels()), 24, {}, 0));
            if (writer == nullptr)
            {
                delete stream;
                return {};
            }
            writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }
        return block;
    }

    /** Loudest sample at a position, across channels. */
    inline float levelAt (const juce::AudioBuffer<float>& a, int i)
    {
        float v = 0.0f;
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            v = juce::jmax (v, std::abs (a.getSample (ch, i)));
        return v;
    }

    /** Cuts the silence before the first sound and after the last, keeping
        10 ms before the start. Returns false if everything is silent. */
    inline bool trimSilence (juce::AudioBuffer<float>& a, double sampleRate, float thresholdDb = -50.0f)
    {
        const float threshold = juce::Decibels::decibelsToGain (thresholdDb);
        const int n = a.getNumSamples();
        int first = -1, last = -1;
        for (int i = 0; i < n; ++i)
            if (levelAt (a, i) > threshold) { first = i; break; }
        if (first < 0)
            return false;
        for (int i = n - 1; i >= first; --i)
            if (levelAt (a, i) > threshold) { last = i; break; }

        first = juce::jmax (0, first - (int) (0.010 * sampleRate));
        last = juce::jmin (n - 1, last + (int) (0.050 * sampleRate));
        const int length = last - first + 1;

        juce::AudioBuffer<float> trimmed (a.getNumChannels(), length);
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            trimmed.copyFrom (ch, 0, a, ch, first, length);
        a = std::move (trimmed);
        return true;
    }

    /** Where the hits are, for Threshold chop: positions (0..1 of the whole
        sample) where the level jumps above sensitivity (0..1, higher finds
        more hits), at most maxHits, at least 50 ms apart. */
    inline std::vector<float> findHits (const juce::AudioBuffer<float>& a, double sampleRate,
                                        float sensitivity, int maxHits)
    {
        std::vector<float> hits;
        const int n = a.getNumSamples();
        if (n < 2)
            return hits;

        const int window = juce::jmax (16, (int) (0.005 * sampleRate));   // 5 ms
        const int minGap = (int) (0.050 * sampleRate);
        const float jump = juce::jmap (juce::jlimit (0.0f, 1.0f, sensitivity), 4.0f, 1.3f);
        const float minLevel = juce::Decibels::decibelsToGain (juce::jmap (sensitivity, -30.0f, -55.0f));

        float previous = 0.0f;
        int lastHit = -minGap;
        for (int start = 0; start + window <= n && (int) hits.size() < maxHits; start += window)
        {
            float level = 0.0f;
            for (int i = start; i < start + window; ++i)
                level = juce::jmax (level, levelAt (a, i));

            if (level > minLevel && level > previous * jump && start - lastHit >= minGap)
            {
                hits.push_back ((float) start / (float) (n - 1));
                lastHit = start;
            }
            previous = juce::jmax (level, previous * 0.7f);   // falls back so the next hit can be seen
        }

        if (hits.empty() || hits.front() > 0.0f)
            hits.insert (hits.begin(), 0.0f);   // the first slice always starts at the beginning
        if ((int) hits.size() > maxHits)
            hits.resize ((size_t) maxHits);
        return hits;
    }

    // ---------------------------------------------------------- processes on a range [from, to)
    inline void normalise (juce::AudioBuffer<float>& a, int from, int to)
    {
        const float peak = a.getMagnitude (from, to - from);
        if (peak > 1.0e-6f)
            a.applyGain (from, to - from, 0.98f / peak);
    }

    inline void reverse (juce::AudioBuffer<float>& a, int from, int to)
    {
        a.reverse (from, to - from);
    }

    inline void fadeIn (juce::AudioBuffer<float>& a, int from, int to)
    {
        a.applyGainRamp (from, to - from, 0.0f, 1.0f);
    }

    inline void fadeOut (juce::AudioBuffer<float>& a, int from, int to)
    {
        a.applyGainRamp (from, to - from, 1.0f, 0.0f);
    }

    inline void silence (juce::AudioBuffer<float>& a, int from, int to)
    {
        a.clear (from, to - from);
    }

    /** Keeps only [from, to). */
    inline void crop (juce::AudioBuffer<float>& a, int from, int to)
    {
        juce::AudioBuffer<float> kept (a.getNumChannels(), to - from);
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            kept.copyFrom (ch, 0, a, ch, from, to - from);
        a = std::move (kept);
    }
}

} // namespace bizgroove
