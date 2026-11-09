#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>

namespace bizgroove
{

/** Plays one loaded recording across the keys and pads.

    Without slices, the sample plays at its own pitch on C4 (MIDI note 60)
    and higher or lower on other keys; the pads play it at its own pitch.
    With slices, each key from C4 upwards and each pad plays its own piece:
    either the chosen part cut into 2, 4, 8 or 16 equal pieces, or the
    slices marked in the sample editor (Chop), which take priority.
*/
class Sampler
{
public:
    static constexpr int rootNote = 60;
    static constexpr int padBase = 36;      // the Launchkey pads in Drum mode: notes 36-51 on channel 10
    static constexpr int numVoices = 8;
    static constexpr int maxSlices = 16;

    struct Params
    {
        std::atomic<float> start      { 0.0f };    // 0..1 of the sample
        std::atomic<float> length     { 1.0f };    // 0..1 of what remains after Start
        std::atomic<float> pitch      { 0.0f };    // semitones, -24..+24
        std::atomic<float> brightness { 1.0f };    // 0..1, 1 = unfiltered
        std::atomic<float> attack     { 0.002f };  // seconds
        std::atomic<float> release    { 0.25f };   // seconds
        std::atomic<float> sustain    { 1.0f };    // 0..1
        std::atomic<bool>  loop       { false };
        std::atomic<bool>  reverse    { false };
        std::atomic<int>   slices     { 1 };       // 1 (off), 2, 4, 8, 16
        std::atomic<float> loopStart  { 0.0f };    // 0..1 of the sample: where a loop returns to (no slices)

        // Slices marked in the editor: the start of each, 0..1 of the sample, in order.
        std::atomic<int> sliceCount { 0 };         // 0 = none
        std::array<std::atomic<float>, maxSlices> slicePoints {};
    };

    Params params;

    // ---------------------------------------------------------- message thread
    void setSample (const juce::AudioBuffer<float>& audio, double fileSampleRate, const juce::String& name);
    void clearSample();
    bool hasSample() const;
    juce::String getSampleName() const;
    double getSampleSeconds() const;

    // ---------------------------------------------------------- audio thread
    void prepare (double sampleRate, int maxBlock);
    void render (juce::AudioBuffer<float>& out, int startSample, int numSamples, const juce::MidiBuffer& midi);

private:
    struct Loaded
    {
        juce::AudioBuffer<float> audio;   // stereo
        double sampleRate = 44100.0;
        juce::String name;
    };

    struct Voice
    {
        bool active = false;
        int note = -1;
        double pos = 0.0;          // in file samples
        double regionStart = 0.0, regionEnd = 0.0, loopStart = 0.0;
        double step = 1.0;         // file samples per output sample (negative when reversed)
        float velocity = 1.0f;
        juce::ADSR env;
        juce::dsp::StateVariableTPTFilter<float> filter;
    };

    void startVoice (int note, float velocity, bool fromPad);
    void stopVoice (int note);
    void renderVoices (juce::AudioBuffer<float>& out, int start, int num);

    std::unique_ptr<Loaded> loaded;
    mutable juce::SpinLock lock;
    std::array<Voice, numVoices> voices;
    double sr = 44100.0;
};

} // namespace bizgroove
