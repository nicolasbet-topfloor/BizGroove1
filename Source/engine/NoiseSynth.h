#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // Performance, TestSynthParams
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace bizgroove
{

/** Settings of the Noise instrument: noise of any colour, from deep brown to
    hissing violet, made into sound in one of five ways, then tuned, filtered
    and shaped. Shared between the UI and the voices. */
struct NoiseParams
{
    enum Source { pure = 0, fmNoise, ring, digital, crackle, numSources };

    static juce::String sourceName (int s)
    {
        static const char* names[numSources] = { "Coloured noise", "FM noise", "Ring noise", "Digital noise", "Crackle" };
        return names[juce::jlimit (0, numSources - 1, s)];
    }

    enum FilterMode { lowPass = 0, bandPass, highPass, numFilterModes };

    static juce::String filterName (int m)
    {
        static const char* names[numFilterModes] = { "Low-pass", "Band-pass", "High-pass" };
        return names[juce::jlimit (0, numFilterModes - 1, m)];
    }

    std::atomic<float> colour      { 0.5f };    // 0 brown, 0.25 pink, 0.5 white, 0.75 blue, 1 violet
    std::atomic<int>   source      { pure };      // how the noise becomes sound, see Source
    std::atomic<float> mod         { 0.5f };    // 0..1, what it does depends on the source (FM depth, ring mix, loop, density)
    std::atomic<float> grain       { 0.0f };    // 0..1: sample-and-hold, from smooth to coarse and steppy
    std::atomic<float> tone        { 0.0f };    // 0..1: a resonator tuned to the note gives the noise a pitch
    std::atomic<int>   filterMode  { lowPass };
    std::atomic<float> cutoffHz    { 12000.0f };
    std::atomic<float> resonance   { 0.2f };    // 0..1
    std::atomic<float> keyFollow   { 0.5f };    // 0..1: how much the filter follows the note
    std::atomic<float> envAmount   { 0.0f };    // -1..1: the envelope opens (or closes) the filter
    std::atomic<float> attack      { 0.002f };  // seconds
    std::atomic<float> decay       { 0.4f };
    std::atomic<float> sustain     { 0.8f };
    std::atomic<float> release     { 0.3f };
    std::atomic<float> width       { 0.5f };    // 0 mono .. 1 a different noise on each side
    std::atomic<float> velocity    { 0.6f };    // 0..1: how much harder hits are louder
    std::atomic<float> lfoRate     { 2.0f };    // Hz
    std::atomic<float> lfoFilter   { 0.0f };    // 0..1, the wobble on the filter
    std::atomic<float> lfoColour   { 0.0f };    // 0..1, the wobble on the colour
    std::atomic<float> level       { 1.0f };    // 0.25..4: the sound's own level (narrow filters make noise quieter)
};

/** White, pink, brown, blue and violet noise, and any colour in between.
    Each colour is scaled to about the same loudness. */
class ColourNoise
{
public:
    explicit ColourNoise (std::uint32_t seed = 0x9e3779b9u) : state (seed | 1u) {}

    float white()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (std::int32_t) state * (1.0f / 2147483648.0f);
    }

    /** One sample of noise of colour c (0 brown .. 1 violet). */
    float next (float c)
    {
        const float x = white();
        // Pink: Paul Kellet's filter. Brown: a leaky integral. Blue and violet: their rising mirror images.
        b0 = 0.99886f * b0 + x * 0.0555179f;
        b1 = 0.99332f * b1 + x * 0.0750759f;
        b2 = 0.96900f * b2 + x * 0.1538520f;
        b3 = 0.86650f * b3 + x * 0.3104856f;
        b4 = 0.55000f * b4 + x * 0.5329522f;
        b5 = -0.7616f * b5 - x * 0.0168980f;
        const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + x * 0.5362f;
        b6 = x * 0.115926f;
        brown = 0.995f * brown + x * 0.1f;
        const float blue = pink - lastPink;
        const float violet = x - lastWhite;
        lastPink = pink;
        lastWhite = x;

        const std::array<float, 5> colours { brown * 0.507f, pink * 0.166f, x * 0.520f, blue * 0.287f, violet * 0.368f };
        const float pos = juce::jlimit (0.0f, 1.0f, c) * 4.0f;
        const int i = juce::jmin (3, (int) pos);
        const float f = pos - (float) i;
        return colours[(size_t) i] + f * (colours[(size_t) i + 1] - colours[(size_t) i]);
    }

private:
    std::uint32_t state;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, brown = 0, lastPink = 0, lastWhite = 0;
};

