#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // Performance, TestSynthParams
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** Settings of the FM synth: four sine operators that bend each other's
    pitch very fast, in one of eight layouts. Shared between the UI and the voices. */
struct FmParams
{
    static constexpr int numOperators = 4;

    std::atomic<int>   algorithm   { 0 };       // 0..7, see FmAlgorithms
    std::atomic<float> brightness  { 0.5f };    // 0..1, scales how hard the modulators bend
    std::array<std::atomic<float>, numOperators> ratio { { 1.0f, 2.0f, 3.0f, 1.0f } };   // pitch of each operator, times the note
    std::array<std::atomic<float>, numOperators> level { { 1.0f, 0.6f, 0.4f, 0.3f } };   // 0..1: loudness of a carrier, bend of a modulator
    std::atomic<float> feedback    { 0.0f };    // 0..1, operator 4 bends itself (from pure to buzzy)
    std::atomic<float> detuneCents { 0.0f };    // operators drift apart in tune, for a chorused, beating sound
    std::atomic<float> attack      { 0.002f };  // seconds, the whole sound
    std::atomic<float> decay       { 0.8f };
    std::atomic<float> sustain     { 0.6f };
    std::atomic<float> release     { 0.4f };
    std::atomic<float> biteDecay   { 0.4f };    // seconds: how quickly the modulators fade (the attack's bite)
    std::atomic<float> biteSustain { 0.4f };    // 0..1: how much of the bend remains while the key is held
    std::atomic<float> velocityBrightness { 0.5f };   // 0..1: harder hits sound brighter
    std::atomic<float> lfoRate     { 5.0f };    // Hz
    std::atomic<float> lfoPitch    { 0.0f };    // 0..1, vibrato
    std::atomic<float> lfoBrightness { 0.0f };  // 0..1
    std::atomic<float> glide       { 0.0f };    // seconds (per octave when gliding by distance)
    std::atomic<int>   glideMode   { Glide::fixedTime };

    mutable double lastNoteHz = 0.0;   // audio thread only: where a glide starts
    OscSettings osc;                    // the Oscillators section
};

/** The eight layouts. Operators are numbered 0..3 here and 1..4 to people;
    a modulator always has a higher number than what it bends. */
struct FmAlgorithms
{
    struct Layout
    {
        std::array<std::array<bool, 4>, 4> modulatedBy {};   // [target][source]
        std::array<bool, 4> carrier {};
        const char* name;
    };

    static const Layout& get (int index)
    {
        static const std::array<Layout, 8> layouts = make();
        return layouts[(size_t) juce::jlimit (0, 7, index)];
    }

private:
    static std::array<Layout, 8> make()
    {
        std::array<Layout, 8> a {};
        auto mod = [&a] (int alg, int target, int source) { a[(size_t) alg].modulatedBy[(size_t) target][(size_t) source] = true; };
        auto car = [&a] (int alg, int op) { a[(size_t) alg].carrier[(size_t) op] = true; };

        mod (0, 0, 1); mod (0, 1, 2); mod (0, 2, 3); car (0, 0);                 a[0].name = "4 > 3 > 2 > 1 (deep, metallic)";
        mod (1, 0, 1); mod (1, 1, 2); mod (1, 1, 3); car (1, 0);                 a[1].name = "3 + 4 > 2 > 1 (rich)";
        mod (2, 0, 1); mod (2, 0, 2); mod (2, 2, 3); car (2, 0);                 a[2].name = "2 + (4 > 3) > 1 (bells, keys)";
        mod (3, 0, 1); mod (3, 2, 3); car (3, 0); car (3, 2);                    a[3].name = "2 > 1 and 4 > 3 (two layers)";
        mod (4, 0, 3); mod (4, 1, 3); mod (4, 2, 3); car (4, 0); car (4, 1); car (4, 2); a[4].name = "4 > 1, 2, 3 (thick)";
        mod (5, 0, 1); car (5, 0); car (5, 2); car (5, 3);                       a[5].name = "2 > 1 plus two tones (organ-like)";
        mod (6, 0, 1); mod (6, 0, 2); mod (6, 0, 3); car (6, 0);                 a[6].name = "2 + 3 + 4 > 1 (bright, brassy)";
        for (int op = 0; op < 4; ++op)
            car (7, op);
        a[7].name = "1, 2, 3, 4 (four tones, organ)";
        return a;
    }
};

struct FmSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** One voice: four sine operators in the chosen layout; carriers follow the
    main envelope, modulators follow the "bite" envelope, so the tone can
    start bright and settle. */
