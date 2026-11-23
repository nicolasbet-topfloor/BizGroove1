#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // Performance, TestSynthParams, Glide
#include "MacroCore.h"
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** Settings of the Macro Synth: thirteen sound models on one instrument, each
    played with the same three knobs (Harmonics, Timbre, Morph), through a
    low-pass gate. The models are by Emilie Gillet (MIT licence, see
    ThirdParty/eurorack). Shared between the UI and the voices. */
struct MacroParams
{
    std::atomic<int>   model      { MacroCore::waveshaper };
    std::atomic<int>   dxBank     { 0 };       // the DX FM's bank, 0..2
    std::atomic<float> harmonics  { 0.5f };    // 0..1
    std::atomic<float> timbre     { 0.5f };
    std::atomic<float> morph      { 0.5f };
    std::atomic<float> decay      { 0.5f };    // 0..1: the gate's decay (and the release of struck models)
    std::atomic<float> colour     { 0.5f };    // 0..1: the gate, from a pure volume change to a filter too
    std::atomic<float> attack     { 0.002f };  // seconds
    std::atomic<float> release    { 0.3f };    // seconds
    std::atomic<float> timbreEnv  { 0.0f };    // -1..1: each note's envelope on Timbre
    std::atomic<float> morphEnv   { 0.0f };    // -1..1: on Morph
    std::atomic<float> pitchEnv   { 0.0f };    // -1..1: on the pitch
    std::atomic<float> aux        { 0.0f };    // 0..1: the main output, or the model's second ("aux") version
    std::atomic<float> width      { 0.0f };    // 0..1: main on the left and aux on the right
    std::atomic<float> velocity   { 0.6f };    // 0..1: how much harder hits are louder
    std::atomic<float> level      { 1.0f };    // 0.25..4
    std::atomic<float> glide      { 0.0f };    // seconds (per octave when gliding by distance)
    std::atomic<int>   glideMode  { Glide::fixedTime };

    mutable double lastNoteHz = 0.0;   // audio thread only: where a glide starts
};

struct MacroSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** One voice: a model running at 48 kHz, converted to the sound card's rate. */
class MacroVoice final : public juce::SynthesiserVoice
{
public:
    MacroVoice (const MacroParams& p, const Performance& perf) : params (p), performance (perf) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<MacroSound*> (s) != nullptr; }

    void prepare (double sampleRate)
    {
        step = MacroCore::sampleRate / juce::jmax (8000.0, sampleRate);
        ampEnv.setSampleRate (sampleRate);
        resetResampler();
    }

    void startNote (int midiNote, float vel, juce::SynthesiserSound*, int) override
    {
        noteHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        glide.start (params.lastNoteHz, noteHz, params.glide.load(), params.glideMode.load(), MacroCore::sampleRate);
        params.lastNoteHz = noteHz;
        const bool wasSounding = isVoiceActive() && (ampEnv.isActive() || tailBlocks > 0);
        if (! wasSounding)
        {
            core.reset();
            resetResampler();
        }
        // A new strike: the gate goes low for a moment first, so a note played over a ringing one strikes again.
        retrigger = wasSounding ? 8 : 0;
        const float vs = juce::jlimit (0.0f, 1.0f, params.velocity.load());
        velocityGain = (1.0f - vs) + vs * vel;
        ampEnv.setParameters ({ juce::jmax (0.001f, params.attack.load()), 0.0f, 1.0f, juce::jmax (0.005f, params.release.load()) });
        ampEnv.reset();
        ampEnv.noteOn();
        gate = true;
        quietBlocks = 0;
        tailBlocks = maxTailBlocks;
    }

    void stopNote (float, bool allowTailOff) override
    {
        gate = false;
        if (allowTailOff)
            ampEnv.noteOff();
        else
        {
            ampEnv.reset();
            tailBlocks = 0;
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const float aux = juce::jlimit (0.0f, 1.0f, params.aux.load());
        const float width = juce::jlimit (0.0f, 1.0f, params.width.load());
        const float gain = 0.5f * juce::jlimit (0.25f, 4.0f, params.level.load());
        const int numChannels = output.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            const float env = ampEnv.getNextSample();
            // Cubic interpolation between the 48 kHz samples.
            while (phase >= 1.0)
            {
                phase -= 1.0;
                if (fifoRead >= MacroCore::blockSize)
                    renderCoreBlock (env);
                for (size_t k = 0; k < 3; ++k) { histL[k] = histL[k + 1]; histR[k] = histR[k + 1]; }
                histL[3] = mainBlock[(size_t) fifoRead];
                histR[3] = auxBlock[(size_t) fifoRead];
                ++fifoRead;
            }
            const float t = (float) phase;
            const float m = hermite (histL.data(), t), a = hermite (histR.data(), t);
            phase += step;

            const float mono = m + aux * (a - m);
            float left = mono + width * (m - mono), right = mono + width * (a - mono);
            // Struck models shape their own notes, so the envelope only fades them when the key is let go.
            const float amp = selfEnveloped ? env : 1.0f;
            left *= gain * amp;
            right *= gain * amp;
            output.addSample (0, startSample + i, left);
            if (numChannels > 1)
                output.addSample (1, startSample + i, right);

            if (tailBlocks <= 0)
            {
                clearCurrentNote();
                ampEnv.reset();
                break;
            }
        }
    }

