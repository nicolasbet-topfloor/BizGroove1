#pragma once

#include "LiteStyle.h"

namespace bizgroove::lite
{

/** The light apps play a loop of four bars: scenes 1 to 4, one bar each, in Song mode. */
constexpr int numBars = 4;

/** How the rap's words sit on the beat. */
enum class Flow { laidBack = 0, doubleTime, choppy, numFlows };

inline juce::String flowName (Flow f)
{
    switch (f)
    {
        case Flow::laidBack:   return "Laid back";
        case Flow::doubleTime: return "Double time";
        case Flow::choppy:     return "Choppy";
        default:               return {};
    }
}

/** What a light app keeps per track, besides the engine: which sound (A or B) is on it. */
struct LiteTrackState
{
    int sound = 0;         // 0 = A, 1 = B
    float rhythm = 0.5f;   // the Rhythm dial: fewer notes (0) .. more (1); 0.5 = as written
};

class Generator
{
public:
    /** Sets the engine up for a style: the tracks, their sounds and mix, the tempo and the four-bar loop. */
    static void setUp (AudioEngine& e, const Style& s, std::vector<LiteTrackState>& states)
    {
        states.assign (s.tracks.size(), {});
        e.transport.bpm.store (s.bpm);
        auto& song = e.song;
        for (int b = 0; b < Song::numScenes; ++b)
            song.clearScene (b);
        song.songMode.store (true);
        song.useTimeline.store (false);
        song.loopSong.store (true);
        song.chainLength.store (numBars);
        for (int b = 0; b < numBars; ++b)
        {
            song.chain[(size_t) b].scene.store (b);
            song.chain[(size_t) b].repeats.store (1);
        }
        song.currentScene.store (0);
        e.effects.reverbSize.store (s.masterReverbSize);
        e.masterGain.store (0.6f);   // headroom: the kick and the bass meet on the one

        for (int t = 0; t < AudioEngine::numTracks; ++t)
        {
            auto& tr = e.track (t);
            const bool used = t < (int) s.tracks.size();
            tr.mute.store (! used);
            song.swing[(size_t) t].store (0.0f);
            if (! used)
                continue;
            const auto& def = s.tracks[(size_t) t];
            if (def.role == Role::sampler && ! tr.samplerEngine.hasSample())
                FieldRecordings::load (tr, (int) s.id);   // something to hear before you load your own
            applySound (tr, def.sounds[0]);
            if (def.role == Role::rap)
                tr.voiceParams.language.store (speech::spanish);   // the lyrics start in Spanish; ES / EN switches
            tr.volume.store (def.volume);
            tr.mix.reverb.store (def.reverb);
            tr.mix.delay.store (def.delay);
            song.swing[(size_t) t].store (s.swing);   // every track shuffles together, or the groove falls apart
        }

        // The ambience (vinyl crackle) on the next track, quietly, not shown.
        if (s.ambience.isNotEmpty())
        {
            const int a = ambienceTrack (s);
            auto& tr = e.track (a);
            applySound (tr, { "Ambience", Track::noise, s.ambience, {} });
            tr.volume.store (0.2f);
            tr.mute.store (false);
        }
    }

    /** The hidden track that holds the ambience: the one after the style's tracks. */
    static int ambienceTrack (const Style& s) { return (int) s.tracks.size(); }

    /** Switches a track to its sound A (0) or B (1). The steps stay. */
    static void chooseSound (AudioEngine& e, const Style& s, std::vector<LiteTrackState>& states, int t, int which)
    {
        if (! juce::isPositiveAndBelow (t, (int) s.tracks.size()))
            return;
        which = juce::jlimit (0, 1, which);
        states[(size_t) t].sound = which;
        auto& tr = e.track (t);
        const auto keepText = tr.voiceParams.getText();
        const int keepMode = tr.voiceParams.mode.load(), keepLanguage = tr.voiceParams.language.load();
        applySound (tr, s.tracks[(size_t) t].sounds[(size_t) which]);
        if (s.tracks[(size_t) t].role == Role::rap)
        {
            // Another voice, the same rap: the words, the language and word or syllable stay.
            tr.voiceParams.mode.store (keepMode);
            tr.voiceParams.language.store (keepLanguage);
            tr.voiceParams.setText (keepText);
        }
    }