class FmVoice final : public juce::SynthesiserVoice
{
public:
    FmVoice (const FmParams& p, const Performance& perf) : params (p), performance (perf) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<FmSound*> (s) != nullptr; }

    void prepare (double sampleRate)
    {
        ampEnv.setSampleRate (sampleRate);
        biteEnv.setSampleRate (sampleRate);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        noteHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        glide.start (params.lastNoteHz, noteHz, params.glide.load(), params.glideMode.load(), getSampleRate());
        params.lastNoteHz = noteHz;
        if (! ampEnv.isActive())
        {
            phases = {};
            fb1 = fb2 = 0.0f;
            layer.reset();
        }
        const float vs = juce::jlimit (0.0f, 1.0f, params.velocityBrightness.load());
        velocityBright = (1.0f - vs) + vs * velocity;
        level = 0.25f * velocity;

        ampEnv.setParameters ({ params.attack.load(), params.decay.load(), params.sustain.load(), params.release.load() });
        biteEnv.setParameters ({ juce::jmin (params.attack.load(), 0.05f), params.biteDecay.load(), params.biteSustain.load(), params.release.load() });
        ampEnv.noteOn();
        biteEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            biteEnv.noteOff();
        }
        else
        {
            ampEnv.reset();
            biteEnv.reset();
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
        const double twoPi = juce::MathConstants<double>::twoPi;
        const double vibratoStep = twoPi * TestSynthParams::vibratoRateHz / sr;
        const double lfoStep = twoPi * juce::jlimit (0.05f, 30.0f, params.lfoRate.load()) / sr;
        const auto& layout = FmAlgorithms::get (params.algorithm.load());
        const float brightness = juce::jlimit (0.0f, 1.0f, params.brightness.load());
        const float lfoBright = juce::jlimit (0.0f, 1.0f, params.lfoBrightness.load());
        const float fbAmount = 1.6f * juce::jlimit (0.0f, 1.0f, params.feedback.load());
        const int numChannels = output.getNumChannels();

        std::array<float, 4> opLevel {};
        int carriers = 0;
        for (size_t op = 0; op < 4; ++op)
        {
            opLevel[op] = juce::jlimit (0.0f, 1.0f, params.level[op].load());
            if (layout.carrier[op])
                ++carriers;
        }
        const float carrierGain = 1.0f / std::sqrt ((float) juce::jmax (1, carriers));

        constexpr int controlInterval = 16;
        std::array<double, 4> inc {};
        float bendScale = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            vibratoPhase += vibratoStep;
            if (vibratoPhase > twoPi) vibratoPhase -= twoPi;
            lfoPhase += lfoStep;
            if (lfoPhase > twoPi) lfoPhase -= twoPi;

            if ((i % controlInterval) == 0)
            {
                const float lfo = (float) std::sin (lfoPhase);
                const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                                       + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (vibratoPhase)
                                       + 0.5 * params.lfoPitch.load() * lfo;
                const double hz = glide.advance (controlInterval) * std::pow (2.0, semitones / 12.0);
                layer.setPitch (hz, sr, params.osc);
                const double cents = params.detuneCents.load();
                float highest = 1.0f;
                for (size_t op = 0; op < 4; ++op)
                {
                    const double r = juce::jlimit (0.125, 32.0, (double) params.ratio[op].load());
                    const double drift = (op % 2 == 0 ? -0.5 : 0.5) * cents * (1.0 + 0.3 * (double) op);
                    inc[op] = hz * r * std::pow (2.0, drift / 1200.0) / sr;
                    if (! layout.carrier[op])
                        highest = juce::jmax (highest, (float) r);
                }
                // High notes bend less, so the sound stays clean at the top of the keyboard.
                const float clean = juce::jlimit (0.1f, 1.0f, (float) (3000.0 / (hz * highest)));
                bendScale = 7.0f * juce::jlimit (0.0f, 1.5f, brightness * velocityBright * (1.0f + 0.5f * lfoBright * lfo)) * std::sqrt (clean);
            }

            const float amp = ampEnv.getNextSample();
            const float bite = biteEnv.getNextSample();

            std::array<float, 4> out {};
            for (int op = 3; op >= 0; --op)
            {
                const auto o = (size_t) op;
                float phaseMod = 0.0f;
                for (size_t src = o + 1; src < 4; ++src)
                    if (layout.modulatedBy[o][src])
                        phaseMod += out[src];
                if (op == 3)
                    phaseMod += fbAmount * 0.5f * (fb1 + fb2);

                const float s = (float) std::sin (twoPi * phases[o] + (double) phaseMod);
                if (op == 3)
                {
                    fb2 = fb1;
                    fb1 = s;
                }
                // A carrier's level is its loudness; a modulator's is how hard it bends (in radians).
                out[o] = layout.carrier[o] ? s * opLevel[o] : s * opLevel[o] * bendScale * bite;

                phases[o] += inc[o];
                if (phases[o] >= 1.0) phases[o] -= 1.0;
            }

            float mixed = 0.0f;
            for (size_t op = 0; op < 4; ++op)
                if (layout.carrier[op])
                    mixed += out[op];

            const float sample = (params.osc.main.load() * mixed * carrierGain + 2.0f * layer.next (params.osc)) * amp * level;
            for (int ch = 0; ch < numChannels; ++ch)
                output.addSample (ch, startSample + i, sample);

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    const FmParams& params;
    const Performance& performance;
    double noteHz = 440.0;
    Glide glide;
    OscLayer layer;
    double vibratoPhase = 0.0, lfoPhase = 0.0;
    std::array<double, 4> phases {};
    float fb1 = 0.0f, fb2 = 0.0f;
    float velocityBright = 1.0f, level = 0.0f;
    juce::ADSR ampEnv, biteEnv;
};

