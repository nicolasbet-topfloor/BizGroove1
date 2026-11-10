#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // Performance, TestSynthParams
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

namespace bizgroove
{

/** Settings of the Wavetable synth, shared between the UI and the voices. */
struct WaveParams
{
    std::atomic<int>   table       { 0 };       // which set of waves: see Wavetables::tableName()
    std::atomic<float> position    { 0.2f };    // 0..1, where in the set the sound sits
    std::atomic<float> sweep       { 0.3f };    // 0..1, how far each note's envelope moves the position
    std::atomic<float> cutoffHz    { 8000.0f };
    std::atomic<float> resonance   { 0.15f };   // 0..1
    std::atomic<float> envAmount   { 0.2f };    // 0..1
    std::atomic<float> attack      { 0.005f };  // seconds
    std::atomic<float> decay       { 0.8f };
    std::atomic<float> sustain     { 0.7f };
    std::atomic<float> release     { 0.4f };
    std::atomic<float> detuneCents { 10.0f };   // spread of the stacked copies
    std::atomic<int>   unison      { 1 };       // 1..4 copies per note, spread across the stereo field
    std::atomic<float> sub         { 0.0f };    // 0..1, a sine an octave below
    std::atomic<float> noise       { 0.0f };    // 0..1
    std::atomic<float> glide       { 0.0f };    // seconds (per octave when gliding by distance)
    std::atomic<int>   glideMode   { Glide::fixedTime };
    std::atomic<float> lfoRate     { 0.5f };    // Hz
    std::atomic<float> lfoPosition { 0.0f };    // 0..1, wobble on the position
    std::atomic<float> lfoFilter   { 0.0f };    // 0..1, wobble on the brightness

    double lastNoteHz = 0.0;   // audio thread only: where a glide starts
};

/** The built-in sets of waves. Each set has eight waves that the position
    morphs through. They are made here from simple recipes (no recordings),
    and each wave is kept in versions with fewer and fewer harmonics so that
    high notes stay clean. Built once, on first use, off the audio thread. */
class Wavetables
{
public:
    static constexpr int numTables = 4;
    static constexpr int framesPerTable = 8;
    static constexpr int tableSize = 2048;
    static constexpr int numMips = 11;   // version m keeps harmonics up to 1024 >> m

    static const Wavetables& get()
    {
        static const Wavetables tables;
        return tables;
    }

    static juce::String tableName (int table)
    {
        switch (table)
        {
            case 0:  return "Classic";    // sine, triangle, saw, square, thin pulses
            case 1:  return "Harmonics";  // from one harmonic to a full, buzzy spectrum
            case 2:  return "Voices";     // vowel-like tones: a, e, i, o, u
            default: return "Digital";    // hard-sync sweeps, sharp and metallic
        }
    }

    const float* frame (int table, int frameIndex, int mip) const
    {
        const size_t index = ((size_t) juce::jlimit (0, numTables - 1, table) * framesPerTable
                              + (size_t) juce::jlimit (0, framesPerTable - 1, frameIndex)) * numMips
                             + (size_t) juce::jlimit (0, numMips - 1, mip);
        return data.data() + index * tableSize;
    }

    /** The version with as many harmonics as fit below the top of the hearing range at this pitch. */
    static int mipFor (double hz, double sampleRate)
    {
        const double maxHarmonic = 0.45 * sampleRate / juce::jmax (1.0, hz);
        int m = 0;
        while (m < numMips - 1 && (double) (1024 >> m) > maxHarmonic)
            ++m;
        return m;
    }

private:
    using Complex = std::complex<double>;

