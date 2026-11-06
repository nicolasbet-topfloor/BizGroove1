#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>

namespace bizgroove
{

/** Four looping audio stems locked to the transport, built for live play
    from a controller such as the Kontrol F1.

    Each stem is a whole audio file played as a loop. Its length in bars is
    worked out from the file and the project tempo, and it is repitched so the
    loop always fits that many bars exactly, whatever the tempo.

    The playback position is a pure function of the transport position, so
    the stems can never drift from the sequencer. Restart and beat roll move
    a stem's offset or loop it briefly, then it rejoins the grid ("slip").
*/
class StemDeck
{
public:
    static constexpr int numStems = 4;

    struct Stem
    {
        // ---- controls, written by UI or controller, read by audio thread
        std::atomic<float> volume { 0.8f };   // 0..1
        std::atomic<float> filter { 0.0f };   // -1 = low-pass closed, 0 = off, +1 = high-pass closed
        std::atomic<bool>  mute   { false };
        std::atomic<bool>  solo   { false };
        std::atomic<int>   roll   { 0 };      // 0 = off, else division of a beat (1 = one beat, 4 = 1/16)
        std::atomic<bool>  restartRequested { false };

        // ---- read-outs for the UI
        std::atomic<float> playFraction { 0.0f };
        std::atomic<float> level { 0.0f };
    };

    StemDeck();

    // ---------------------------------------------------------- message thread
    /** Installs audio in a slot. The deck takes a copy; safe while playing. */
    void setStemAudio (int index, const juce::AudioBuffer<float>& audio, double fileSampleRate,
                       const juce::String& name, double projectBpm);
    void clearStem (int index);
    bool hasAudio (int index) const;
    juce::String getStemName (int index) const;
    int getStemBars (int index) const;

    /** Bars a file of this length occupies at the given tempo (at least 1). */
    static int barsForLength (double seconds, double bpm);

    Stem& stem (int index) { return stems[(size_t) index]; }

    // ---------------------------------------------------------- audio thread
    void prepare (double deviceSampleRate, int maxBlockSize);

    /** Mixes the stems into `out`. `transportSample` is the transport position
        at the block start, counted in device samples since play was pressed. */
    void process (juce::AudioBuffer<float>& out, int startSample, int numSamples,
                  juce::int64 transportSample, bool playing, double bpm);

private:
    struct Loaded
    {
        juce::AudioBuffer<float> audio;   // always stereo
        double sampleRate = 44100.0;
        juce::String name;
        int bars = 1;
    };

    struct Voice
    {
        std::unique_ptr<Loaded> loaded;
        juce::int64 offset = 0;           // transport sample where the loop starts
        juce::int64 pendingRestartAt = -1;
        int activeRoll = 0;
        double rollStartPos = 0.0;        // in file samples
        juce::int64 rollStartedAt = 0;    // transport sample
        juce::dsp::StateVariableTPTFilter<float> lowPass, highPass;
        juce::SmoothedValue<float> gain;
        float lastFilter = 0.0f;
        bool snapGain = true;             // jump straight to the fader level when play starts
    };

    void renderVoice (Voice&, Stem&, juce::AudioBuffer<float>& out, int startSample, int numSamples,
                      juce::int64 transportSample, double bpm, bool audible);

    std::array<Stem, numStems> stems;
    std::array<Voice, numStems> voices;
    mutable juce::SpinLock lock;          // guards Voice::loaded swaps only
    juce::AudioBuffer<float> scratch;
    double deviceRate = 44100.0;
};

} // namespace bizgroove
