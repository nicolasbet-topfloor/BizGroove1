#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** The arpeggiator's settings, shared with the UI and controllers. */
struct ArpSettings
{
    enum Mode { up = 0, down, upDown, downUp, random, played, chord, converge, diverge, numModes };
    static constexpr int numRates = 6;

    std::atomic<bool>  on      { false };
    std::atomic<int>   mode    { up };
    std::atomic<int>   rate    { 3 };      // see rateName: 1/16
    std::atomic<int>   octaves { 1 };      // 1..4
    std::atomic<float> gate    { 0.5f };   // 0.05..1: how much of each step a note lasts
    std::atomic<float> swing   { 0.0f };   // 0..1: every second note later, up to half a step

    static juce::String modeName (int m)
    {
        static const char* names[numModes] = { "Up", "Down", "Up and down", "Down and up", "Random",
                                               "As played", "Chord", "Converge", "Diverge" };
        return names[juce::jlimit (0, numModes - 1, m)];
    }

    static juce::String rateName (int r)
    {
        static const char* names[numRates] = { "1/4", "1/8", "1/8 triplet", "1/16", "1/16 triplet", "1/32" };
        return names[juce::jlimit (0, numRates - 1, r)];
    }

    static double rateBeats (int r)
    {
        static constexpr double beats[numRates] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return beats[juce::jlimit (0, numRates - 1, r)];
    }
};

/** Audio thread: turns the notes held down into a pattern, one note per step
    (or the whole chord, in Chord mode), in time with the song while it plays,
    and with its own clock while stopped. */
class Arpeggiator
{
public:
    /** Takes the held notes from in (note-ons and offs) and writes the pattern to out.
        songSamples: where the song is, in samples since play (only used when synced). */
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, double sampleRate,
                  double bpm, bool synced, juce::int64 songSamples, const ArpSettings& s)
    {
        const double stepLength = juce::jmax (16.0, sampleRate * 60.0 / juce::jlimit (20.0, 300.0, bpm) * ArpSettings::rateBeats (s.rate.load()));
        const double swingDelay = juce::jlimit (0.0f, 1.0f, s.swing.load()) * 0.5 * stepLength;
        const double gateLength = juce::jlimit (0.05f, 1.0f, s.gate.load()) * stepLength;

        auto it = in.begin();
        for (int i = 0; i < numSamples; ++i)
        {
            // The keys first, so a note pressed on a step's own sample plays at once.
            while (it != in.end() && (*it).samplePosition <= i)
            {
                const auto m = (*it).getMessage();
                if (m.isNoteOn() && m.getVelocity() > 0)
                {
                    const bool wasEmpty = numHeld == 0;
                    press (m.getNoteNumber(), m.getVelocity());
                    if (wasEmpty && ! synced)
                    {
                        untilStep = 0.0;   // free-running: start at once
                        stepCount = 0;
                    }
                    if (wasEmpty)
                        index = 0;
                }
                else if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
                    letGo (m.getNoteNumber());
                else if (m.isAllNotesOff() || m.isAllSoundOff())
                    numHeld = 0;
                else
                    out.addEvent (m, i);   // pitch bend, controllers: straight through
                ++it;
            }

            // When the next step is due.
            bool stepNow;
            if (synced)
            {
                const double pos = (double) (songSamples + i);
                const double phase = std::fmod (pos, stepLength);
                stepNow = phase < 1.0;
                if (stepNow)
                    stepCount = (juce::int64) std::floor (pos / stepLength + 0.5);
            }
            else
            {
                stepNow = untilStep <= 0.0;
                if (stepNow)
                {
                    untilStep += stepLength;
                    ++stepCount;
                }
                untilStep -= 1.0;
            }

            if (stepNow && numHeld > 0)
            {
                pendingDelay = (stepCount % 2 == 1) ? swingDelay : 0.0;
                stepPending = true;
            }

            if (stepPending)
            {
                if (pendingDelay <= 0.0)
                {
                    stepPending = false;
                    releaseSounding (out, i);
                    playStep (out, i, s);
                    offIn = gateLength - 1.0;
                    offPending = true;
                }
                else pendingDelay -= 1.0;
            }

            if (offPending && offIn <= 0.0)
            {
                releaseSounding (out, i);
                offPending = false;
            }
            else if (offPending)
                offIn -= 1.0;
        }
        while (it != in.end())
        {
            out.addEvent ((*it).getMessage(), numSamples - 1);
            ++it;
        }

        if (numHeld == 0 && numSounding > 0 && ! offPending)
            releaseSounding (out, numSamples - 1);
    }

    /** All notes go: when the arpeggiator is switched off, or the track changes. */
    void reset (juce::MidiBuffer& out, int position)
    {
        releaseSounding (out, position);
        numHeld = 0;
        offPending = stepPending = false;
        untilStep = 0.0;
    }

    int heldCount() const { return numHeld; }

