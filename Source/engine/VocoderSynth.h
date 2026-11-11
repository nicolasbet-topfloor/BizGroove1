#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // Performance, TestSynthParams, Glide, OscSettings
#include "Vocoder.h"
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** Settings of the Vocoder instrument: a bright synth (the carrier) that you
    play on the keys, made to speak by what the vocoder listens to: a
    microphone, the Voice tracks and any track, mixed. */
struct VocoderParams
{
    enum Wave { saw = 0, square, pulse, threeSaws, numWaves };

    static juce::String waveName (int w)
    {
        static const char* names[numWaves] = { "Saw", "Square", "Thin pulse", "Three saws" };
        return names[juce::jlimit (0, numWaves - 1, w)];
    }

    VocoderSettings vocoder;   // what it listens to, and how

    std::atomic<int>   wave      { threeSaws };
    std::atomic<float> detune    { 0.3f };     // 0..1: the three saws (and the stereo spread) apart, up to 30 cents
    std::atomic<float> noise     { 0.1f };     // 0..1: breath in the carrier, so whispers and hiss come through
    std::atomic<float> attack    { 0.01f };    // seconds
    std::atomic<float> release   { 0.25f };
    std::atomic<float> level     { 1.0f };     // 0.25..4
    std::atomic<float> velocity  { 0.3f };
    std::atomic<float> glide     { 0.0f };
    std::atomic<int>   glideMode { Glide::fixedTime };

    mutable double lastNoteHz = 0.0;   // audio thread only
    OscSettings osc;                    // the Oscillators section: more carrier
};