    /** Writes a new four-bar part for one track, in the style. A rap track needs 'text' and 'flow'. */
    static int generate (AudioEngine& e, const Style& s, const std::vector<LiteTrackState>& states, int t,
                         juce::Random& rng, const juce::String& text = {}, Flow flow = Flow::laidBack, bool bySyllable = false)
    {
        if (! juce::isPositiveAndBelow (t, (int) s.tracks.size()))
            return 0;
        clearTrack (e, t);
        const int variant = states[(size_t) t].sound;
        if (progression.empty() || (int) progression.size() != numBars)
            pickProgression (s, rng);
        switch (s.tracks[(size_t) t].role)
        {
            case Role::beats:  beats (e, s, t, variant, rng); break;
            case Role::bass:   bass (e, s, t, variant, rng); break;
            case Role::melody: melody (e, s, t, variant, rng); break;
            case Role::rap:    return rap (e, s, t, text, flow, rng, bySyllable);
            case Role::pads:   pads (e, s, t, rng); break;
            case Role::noise:
            case Role::granular: texture (e, s, t, rng); break;
            case Role::sampler: sampler (e, s, t, variant, rng); break;
        }
        applyRhythm (e, t, states[(size_t) t].rhythm, rng);
        return 0;
    }

    /** The Rhythm dial: below the middle it takes notes out (never the first of a bar), above it
        adds more, copied from the notes nearby onto empty eighths (or sixteenths, near the top). */
    static void applyRhythm (AudioEngine& e, int t, float rhythm, juce::Random& rng)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, rhythm) - 0.5f;
        if (std::abs (amount) < 0.02f)
            return;
        for (int b = 0; b < numBars; ++b)
        {
            auto& p = e.song.scene (b).tracks[(size_t) t];
            if (amount < 0.0f)
            {
                const float drop = -amount * 1.6f;   // up to 80% of the notes go
                for (int st = 1; st < 16; ++st)
                {
                    if (chance (rng, drop)) p.steps[(size_t) st].on.store (false);
                    for (auto& lane : p.drumLanes)
                        if (st % 4 != 0 && chance (rng, drop)) lane.store (lane.load() & ~((std::uint64_t) 1 << st));
                }
            }
            else
            {
                const int grid = amount > 0.3f ? 1 : 2;
                const float add = amount * 1.4f;
                for (int st = 0; st < 16; st += grid)
                {
                    if (! chance (rng, add)) continue;
                    // A note: the nearest one before it, shorter.
                    if (! p.steps[(size_t) st].on.load())
                        for (int back = 1; back <= st; ++back)
                            if (p.steps[(size_t) (st - back)].on.load())
                            {
                                p.steps[(size_t) st].copyFrom (p.steps[(size_t) (st - back)]);
                                p.steps[(size_t) st].length.store (juce::jmin (1.0f, p.steps[(size_t) st].length.load()));
                                p.steps[(size_t) st].velocity.store (85);
                                break;
                            }
                    // Drums: hats and percussion get the extra hits.
                    for (int d : { (int) closedHat, 8, 5, 7 })
                        if ((p.drumLanes[(size_t) d].load() != 0) && chance (rng, 0.5f))
                            p.drumLanes[(size_t) d].store (p.drumLanes[(size_t) d].load() | ((std::uint64_t) 1 << st));
                }
            }
        }
    }

    /** A new song: a new chord progression, then every track. */
    static void generateAll (AudioEngine& e, const Style& s, const std::vector<LiteTrackState>& states, juce::Random& rng,
                             const juce::String& rapText = {}, Flow flow = Flow::laidBack, bool bySyllable = false)
    {
        pickProgression (s, rng);
        for (int t = 0; t < (int) s.tracks.size(); ++t)
            generate (e, s, states, t, rng, rapText, flow, bySyllable);
        if (s.ambience.isNotEmpty())
        {
            const int a = ambienceTrack (s);
            clearTrack (e, a);
            for (int b = 0; b < numBars; ++b)
                note (e, b, a, 0, 60, 16.0f, 80);
        }
    }

    /** Syllables in a word, roughly (groups of vowels), at least one. */
    static int syllables (const juce::String& word)
    {
        static const juce::String vowels = juce::String::fromUTF8 ("aeiouyAEIOUY\xc3\xa1\xc3\xa9\xc3\xad\xc3\xb3\xc3\xba\xc3\xbc");
        int count = 0;
        bool inVowel = false;
        for (auto c : word)
        {
            const bool v = vowels.containsChar (c);
            if (v && ! inVowel) ++count;
            inVowel = v;
        }
        return juce::jmax (1, count);
    }

    /** The chord progression in use: scale degrees, one per bar. */
    static inline std::vector<int> progression;

