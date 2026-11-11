#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** What a track's vocoder listens to (the modulator), mixed from three
    sources: the sound card's input (a microphone), the Voice tracks (the
    speaking synth) and any one track. And how it hears it. Used by the
    Vocoder instrument and by the Vocoder effect on any track. Shared between
    the UI and the audio thread. */
struct VocoderSettings
{
    static constexpr int numBandChoices = 5;
    static int bandCount (int choice)
    {
        static constexpr int counts[numBandChoices] = { 8, 12, 16, 24, 32 };
        return counts[juce::jlimit (0, numBandChoices - 1, choice)];
    }

    std::atomic<float> micLevel    { 0.0f };   // 0..1: the sound card's input
    std::atomic<float> voiceLevel  { 1.0f };   // 0..1: every Voice track, mixed
    std::atomic<float> trackLevel  { 0.0f };   // 0..1: the track below
    std::atomic<int>   sourceTrack { 0 };      // 0..15
    std::atomic<int>   bands       { 2 };      // see bandCount: 16
    std::atomic<float> shift       { 0.5f };   // 0..1: the voice's formants down or up an octave (0.5: as they are)
    std::atomic<float> response    { 0.35f };  // 0..1: how quickly the bands follow the voice, from tight to smooth
    std::atomic<float> sibilance   { 0.3f };   // 0..1: the voice's own hiss (s, t, ch) let through, for clearer words

    bool listens() const { return micLevel.load() > 0.0005f || voiceLevel.load() > 0.0005f || trackLevel.load() > 0.0005f; }

    static float shiftSemitones (float v)   { return (juce::jlimit (0.0f, 1.0f, v) - 0.5f) * 24.0f; }
    static float releaseMs (float v)        { return 4.0f * std::pow (60.0f, juce::jlimit (0.0f, 1.0f, v)); }   // 4 .. 240 ms
};

/** The vocoder: the modulator (a voice) is split into bands, and how loud
    each band is shapes the same band of the carrier (a synth), so the synth
    speaks. Audio thread. */
class VocoderCore
{
public:
    static constexpr int maxBands = 32;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        shownBands = -1;
        reset();
    }

    void reset()
    {
        for (auto& b : analysis) b = {};
        for (auto& side : synthesis) for (auto& b : side) b = {};
        env.fill (0.0f);
        sibState = {};
        modLevel = 0.0f;
        levelGain = 1.0f;
        carrierLevel = 0.0f;
    }

    /** Vocodes the carrier (l, r) in place with the modulator. */
    void process (const float* modulator, float* l, float* r, int numSamples,
                  int numBands, float shiftSemitones, float releaseMs, float sibilance)
    {
        numBands = juce::jlimit (4, maxBands, numBands);
        if (numBands != shownBands || std::abs (shiftSemitones - shownShift) > 0.01f)
            configure (numBands, shiftSemitones);

        const float attack = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
        const float release = 1.0f - std::exp (-1.0f / (juce::jmax (1.0f, releaseMs) * 0.001f * (float) sr));
        const float makeup = 9.0f * std::sqrt ((float) numBands / 16.0f);
        const float sib = juce::jlimit (0.0f, 1.0f, sibilance) * 1.5f;

        const float levelCoef = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));
        for (int i = 0; i < numSamples; ++i)
        {
            // Levelling: a quiet microphone and a loud synth make the vocoder speak about as loud.
            const float raw = modulator != nullptr ? modulator[i] : 0.0f;
            modLevel += levelCoef * (raw * raw - modLevel);
            const float wanted = juce::jlimit (0.3f, 8.0f, 0.12f / std::sqrt (modLevel + 1.0e-9f));
            levelGain += 0.0005f * (wanted - levelGain);
            const float m = raw * levelGain;
            const float inL = l[i], inR = r[i];
            float outL = 0.0f, outR = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                const float a = std::abs (analysis[(size_t) b].process (m, coefA[(size_t) b]));
                float& e = env[(size_t) b];
                e += (a > e ? attack : release) * (a - e);
                outL += synthesis[0][(size_t) b].process (inL, coefS[(size_t) b]) * e;
                outR += synthesis[1][(size_t) b].process (inR, coefS[(size_t) b]) * e;
            }
            // The hiss of the voice itself: the high end of the modulator, so s and t come through.
            const float h1 = m - sibState.lp1;
            sibState.lp1 += sibCoef * h1;
            const float h2 = h1 - sibState.lp2;
            sibState.lp2 += sibCoef * h2;
            // Only while the carrier plays: no notes, no hiss.
            const float carrier = std::abs (inL) + std::abs (inR);
            carrierLevel += (carrier > carrierLevel ? attack : release) * (carrier - carrierLevel);
            const float hiss = h2 * sib * juce::jmin (1.0f, carrierLevel * 4.0f);

            l[i] = outL * makeup + hiss;
            r[i] = outR * makeup + hiss;
        }
    }

    /** How loud each band is right now (for tests and meters). */
    float bandLevel (int b) const { return env[(size_t) juce::jlimit (0, maxBands - 1, b)]; }
    float bandHz (int b) const    { return centre[(size_t) juce::jlimit (0, maxBands - 1, b)]; }

