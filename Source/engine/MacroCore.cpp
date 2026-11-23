// The Macro Synth's link to the sound engines by Emilie Gillet (MIT licence,
// see ThirdParty/eurorack). Built in the bizmacro library with TEST defined,
// as the authors' own computer test program is.

#include "MacroCore.h"
#include "plaits/dsp/voice.h"
#include <algorithm>
#include <array>

namespace bizgroove
{

namespace
{
    struct ModelInfo { const char* name; int engine; float gain; bool selfEnveloped; const char* help; };

    // engine: the engine's place in the original voice (see voice.cc); gain evens out their levels
    // (measured on a held A3, to about -24 dB like the other instruments; struck ones a little less).
    // The chiptune engine shapes its own notes when struck, so it counts as self-enveloped.
    const std::array<ModelInfo, MacroCore::numModels>& models()
    {
        static const std::array<ModelInfo, MacroCore::numModels> list { {
            { "Waveshaper",       9, 0.60f, false, "A wave shaped and folded over on itself. Harmonics: the shaper's curve. Timbre: how hard it folds (brightness). Morph: the shape of the wave going in." },
            { "Phase distortion", 1, 0.40f, false, "Phase distortion, as on 1980s Casio CZ synths. Harmonics: the pitch of the wave that bends it. Timbre: how much it bends. Morph: how lopsided the wave is." },
            { "Additive",        12, 0.85f, false, "32 harmonics added up, like an organ. Harmonics: peaks among them. Timbre: where the strongest sit (brightness). Morph: the balance of low and high ones." },
            { "Formant",         11, 0.67f, false, "Vowel-like, singing tones. Harmonics: the gap between its two resonances. Timbre: where they sit (the vowel). Morph: the shape of the tone underneath." },
            { "Wave terrain",     5, 0.54f, false, "A circle traced across a landscape of waves. Harmonics: which of 8 landscapes. Timbre: the circle's size (brightness). Morph: where it sits on the landscape." },
            { "Chords",          14, 0.54f, false, "A whole chord on each key. Harmonics: which chord. Timbre: its voicing (inversion). Morph: the waves it is made of." },
            { "String machine",   6, 1.50f, false, "The string ensembles of the 1970s, with chorus. Harmonics: the chord. Timbre: the filter (brightness). Morph: the mix of tones (registration)." },
            { "Swarm",           16, 0.83f, false, "A swarm of oscillators drifting together: big and thick. Harmonics: how far apart they drift. Timbre: how often new ones start. Morph: from long to short ones." },
            { "DX FM",            2, 0.55f, true,  "Six-operator FM, as on the Yamaha DX7: three banks of 32 sounds. Harmonics: which sound. Timbre: brightness. Morph: the envelopes' speed." },
            { "Chiptune",         7, 0.42f, true, "The sound of 8-bit game consoles. Harmonics: the chord. Timbre: the arpeggio pattern. Morph: the pulse width." },
            { "Plucked string",  19, 2.00f, true,  "A plucked string, modelled. Harmonics: the string's material. Timbre: where it is plucked (brightness). Morph: how long it rings." },
            { "Modal",           20, 1.17f, true,  "Struck bells, wood and metal bars, modelled. Harmonics: the material. Timbre: where it is struck. Morph: how long it rings." },
            { "Particles",       18, 1.60f, false, "Drops and sparks through resonant filters. Harmonics: how much their pitches vary. Timbre: how many. Morph: the filter and its echoes." }
        } };
        return list;
    }

    const ModelInfo& info (int model) { return models()[(size_t) std::clamp (model, 0, MacroCore::numModels - 1)]; }
}

const char* MacroCore::modelName (int model)  { return info (model).name; }
const char* MacroCore::modelHelp (int model)  { return info (model).help; }
bool MacroCore::selfEnveloped (int model)     { return info (model).selfEnveloped; }

struct MacroCore::Impl
{
    Impl()
    {
        allocator.Init (ram, sizeof (ram));
        voice.Init (&allocator);
    }

    plaits::Voice voice;
    char ram[16384];   // the engines' shared memory, as on the module
    stmlib::BufferAllocator allocator;
    plaits::Voice::Frame frames[blockSize];
};

MacroCore::MacroCore() : impl (std::make_unique<Impl>()) {}
MacroCore::~MacroCore() = default;

void MacroCore::reset()
{
    impl->allocator.Init (impl->ram, sizeof (impl->ram));
    impl->voice.Init (&impl->allocator);
}

void MacroCore::render (const Settings& s, float* main, float* aux)
{
    const auto& m = info (s.model);
    int engine = m.engine;
    if (s.model == dxFm)
        engine += std::clamp (s.dxBank, 0, 2);   // the three DX banks sit side by side

    plaits::Patch patch {};
    patch.note = s.note;
    patch.harmonics = std::clamp (s.harmonics, 0.0f, 1.0f);
    patch.timbre = std::clamp (s.timbre, 0.0f, 1.0f);
    patch.morph = std::clamp (s.morph, 0.0f, 1.0f);
    patch.frequency_modulation_amount = std::clamp (s.pitchEnv, -1.0f, 1.0f);
    patch.timbre_modulation_amount = std::clamp (s.timbreEnv, -1.0f, 1.0f);
    patch.morph_modulation_amount = std::clamp (s.morphEnv, -1.0f, 1.0f);
    patch.engine = engine;
    patch.decay = std::clamp (s.decay, 0.0f, 1.0f);
    patch.lpg_colour = std::clamp (s.colour, 0.0f, 1.0f);

    plaits::Modulations mod {};
    mod.trigger = s.gate ? 1.0f : 0.0f;
    mod.level = std::clamp (s.level, 0.0f, 1.0f);
    mod.trigger_patched = true;   // each key strikes the engine and starts its own envelope
    mod.level_patched = true;     // the gate follows the note's envelope, so held notes sustain

    impl->voice.Render (patch, mod, impl->frames, (size_t) blockSize);
    for (int i = 0; i < blockSize; ++i)
    {
        main[i] = m.gain * (float) impl->frames[i].out / 32768.0f;
        aux[i] = m.gain * (float) impl->frames[i].aux / 32768.0f;
    }
}

} // namespace bizgroove