private:
    static void pickProgression (const Style& s, juce::Random& rng)
    {
        progression = s.progressions[(size_t) rng.nextInt ((int) s.progressions.size())];
    }

    static void clearTrack (AudioEngine& e, int t)
    {
        for (int b = 0; b < numBars; ++b)
        {
            auto& p = e.song.scene (b).tracks[(size_t) t];
            for (auto& lane : p.drumLanes)
                lane.store (0);
            for (auto& st : p.steps)
            {
                st.on.store (false);
                st.setSingle (48);
            }
        }
    }

    /** A note of the scale: 'degree' steps up from the root (any integer), plus octaves. */
    static int scaleNote (const Style& s, int degree, int octave = 0)
    {
        const int n = (int) s.scale.size();
        const int oct = (degree >= 0 ? degree / n : -((-degree + n - 1) / n));
        const int idx = degree - oct * n;
        return s.root + s.scale[(size_t) idx] + 12 * (oct + octave);
    }

    /** The chord of a bar: root, third and fifth as scale degrees. */
    static std::array<int, 3> chord (int bar)
    {
        const int d = progression[(size_t) (bar % (int) progression.size())];
        return { d, d + 2, d + 4 };
    }

    static void hit (AudioEngine& e, int bar, int t, int d, int step)
    {
        auto& lane = e.song.scene (bar).tracks[(size_t) t].drumLanes[(size_t) d];
        lane.store (lane.load() | ((std::uint64_t) 1 << step));
    }

    static void note (AudioEngine& e, int bar, int t, int step, int midi, float length, int velocity = 100)
    {
        auto& st = e.song.scene (bar).tracks[(size_t) t].steps[(size_t) step];
        st.setSingle (midi, velocity);
        st.length.store (length);
        st.on.store (true);
    }

    static bool chance (juce::Random& rng, float p) { return rng.nextFloat() < p; }

    static int trackWith (const Style& s, Role r)
    {
        for (int t = 0; t < (int) s.tracks.size(); ++t)
            if (s.tracks[(size_t) t].role == r) return t;
        return -1;
    }

    // ---------------------------------------------------------------- beats
    static void beats (AudioEngine& e, const Style& style, int t, int variant, juce::Random& rng)
    {
        // BizSampler's beats are boom bap; BizNoise's are sparse, to leave room for the textures.
        struct { Style::Id id; } s { style.id == Style::sampler ? Style::hop : style.id };
        if (style.id == Style::sampler) variant = 0;
        if (style.id == Style::noise)
        {
            for (int b = 0; b < numBars; ++b)
            {
                hit (e, b, t, kick, 0);
                if (chance (rng, 0.7f)) hit (e, b, t, kick, chance (rng, 0.5f) ? 10 : 7);
                hit (e, b, t, variant == 0 ? sideStick : clap, 12);
                for (int st = 2; st < 16; st += 4) if (chance (rng, 0.6f)) hit (e, b, t, closedHat, st);
                if (b == numBars - 1 && chance (rng, 0.5f)) hit (e, b, t, lowTom, 14);
            }
            return;
        }
        // One bar repeated with small changes, and a fill in the fourth. The kick's pattern stays: the bass follows it.
        const int kickTwo = chance (rng, 0.5f) ? 10 : 8;
        const int kickExtra = chance (rng, 0.6f) ? (chance (rng, 0.5f) ? 7 : 3) : -1;
        for (int b = 0; b < numBars; ++b)
        {
            const bool fill = b == numBars - 1;
            if (s.id == Style::hop && variant == 0)   // boom bap: kick and snare, hats in eighths, soft hats and ghost notes between
            {
                hit (e, b, t, kick, 0);
                hit (e, b, t, kick, kickTwo);
                if (kickExtra >= 0) hit (e, b, t, kick, kickExtra);
                if (b % 2 == 1 && chance (rng, 0.5f)) hit (e, b, t, kick, 15);
                hit (e, b, t, snare, 4); hit (e, b, t, snare, 12);
                for (int st = 0; st < 16; st += 2) hit (e, b, t, closedHat, st);
                for (int st = 1; st < 16; st += 2) if (chance (rng, 0.35f)) hit (e, b, t, 8, st);   // soft hats
                for (int st : { 7, 9, 15 }) if (chance (rng, 0.3f)) hit (e, b, t, snare2, st);      // ghost snares
                if (chance (rng, 0.25f)) hit (e, b, t, 9, 14);                                       // shaker
                if (fill) { hit (e, b, t, snare, 14); hit (e, b, t, snare2, 15); hit (e, b, t, openHat, 10); }
            }
            else if (s.id == Style::hop)   // trap: half-time clap and snare, rolling hats, the kick with the 808
            {
                hit (e, b, t, kick, 0);
                if (kickExtra >= 0) hit (e, b, t, kick, kickExtra);
                hit (e, b, t, kick, kickTwo == 8 ? 11 : kickTwo);
                hit (e, b, t, clap, 8); hit (e, b, t, snare, 8);
                for (int st = 0; st < 16; st += 2) hit (e, b, t, closedHat, st);
                const int roll = 4 * rng.nextInt (4);   // a beat of 1/16 hats, softer between
                for (int st = roll + 1; st < roll + 4; st += 2) hit (e, b, t, 8, st);
                if (chance (rng, 0.5f)) hit (e, b, t, openHat, 14);
                if (chance (rng, 0.3f)) hit (e, b, t, sideStick, 13);
                if (fill) { hit (e, b, t, clap, 14); hit (e, b, t, 8, 15); }
            }
            if (s.id == Style::hop)
            {
                // Now and then a shout: at the top of the loop, and into the next one.
                if (b == 0 && chance (rng, 0.5f)) hit (e, b, t, crash, 0);
                if (fill && chance (rng, 0.6f)) hit (e, b, t, highTom, 12);
            }
            else if (s.id == Style::wave)
            {
                hit (e, b, t, kick, 0); hit (e, b, t, kick, 8);
                if (variant == 1 || chance (rng, 0.3f)) { hit (e, b, t, kick, 6); }
                hit (e, b, t, snare, 4); hit (e, b, t, snare, 12);
                for (int st = 0; st < 16; st += 2) hit (e, b, t, closedHat, st);
                if (variant == 1) for (int st = 2; st < 16; st += 4) hit (e, b, t, openHat, st);
                if (fill) for (int st = 12; st < 16; ++st) hit (e, b, t, st % 2 == 0 ? highTom : lowTom, st);
                if (b == 0) hit (e, b, t, crash, 0);
            }
            else   // techno, and anything else: four on the floor
            {
                for (int st = 0; st < 16; st += 4) hit (e, b, t, kick, st);
                for (int st = 2; st < 16; st += 4) hit (e, b, t, openHat, st);
                if (variant == 0)
                {
                    hit (e, b, t, clap, 4); hit (e, b, t, clap, 12);
                    for (int st = 0; st < 16; ++st) if (chance (rng, 0.55f)) hit (e, b, t, closedHat, st);
                }
                else
                {
                    hit (e, b, t, sideStick, 3 + 4 * rng.nextInt (3));
                    for (int st = 1; st < 16; st += 2) if (chance (rng, 0.5f)) hit (e, b, t, ride, st);
                }
                if (fill) { hit (e, b, t, clap, 14); hit (e, b, t, clap, 15); }
            }
        }
    }

    // ---------------------------------------------------------------- bass
    static void bass (AudioEngine& e, const Style& s, int t, int variant, juce::Random& rng)
    {
        if (s.id == Style::hop || s.id == Style::sampler)
        {
            // The bass plays with the kick: a note on each kick, the root on the one, other chord tones after,
            // and sometimes a note leading into the next bar. The 808 (A) holds to the next note; the bass (B) is short.
            const int beatsTrack = trackWith (s, Role::beats);
            for (int b = 0; b < numBars; ++b)
            {
                std::vector<int> hits;
                const auto kicks = beatsTrack >= 0 ? e.song.scene (b).tracks[(size_t) beatsTrack].drumLanes[kick].load() : (std::uint16_t) 0x0101;
                for (int st = 0; st < 16; ++st)
                    if ((kicks >> st & 1u) != 0) hits.push_back (st);
                if (hits.empty() || hits.front() != 0) hits.insert (hits.begin(), 0);
                const bool lead = b < numBars - 1 && chance (rng, 0.4f) && hits.back() < 14;
                if (lead) hits.push_back (14);
                const auto c = chord (b);
                for (size_t i = 0; i < hits.size(); ++i)
                {
                    const int st = hits[i];
                    const int next = i + 1 < hits.size() ? hits[i + 1] : 16;
                    int n = scaleNote (s, c[0]);
                    if (i > 0) n = chance (rng, 0.6f) ? scaleNote (s, c[0]) : (chance (rng, 0.5f) ? scaleNote (s, c[2]) : scaleNote (s, c[0], 1));
                    if (lead && i + 1 == hits.size()) n = scaleNote (s, chord (b + 1)[0] - 1);   // a step below the next root
                    const float len = variant == 0 ? (float) juce::jmax (1, next - st) : 1.5f;
                    note (e, b, t, st, n, len, i == 0 ? 120 : 100);
                }
            }
        }
        else if (s.id == Style::wave)
        {
            // Driving eighths on the chord's root; sound A jumps the octave.
            for (int b = 0; b < numBars; ++b)
                for (int st = 0; st < 16; st += 2)
                {
                    const int root = scaleNote (s, chord (b)[0]);
                    const bool up = variant == 0 && (st / 2) % 2 == 1;
                    note (e, b, t, st, root + (up ? 12 : 0), 1.0f, st % 4 == 0 ? 115 : 90);
                }
        }
        else
        {
            if (variant == 0)
            {
                // An acid line: sixteenths from the scale, the same in every bar, with accents.
                std::array<int, 16> line {};
                std::array<bool, 16> on {};
                for (int st = 0; st < 16; ++st)
                {
                    on[(size_t) st] = st % 4 == 2 || chance (rng, 0.45f);
                    line[(size_t) st] = chance (rng, 0.6f) ? 0 : rng.nextInt ((int) s.scale.size()) - 1;
                }
                for (int b = 0; b < numBars; ++b)
                    for (int st = 0; st < 16; ++st)
                        if (on[(size_t) st])
                            note (e, b, t, st, scaleNote (s, chord (b)[0] + line[(size_t) st]) + (chance (rng, 0.15f) ? 12 : 0),
                                  0.0f, chance (rng, 0.3f) ? 127 : 90);
            }
            else
            {
                for (int b = 0; b < numBars; ++b)   // off-beat bass
                    for (int st = 2; st < 16; st += 4)
                        note (e, b, t, st, scaleNote (s, chord (b)[0]), 1.5f, 110);
            }
        }
    }

    // ---------------------------------------------------------------- melody
    static void chordAt (AudioEngine& e, const Style& s, int bar, int t, int step, float length, int octave, int velocity = 100, bool seventh = false)
    {
        const auto c = chord (bar);
        auto& st = e.song.scene (bar).tracks[(size_t) t].steps[(size_t) step];
        st.setSingle (scaleNote (s, c[0], octave), velocity);
        st.addNote (scaleNote (s, c[1], octave));
        st.addNote (scaleNote (s, c[2], octave));
        if (seventh)
            st.addNote (scaleNote (s, c[0] + 6, octave));
        st.length.store (length);
        st.on.store (true);
    }

    static void melody (AudioEngine& e, const Style& s, int t, int variant, juce::Random& rng)
    {
        if (s.id == Style::hop)
        {
            // Seventh chords, laid back: the strings (B) hold them; the piano (A) plays them in a loose rhythm.
            const int second = chance (rng, 0.5f) ? 6 : 7, third = chance (rng, 0.5f) ? 10 : 11;
            for (int b = 0; b < numBars; ++b)
            {
                if (variant == 1) { chordAt (e, s, b, t, 0, 16.0f, 2, 80, true); continue; }
                chordAt (e, s, b, t, 0, 5.0f, 2, 95, true);
                chordAt (e, s, b, t, second, 3.0f, 2, 70, true);
                if (b % 2 == 1) chordAt (e, s, b, t, third, 5.0f, 2, 80, true);
            }
        }
        else if (s.id == Style::wave)
        {
            if (variant == 1)
            {
                for (int b = 0; b < numBars; ++b)
                    chordAt (e, s, b, t, 0, 16.0f, 2, 85);
                return;
            }
            // A two-bar phrase in eighths, sung twice; the second time it ends differently.
            std::array<int, 16> phrase {};
            int degree = 0;
            for (auto& d : phrase)
            {
                degree = juce::jlimit (-2, 9, degree + rng.nextInt (5) - 2);
                d = degree;
            }
            for (int b = 0; b < numBars; ++b)
                for (int i = 0; i < 8; ++i)
                {
                    const int idx = (b % 2) * 8 + i;
                    if (idx % 8 != 0 && chance (rng, 0.2f)) continue;   // breaths
                    int d = phrase[(size_t) idx];
                    if (b == numBars - 1 && i >= 6) d = chord (b)[0] + 7;   // home at the end
                    note (e, b, t, i * 2, scaleNote (s, d, 2), 1.8f, 100);
                }
        }
        else
        {
            // Stabs: the same rhythm in every bar, the chord moving with the bass.
            std::vector<int> hits;
            for (int st = 0; st < 16; ++st)
                if ((st % 4 == 3 && chance (rng, 0.6f)) || (st % 4 == 2 && chance (rng, 0.25f)))
                    hits.push_back (st);
            if (hits.empty()) hits.push_back (3);
            for (int b = 0; b < numBars; ++b)
                for (int st : hits)
                    chordAt (e, s, b, t, st, variant == 0 ? 0.5f : 0.0f, 1, 95);
        }
    }

    static void pads (AudioEngine& e, const Style& s, int t, juce::Random&)
    {
        for (int b = 0; b < numBars; ++b)
            chordAt (e, s, b, t, 0, 16.0f, 1, 80);
    }

    // ---------------------------------------------------------------- sampler
    /** Grains (A): a long note a bar, on the chord, so the field recording sings in the key; now and
        then a pad's spot as an accent. Chops (B): its 16 slices in a broken rhythm. */
    static void sampler (AudioEngine& e, const Style& s, int t, int variant, juce::Random& rng)
    {
        if (variant == 0)
        {
            for (int b = 0; b < numBars; ++b)
            {
                const int root = scaleNote (s, chord (b)[0]);
                note (e, b, t, 0, juce::jlimit (48, 72, root + 12 * ((60 - root + 6) / 12)), 16.0f, 90);
                if (chance (rng, 0.4f))
                    note (e, b, t, 8 + 2 * rng.nextInt (4), GranularParams::padNoteBase + rng.nextInt (GranularParams::numPads), 3.0f, 80);
            }
            return;
        }
        std::vector<int> hits;
        for (int st = 0; st < 16; ++st)
            if (st % 4 == 0 || chance (rng, st % 2 == 0 ? 0.5f : 0.2f))
                hits.push_back (st);
        std::array<int, 16> slice {};
        for (auto& k : slice) k = rng.nextInt (16);
        for (int b = 0; b < numBars; ++b)
            for (int st : hits)
                note (e, b, t, st, Sampler::rootNote + slice[(size_t) st], 1.0f, st % 4 == 0 ? 110 : 85);
    }

    static void texture (AudioEngine& e, const Style& s, int t, juce::Random& rng)
    {
        for (int b = 0; b < numBars; b += 2)
            note (e, b, t, 0, scaleNote (s, rng.nextInt ((int) s.scale.size()), 1), 32.0f, 80);
    }

    // ---------------------------------------------------------------- rap
    /** Lays the words of the text on the beat, one word per note, each held for its syllables.
        Returns how many words fit in the four bars; the voice is given those words only. */
    static int rap (AudioEngine& e, const Style& s, int t, const juce::String& text, Flow flow, juce::Random& rng, bool bySyllable)
    {
        auto& vp = e.track (t).voiceParams;
        const int lang = vp.language.load();
        // Syllables as the Voice counts them, so each note says exactly one.
        auto syllablesOf = [lang] (const juce::String& w) {
            auto sc = std::make_unique<speech::Script>();
            speech::Pieces a {}, b {};
            speech::compile (w, lang, *sc);
            return juce::jmax (1, speech::syllables (*sc, a, b));
        };
        juce::StringArray words;
        words.addTokens (text.replaceCharacters (",.;:!?\"", "       "), " \n\t", "");
        words.removeEmptyStrings();

        const int stepsPerSyllable = flow == Flow::laidBack ? 2 : 1;
        const int total = numBars * 16;
        int pos = 0;
        juce::StringArray placed;
        const int base = 50;   // around D3: a speaking voice
        for (int w = 0; w < words.size(); ++w)
        {
            const int syl = syllablesOf (words[w]);
            const int len = syl * stepsPerSyllable;
            if (pos + len > total)
                break;
            const int bar = pos / 16, step = pos % 16;
            // The voice falls a little at the end of each bar's line, rises on the last word before a pause.
            const bool lineEnd = (pos + len) % 16 < len || (pos + len) % 16 == 0;
            const int pitch = base + (lineEnd ? -2 : (chance (rng, 0.3f) ? 2 : 0));
            if (bySyllable)
            {
                // One note per syllable, the first one accented.
                for (int k = 0; k < syl; ++k)
                {
                    const int at = pos + k * stepsPerSyllable;
                    note (e, at / 16, t, at % 16, pitch + (k == 0 ? 0 : -1), (float) stepsPerSyllable - 0.25f, k == 0 ? 115 : 95);
                }
            }
            else
                note (e, bar, t, step, pitch, (float) len, lineEnd ? 120 : 100);
            placed.add (words[w]);
            pos += len;
            if (flow == Flow::choppy && chance (rng, 0.45f))
                pos += 1 + rng.nextInt (2);   // a gap: the words come in bursts
            if (lineEnd && flow != Flow::doubleTime)
                pos += 1;                     // a breath at the end of a line
        }

        vp.mode.store (bySyllable ? VoiceParams::syllableBySyllable : VoiceParams::wordByWord);
        // A syllable takes about a fifth of a second at speed 1: fit it to a sixteenth, so the words stay
        // crisp. (Laid back spaces the words out instead of drawling them.)
        const double stepSeconds = 60.0 / juce::jmax (40.0, e.transport.bpm.load()) / 4.0;
        vp.speed.store ((float) juce::jlimit (0.8, 1.8, 0.2 / stepSeconds));
        vp.setText (placed.joinIntoString (" "));
        return placed.size();
    }
};

} // namespace bizgroove::lite
