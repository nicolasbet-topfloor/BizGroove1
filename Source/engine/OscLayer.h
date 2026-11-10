#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** The Oscillators section every synth has: two oscillators of their own,
    mixed with the synth's own sound before its filter. Each has a wave,
    octave, semitone, fine tune, pulse width and level; oscillator 2 can be
    synced to oscillator 1, or ring-modulated by it. Main is the level of the
    synth's own sound, so the two oscillators can replace it altogether.
    Shared between the UI and the voices; both levels start at 0, so a synth
    sounds as before until you turn them up. */
struct OscSettings
{
    enum Wave { sine = 0, triangle, saw, square, pulse, noise, numWaves };

    static juce::String waveName (int w)
    {
        static const char* names[numWaves] = { "Sine", "Triangle", "Saw", "Square", "Pulse", "Noise" };
        return names[juce::jlimit (0, numWaves - 1, w)];
    }

    struct Osc
    {
        std::atomic<int>   wave   { saw };
        std::atomic<int>   octave { 0 };      // -2..+2
        std::atomic<int>   semi   { 0 };      // -12..+12
        std::atomic<float> fine   { 0.0f };   // cents, -50..+50
        std::atomic<float> width  { 0.5f };   // pulse width, 0.05..0.95
        std::atomic<float> level  { 0.0f };   // 0..1
    };

    std::array<Osc, 2> osc;
    std::atomic<float> main { 1.0f };   // the synth's own sound, 0..1
    std::atomic<bool>  sync { false };  // oscillator 2 restarts with oscillator 1
    std::atomic<bool>  ring { false };  // oscillator 2 is multiplied by oscillator 1

    bool active() const { return osc[0].level.load() > 0.0005f || osc[1].level.load() > 0.0005f; }
};

/** One voice's two oscillators. Audio thread. */
class OscLayer
{
public:
    void reset()
    {
        phase = { 0.0, 0.25 };
        noiseState = 0x2545f491u;
    }

    /** The note's pitch (with bend and glide), every few samples. */
    void setPitch (double hz, double sampleRate, const OscSettings& s)
    {
        for (size_t i = 0; i < 2; ++i)
        {
            const auto& o = s.osc[i];
            const double semis = 12.0 * juce::jlimit (-2, 2, o.octave.load()) + juce::jlimit (-12, 12, o.semi.load())
                               + juce::jlimit (-50.0f, 50.0f, o.fine.load()) / 100.0;
            inc[i] = juce::jlimit (0.0, 0.45, hz * std::pow (2.0, semis / 12.0) / sampleRate);
        }
    }

    /** One sample of both oscillators, at their levels. */
    float next (const OscSettings& s)
    {
        const bool doSync = s.sync.load(), doRing = s.ring.load();
        const float l1 = juce::jlimit (0.0f, 1.0f, s.osc[0].level.load());
        const float l2 = juce::jlimit (0.0f, 1.0f, s.osc[1].level.load());

        const float a = shape (0, s.osc[0].wave.load(), s.osc[0].width.load());
        phase[0] += inc[0];
        bool wrapped = false;
        if (phase[0] >= 1.0)
        {
            phase[0] -= 1.0;
            wrapped = true;
        }

        float b = shape (1, s.osc[1].wave.load(), s.osc[1].width.load());
        phase[1] += inc[1];
        if (phase[1] >= 1.0)
            phase[1] -= 1.0;
        if (doSync && wrapped)
            phase[1] = phase[0] * (inc[1] / juce::jmax (1.0e-9, inc[0]));   // restart in step with oscillator 1
        if (doRing)
            b *= a * 1.4f;

        return 0.5f * (l1 * a + l2 * b);
    }

private:
    /** PolyBLEP: rounds off the jumps of saw and square waves, so high notes don't alias. */
    static double blep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    float shape (size_t i, int wave, float width)
    {
        const double p = phase[i], dt = inc[i];
        switch (juce::jlimit (0, OscSettings::numWaves - 1, wave))
        {
            case OscSettings::sine:     return (float) std::sin (juce::MathConstants<double>::twoPi * p);
            case OscSettings::triangle: return (float) (1.0 - 4.0 * std::abs (p - 0.5));
            case OscSettings::saw:      return (float) (2.0 * p - 1.0 - blep (p, dt));
            case OscSettings::square:
            case OscSettings::pulse:
            {
                const double w = wave == OscSettings::square ? 0.5 : juce::jlimit (0.05, 0.95, (double) width);
                double v = p < w ? 1.0 : -1.0;
                v += blep (p, dt);
                double q = p - w;
                if (q < 0.0) q += 1.0;
                v -= blep (q, dt);
                return (float) (0.9 * v);
            }
            default:
            {
                noiseState ^= noiseState << 13;
                noiseState ^= noiseState >> 17;
                noiseState ^= noiseState << 5;
                return (float) (std::int32_t) noiseState * (0.6f / 2147483648.0f);
            }
        }
    }

    std::array<double, 2> phase { { 0.0, 0.25 } };
    std::array<double, 2> inc { { 0.0, 0.0 } };
    std::uint32_t noiseState = 0x2545f491u;
};

/** The Oscillators settings as JSON, for projects and sounds. */
inline juce::var oscToVar (const OscSettings& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("main", s.main.load());
    o->setProperty ("sync", s.sync.load());
    o->setProperty ("ring", s.ring.load());
    juce::Array<juce::var> list;
    for (const auto& osc : s.osc)
    {
        auto* p = new juce::DynamicObject();
        p->setProperty ("wave", osc.wave.load());     p->setProperty ("octave", osc.octave.load());
        p->setProperty ("semi", osc.semi.load());     p->setProperty ("fine", osc.fine.load());
        p->setProperty ("width", osc.width.load());   p->setProperty ("level", osc.level.load());
        list.add (juce::var (p));
    }
    o->setProperty ("oscs", list);
    return juce::var (o);
}

/** Reads settings written by oscToVar(); a sound or project without them gets the defaults (levels at 0). */
inline void applyOsc (OscSettings& s, const juce::var& v)
{
    auto f = [] (const juce::var& x, const char* key, float fallback) { return x.hasProperty (key) ? (float) (double) x[key] : fallback; };
    auto i = [] (const juce::var& x, const char* key, int fallback)   { return x.hasProperty (key) ? (int) x[key] : fallback; };
    s.main.store (f (v, "main", 1.0f));
    s.sync.store (v.hasProperty ("sync") && (bool) v["sync"]);
    s.ring.store (v.hasProperty ("ring") && (bool) v["ring"]);
    const auto* list = v["oscs"].getArray();
    for (int n = 0; n < 2; ++n)
    {
        const auto p = list != nullptr && n < list->size() ? (*list)[n] : juce::var();
        auto& o = s.osc[(size_t) n];
        o.wave.store (juce::jlimit (0, OscSettings::numWaves - 1, i (p, "wave", OscSettings::saw)));
        o.octave.store (juce::jlimit (-2, 2, i (p, "octave", 0)));
        o.semi.store (juce::jlimit (-12, 12, i (p, "semi", 0)));
        o.fine.store (juce::jlimit (-50.0f, 50.0f, f (p, "fine", 0.0f)));
        o.width.store (juce::jlimit (0.05f, 0.95f, f (p, "width", 0.5f)));
        o.level.store (juce::jlimit (0.0f, 1.0f, f (p, "level", 0.0f)));
    }
}

} // namespace bizgroove