struct VocoderSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** One carrier voice: the synth that the vocoder makes speak. */
class VocoderVoice final : public juce::SynthesiserVoice
{
public:
    VocoderVoice (const VocoderParams& p, const Performance& perf, std::uint32_t seed)
        : params (p), performance (perf), noiseState (seed | 1u) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<VocoderSound*> (s) != nullptr; }

    void prepare (double sampleRate) { ampEnv.setSampleRate (sampleRate); }

    void startNote (int midiNote, float vel, juce::SynthesiserSound*, int) override
    {
        noteHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        glide.start (params.lastNoteHz, noteHz, params.glide.load(), params.glideMode.load(), getSampleRate());
        params.lastNoteHz = noteHz;
        if (! ampEnv.isActive())
        {
            phases = { 0.0, 0.37, 0.71, 0.13 };
            layer.reset();
        }
        const float vs = juce::jlimit (0.0f, 1.0f, params.velocity.load());
        gain = 0.25f * juce::jlimit (0.25f, 4.0f, params.level.load()) * ((1.0f - vs) + vs * vel);
        ampEnv.setParameters ({ params.attack.load(), 0.05f, 1.0f, params.release.load() });
        ampEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
            ampEnv.noteOff();
        else
        {
            ampEnv.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;
        const double sr = getSampleRate();
        const int wave = juce::jlimit (0, VocoderParams::numWaves - 1, params.wave.load());
        const float detune = juce::jlimit (0.0f, 1.0f, params.detune.load());
        const float noiseLevel = juce::jlimit (0.0f, 1.0f, params.noise.load());
        const float main = params.osc.main.load();
        const int numChannels = output.getNumChannels();
        constexpr int controlInterval = 16;

        for (int i = 0; i < numSamples; ++i)
        {
            if ((i % controlInterval) == 0)
            {
                vibratoPhase += juce::MathConstants<double>::twoPi * TestSynthParams::vibratoRateHz * controlInterval / sr;
                if (vibratoPhase > juce::MathConstants<double>::twoPi) vibratoPhase -= juce::MathConstants<double>::twoPi;
                const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                                       + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (vibratoPhase);
                const double hz = glide.advance (controlInterval) * std::pow (2.0, semitones / 12.0);
                const double spread = 30.0 * detune;   // cents
                incs[0] = hz / sr;
                incs[1] = hz * std::pow (2.0, -spread / 1200.0) / sr;
                incs[2] = hz * std::pow (2.0, spread / 1200.0) / sr;
                incs[3] = hz * std::pow (2.0, spread * 0.5 / 1200.0) / sr;
                layer.setPitch (hz, sr, params.osc);
            }

            float left = 0.0f, right = 0.0f;
            switch (wave)
            {
                case VocoderParams::square:
                case VocoderParams::pulse:
                {
                    const double w = wave == VocoderParams::square ? 0.5 : 0.18;
                    const float a = pulseWave (0, w), b = pulseWave (3, w);
                    left = 0.8f * a + 0.2f * detune * b;
                    right = 0.8f * a - 0.2f * detune * b;
                    break;
                }
                case VocoderParams::threeSaws:
                {
                    const float a = sawWave (0), b = sawWave (1), c = sawWave (2);
                    left = 0.5f * a + 0.45f * b + 0.25f * c;
                    right = 0.5f * a + 0.25f * b + 0.45f * c;
                    break;
                }
                default:
                {
                    const float a = sawWave (0), b = sawWave (3);
                    left = 0.85f * a + 0.15f * detune * b;
                    right = 0.85f * a - 0.15f * detune * b;
                    break;
                }
            }
            const float extra = 2.0f * layer.next (params.osc);
            const float n = white() * noiseLevel * 0.8f;
            const float amp = ampEnv.getNextSample() * gain;
            if (numChannels > 0) output.addSample (0, startSample + i, (main * left + extra + n) * amp);
            if (numChannels > 1) output.addSample (1, startSample + i, (main * right + extra + n) * amp);

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    static double blep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    float sawWave (size_t k)
    {
        const double p = phases[k], dt = juce::jlimit (1.0e-6, 0.45, incs[k]);
        phases[k] = p + dt >= 1.0 ? p + dt - 1.0 : p + dt;
        return (float) (2.0 * p - 1.0 - blep (p, dt));
    }

    float pulseWave (size_t k, double w)
    {
        const double p = phases[k], dt = juce::jlimit (1.0e-6, 0.45, incs[k]);
        phases[k] = p + dt >= 1.0 ? p + dt - 1.0 : p + dt;
        double v = p < w ? 1.0 : -1.0;
        v += blep (p, dt);
        double q = p - w;
        if (q < 0.0) q += 1.0;
        v -= blep (q, dt);
        return (float) v;
    }

    float white()
    {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return (float) (std::int32_t) noiseState * (1.0f / 2147483648.0f);
    }

    const VocoderParams& params;
    const Performance& performance;
    std::array<double, 4> phases { { 0.0, 0.37, 0.71, 0.13 } }, incs {};
    double noteHz = 220.0, vibratoPhase = 0.0;
    float gain = 0.25f;
    Glide glide;
    OscLayer layer;
    std::uint32_t noiseState;
    juce::ADSR ampEnv;
};

inline juce::var vocoderParamsToVar (const VocoderParams& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("vocoder", vocoderSettingsToVar (p.vocoder));
    o->setProperty ("wave", p.wave.load());            o->setProperty ("detune", p.detune.load());
    o->setProperty ("noise", p.noise.load());          o->setProperty ("attack", p.attack.load());
    o->setProperty ("release", p.release.load());      o->setProperty ("level", p.level.load());
    o->setProperty ("velocity", p.velocity.load());    o->setProperty ("glide", p.glide.load());
    o->setProperty ("glideMode", p.glideMode.load());  o->setProperty ("osc", oscToVar (p.osc));
    return juce::var (o);
}

inline void applyVocoderParams (VocoderParams& p, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    applyVocoderSettings (p.vocoder, v["vocoder"]);
    p.wave.store (juce::jlimit (0, VocoderParams::numWaves - 1, i ("wave", VocoderParams::threeSaws)));
    p.detune.store (f ("detune", 0.3f));          p.noise.store (f ("noise", 0.1f));
    p.attack.store (f ("attack", 0.01f));         p.release.store (f ("release", 0.25f));
    p.level.store (f ("level", 1.0f));            p.velocity.store (f ("velocity", 0.3f));
    p.glide.store (f ("glide", 0.0f));
    p.glideMode.store (juce::jlimit (0, Glide::numModes - 1, i ("glideMode", 0)));
    applyOsc (p.osc, v["osc"]);
}

} // namespace bizgroove
