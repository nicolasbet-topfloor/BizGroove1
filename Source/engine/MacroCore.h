#pragma once

#include <memory>

namespace bizgroove
{

/** One voice of the Macro Synth's sound engines (by Emilie Gillet, MIT
    licence: see ThirdParty/eurorack). They run at 48 kHz in blocks of 12
    samples; MacroVoice converts to the sound card's rate. Only MacroCore.cpp
    sees the engines' own code. Audio thread. */
class MacroCore
{
public:
    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 12;

    /** The models a person chooses from, in menu order. */
    enum Model { waveshaper = 0, phaseDistortion, additive, formant, waveTerrain, chords, stringMachine,
                 swarm, dxFm, chiptune, plucked, modal, particles, numModels };

    static const char* modelName (int model);
    /** What Harmonics, Timbre and Morph do on this model, for the knobs' help. */
    static const char* modelHelp (int model);
    /** Models that shape their own notes (the DX FM, the chiptune, the plucked string,
        the modal resonator): the low-pass gate leaves them alone, so their own decay rules. */
    static bool selfEnveloped (int model);

    struct Settings
    {
        int model = 0;
        int dxBank = 0;         // the DX FM's three banks of 32 sounds
        float note = 60.0f;     // in semitones, as MIDI notes (fractions allowed)
        float harmonics = 0.5f, timbre = 0.5f, morph = 0.5f;   // 0..1
        float decay = 0.5f;     // 0..1: the gate's decay
        float colour = 0.5f;    // 0..1: the gate from a pure volume change to a filter too
        float timbreEnv = 0.0f, morphEnv = 0.0f, pitchEnv = 0.0f;   // -1..1: each note's own envelope on them
        float level = 0.0f;     // 0..1: how open the gate is (the note's envelope and velocity)
        bool gate = false;      // a key is down
    };

    MacroCore();
    ~MacroCore();

    /** Renders one block of blockSize samples at 48 kHz: the main output and the
        second ("aux") one, each a variation of the sound. */
    void render (const Settings& s, float* main, float* aux);

    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace bizgroove
