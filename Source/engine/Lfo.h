#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** One of a track's two LFOs: a slow wave that moves any knob of the track
    up and down around where you set it. Shared between the UI and the
    audio thread. The destination is a ControlTargets id; the audio thread
    uses its index in ControlTargets::all(), found on the message thread. */
struct LfoSettings
{
    enum Shape { sine = 0, triangle, sawUp, sawDown, square, stepped, smooth, numShapes };

    static juce::String shapeName (int s)
    {
        static const char* names[numShapes] = { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Random steps", "Smooth random" };
        return names[juce::jlimit (0, numShapes - 1, s)];
    }

    // Rate: the left half of the knob is free (0.05 to 20 Hz), the right half in time with the tempo.
    static constexpr int numSyncedRates = 10;
    static bool synced (float v)         { return v >= 0.5f; }
    static double freeHz (float v)       { return 0.05 * std::pow (400.0, juce::jlimit (0.0, 1.0, (double) v * 2.0)); }
    static int syncedIndex (float v)     { return juce::jlimit (0, numSyncedRates - 1, (int) ((v - 0.5f) * 2.0f * ((float) numSyncedRates - 0.001f))); }
    static double syncedBeats (float v)
    {
        static constexpr double beats[numSyncedRates] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return beats[syncedIndex (v)];
    }
    static juce::String rateName (float v)
    {
        static const char* names[numSyncedRates] = { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/8 triplet", "1/16", "1/16 triplet", "1/32" };
        if (synced (v)) return juce::String (names[syncedIndex (v)]) + " (in time)";
        const double hz = freeHz (v);
        return juce::String (hz, hz < 1.0 ? 2 : 1) + " Hz";
    }

    std::atomic<int>   shape     { sine };
    std::atomic<float> rate      { 0.35f };   // the knob, 0..1 (see above)
    std::atomic<float> depth     { 0.0f };    // -1..1: how far, and which way, it moves the knob
    std::atomic<float> fade      { 0.0f };    // seconds: grows in after each note
    std::atomic<bool>  retrigger { false };   // starts its wave again on each note
    std::atomic<int>   destIndex { -1 };      // index in ControlTargets::all(), -1 none, -2 to be found from destId

    juce::String destId;                      // message thread: what it moves, by id (saved in projects and sounds)

    // Read-outs for the window's picture (audio thread writes).
    std::atomic<float> shownPhase { 0.0f }, shownValue { 0.0f };

    /** Message thread: what it moves ("" for nothing). The audio thread picks it up once it is found. */
    void setDestination (const juce::String& id)
    {
        destId = id;
        destIndex.store (id.isEmpty() ? -1 : -2);
    }

    bool active() const { return destIndex.load() >= 0 && std::abs (depth.load()) > 0.0005f; }
};

/** The audio thread's side of one LFO: where its wave is. */
struct LfoRunner
{
    double phase = 0.0;
    double sinceNote = 1.0e9;      // seconds since the last note (for the fade in)
    float held = 0.0f, from = 0.0f, to = 0.0f;   // the random shapes
    juce::Random random { 0x1f0 };

    // The knob being moved: where the person set it, and what this LFO last wrote.
    int dest = -1;
    float base = 0.0f, lastWritten = -1.0f;

    /** Moves the wave on by the given seconds; beatPos is the song's position in beats when it plays in time. */
    float advance (const LfoSettings& s, double seconds, bool playing, double beatPos, double bpm)
    {
        const float r = s.rate.load();
        double newPhase;
        if (LfoSettings::synced (r) && playing && ! s.retrigger.load())
            newPhase = std::fmod (beatPos / LfoSettings::syncedBeats (r), 1.0);   // locked to the song
        else
        {
            const double hz = LfoSettings::synced (r) ? bpm / 60.0 / LfoSettings::syncedBeats (r) : LfoSettings::freeHz (r);
            newPhase = phase + hz * seconds;
            newPhase -= std::floor (newPhase);
        }
        if (newPhase < phase)   // a new cycle: the random shapes take their next value
        {
            from = to;
            to = random.nextFloat() * 2.0f - 1.0f;
            held = to;
        }
        phase = juce::jlimit (0.0, 0.999999, newPhase);
        sinceNote += seconds;
        return value (s);
    }

    void noteStarted (const LfoSettings& s)
    {
        sinceNote = 0.0;
        if (s.retrigger.load())
            phase = 0.0;
    }

    /** The wave now, -1..1, with the fade in. */
    float value (const LfoSettings& s) const
    {
        const double p = phase;
        float v;
        switch (juce::jlimit (0, LfoSettings::numShapes - 1, s.shape.load()))
        {
            case LfoSettings::triangle: v = (float) (1.0 - 4.0 * std::abs (p - 0.5)); break;
            case LfoSettings::sawUp:    v = (float) (2.0 * p - 1.0); break;
            case LfoSettings::sawDown:  v = (float) (1.0 - 2.0 * p); break;
            case LfoSettings::square:   v = p < 0.5 ? 1.0f : -1.0f; break;
            case LfoSettings::stepped:  v = held; break;
            case LfoSettings::smooth:   v = from + (to - from) * (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * p)); break;
            default:                    v = (float) std::sin (juce::MathConstants<double>::twoPi * p); break;
        }
        const float fade = juce::jmax (0.0f, s.fade.load());
        if (fade > 0.001f)
            v *= (float) juce::jlimit (0.0, 1.0, sinceNote / fade);
        return v;
    }

    /** The wave's shape on its own, for pictures: phase 0..1 to -1..1 (random shapes drawn as examples). */
    static float shapeAt (int shape, double p)
    {
        switch (juce::jlimit (0, LfoSettings::numShapes - 1, shape))
        {
            case LfoSettings::triangle: return (float) (1.0 - 4.0 * std::abs (p - 0.5));
            case LfoSettings::sawUp:    return (float) (2.0 * p - 1.0);
            case LfoSettings::sawDown:  return (float) (1.0 - 2.0 * p);
            case LfoSettings::square:   return p < 0.5 ? 1.0f : -1.0f;
            case LfoSettings::stepped:
            {
                static const float steps[8] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, 0.5f, -0.6f, -0.1f };
                return steps[juce::jlimit (0, 7, (int) (p * 8.0))];
            }
            case LfoSettings::smooth:
            {
                static const float pts[9] = { 0.0f, 0.7f, -0.4f, 0.5f, -0.9f, 0.3f, 0.8f, -0.5f, 0.0f };
                const double x = p * 8.0;
                const int i = juce::jlimit (0, 7, (int) x);
                const float f = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * (x - i)));
                return pts[i] + (pts[i + 1] - pts[i]) * f;
            }
            default: return (float) std::sin (juce::MathConstants<double>::twoPi * p);
        }
    }
};