/** The operator pitches the Ratio knobs step through. */
inline const std::array<float, 16>& fmRatioSteps()
{
    static const std::array<float, 16> steps { { 0.5f, 1.0f, 1.41f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 11.0f, 13.0f, 15.0f } };
    return steps;
}

/** The FM settings as JSON, for projects and saved sounds. */
inline juce::var fmParamsToVar (const FmParams& fp)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("algorithm", fp.algorithm.load());   o->setProperty ("brightness", fp.brightness.load());
    juce::Array<juce::var> ratios, levels;
    for (size_t op = 0; op < FmParams::numOperators; ++op)
    {
        ratios.add (fp.ratio[op].load());
        levels.add (fp.level[op].load());
    }
    o->setProperty ("ratios", ratios);                   o->setProperty ("levels", levels);
    o->setProperty ("feedback", fp.feedback.load());     o->setProperty ("detune", fp.detuneCents.load());
    o->setProperty ("attack", fp.attack.load());         o->setProperty ("decay", fp.decay.load());
    o->setProperty ("sustain", fp.sustain.load());       o->setProperty ("release", fp.release.load());
    o->setProperty ("biteDecay", fp.biteDecay.load());   o->setProperty ("biteSustain", fp.biteSustain.load());
    o->setProperty ("velocityBrightness", fp.velocityBrightness.load());
    o->setProperty ("lfoRate", fp.lfoRate.load());       o->setProperty ("lfoPitch", fp.lfoPitch.load());
    o->setProperty ("lfoBrightness", fp.lfoBrightness.load());
    o->setProperty ("glide", fp.glide.load());           o->setProperty ("glideMode", fp.glideMode.load());
    o->setProperty ("osc", oscToVar (fp.osc));
    return juce::var (o);
}

/** Reads settings written by fmParamsToVar(); anything missing gets its default. */
inline void applyFmParams (FmParams& fp, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    static constexpr float defaultRatio[4] = { 1.0f, 2.0f, 3.0f, 1.0f };
    static constexpr float defaultLevel[4] = { 1.0f, 0.6f, 0.4f, 0.3f };

    fp.algorithm.store (juce::jlimit (0, 7, i ("algorithm", 0)));
    fp.brightness.store (f ("brightness", 0.5f));
    const auto* ratios = v["ratios"].getArray();
    const auto* levels = v["levels"].getArray();
    for (int op = 0; op < FmParams::numOperators; ++op)
    {
        fp.ratio[(size_t) op].store (ratios != nullptr && op < ratios->size() ? (float) (double) (*ratios)[op] : defaultRatio[op]);
        fp.level[(size_t) op].store (levels != nullptr && op < levels->size() ? (float) (double) (*levels)[op] : defaultLevel[op]);
    }
    fp.feedback.store (f ("feedback", 0.0f));        fp.detuneCents.store (f ("detune", 0.0f));
    fp.attack.store (f ("attack", 0.002f));          fp.decay.store (f ("decay", 0.8f));
    fp.sustain.store (f ("sustain", 0.6f));          fp.release.store (f ("release", 0.4f));
    fp.biteDecay.store (f ("biteDecay", 0.4f));      fp.biteSustain.store (f ("biteSustain", 0.4f));
    fp.velocityBrightness.store (f ("velocityBrightness", 0.5f));
    fp.lfoRate.store (f ("lfoRate", 5.0f));          fp.lfoPitch.store (f ("lfoPitch", 0.0f));
    fp.lfoBrightness.store (f ("lfoBrightness", 0.0f));
    fp.glide.store (f ("glide", 0.0f));
    fp.glideMode.store (juce::jlimit (0, Glide::numModes - 1, i ("glideMode", 0)));
    applyOsc (fp.osc, v["osc"]);
}

} // namespace bizgroove
