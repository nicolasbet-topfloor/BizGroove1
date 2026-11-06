#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace bizgroove
{

/** Sixteen synthesised drums in General MIDI order (notes 36-51), the kick
    first. No samples: every drum is built from oscillators and noise.

    Per drum: pitch, length, tone, snap (attack click), drive (distortion),
    pan and volume. Kit-wide: choke (closed and pedal hi-hats cut the open
    hi-hat) and velocity (how much harder hits are louder).
*/
class DrumKit
{
public:
    static constexpr int numDrums = 16;

    struct DrumParams
    {
        std::atomic<float> pitch  { 0.0f };   // semitones, -12..+12
        std::atomic<float> length { 0.5f };   // 0..1; 0.5 = natural length
        std::atomic<float> tone   { 0.5f };   // 0..1, dull to bright
        std::atomic<float> snap   { 0.3f };   // 0..1, attack click
        std::atomic<float> drive  { 0.0f };   // 0..1, distortion
        std::atomic<float> pan    { 0.0f };   // -1 left .. +1 right
        std::atomic<float> volume { 0.8f };   // 0..1
    };

    DrumKit();

    std::array<DrumParams, numDrums> drums;
    std::atomic<bool>  choke    { true };
    std::atomic<float> velocity { 0.5f };   // 0 = every hit equal, 1 = fully velocity-sensitive

    static juce::String drumName (int drum);

    /** Which drum a MIDI note plays: notes 36-51 and every two octaves up or
        down from there; -1 for notes with no drum. */
    static int drumForNote (int note);

    // ---------------------------------------------------------- audio thread
    void prepare (double sampleRate);
    void trigger (int drum, float velocity01);

    /** Renders this block, triggering drums at the note-ons in `midi`. */
    void render (juce::AudioBuffer<float>& out, int startSample, int numSamples, const juce::MidiBuffer& midi);

    /** Sound left in any drum (for tests and meters). */
    bool isSounding() const;

private:
    enum class Model { kick, rim, snare, clap, tom, hat, cymbal };

    struct Spec
    {
        Model model;
        float baseHz;       // fundamental, or filter centre for noise drums
        float baseDecay;    // seconds to fade to silence at length 0.5
        bool ride = false;
    };

    struct Filter   // TPT state-variable filter, mono
    {
        float ic1 = 0.0f, ic2 = 0.0f, g = 0.0f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
        void set (float cutoff, float q, double sr);
        void process (float x, float& low, float& band, float& high);
        void reset() { ic1 = ic2 = 0.0f; }
    };

    struct Voice
    {
        bool active = false;
        float amp = 0.0f, ampCoef = 0.0f;
        float pitchEnv = 0.0f, pitchCoef = 0.0f;
        float click = 0.0f, clickCoef = 0.0f;
        float body = 0.0f, bodyCoef = 0.0f;   // snare body decays faster than its noise
        float velocityGain = 1.0f;
        double phase = 0.0;
        std::array<double, 6> metalPhase {};
        int sampleCount = 0;
        bool choking = false;
        Filter f1, f2;
    };

    void renderVoice (int drum, Voice& v, float* left, float* right, int numSamples);
    float noise();

    std::array<Spec, numDrums> specs;
    std::array<Voice, numDrums> voices;
    double sampleRate = 44100.0;
    std::uint32_t rng = 0x12345678u;
};

} // namespace bizgroove