private:
    struct Coef { float b0 = 0, b2 = 0, a1 = 0, a2 = 0; };

    /** A band-pass: two of the same in a row, for a steeper band. */
    struct Band
    {
        float z1 = 0, z2 = 0, w1 = 0, w2 = 0;

        float process (float x, const Coef& c)
        {
            const float y = c.b0 * x + z1;
            z1 = -c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            const float v = c.b0 * y + w1;
            w1 = -c.a1 * v + w2;
            w2 = c.b2 * y - c.a2 * v;
            return v;
        }
    };

    static Coef bandPass (double hz, double q, double sampleRate)
    {
        const double w = juce::MathConstants<double>::twoPi * hz / sampleRate;
        const double alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        Coef c;
        c.b0 = (float) (alpha / a0);
        c.b2 = (float) (-alpha / a0);
        c.a1 = (float) (-2.0 * std::cos (w) / a0);
        c.a2 = (float) ((1.0 - alpha) / a0);
        return c;
    }

    void configure (int numBands, float shiftSemitones)
    {
        const double lo = 110.0, hi = juce::jmin (7500.0, 0.42 * sr);
        const double octaves = std::log2 (hi / lo);
        const double bw = octaves / numBands;                              // each band's width, in octaves
        const double q = 1.0 / (2.0 * std::sinh (std::log (2.0) / 2.0 * bw * 1.4));
        const double ratio = std::pow (2.0, (double) shiftSemitones / 12.0);
        for (int b = 0; b < numBands; ++b)
        {
            const double f = lo * std::pow (hi / lo, (double) b / (double) (numBands - 1));
            centre[(size_t) b] = (float) f;
            coefA[(size_t) b] = bandPass (f, q, sr);
            coefS[(size_t) b] = bandPass (juce::jlimit (40.0, 0.45 * sr, f * ratio), q, sr);
        }
        sibCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 5000.0 / sr));
        if (numBands != shownBands)
            reset();
        shownBands = numBands;
        shownShift = shiftSemitones;
    }

    double sr = 48000.0;
    int shownBands = -1;
    float shownShift = 0.0f, sibCoef = 0.5f;
    std::array<Coef, maxBands> coefA {}, coefS {};
    std::array<float, maxBands> centre {};
    std::array<Band, maxBands> analysis {};
    std::array<std::array<Band, maxBands>, 2> synthesis {};
    std::array<float, maxBands> env {};
    float modLevel = 0.0f, levelGain = 1.0f, carrierLevel = 0.0f;
    struct { float lp1 = 0.0f, lp2 = 0.0f; } sibState;
};

inline juce::var vocoderSettingsToVar (const VocoderSettings& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("mic", s.micLevel.load());          o->setProperty ("voice", s.voiceLevel.load());
    o->setProperty ("track", s.trackLevel.load());      o->setProperty ("source", s.sourceTrack.load());
    o->setProperty ("bands", s.bands.load());           o->setProperty ("shift", s.shift.load());
    o->setProperty ("response", s.response.load());     o->setProperty ("sibilance", s.sibilance.load());
    return juce::var (o);
}

inline void applyVocoderSettings (VocoderSettings& s, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    s.micLevel.store (f ("mic", 0.0f));        s.voiceLevel.store (f ("voice", 1.0f));
    s.trackLevel.store (f ("track", 0.0f));    s.sourceTrack.store (juce::jlimit (0, 15, i ("source", 0)));
    s.bands.store (juce::jlimit (0, VocoderSettings::numBandChoices - 1, i ("bands", 2)));
    s.shift.store (f ("shift", 0.5f));         s.response.store (f ("response", 0.35f));
    s.sibilance.store (f ("sibilance", 0.3f));
}

} // namespace bizgroove