inline juce::var lfosToVar (const std::array<LfoSettings, 2>& lfos)
{
    juce::Array<juce::var> list;
    for (const auto& l : lfos)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("shape", l.shape.load());     o->setProperty ("rate", l.rate.load());
        o->setProperty ("depth", l.depth.load());     o->setProperty ("fade", l.fade.load());
        o->setProperty ("retrigger", l.retrigger.load());
        o->setProperty ("dest", l.destId);
        list.add (juce::var (o));
    }
    return list;
}

/** Message thread. Anything missing: the LFO is off. */
inline void applyLfos (std::array<LfoSettings, 2>& lfos, const juce::var& v)
{
    const auto* list = v.getArray();
    for (int n = 0; n < 2; ++n)
    {
        const auto p = list != nullptr && n < list->size() ? (*list)[n] : juce::var();
        auto f = [&p] (const char* key, float fallback) { return p.hasProperty (key) ? (float) (double) p[key] : fallback; };
        auto& l = lfos[(size_t) n];
        l.shape.store (juce::jlimit (0, LfoSettings::numShapes - 1, p.hasProperty ("shape") ? (int) p["shape"] : 0));
        l.rate.store (juce::jlimit (0.0f, 1.0f, f ("rate", 0.35f)));
        l.depth.store (juce::jlimit (-1.0f, 1.0f, f ("depth", 0.0f)));
        l.fade.store (juce::jlimit (0.0f, 8.0f, f ("fade", 0.0f)));
        l.retrigger.store (p.hasProperty ("retrigger") && (bool) p["retrigger"]);
        l.setDestination (p.hasProperty ("dest") ? p["dest"].toString() : juce::String());
    }
}

} // namespace bizgroove
