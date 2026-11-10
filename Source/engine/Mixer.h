#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "SendEffects.h"
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** A biquad filter with RBJ cookbook coefficients. Coefficients are computed
    in place, so updating them on the audio thread never allocates. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0; }

    void setPeak (double sr, double hz, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * hz / sr;
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    void setLowShelf (double sr, double hz, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * hz / sr;
        const double c = std::cos (w), s = std::sin (w), alpha = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }

    void setHighShelf (double sr, double hz, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * hz / sr;
        const double c = std::cos (w), s = std::sin (w), alpha = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }

private:
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
};

/** Mixer settings of one track, shared with the UI and controllers. */
struct ChannelSettings
{
    std::atomic<float> pan       { 0.0f };   // -1 left .. +1 right
    std::atomic<float> eqLow     { 0.0f };   // dB, -12..+12, shelf at 120 Hz
    std::atomic<float> eqMid     { 0.0f };   // dB, -12..+12, peak at 1 kHz
    std::atomic<float> eqHigh    { 0.0f };   // dB, -12..+12, shelf at 8 kHz
    std::atomic<float> reverb    { 0.0f };   // send amount 0..1
    std::atomic<float> delay     { 0.0f };   // send amount 0..1
    std::atomic<bool>  solo      { false };
};

/** The DSP of one channel strip: EQ, pan and the two sends. Audio thread. */
class ChannelStrip
{
public:
    void prepare (double sampleRate);

    /** Processes `buffer` in place (EQ and pan, after volume) and adds the
        sends into the two bus buffers. */
    void process (juce::AudioBuffer<float>& buffer, int numSamples, const ChannelSettings& settings,
                  juce::AudioBuffer<float>& reverbBus, juce::AudioBuffer<float>& delayBus);

private:
    double sr = 44100.0;
    std::array<Biquad, 2> low, mid, high;   // per channel
    float lastLow = 99, lastMid = 99, lastHigh = 99;
    juce::SmoothedValue<float> panL, panR, sendR, sendD;
};

/** Master effects every track can send to: a reverb and a tempo-synced
    stereo delay. */
class MasterEffects
{
public:
    enum Division { sixteenth = 0, eighth, dottedEighth, quarter };

    enum ReverbType { roomHall = 0, plate, spring, numReverbTypes };
    enum DelayType { digital = 0, tape, numDelayTypes };

    std::atomic<float> reverbSize     { 0.5f };   // 0..1, small room to large hall (spring: how long it rings)
    std::atomic<int>   delayDivision  { dottedEighth };
    std::atomic<float> delayFeedback  { 0.35f };  // 0..0.9 (tape echo: at the top it self-oscillates)
    std::atomic<int>   reverbType     { roomHall };
    std::atomic<float> reverbTone     { 0.5f };   // 0..1, dark to bright
    std::atomic<float> springDrip     { 0.5f };   // 0..1, how springy the spring's chirp is
    std::atomic<int>   delayType      { digital };
    std::atomic<int>   tapeHeads      { 6 };      // see TapeEcho::modeName
    std::atomic<float> tapeWow        { 0.4f };   // 0..1, wow and flutter
    std::atomic<float> tapeTone       { 0.5f };   // 0..1, dark to bright
    std::atomic<float> tapeWear       { 0.3f };   // 0..1, new tape to worn

    static juce::String divisionName (int division);
    static juce::String reverbTypeName (int type);
    static juce::String delayTypeName (int type);

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** Runs the effects on the bus buffers and adds their output to `out`. */
    void process (juce::AudioBuffer<float>& reverbBus, juce::AudioBuffer<float>& delayBus,
                  juce::AudioBuffer<float>& out, int startSample, int numSamples, double bpm);

private:
    juce::Reverb reverb;
    float lastSize = -1.0f, lastTone = -1.0f;
    int lastReverbType = -1, lastDelayType = -1;
    juce::AudioBuffer<float> delayLine;
    int writePos = 0;
    double sr = 44100.0;
    juce::SmoothedValue<float> delayTime;   // in samples
    TapeEcho tapeEcho;
    SpringReverb springReverb;
    juce::AudioBuffer<float> wet;   // the spring's output for one block
};

} // namespace bizgroove