    Wavetables() : data ((size_t) numTables * framesPerTable * numMips * tableSize, 0.0f)
    {
        std::vector<double> wave ((size_t) tableSize);
        std::vector<Complex> spectrum ((size_t) tableSize), work ((size_t) tableSize);

        for (int t = 0; t < numTables; ++t)
        {
            for (int f = 0; f < framesPerTable; ++f)
            {
                for (int n = 0; n < tableSize; ++n)
                    wave[(size_t) n] = shape (t, f, (double) n / tableSize);

                for (int n = 0; n < tableSize; ++n)
                    spectrum[(size_t) n] = Complex (wave[(size_t) n], 0.0);
                fft (spectrum, false);
                spectrum[0] = 0.0;                    // no offset
                spectrum[(size_t) tableSize / 2] = 0.0;

                float fullScale = 1.0f;
                for (int m = 0; m < numMips; ++m)
                {
                    const int keep = 1024 >> m;
                    work = spectrum;
                    for (int k = keep + 1; k < tableSize - keep; ++k)
                        work[(size_t) k] = 0.0;
                    fft (work, true);

                    float* out = data.data() + (((size_t) t * framesPerTable + (size_t) f) * numMips + (size_t) m) * tableSize;
                    // Each version is brought up to full level, but a thin one at most 4x,
                    // so high notes on waves with weak low harmonics still sound.
                    double peak = 0.0;
                    for (int n = 0; n < tableSize; ++n)
                        peak = juce::jmax (peak, std::abs (work[(size_t) n].real()));
                    const float own = peak > 1.0e-9 ? (float) (0.9 / peak) : 0.0f;
                    if (m == 0)
                        fullScale = own;
                    const float scale = juce::jmin (own, 4.0f * fullScale);
                    for (int n = 0; n < tableSize; ++n)
                        out[n] = scale * (float) work[(size_t) n].real();
                }
            }
        }
    }

    /** One cycle of wave f (0..7) of set t, at phase p (0..1). */
    static double shape (int t, int f, double p)
    {
        const double twoPi = juce::MathConstants<double>::twoPi;
        auto saw = [] (double x) { return 2.0 * (x - std::floor (x)) - 1.0; };
        auto tri = [] (double x) { x -= std::floor (x); return x < 0.5 ? 4.0 * x - 1.0 : 3.0 - 4.0 * x; };
        auto pulse = [] (double x, double w) { x -= std::floor (x); return x < w ? 1.0 : -1.0; };

        switch (t)
        {
            case 0:   // Classic
                switch (f)
                {
                    case 0:  return std::sin (twoPi * p);
                    case 1:  return tri (p + 0.25);
                    case 2:  return 0.5 * (tri (p + 0.25) + saw (p + 0.5));
                    case 3:  return saw (p + 0.5);
                    case 4:  return 0.5 * (saw (p + 0.5) + pulse (p, 0.5));
                    case 5:  return pulse (p, 0.5);
                    case 6:  return pulse (p, 0.25);
                    default: return pulse (p, 0.1);
                }

            case 1:   // Harmonics: 1, 2, 4 ... 128 harmonics, softly falling; odd frames odd harmonics only
            {
                const int count = 1 << f;
                double x = 0.0;
                for (int n = 1; n <= count; ++n)
                    if ((f % 2) == 0 || (n % 2) == 1 || count == 1)
                        x += std::sin (twoPi * n * p) / std::pow ((double) n, 0.8);
                return x;
            }

            case 2:   // Voices: harmonics shaped by vowel resonances, a > e > i > o > u
            {
                static constexpr double formants[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                                            { 570,  840, 2410 }, { 300,  870, 2240 } };
                const double v = f * 4.0 / (framesPerTable - 1);
                const int v0 = juce::jlimit (0, 3, (int) v);
                const double frac = v - v0;
                const double f0 = 130.0;   // the pitch the vowels are drawn for
                double x = 0.0;
                for (int n = 1; n <= 60; ++n)
                {
                    const double hz = n * f0;
                    double amp = 0.0;
                    for (int k = 0; k < 3; ++k)
                    {
                        const double centre = formants[v0][k] + frac * (formants[v0 + 1][k] - formants[v0][k]);
                        const double width = 90.0 + 40.0 * k;
                        amp += std::exp (-std::pow ((hz - centre) / width, 2.0)) / (1.0 + k);
                    }
                    x += (amp + 0.08 / n) * std::sin (twoPi * n * p);
                }
                return x;
            }

            default:  // Digital: a saw restarted every cycle (hard sync), its own pitch rising
            {
                const double ratio = 1.0 + 0.75 * f;
                const double fade = 1.0 - p;   // softens the restart click
                return saw (p * ratio + 0.5) * (0.4 + 0.6 * fade);
            }
        }
    }