struct NoiseSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** One voice: per side, coloured noise made into sound by the chosen source,
    through the sample-and-hold, the tuned resonator and the filter. */
class NoiseVoice final : public juce::SynthesiserVoice
{
public:
    NoiseVoice (const NoiseParams& p, const Performance& perf, std::uint32_t seed)
        : params (p), performance (perf)
    {
        for (size_t ch = 0; ch < 2; ++ch)
            sides[ch].noise = ColourNoise (seed * 2654435761u + (std::uint32_t) ch * 40503u + 1u);
    }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<NoiseSound*> (s) != nullptr; }

    void prepare (double sampleRate)
    {
        ampEnv.setSampleRate (sampleRate);
        const int size = juce::nextPowerOfTwo ((int) (sampleRate / 15.0) + 4);   // the resonator down to about 15 Hz
        for (auto& s : sides)
        {
            s.comb.assign ((size_t) size, 0.0f);
            s.combMask = size - 1;
        }
    }

    void startNote (int midiNote, float vel, juce::SynthesiserSound*, int) override
    {
        noteHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        if (! ampEnv.isActive())
            for (auto& s : sides)
                s.reset();
        const float vs = juce::jlimit (0.0f, 1.0f, params.velocity.load());
        level = 0.3f * juce::jlimit (0.25f, 4.0f, params.level.load()) * ((1.0f - vs) + vs * vel);
        ampEnv.setParameters ({ params.attack.load(), params.decay.load(), params.sustain.load(), params.release.load() });
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
        const double twoPi = juce::MathConstants<double>::twoPi;
        const int source = juce::jlimit (0, NoiseParams::numSources - 1, params.source.load());
        const float mod = juce::jlimit (0.0f, 1.0f, params.mod.load());
        const float grain = juce::jlimit (0.0f, 1.0f, params.grain.load());
        const float tone = juce::jlimit (0.0f, 1.0f, params.tone.load());
        const float width = juce::jlimit (0.0f, 1.0f, params.width.load());
        const int filterMode = juce::jlimit (0, NoiseParams::numFilterModes - 1, params.filterMode.load());
        const float k = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, params.resonance.load());   // damping
        const float envAmount = juce::jlimit (-1.0f, 1.0f, params.envAmount.load());
        const double lfoStep = twoPi * juce::jlimit (0.05f, 30.0f, params.lfoRate.load()) / sr;
        const int numChannels = output.getNumChannels();
        constexpr int controlInterval = 16;

        for (int i = 0; i < numSamples; ++i)
        {
            lfoPhase += lfoStep;
            if (lfoPhase > twoPi) lfoPhase -= twoPi;
            vibratoPhase += twoPi * TestSynthParams::vibratoRateHz / sr;
            if (vibratoPhase > twoPi) vibratoPhase -= twoPi;
            const float amp = ampEnv.getNextSample();

            if ((i % controlInterval) == 0)
            {
                const float lfo = (float) std::sin (lfoPhase);
                const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                                       + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (vibratoPhase);
                hz = noteHz * std::pow (2.0, semitones / 12.0);
                colourNow = juce::jlimit (0.0f, 1.0f, params.colour.load() + 0.5f * params.lfoColour.load() * lfo);

                // The filter: its cutoff, following the note, the envelope and the wobble.
                const double follow = std::pow (hz / 261.63, (double) juce::jlimit (0.0f, 1.0f, params.keyFollow.load()));
                const double octaves = 5.0 * envAmount * amp + 2.0 * params.lfoFilter.load() * lfo;
                const double cutoff = juce::jlimit (30.0, 0.45 * sr, params.cutoffHz.load() * follow * std::pow (2.0, octaves));
                g = (float) std::tan (juce::MathConstants<double>::pi * cutoff / sr);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;

                // Sample and hold: from every sample (smooth) to a few hundred times a second (steppy).
                holdLength = grain <= 0.001f ? 1.0 : sr / (40.0 * std::pow (400.0, 1.0 - (double) grain));
                combLength = juce::jlimit (2.0, (double) sides[0].combMask - 2.0, sr / hz);
            }

            float shared = 0.0f;
            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto& s = sides[ch];
                float x = makeSource (s, source, mod, sr, twoPi);

                // Sample and hold.
                if (holdLength > 1.0)
                {
                    s.holdCount -= 1.0;
                    if (s.holdCount <= 0.0)
                    {
                        s.held = x;
                        s.holdCount += holdLength;
                    }
                    x = s.held;
                }

                // The tuned resonator: a comb at the note's period, darkened inside, so the noise sings.
                if (tone > 0.001f)
                {
                    double pos = (double) s.combWrite - combLength;
                    while (pos < 0.0) pos += (double) (s.combMask + 1);
                    const int i0 = (int) pos & s.combMask;
                    const int i1 = (i0 + 1) & s.combMask;
                    const float frac = (float) (pos - std::floor (pos));
                    const float delayed = s.comb[(size_t) i0] + frac * (s.comb[(size_t) i1] - s.comb[(size_t) i0]);
                    s.combLow += 0.5f * (delayed - s.combLow);
                    const float fb = 0.985f * tone;
                    const float y = x * (1.0f - 0.7f * tone) + fb * s.combLow;
                    s.comb[(size_t) s.combWrite] = y;
                    s.combWrite = (s.combWrite + 1) & s.combMask;
                    x = (1.0f - tone) * x + tone * y * (1.0f - fb * 0.8f) * 2.0f * (1.0f + 2.5f * tone);   // as loud as plain noise
                }

                // State-variable filter.
                const float v3 = x - s.ic2;
                const float v1 = a1 * s.ic1 + a2 * v3;
                const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
                s.ic1 = 2.0f * v1 - s.ic1;
                s.ic2 = 2.0f * v2 - s.ic2;
                const float y = filterMode == NoiseParams::lowPass ? v2 : (filterMode == NoiseParams::bandPass ? v1 * k : x - k * v1 - v2);

                if (ch == 0)
                    shared = y;
                const float out = (ch == 0 ? y : shared + width * (y - shared)) * amp * level;
                if ((int) ch < numChannels)
                    output.addSample ((int) ch, startSample + i, out);
            }

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    struct Side
    {
        ColourNoise noise;
        double phase = 0.0;
        std::uint16_t lfsr = 0x7fff;
        double lfsrClock = 0.0;
        float lfsrOut = 0.0f, crackle = 0.0f, held = 0.0f, combLow = 0.0f, fmSmooth = 0.0f;
        double holdCount = 0.0;
        std::vector<float> comb;
        int combMask = 0, combWrite = 0;
        float ic1 = 0.0f, ic2 = 0.0f;

        void reset()
        {
            phase = lfsrClock = holdCount = 0.0;
            lfsr = 0x7fff;
            lfsrOut = crackle = held = combLow = fmSmooth = 0.0f;
            std::fill (comb.begin(), comb.end(), 0.0f);
            combWrite = 0;
            ic1 = ic2 = 0.0f;
        }
    };

    float makeSource (Side& s, int source, float mod, double sr, double twoPi)
    {
        switch (source)
        {
            case NoiseParams::fmNoise:
            {
                // A tone at the note whose pitch the noise shakes: from a pure tone to a roaring band.
                const float n = s.noise.next (colourNow);
                const double depth = 8.0 * (double) mod * (double) mod;
                s.phase += hz * (1.0 + depth * (double) n * 3.0) / sr;
                s.phase -= std::floor (s.phase);
                return 0.6f * (float) std::sin (twoPi * s.phase);
            }
            case NoiseParams::ring:
            {
                // Noise times a tone at the note: a band of noise around the pitch.
                const float n = s.noise.next (colourNow);
                s.phase += hz / sr;
                s.phase -= std::floor (s.phase);
                const float sine = (float) std::sin (twoPi * s.phase);
                return 1.6f * ((1.0f - mod) * 0.4f * sine + mod * n * sine);
            }
            case NoiseParams::digital:
            {
                // A shift-register noise clocked at the note, like old sound chips. Past halfway its loop
                // is short, so it rings like metal.
                s.lfsrClock += hz * 8.0 / sr;
                while (s.lfsrClock >= 1.0)
                {
                    s.lfsrClock -= 1.0;
                    const int tap = mod >= 0.5f ? 6 : 1;
                    const auto bit = (std::uint16_t) (((s.lfsr >> 0) ^ (s.lfsr >> tap)) & 1u);
                    s.lfsr = (std::uint16_t) ((s.lfsr >> 1) | (bit << 14));
                    s.lfsrOut = (s.lfsr & 1u) ? 0.35f : -0.35f;
                }
                return tilt (s, s.lfsrOut);
            }
            case NoiseParams::crackle:
            {
                // Dust and crackle: random clicks, from a few to a storm.
                const float density = 0.0002f + 0.05f * mod * mod;
                if (std::abs (s.noise.white()) < density)
                    s.crackle = s.noise.white() * 1.5f;
                const float out = s.crackle;
                s.crackle *= 0.6f;
                return tilt (s, out);
            }
            default:
                return s.noise.next (colourNow);
        }
    }

    /** The colour knob on sources that aren't made from coloured noise: darker or brighter. */
    float tilt (Side& s, float x) const
    {
        s.fmSmooth += 0.15f * (x - s.fmSmooth);
        const float t = colourNow * 2.0f - 1.0f;   // -1 dark .. +1 bright
        return t < 0.0f ? x + (-t) * (s.fmSmooth * 1.5f - x) : x + t * (x - s.fmSmooth);
    }

    const NoiseParams& params;
    const Performance& performance;
    std::array<Side, 2> sides;
    double noteHz = 440.0, hz = 440.0, lfoPhase = 0.0, vibratoPhase = 0.0;
    double holdLength = 1.0, combLength = 100.0;
    float colourNow = 0.5f, level = 0.0f;
    float g = 0.5f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    juce::ADSR ampEnv;
};