private:
    static float hermite (const float* y, float t)
    {
        const float c0 = y[1], c1 = 0.5f * (y[2] - y[0]);
        const float c2 = y[0] - 2.5f * y[1] + 2.0f * y[2] - 0.5f * y[3];
        const float c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    void resetResampler()
    {
        phase = 1.0;
        fifoRead = MacroCore::blockSize;
        histL.fill (0.0f);
        histR.fill (0.0f);
    }

    void renderCoreBlock (float env)
    {
        MacroCore::Settings s;
        s.model = juce::jlimit (0, MacroCore::numModels - 1, params.model.load());
        s.dxBank = params.dxBank.load();
        selfEnveloped = MacroCore::selfEnveloped (s.model);

        // Pitch: the glide, the bend and the mod wheel's vibrato.
        vibratoPhase += juce::MathConstants<double>::twoPi * TestSynthParams::vibratoRateHz * MacroCore::blockSize / MacroCore::sampleRate;
        if (vibratoPhase > juce::MathConstants<double>::twoPi) vibratoPhase -= juce::MathConstants<double>::twoPi;
        const double hz = glide.advance (MacroCore::blockSize);
        const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                               + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (vibratoPhase);
        s.note = (float) (69.0 + 12.0 * std::log2 (juce::jmax (1.0, hz) / 440.0) + semitones);

        s.harmonics = params.harmonics.load();
        s.timbre = params.timbre.load();
        s.morph = params.morph.load();
        s.decay = params.decay.load();
        s.colour = params.colour.load();
        s.timbreEnv = params.timbreEnv.load();
        s.morphEnv = params.morphEnv.load();
        s.pitchEnv = params.pitchEnv.load();
        s.gate = gate && retrigger <= 0;
        s.level = (gate || ampEnv.isActive() ? env : 0.0f) * velocityGain * (retrigger > 0 ? 0.0f : 1.0f);
        if (retrigger > 0) --retrigger;
        if (selfEnveloped)
            s.level = velocityGain;   // their loudness on the strike; the envelope fades them on release

        core.render (s, mainBlock.data(), auxBlock.data());
        fifoRead = 0;

        // The voice ends once the key is up, the envelope has finished and the sound has died away.
        float peak = 0.0f;
        for (int k = 0; k < MacroCore::blockSize; ++k)
            peak = juce::jmax (peak, std::abs (mainBlock[(size_t) k]), std::abs (auxBlock[(size_t) k]));
        quietBlocks = peak < 1.0e-4f ? quietBlocks + 1 : 0;
        if (! gate)
        {
            --tailBlocks;
            const bool envDone = ! ampEnv.isActive();
            if ((envDone && selfEnveloped) || (envDone && quietBlocks > 400))   // 0.1 s of silence
                tailBlocks = 0;
        }
    }

    static constexpr int maxTailBlocks = (int) (20.0 * MacroCore::sampleRate / MacroCore::blockSize);   // 20 s at most

    const MacroParams& params;
    const Performance& performance;
    MacroCore core;
    std::array<float, MacroCore::blockSize> mainBlock {}, auxBlock {};
    std::array<float, 4> histL {}, histR {};
    int fifoRead = MacroCore::blockSize;
    double phase = 1.0, step = 1.0;
    double noteHz = 440.0, vibratoPhase = 0.0;
    Glide glide;
    juce::ADSR ampEnv;
    float velocityGain = 1.0f;
    bool gate = false, selfEnveloped = false;
    int retrigger = 0, quietBlocks = 0, tailBlocks = 0;
};

/** The Macro Synth settings as JSON, for projects and saved sounds. */
inline juce::var macroParamsToVar (const MacroParams& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("model", p.model.load());          o->setProperty ("dxBank", p.dxBank.load());
    o->setProperty ("harmonics", p.harmonics.load());  o->setProperty ("timbre", p.timbre.load());
    o->setProperty ("morph", p.morph.load());          o->setProperty ("decay", p.decay.load());
    o->setProperty ("colour", p.colour.load());        o->setProperty ("attack", p.attack.load());
    o->setProperty ("release", p.release.load());      o->setProperty ("timbreEnv", p.timbreEnv.load());
    o->setProperty ("morphEnv", p.morphEnv.load());    o->setProperty ("pitchEnv", p.pitchEnv.load());
    o->setProperty ("aux", p.aux.load());              o->setProperty ("width", p.width.load());
    o->setProperty ("velocity", p.velocity.load());    o->setProperty ("level", p.level.load());
    o->setProperty ("glide", p.glide.load());          o->setProperty ("glideMode", p.glideMode.load());
    return juce::var (o);
}

/** Reads settings written by macroParamsToVar(); anything missing gets its default. */
inline void applyMacroParams (MacroParams& p, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    p.model.store (juce::jlimit (0, MacroCore::numModels - 1, i ("model", 0)));
    p.dxBank.store (juce::jlimit (0, 2, i ("dxBank", 0)));
    p.harmonics.store (f ("harmonics", 0.5f));   p.timbre.store (f ("timbre", 0.5f));
    p.morph.store (f ("morph", 0.5f));           p.decay.store (f ("decay", 0.5f));
    p.colour.store (f ("colour", 0.5f));         p.attack.store (f ("attack", 0.002f));
    p.release.store (f ("release", 0.3f));       p.timbreEnv.store (f ("timbreEnv", 0.0f));
    p.morphEnv.store (f ("morphEnv", 0.0f));     p.pitchEnv.store (f ("pitchEnv", 0.0f));
    p.aux.store (f ("aux", 0.0f));               p.width.store (f ("width", 0.0f));
    p.velocity.store (f ("velocity", 0.6f));     p.level.store (f ("level", 1.0f));
    p.glide.store (f ("glide", 0.0f));
    p.glideMode.store (juce::jlimit (0, Glide::numModes - 1, i ("glideMode", 0)));
}

} // namespace bizgroove