private:
    void press (int note, int velocity)
    {
        letGo (note);
        if (numHeld < (int) held.size())
        {
            held[(size_t) numHeld] = note;
            velocities[(size_t) numHeld] = velocity;
            ++numHeld;
        }
        lastVelocity = velocity;
    }

    void letGo (int note)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < numHeld - 1; ++j)
                {
                    held[(size_t) j] = held[(size_t) j + 1];
                    velocities[(size_t) j] = velocities[(size_t) j + 1];
                }
                --numHeld;
                return;
            }
    }

    void releaseSounding (juce::MidiBuffer& out, int position)
    {
        for (int i = 0; i < numSounding; ++i)
            out.addEvent (juce::MidiMessage::noteOff (1, sounding[(size_t) i]), position);
        numSounding = 0;
    }

    void sound (juce::MidiBuffer& out, int position, int note, int velocity)
    {
        if (note < 0 || note > 127 || numSounding >= (int) sounding.size())
            return;
        out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, velocity)), position);
        sounding[(size_t) numSounding++] = note;
    }

    void playStep (juce::MidiBuffer& out, int position, const ArpSettings& s)
    {
        const int octaves = juce::jlimit (1, 4, s.octaves.load());
        const int mode = juce::jlimit (0, ArpSettings::numModes - 1, s.mode.load());

        // The held notes, low to high (or as played), over the octaves.
        std::array<int, 16> base {};
        const int n = numHeld;
        for (int i = 0; i < n; ++i)
            base[(size_t) i] = held[(size_t) i];
        if (mode != ArpSettings::played)
            std::sort (base.begin(), base.begin() + n);

        if (mode == ArpSettings::chord)
        {
            const int octave = (int) (index % octaves);
            for (int i = 0; i < n; ++i)
                sound (out, position, base[(size_t) i] + 12 * octave, lastVelocity);
            ++index;
            return;
        }

        std::array<int, 64> run {};
        int len = 0;
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < n && len < (int) run.size(); ++i)
                run[(size_t) len++] = base[(size_t) i] + 12 * o;
        if (len == 0)
            return;

        int pick = 0;
        const auto at = [&] (juce::int64 k) { return (int) (((k % len) + len) % len); };
        switch (mode)
        {
            case ArpSettings::down:
                pick = len - 1 - at (index);
                break;
            case ArpSettings::upDown:
            case ArpSettings::downUp:
            {
                const int cycle = juce::jmax (1, 2 * len - 2);
                int k = (int) (index % cycle);
                k = k < len ? k : cycle - k;
                pick = mode == ArpSettings::upDown ? k : len - 1 - k;
                break;
            }
            case ArpSettings::random:
            {
                pick = (int) (random.nextInt (len));
                if (len > 1 && pick == lastPick)
                    pick = (pick + 1 + random.nextInt (len - 1)) % len;
                break;
            }
            case ArpSettings::converge:
            case ArpSettings::diverge:
            {
                // Converge: outside in (low, high, next low, next high...). Diverge: the same backwards.
                int k = at (index);
                if (mode == ArpSettings::diverge)
                    k = len - 1 - k;
                pick = (k % 2 == 0) ? k / 2 : len - 1 - k / 2;
                break;
            }
            default:   // up, as played
                pick = at (index);
                break;
        }
        lastPick = pick;
        ++index;
        sound (out, position, run[(size_t) pick], lastVelocity);
    }

    std::array<int, 16> held {}, velocities {};
    int numHeld = 0, lastVelocity = 100, lastPick = -1;
    std::array<int, 16> sounding {};
    int numSounding = 0;
    juce::int64 index = 0, stepCount = 0;
    double untilStep = 0.0, offIn = 0.0, pendingDelay = 0.0;
    bool offPending = false, stepPending = false;
    juce::Random random { 0xa77e };
};

/** The arpeggiator's settings as JSON, for projects. */
inline juce::var arpToVar (const ArpSettings& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("on", s.on.load());           o->setProperty ("mode", s.mode.load());
    o->setProperty ("rate", s.rate.load());       o->setProperty ("octaves", s.octaves.load());
    o->setProperty ("gate", s.gate.load());       o->setProperty ("swing", s.swing.load());
    return juce::var (o);
}

inline void applyArp (ArpSettings& s, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    s.on.store (v.hasProperty ("on") && (bool) v["on"]);
    s.mode.store (juce::jlimit (0, ArpSettings::numModes - 1, i ("mode", 0)));
    s.rate.store (juce::jlimit (0, ArpSettings::numRates - 1, i ("rate", 3)));
    s.octaves.store (juce::jlimit (1, 4, i ("octaves", 1)));
    s.gate.store (f ("gate", 0.5f));
    s.swing.store (f ("swing", 0.0f));
}

} // namespace bizgroove
