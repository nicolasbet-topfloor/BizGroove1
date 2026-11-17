#pragma once

#include "engine/Presets.h"
#include "OneShots.h"
#include "engine/VocalShots.h"
#include "FieldRecordings.h"
#include <functional>
#include <vector>

namespace bizgroove::lite
{

/** What a track does in a light app; the generator writes each kind differently. */
enum class Role { beats, bass, melody, rap, pads, granular, noise, sampler };

/** One of a track's two sounds (A or B): a factory sound, then a few changes of its own. */
struct SoundChoice
{
    juce::String name;
    int type = Track::poly;              // the instrument
    juce::String factory;                // a factory sound of that instrument, by name
    std::function<void (Track&)> tweak;  // optional changes after it
};

struct TrackDef
{
    juce::String label;
    Role role;
    std::array<SoundChoice, 2> sounds;
    float volume = 0.8f, reverb = 0.0f, delay = 0.0f;
};

/** A light app: its name, tempo, key, tracks and background. */
struct Style
{
    enum Id { hop = 0, wave, techno, noise, sampler, numStyles };

    Id id;
    juce::String appName, tagline;
    double bpm;
    float swing;                  // the beats' shuffle, 0..1
    int root;                     // the key's root note (MIDI, the bass octave)
    std::vector<int> scale;       // semitones above the root
    std::vector<std::vector<int>> progressions;   // chords as scale degrees, one per bar; the generator picks one
    std::vector<TrackDef> tracks;
    float masterReverbSize = 0.5f;
    juce::String ambience;        // a Noise sound held under everything, quietly (vinyl crackle); empty for none
};

/** Puts a sound on a track: the factory sound by name (the first one if the name is not found), then the tweak. */
inline void applySound (Track& track, const SoundChoice& s)
{
    const auto& list = Presets::factorySounds (s.type);
    const Presets::FactorySound* found = list.empty() || s.factory.isEmpty() ? nullptr : &list.front();   // no factory sound: only the tweak
    for (const auto& f : list)
        if (f.name == s.factory)
            found = &f;
    if (found != nullptr)
        Presets::applySoundVar (track, found->sound);
    track.type.store (s.type);
    if (s.tweak)
        s.tweak (track);
}

/** A Sampler track's two sounds: its field recording as grains (A), or chopped into 16 slices (B).
    Neither touches the recording itself. */
inline std::array<SoundChoice, 2> samplerSounds (float size = 0.14f, float density = 24.0f, float scan = 0.05f)
{
    return { {
        { "Grains", Track::sampler, {}, [size, density, scan] (Track& t) {
            t.grainMode.store (true);
            auto& g = t.granularParams;
            g.size.store (size); g.density.store (density); g.spread.store (0.12f); g.scan.store (scan);
            g.pitch.store (0.0f); g.detune.store (0.08f); g.reverse.store (0.1f); g.spray.store (0.7f);
            g.attack.store (0.3f); g.decay.store (0.6f); g.sustain.store (0.85f); g.release.store (1.2f);
            g.cutoffHz.store (9000.0f); g.level.store (4.0f); g.freeze.store (false);
        } },
        { "Chops", Track::sampler, {}, [] (Track& t) {
            t.grainMode.store (false);
            auto& p = t.samplerEngine.params;
            p.start.store (0.0f); p.length.store (1.0f); p.pitch.store (0.0f); p.brightness.store (0.9f);
            p.attack.store (0.003f); p.release.store (0.12f); p.sustain.store (1.0f);
            p.loop.store (false); p.reverse.store (false); p.slices.store (16); p.sliceCount.store (0);
        } } } };
}

/** The two effect slots of a track: a type and its four knobs each (InsertEffects::off for none). */
inline void inserts (Track& t, int typeA, std::array<float, 4> knobsA, int typeB = InsertEffects::off, std::array<float, 4> knobsB = {})
{
    t.inserts[0].choose (typeA);
    t.inserts[1].choose (typeB);
    for (size_t k = 0; k < 4; ++k)
    {
        if (typeA != InsertEffects::off) t.inserts[0].knob[k].store (knobsA[k]);
        if (typeB != InsertEffects::off) t.inserts[1].knob[k].store (knobsB[k]);
    }
}

/** Per-drum settings, for the kits below. */
inline void drum (Track& t, int d, float pitch, float length, float tone, float snap = 0.3f, float drive = 0.0f, float volume = 0.8f)
{
    auto& p = t.kit.drums[(size_t) d];
    p.pitch.store (pitch); p.length.store (length); p.tone.store (tone);
    p.snap.store (snap); p.drive.store (drive); p.volume.store (volume);
}

// Drums by their place in the kit (General MIDI order).
enum Drum { kick = 0, sideStick = 1, snare = 2, clap = 3, snare2 = 4, closedHat = 6, openHat = 10, crash = 13, ride = 15, lowTom = 9, highTom = 14 };

inline const Style& style (int id)
{
    static const std::vector<Style> styles = [] {
        std::vector<Style> s;

        // ---------------------------------------------------------------- BizHop
        s.push_back ({ Style::hop, "BizHop Generator", "Beats, bass, keys and a rapping voice", 88.0, 0.28f, 36,
                       { 0, 2, 3, 5, 7, 9, 10 },   // dorian: minor, with a jazzier sixth
                       { { 0, 3, 0, 4 }, { 0, 2, 3, 4 }, { 0, 6, 5, 3 }, { 0, 0, 3, 3 } },
                       {
                           { "Beats", Role::beats, { {
                               { "Boom bap", Track::drums, "Standard", [] (Track& t) {
                                   OneShots::loadKit (t, OneShots::boomBap);
                                   VocalShots::loadOne (t, 13, 0);   // ad-libs in the Perc lane: "yeah", "come on", "let's go"
                                   VocalShots::loadOne (t, 14, 2);
                                   VocalShots::loadOne (t, 12, 7);
                                   inserts (t, InsertEffects::compressor, { 0.45f, 0.35f, 0.4f, 0.6f });
                               } },
                               { "Trap", Track::drums, "Standard", [] (Track& t) {
                                   OneShots::loadKit (t, OneShots::trap);
                                   VocalShots::loadOne (t, 13, 0);
                                   VocalShots::loadOne (t, 14, 7);
                                   VocalShots::loadOne (t, 12, 3);
                                   inserts (t, InsertEffects::compressor, { 0.5f, 0.3f, 0.6f, 0.4f });
                               } } } }, 0.75f, 0.04f, 0.0f },
                           { "Bass", Role::bass, { {
                               { "808", Track::sampler, "One-shot", [] (Track& t) {
                                   OneShots::loadInstrument (t, OneShots::eightOhEight);
                                   inserts (t, InsertEffects::drive, { 0.25f, 0.45f, 0.2f, 0.4f });
                               } },
                               { "Dusty bass", Track::sampler, "One-shot", [] (Track& t) {
                                   OneShots::loadInstrument (t, OneShots::dustyBass);
                                   inserts (t, InsertEffects::off, {});
                               } } } }, 0.55f },
                           { "Keys", Role::melody, { {
                               { "Dusty piano", Track::sampler, "One-shot", [] (Track& t) {
                                   OneShots::loadInstrument (t, OneShots::electricPiano);
                                   inserts (t, InsertEffects::chorus, { 0.3f, 0.3f, 0.5f, 0.3f });
                               } },
                               { "Strings", Track::poly, "Pad: Juno strings", [] (Track& t) {
                                   inserts (t, InsertEffects::lofi, { 0.6f, 0.5f, 0.4f, 0.5f });
                               } } } }, 0.5f, 0.2f, 0.08f },
                           { "Rap", Role::rap, { {
                               { "Hip-house MC", Track::voice, "MC: hip house", [] (Track& t) {
                                   inserts (t, InsertEffects::compressor, { 0.5f, 0.4f, 0.6f, 1.0f }, InsertEffects::drive, { 0.25f, 0.6f, 0.2f, 0.35f });
                               } },
                               { "Boom-bap MC", Track::voice, "MC: boom bap", [] (Track& t) {
                                   inserts (t, InsertEffects::compressor, { 0.5f, 0.4f, 0.6f, 1.0f });
                               } } } }, 0.95f, 0.07f, 0.04f },
                           { "Sampler", Role::sampler, samplerSounds (0.12f, 20.0f, 0.03f), 0.45f, 0.25f, 0.1f }
                       }, 0.35f, "Vinyl crackle" });

        // ---------------------------------------------------------------- BizWave
        s.push_back ({ Style::wave, "BizWave Generator", "Synthwave: beats, bass and lead", 104.0, 0.0f, 33,
                       { 0, 2, 3, 5, 7, 8, 10 },   // natural minor
                       { { 0, 5, 2, 6 }, { 0, 3, 5, 4 }, { 0, 6, 5, 4 }, { 5, 6, 0, 0 } },
                       {
                           { "Beats", Role::beats, { {
                               { "Gated 80s", Track::drums, "Standard", [] (Track& t) {
                                   drum (t, kick, -1.0f, 0.5f, 0.45f, 0.5f, 0.1f, 0.95f);
                                   drum (t, snare, 0.0f, 0.8f, 0.6f, 0.4f, 0.1f, 0.9f);
                               } },
                               { "Drive", Track::drums, "FM kit", {} } } }, 0.8f, 0.25f, 0.0f },
                           { "Bass", Role::bass, { {
                               { "Octave saw", Track::poly, "Fat bass", [] (Track& t) {
                                   t.polyParams.octave.store (1); t.polyParams.decay.store (0.18f); t.polyParams.sustain.store (0.3f);
                               } },
                               { "Talking bass", Track::wavetable, "Talking bass", {} } } }, 0.75f },
                           { "Lead", Role::melody, { {
                               { "Unison lead", Track::poly, "Unison lead", {} },
                               { "Juno strings", Track::poly, "Pad: Juno strings", {} } } }, 0.6f, 0.3f, 0.3f },
                           { "Sampler", Role::sampler, samplerSounds (0.2f, 18.0f, 0.06f), 0.45f, 0.4f, 0.2f }
                       }, 0.7f });

        // ---------------------------------------------------------------- BizTechno
        s.push_back ({ Style::techno, "BizTechno Generator", "Four on the floor: beats, bass and stabs", 128.0, 0.0f, 31,
                       { 0, 2, 3, 5, 7, 8, 10 },
                       { { 0, 0, 0, 0 }, { 0, 0, 5, 0 }, { 0, 3, 0, 4 }, { 0, 0, 6, 5 } },
                       {
                           { "Beats", Role::beats, { {
                               { "Warehouse", Track::drums, "Dirty", [] (Track& t) {
                                   drum (t, kick, -3.0f, 0.65f, 0.3f, 0.6f, 0.35f, 1.0f);
                               } },
                               { "Minimal", Track::drums, "Tight", {} } } }, 0.85f, 0.1f, 0.0f },
                           { "Bass", Role::bass, { {
                               { "Acid line", Track::testSynth, "Acid line", {} },
                               { "FM bass", Track::fm, "FM bass", {} } } }, 0.75f, 0.0f, 0.1f },
                           { "Stabs", Role::melody, { {
                               { "Sync stab", Track::wavetable, "Sync stab", {} },
                               { "Metal hit", Track::fm, "Metal hit", {} } } }, 0.55f, 0.3f, 0.35f },
                           { "Sampler", Role::sampler, samplerSounds (0.05f, 40.0f, 0.0f), 0.45f, 0.2f, 0.3f }
                       }, 0.6f });

        // ---------------------------------------------------------------- BizNoise
        s.push_back ({ Style::noise, "BizNoise Generator", "Pads, grains and noise", 90.0, 0.0f, 38,
                       { 0, 2, 4, 7, 9 },
                       { { 0, 3, 4, 0 }, { 0, 2, 3, 4 } },
                       {
                           { "Pads", Role::pads, { {
                               { "Wind choir", Track::noise, "Pad: Wind choir", {} },
                               { "Deep space", Track::wavetable, "Pad: Deep space", {} } } }, 0.7f, 0.4f, 0.2f },
                           { "Granular", Role::granular, { {
                               { "Frozen choir", Track::granular, "Frozen choir", {} },
                               { "Glass mist", Track::granular, "Pad: Glass mist", {} } } }, 0.55f, 0.4f, 0.15f },
                           { "Noise", Role::noise, { {
                               { "Ocean", Track::noise, "Ocean waves", {} },
                               { "Radio", Track::noise, "Radio static", {} } } }, 0.6f, 0.3f, 0.1f },
                           { "Beats", Role::beats, { {
                               { "Soft kit", Track::drums, "Tight", [] (Track& t) {
                                   drum (t, kick, -2.0f, 0.6f, 0.35f, 0.2f, 0.0f, 0.8f);
                                   drum (t, closedHat, 2.0f, 0.3f, 0.5f, 0.2f, 0.0f, 0.45f);
                               } },
                               { "FM kit", Track::drums, "FM kit", {} } } }, 0.6f, 0.3f, 0.05f },
                           { "Sampler", Role::sampler, samplerSounds (0.3f, 16.0f, 0.08f), 0.5f, 0.45f, 0.2f }
                       }, 0.9f });

        // ---------------------------------------------------------------- BizSampler
        s.push_back ({ Style::sampler, "BizSampler", "Field recordings as grains and chops, over a beat", 92.0, 0.18f, 38,
                       { 0, 2, 3, 5, 7, 8, 10 },
                       { { 0, 5, 3, 4 }, { 0, 3, 5, 4 }, { 0, 0, 5, 5 } },
                       {
                           { "Field", Role::sampler, samplerSounds (0.16f, 22.0f, 0.04f), 0.6f, 0.35f, 0.15f },
                           { "Beats", Role::beats, { {
                               { "Dusty", Track::drums, "Standard", [] (Track& t) {
                                   OneShots::loadKit (t, OneShots::boomBap);
                                   inserts (t, InsertEffects::compressor, { 0.45f, 0.35f, 0.4f, 0.6f });
                               } },
                               { "Tight", Track::drums, "Tight", {} } } }, 0.75f, 0.05f, 0.0f },
                           { "Bass", Role::bass, { {
                               { "Round bass", Track::sampler, "One-shot", [] (Track& t) {
                                   OneShots::loadInstrument (t, OneShots::dustyBass);
                               } },
                               { "Sub", Track::sampler, "One-shot", [] (Track& t) {
                                   OneShots::loadInstrument (t, OneShots::eightOhEight);
                               } } } }, 0.55f },
                           { "Keys", Role::pads, { {
                               { "Glass mist", Track::granular, "Pad: Glass mist", {} },
                               { "Juno strings", Track::poly, "Pad: Juno strings", {} } } }, 0.45f, 0.4f, 0.15f }
                       }, 0.6f, "Vinyl crackle" });
        return s;
    }();
    return styles[(size_t) juce::jlimit (0, (int) styles.size() - 1, id)];
}

} // namespace bizgroove::lite