    /** In-place radix-2 FFT; inverse = true also divides by the size. */
    static void fft (std::vector<Complex>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; (j & bit) != 0; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double angle = juce::MathConstants<double>::twoPi / (double) len * (inverse ? 1.0 : -1.0);
            const Complex step (std::cos (angle), std::sin (angle));
            for (size_t i = 0; i < n; i += len)
            {
                Complex w (1.0, 0.0);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const Complex u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= step;
                }
            }
        }
        if (inverse)
            for (auto& x : a)
                x /= (double) n;
    }

    std::vector<float> data;
};

struct WaveSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** One voice: up to four stacked copies of the wave (spread in tune and
    across left and right), a sine an octave down, noise, a resonant low-pass
    filter per side, and an envelope that moves both the position in the
    wave set and the brightness. */
class WaveVoice final : public juce::SynthesiserVoice
{
public:
    static constexpr int maxUnison = 4;

    WaveVoice (WaveParams& p, const Performance& perf) : params (p), performance (perf), tables (Wavetables::get()) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<WaveSound*> (s) != nullptr; }

    void prepare (double sampleRate)
    {
        ampEnv.setSampleRate (sampleRate);
        modEnv.setSampleRate (sampleRate);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        targetHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        glide.start (params.lastNoteHz, targetHz, params.glide.load(), params.glideMode.load(), getSampleRate());
        currentHz = glide.hz();
        params.lastNoteHz = targetHz;

        unison = juce::jlimit (1, maxUnison, params.unison.load());
        if (! ampEnv.isActive())
        {
            for (int u = 0; u < maxUnison; ++u)
                phases[(size_t) u] = 0.17 * u;
            subPhase = 0.0;
            for (auto& s : filterState)
                s = 0.0f;
        }

        level = 0.22f * velocity / std::sqrt ((float) unison);
        ampEnv.setParameters ({ params.attack.load(), params.decay.load(), params.sustain.load(), params.release.load() });
        modEnv.setParameters ({ params.attack.load(), params.decay.load(), params.sustain.load() * 0.5f, params.release.load() });
        ampEnv.noteOn();
        modEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            modEnv.noteOff();
        }
        else
        {
            ampEnv.reset();
            modEnv.reset();
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
        const double lfoStep = twoPi * juce::jlimit (0.02f, 30.0f, params.lfoRate.load()) / sr;
        const int table = juce::jlimit (0, Wavetables::numTables - 1, params.table.load());
        const float basePosition = juce::jlimit (0.0f, 1.0f, params.position.load());
        const float sweep = juce::jlimit (0.0f, 1.0f, params.sweep.load());
        const float lfoPosition = juce::jlimit (0.0f, 1.0f, params.lfoPosition.load());
        const float lfoFilter = juce::jlimit (0.0f, 1.0f, params.lfoFilter.load());
        const float baseCutoff = params.cutoffHz.load();
        const float envDepth = juce::jlimit (0.0f, 1.0f, params.envAmount.load());
        const float subLevel = juce::jlimit (0.0f, 1.0f, params.sub.load());
        const float noiseLevel = juce::jlimit (0.0f, 1.0f, params.noise.load());
        const float k = 2.0f - 1.94f * juce::jlimit (0.0f, 1.0f, params.resonance.load());
        const float nyquistLimit = (float) sr * 0.45f;
        const int numChannels = output.getNumChannels();

        // Where each copy sits from left to right.
        std::array<float, maxUnison> gainL {}, gainR {};
        for (int u = 0; u < unison; ++u)
        {
            const float pan = unison == 1 ? 0.0f : ((float) u / (float) (unison - 1) * 2.0f - 1.0f) * 0.7f;
            gainL[(size_t) u] = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2;
            gainR[(size_t) u] = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2;
        }

        constexpr int controlInterval = 16;
        constexpr int mask = Wavetables::tableSize - 1;
        float g = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        const float* frameA[maxUnison] {};
        const float* frameB[maxUnison] {};
        float morph = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float mEnv = modEnv.getNextSample();
            vibratoPhase += vibratoStep;
            if (vibratoPhase > twoPi) vibratoPhase -= twoPi;
            lfoPhase += lfoStep;
            if (lfoPhase > twoPi) lfoPhase -= twoPi;

            if ((i % controlInterval) == 0)
            {
                const float lfo = (float) std::sin (lfoPhase);
                currentHz = glide.advance (controlInterval);
                updatePitch (sr);

                const float pos = juce::jlimit (0.0f, 1.0f, basePosition + sweep * mEnv + 0.5f * lfoPosition * lfo);
                const float framePos = pos * (float) (Wavetables::framesPerTable - 1);
                const int f0 = juce::jlimit (0, Wavetables::framesPerTable - 2, (int) framePos);
                morph = framePos - (float) f0;
                for (int u = 0; u < unison; ++u)
                {
                    frameA[u] = tables.frame (table, f0, mips[(size_t) u]);
                    frameB[u] = tables.frame (table, f0 + 1, mips[(size_t) u]);
                }

                const float cutoff = baseCutoff * std::pow (2.0f, 5.0f * envDepth * mEnv + 2.0f * lfoFilter * lfo);
                g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, nyquistLimit, cutoff) / (float) sr);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            float left = 0.0f, right = 0.0f;
            for (int u = 0; u < unison; ++u)
            {
                auto& ph = phases[(size_t) u];
                const double pos = ph * Wavetables::tableSize;
                const int i0 = (int) pos & mask;
                const int i1 = (i0 + 1) & mask;
                const float frac = (float) (pos - std::floor (pos));
                const float a = frameA[u][i0] + frac * (frameA[u][i1] - frameA[u][i0]);
                const float b = frameB[u][i0] + frac * (frameB[u][i1] - frameB[u][i0]);
                const float x = a + morph * (b - a);
                left += x * gainL[(size_t) u];
                right += x * gainR[(size_t) u];
                ph += increments[(size_t) u];
                if (ph >= 1.0) ph -= 1.0;
            }

            float common = 0.0f;
            if (subLevel > 0.0f)
            {
                common += subLevel * (float) std::sin (twoPi * subPhase) * std::sqrt ((float) unison);
                subPhase += subIncrement;
                if (subPhase >= 1.0) subPhase -= 1.0;
            }
            if (noiseLevel > 0.0f)
                common += noiseLevel * (random.nextFloat() * 2.0f - 1.0f) * 0.7f * std::sqrt ((float) unison);

            const float env = ampEnv.getNextSample() * level;
            const float outL = lowPass (left + common, 0, a1, a2, a3) * env;
            const float outR = lowPass (right + common, 2, a1, a2, a3) * env;

            if (numChannels == 1)
                output.addSample (0, startSample + i, 0.5f * (outL + outR));
            else
            {
                output.addSample (0, startSample + i, outL);
                output.addSample (1, startSample + i, outR);
            }

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    /** State-variable low-pass (TPT form); state lives at filterState[s], [s + 1]. */
    float lowPass (float x, int s, float a1, float a2, float a3)
    {
        float& ic1 = filterState[(size_t) s];
        float& ic2 = filterState[(size_t) s + 1];
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    /** Note pitch with glide, bend and vibrato; the copies fan out around it. */
    void updatePitch (double sampleRate)
    {
        const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                               + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (vibratoPhase);
        const double hz = currentHz * std::pow (2.0, semitones / 12.0);
        const double cents = params.detuneCents.load();
        for (int u = 0; u < unison; ++u)
        {
            const double offset = unison == 1 ? 0.0 : ((double) u / (unison - 1) - 0.5) * 2.0 * cents;
            const double copyHz = hz * std::pow (2.0, offset / 1200.0);
            increments[(size_t) u] = copyHz / sampleRate;
            mips[(size_t) u] = Wavetables::mipFor (copyHz, sampleRate);
        }
        subIncrement = 0.5 * hz / sampleRate;
    }

    WaveParams& params;
    const Performance& performance;
    const Wavetables& tables;
    double targetHz = 440.0, currentHz = 440.0;
    Glide glide;
    double vibratoPhase = 0.0, lfoPhase = 0.0, subPhase = 0.0, subIncrement = 0.0;
    int unison = 1;
    std::array<double, maxUnison> phases {}, increments {};
    std::array<int, maxUnison> mips {};
    std::array<float, 4> filterState {};
    juce::ADSR ampEnv, modEnv;
    juce::Random random;
    float level = 0.0f;
};