/** The Noise settings as JSON, for projects and saved sounds. */
inline juce::var noiseParamsToVar (const NoiseParams& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("colour", p.colour.load());        o->setProperty ("source", p.source.load());
    o->setProperty ("mod", p.mod.load());              o->setProperty ("grain", p.grain.load());
    o->setProperty ("tone", p.tone.load());            o->setProperty ("filterMode", p.filterMode.load());
    o->setProperty ("cutoff", p.cutoffHz.load());      o->setProperty ("resonance", p.resonance.load());
    o->setProperty ("keyFollow", p.keyFollow.load());  o->setProperty ("envAmount", p.envAmount.load());
    o->setProperty ("attack", p.attack.load());        o->setProperty ("decay", p.decay.load());
    o->setProperty ("sustain", p.sustain.load());      o->setProperty ("release", p.release.load());
    o->setProperty ("width", p.width.load());          o->setProperty ("velocity", p.velocity.load());
    o->setProperty ("lfoRate", p.lfoRate.load());      o->setProperty ("lfoFilter", p.lfoFilter.load());
    o->setProperty ("lfoColour", p.lfoColour.load());  o->setProperty ("level", p.level.load());
    return juce::var (o);
}

/** Reads settings written by noiseParamsToVar(); anything missing gets its default. */
inline void applyNoiseParams (NoiseParams& p, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    p.colour.store (f ("colour", 0.5f));
    p.source.store (juce::jlimit (0, NoiseParams::numSources - 1, i ("source", 0)));
    p.mod.store (f ("mod", 0.5f));                   p.grain.store (f ("grain", 0.0f));
    p.tone.store (f ("tone", 0.0f));
    p.filterMode.store (juce::jlimit (0, NoiseParams::numFilterModes - 1, i ("filterMode", 0)));
    p.cutoffHz.store (f ("cutoff", 12000.0f));       p.resonance.store (f ("resonance", 0.2f));
    p.keyFollow.store (f ("keyFollow", 0.5f));       p.envAmount.store (f ("envAmount", 0.0f));
    p.attack.store (f ("attack", 0.002f));           p.decay.store (f ("decay", 0.4f));
    p.sustain.store (f ("sustain", 0.8f));           p.release.store (f ("release", 0.3f));
    p.width.store (f ("width", 0.5f));               p.velocity.store (f ("velocity", 0.6f));
    p.lfoRate.store (f ("lfoRate", 2.0f));           p.lfoFilter.store (f ("lfoFilter", 0.0f));
    p.lfoColour.store (f ("lfoColour", 0.0f));       p.level.store (f ("level", 1.0f));
}

} // namespace bizgroove
