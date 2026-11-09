#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace bizgroove
{

/** Records audio for the sampler: from the sound card's input (a microphone
    or line in), or from BizGroove's own output (resampling).

    The message thread starts and stops it; the audio thread only copies
    samples into a buffer that was allocated beforehand, so recording never
    allocates on the audio thread. A recording stops by itself when the
    buffer is full.
*/
class SampleRecorder
{
public:
    enum Source { input = 0, output = 1 };

    static constexpr double maxSeconds = 120.0;

    std::atomic<int>  source { input };
    std::atomic<bool> monoInput { false };          // the input is one channel: copy it to both sides
    std::atomic<bool> toggleRequested { false };    // set by controllers and buttons; the message thread acts on it
    std::atomic<double> deviceRate { 44100.0 };     // set by the audio callback's prepare

    // ---------------------------------------------------------- message thread
    void start()
    {
        stopAndWait();
        rate = deviceRate.load();
        buffer.setSize (2, (int) (rate * maxSeconds), false, true, false);
        written.store (0);
        peak.store (0.0f);
        recording.store (true);
    }

    /** Stops and copies what was recorded into out. */
    void stop (juce::AudioBuffer<float>& out, double& sampleRate)
    {
        stopAndWait();
        const int n = written.load();
        out.setSize (2, juce::jmax (0, n));
        for (int ch = 0; ch < 2 && n > 0; ++ch)
            out.copyFrom (ch, 0, buffer, ch, 0, n);
        sampleRate = rate;
        buffer.setSize (0, 0);
        written.store (0);
    }

    bool isRecording() const   { return recording.load(); }
    bool hasStoppedItself()    { return autoStopped.exchange (false); }
    double getSeconds() const  { return rate > 0.0 ? written.load() / rate : 0.0; }
    float getPeak() const      { return peak.load(); }

    // ---------------------------------------------------------- audio thread
    /** Copies a block, if recording from fromSource. */
    void write (const juce::AudioBuffer<float>& src, int start, int num, int fromSource)
    {
        if (! recording.load() || source.load() != fromSource || src.getNumChannels() == 0)
            return;

        busy.store (true);
        if (recording.load())
        {
            const int at = written.load();
            const int n = juce::jmin (num, buffer.getNumSamples() - at);
            if (n > 0)
            {
                const bool mono = src.getNumChannels() < 2 || (fromSource == input && monoInput.load());
                buffer.copyFrom (0, at, src, 0, start, n);
                buffer.copyFrom (1, at, src, mono ? 0 : 1, start, n);
                peak.store (juce::jmax (peak.load() * 0.9f, src.getMagnitude (start, n)));
                written.store (at + n);
            }
            if (at + n >= buffer.getNumSamples())
            {
                recording.store (false);
                autoStopped.store (true);
            }
        }
        busy.store (false);
    }

private:
    void stopAndWait()
    {
        recording.store (false);
        while (busy.load())
            juce::Thread::yield();
    }

    juce::AudioBuffer<float> buffer;
    double rate = 44100.0;
    std::atomic<int> written { 0 };
    std::atomic<bool> recording { false }, busy { false }, autoStopped { false };
    std::atomic<float> peak { 0.0f };
};

} // namespace bizgroove