/** The Wavetable settings as JSON, for projects and saved sounds. */
inline juce::var waveParamsToVar (const WaveParams& wp)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("table", wp.table.load());          o->setProperty ("position", wp.position.load());
    o->setProperty ("sweep", wp.sweep.load());          o->setProperty ("cutoff", wp.cutoffHz.load());
    o->setProperty ("resonance", wp.resonance.load());  o->setProperty ("envAmount", wp.envAmount.load());
    o->setProperty ("attack", wp.attack.load());        o->setProperty ("decay", wp.decay.load());
    o->setProperty ("sustain", wp.sustain.load());      o->setProperty ("release", wp.release.load());
    o->setProperty ("detune", wp.detuneCents.load());   o->setProperty ("unison", wp.unison.load());
    o->setProperty ("sub", wp.sub.load());              o->setProperty ("noise", wp.noise.load());
    o->setProperty ("glideMode", wp.glideMode.load());
    o->setProperty ("glide", wp.glide.load());          o->setProperty ("lfoRate", wp.lfoRate.load());
    o->setProperty ("lfoPosition", wp.lfoPosition.load()); o->setProperty ("lfoFilter", wp.lfoFilter.load());
    return juce::var (o);
}

/** Reads settings written by waveParamsToVar(); anything missing gets its default. */
inline void applyWaveParams (WaveParams& wp, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    wp.table.store (juce::jlimit (0, Wavetables::numTables - 1, i ("table", 0)));
    wp.position.store (f ("position", 0.2f));       wp.sweep.store (f ("sweep", 0.3f));
    wp.cutoffHz.store (f ("cutoff", 8000.0f));      wp.resonance.store (f ("resonance", 0.15f));
    wp.envAmount.store (f ("envAmount", 0.2f));     wp.attack.store (f ("attack", 0.005f));
    wp.decay.store (f ("decay", 0.8f));             wp.sustain.store (f ("sustain", 0.7f));
    wp.release.store (f ("release", 0.4f));         wp.detuneCents.store (f ("detune", 10.0f));
    wp.unison.store (juce::jlimit (1, 4, i ("unison", 1)));
    wp.sub.store (f ("sub", 0.0f));                 wp.noise.store (f ("noise", 0.0f));
    wp.glideMode.store (juce::jlimit (0, Glide::numModes - 1, (int) (v.hasProperty ("glideMode") ? (int) v["glideMode"] : 0)));
    wp.glide.store (f ("glide", 0.0f));             wp.lfoRate.store (f ("lfoRate", 0.5f));
    wp.lfoPosition.store (f ("lfoPosition", 0.0f)); wp.lfoFilter.store (f ("lfoFilter", 0.0f));
}

} // namespace bizgroove
