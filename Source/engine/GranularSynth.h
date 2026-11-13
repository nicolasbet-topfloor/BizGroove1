#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // Performance, TestSynthParams
#include "Glide.h"
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace bizgroove
{

/** The sounds the Granular takes its grains from: five made inside BizGroove,
    each a two-second tone at C4 (noise aside), and the track's own recording. */
class GranularSources
{
public:
    enum Source { glass = 0, choir, strings, noise, metal, recording, numSources };
    static constexpr double rate = 48000.0;
    static constexpr double baseHz = 261.626;   // C4: a note plays a grain at its own pitch

    static juce::String name (int s)
    {
        static const char* names[numSources] = { "Glass", "Choir", "Strings", "Noise", "Metal", "Your recording" };
        return names[juce::jlimit (0, numSources - 1, s)];
    }

    /** The built-in sound (not the recording). Made once, the first time; call from the message thread first. */
    static const std::vector<float>& table (int s)
    {
        static const std::array<std::vector<float>, recording> tables = [] {
            std::array<std::vector<float>, recording> t;
            for (int i = 0; i < recording; ++i)
                t[(size_t) i] = make (i);
            return t;
        }();
        return tables[(size_t) juce::jlimit (0, recording - 1, s)];
    }

private:
    static std::vector<float> make (int s)
    {
        const int n = (int) (rate * 2.0);
        std::vector<float> y ((size_t) n, 0.0f);
        const double w0 = juce::MathConstants<double>::twoPi * baseHz / rate;
        juce::Random rng (17 + s);
        if (s == glass)
        {
            // Bell-like partials that shimmer as they slowly beat against each other.
            static const double ratios[5] = { 1.0, 2.0, 3.01, 4.2, 5.43 };
            static const float amps[5] = { 1.0f, 0.5f, 0.3f, 0.2f, 0.12f };
            for (int i = 0; i < n; ++i)
            {
                const double t = i / rate;
                float v = 0.0f;
                for (int k = 0; k < 5; ++k)
                    v += amps[k] * (float) std::sin (w0 * ratios[k] * i) * (float) (0.75 + 0.25 * std::sin (t * (1.3 + k * 0.7)));
                y[(size_t) i] = v;
            }
        }
        else if (s == choir || s == strings)
        {
            // Choir: a saw sung through vowel formants that move from "ah" to "oh". Strings: seven detuned saws.
            const int voices = s == choir ? 3 : 7;
            std::array<double, 7> ph {};
            for (auto& p : ph) p = rng.nextDouble();
            std::array<float, 6> bp {};   // two formant filters' states
            for (int i = 0; i < n; ++i)
            {
                const double t = i / rate;
                float v = 0.0f;
                for (int k = 0; k < voices; ++k)
                {
                    const double detune = 1.0 + (k - (voices - 1) * 0.5) * (s == choir ? 0.004 : 0.006)
                                          + (s == choir ? 0.003 * std::sin (t * 5.5 + k) : 0.0);
                    ph[(size_t) k] += baseHz * detune / rate;
                    ph[(size_t) k] -= std::floor (ph[(size_t) k]);
                    v += (float) (2.0 * ph[(size_t) k] - 1.0);
                }
                v /= (float) voices;
                if (s == choir)
                {
                    const float morph = (float) (0.5 - 0.5 * std::cos (t * juce::MathConstants<double>::pi));   // ah -> oh -> ah
                    const float f1 = 730.0f + (570.0f - 730.0f) * morph, f2 = 1090.0f + (840.0f - 1090.0f) * morph;
                    auto formant = [] (float x, float f, float* st) {   // a simple resonant band-pass (state variable)
                        const float g = std::tan (juce::MathConstants<float>::pi * f / (float) rate), k = 0.25f;
                        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                        const float v3 = x - st[1], v1 = a1 * st[0] + a2 * v3, v2 = st[1] + a2 * st[0] + a3 * v3;
                        st[0] = 2.0f * v1 - st[0]; st[1] = 2.0f * v2 - st[1];
                        return v1;
                    };
                    v = 0.6f * formant (v, f1, &bp[0]) + 0.4f * formant (v, f2, &bp[2]) + 0.05f * v;
                }
                y[(size_t) i] = v;
            }
            if (s == strings)
            {
                float lp = 0.0f;   // soften the top
                for (auto& v : y) { lp += 0.25f * (v - lp); v = lp; }
            }
        }
        else if (s == noise)
        {
            float b0 = 0, b1 = 0, b2 = 0;   // pink
            for (auto& v : y)
            {
                const float w = rng.nextFloat() * 2.0f - 1.0f;
                b0 = 0.99765f * b0 + w * 0.0990460f;
                b1 = 0.96300f * b1 + w * 0.2965164f;
                b2 = 0.57000f * b2 + w * 1.0526913f;
                v = b0 + b1 + b2 + w * 0.1848f;
            }
        }
        else
        {
            // Metal: clashing partials, each fading at its own speed, struck again and again.
            static const double ratios[6] = { 1.0, 2.76, 5.40, 8.93, 1.41, 3.73 };
            for (int i = 0; i < n; ++i)
            {
                const double t = std::fmod (i / rate, 0.5);
                float v = 0.0f;
                for (int k = 0; k < 6; ++k)
                    v += (float) std::sin (w0 * ratios[k] * i) * (float) std::exp (-t * (2.0 + k * 1.5)) / (float) (1 + k);
                y[(size_t) i] = v;
            }
        }
        const float peak = juce::jmax (1.0e-6f, juce::FloatVectorOperations::findMaximum (y.data(), n),
                                       -juce::FloatVectorOperations::findMinimum (y.data(), n));
        juce::FloatVectorOperations::multiply (y.data(), 0.8f / peak, n);
        return y;
    }
};

/** Settings of the Granular instrument: a cloud of short grains read from a
    sound, at the note's pitch. Shared between the UI and the voices. */
struct GranularParams
{
    GranularParams() { GranularSources::table (0); }   // the built-in sounds are made here, never on the audio thread

    std::atomic<int>   source     { GranularSources::choir };
    std::atomic<float> position   { 0.3f };    // 0..1: where in the sound the grains are read
    std::atomic<float> size       { 0.12f };   // seconds, 0.01..0.5
    std::atomic<float> density    { 20.0f };   // grains a second, 1..100
    std::atomic<float> spread     { 0.15f };   // 0..1: how far around the position grains are taken
    std::atomic<float> scan       { 0.0f };    // -1..1: the position moves on by itself, either way
    std::atomic<float> pitch      { 0.0f };    // semitones, -24..+24
    std::atomic<float> detune     { 0.1f };    // 0..1: each grain a little out of tune, up to 50 cents
    std::atomic<float> reverse    { 0.0f };    // 0..1: how many grains play backwards
    std::atomic<float> spray      { 0.6f };    // 0..1: grains spread from the middle to left and right
    std::atomic<float> cutoffHz   { 12000.0f };
    std::atomic<float> resonance  { 0.1f };    // 0..1
    std::atomic<float> attack     { 0.2f };    // seconds
    std::atomic<float> decay      { 0.6f };
    std::atomic<float> sustain    { 0.8f };
    std::atomic<float> release    { 0.9f };
    std::atomic<float> level      { 1.0f };    // 0.25..4
    std::atomic<float> velocity   { 0.5f };    // 0..1
    std::atomic<float> glide      { 0.0f };
    std::atomic<int>   glideMode  { Glide::fixedTime };

    mutable double lastNoteHz = 0.0;   // audio thread only: where a glide starts

    /** Audio thread: the track's recording for this block (set around rendering; none otherwise). */
    struct View { const float* left = nullptr; const float* right = nullptr; int length = 0; double rate = 48000.0; };
    View recording;
};

struct GranularSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class GranularVoice final : public juce::SynthesiserVoice
{
public:
    GranularVoice (const GranularParams& p, const Performance& perf, std::uint32_t seed)
        : params (p), performance (perf), rng ((juce::int64) seed) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<GranularSound*> (s) != nullptr; }

    void prepare (double sampleRate) { ampEnv.setSampleRate (sampleRate); }

    void startNote (int midiNote, float vel, juce::SynthesiserSound*, int) override
    {
        noteHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        glide.start (params.lastNoteHz, noteHz, params.glide.load(), params.glideMode.load(), getSampleRate());
        params.lastNoteHz = noteHz;
        if (! ampEnv.isActive())
        {
            for (auto& g : grains) g.active = false;
            scanOffset = 0.0;
            countdown = 0.0;
            ic1 = { 0.0f, 0.0f }; ic2 = { 0.0f, 0.0f };
        }
        const float vs = juce::jlimit (0.0f, 1.0f, params.velocity.load());
        level = 0.6f * juce::jlimit (0.25f, 4.0f, params.level.load()) * ((1.0f - vs) + vs * vel);
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

        // The sound the grains come from: a built-in one, or the track's recording (silence without one).
        const int src = juce::jlimit (0, GranularSources::numSources - 1, params.source.load());
        const float* srcL = nullptr;
        const float* srcR = nullptr;
        int srcLen = 0;
        double srcRate = GranularSources::rate;
        if (src == GranularSources::recording)
        {
            const auto& v = params.recording;
            srcL = v.left; srcR = v.right != nullptr ? v.right : v.left; srcLen = v.length; srcRate = v.rate;
        }
        else
        {
            const auto& t = GranularSources::table (src);
            srcL = srcR = t.data();
            srcLen = (int) t.size();
        }

        const double sr = getSampleRate();
        const float k = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, params.resonance.load());
        const double cutoff = juce::jlimit (30.0, 0.45 * sr, (double) params.cutoffHz.load());
        const float g = (float) std::tan (juce::MathConstants<double>::pi * cutoff / sr);
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double density = juce::jlimit (1.0, 100.0, (double) params.density.load());
        const double sizeSec = juce::jlimit (0.01, 0.5, (double) params.size.load());
        const double spread = juce::jlimit (0.0, 1.0, (double) params.spread.load());
        const float detune = juce::jlimit (0.0f, 1.0f, params.detune.load());
        const float reverse = juce::jlimit (0.0f, 1.0f, params.reverse.load());
        const float spray = juce::jlimit (0.0f, 1.0f, params.spray.load());
        // Overlapping grains add up: keep the cloud about as loud however thick it is.
        const float norm = 1.0f / std::sqrt ((float) juce::jmax (1.0, density * sizeSec * 1.5));
        const int numChannels = output.getNumChannels();
        scanOffset += (double) juce::jlimit (-1.0f, 1.0f, params.scan.load()) * 0.1 * numSamples / sr;   // a full sweep in ten seconds
        scanOffset -= std::floor (scanOffset);
        const double bend = std::pow (2.0, (performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                                            + juce::jlimit (-24.0f, 24.0f, params.pitch.load())) / 12.0);

        for (int i = 0; i < numSamples; ++i)
        {
            if ((i & 15) == 0)
                hz = glide.advance (16) * bend;
            const float amp = ampEnv.getNextSample();

            // A new grain when it is due (a little early or late, so the cloud does not buzz).
            countdown -= 1.0;
            if (countdown <= 0.0 && srcLen > 64)
            {
                countdown += sr / density * (0.7 + 0.6 * rng.nextDouble());
                startGrain (srcLen, srcRate, sr, sizeSec, spread, detune, reverse, spray);
            }

            float l = 0.0f, r = 0.0f;
            if (srcLen > 1)
                for (auto& gr : grains)
                {
                    if (! gr.active) continue;
                    const double x = (double) gr.age / (double) gr.length;
                    const float w = (float) std::sin (juce::MathConstants<double>::pi * x);
                    const float window = w * w;
                    double p = gr.pos;
                    while (p < 0.0) p += srcLen;
                    while (p >= srcLen) p -= srcLen;
                    const int i0 = (int) p, i1 = (i0 + 1) % srcLen;
                    const float f = (float) (p - i0);
                    const float sl = srcL[i0] + f * (srcL[i1] - srcL[i0]);
                    const float sR = srcR[i0] + f * (srcR[i1] - srcR[i0]);
                    l += sl * window * gr.left;
                    r += sR * window * gr.right;
                    gr.pos += gr.step;
                    if (++gr.age >= gr.length) gr.active = false;
                }

            const float in[2] = { l * norm, r * norm };
            for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
            {
                // State-variable low-pass.
                const float v3 = in[ch] - ic2[(size_t) ch];
                const float v1 = a1 * ic1[(size_t) ch] + a2 * v3;
                const float v2 = ic2[(size_t) ch] + a2 * ic1[(size_t) ch] + a3 * v3;
                ic1[(size_t) ch] = 2.0f * v1 - ic1[(size_t) ch];
                ic2[(size_t) ch] = 2.0f * v2 - ic2[(size_t) ch];
                output.addSample (ch, startSample + i, v2 * amp * level);
            }
            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                for (auto& gr : grains) gr.active = false;
                break;
            }
        }
    }

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0, step = 1.0;
        int age = 0, length = 1;
        float left = 0.7f, right = 0.7f;
    };

    void startGrain (int srcLen, double srcRate, double sr, double sizeSec, double spread, float detune, float reverse, float spray)
    {
        Grain* free = nullptr;
        for (auto& gr : grains)
            if (! gr.active) { free = &gr; break; }
        if (free == nullptr)
            return;   // the cloud is full: skip this grain
        auto& gr = *free;
        const double centre = std::fmod ((double) juce::jlimit (0.0f, 1.0f, params.position.load()) + scanOffset, 1.0);
        double where = centre + spread * 0.5 * (rng.nextDouble() * 2.0 - 1.0);
        where -= std::floor (where);
        const double cents = (rng.nextDouble() * 2.0 - 1.0) * 50.0 * detune;
        const double ratio = hz / GranularSources::baseHz * std::pow (2.0, cents / 1200.0) * srcRate / sr;
        const bool backwards = rng.nextFloat() < reverse;
        gr.length = juce::jmax (16, (int) (sizeSec * sr));
        gr.step = backwards ? -ratio : ratio;
        gr.pos = where * (srcLen - 1);
        gr.age = 0;
        const float pan = 0.5f + 0.5f * spray * (rng.nextFloat() * 2.0f - 1.0f);
        gr.left = std::cos (pan * juce::MathConstants<float>::halfPi) * 1.41f;
        gr.right = std::sin (pan * juce::MathConstants<float>::halfPi) * 1.41f;
        gr.active = true;
    }

    const GranularParams& params;
    const Performance& performance;
    juce::Random rng;
    juce::ADSR ampEnv;
    Glide glide;
    std::array<Grain, 48> grains;
    std::array<float, 2> ic1 {}, ic2 {};
    double noteHz = 261.6, hz = 261.6, countdown = 0.0, scanOffset = 0.0;
    float level = 0.5f;
};

/** The Granular settings as JSON, for projects and saved sounds. */
inline juce::var granularParamsToVar (const GranularParams& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("source", p.source.load());       o->setProperty ("position", p.position.load());
    o->setProperty ("size", p.size.load());           o->setProperty ("density", p.density.load());
    o->setProperty ("spread", p.spread.load());       o->setProperty ("scan", p.scan.load());
    o->setProperty ("pitch", p.pitch.load());         o->setProperty ("detune", p.detune.load());
    o->setProperty ("reverse", p.reverse.load());     o->setProperty ("spray", p.spray.load());
    o->setProperty ("cutoff", p.cutoffHz.load());     o->setProperty ("resonance", p.resonance.load());
    o->setProperty ("attack", p.attack.load());       o->setProperty ("decay", p.decay.load());
    o->setProperty ("sustain", p.sustain.load());     o->setProperty ("release", p.release.load());
    o->setProperty ("level", p.level.load());         o->setProperty ("velocity", p.velocity.load());
    o->setProperty ("glide", p.glide.load());         o->setProperty ("glideMode", p.glideMode.load());
    return juce::var (o);
}

/** Reads settings written by granularParamsToVar(); anything missing gets its default. */
inline void applyGranularParams (GranularParams& p, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    p.source.store (juce::jlimit (0, GranularSources::numSources - 1, i ("source", GranularSources::choir)));
    p.position.store (f ("position", 0.3f));   p.size.store (f ("size", 0.12f));
    p.density.store (f ("density", 20.0f));    p.spread.store (f ("spread", 0.15f));
    p.scan.store (f ("scan", 0.0f));           p.pitch.store (f ("pitch", 0.0f));
    p.detune.store (f ("detune", 0.1f));       p.reverse.store (f ("reverse", 0.0f));
    p.spray.store (f ("spray", 0.6f));         p.cutoffHz.store (f ("cutoff", 12000.0f));
    p.resonance.store (f ("resonance", 0.1f)); p.attack.store (f ("attack", 0.2f));
    p.decay.store (f ("decay", 0.6f));         p.sustain.store (f ("sustain", 0.8f));
    p.release.store (f ("release", 0.9f));     p.level.store (f ("level", 1.0f));
    p.velocity.store (f ("velocity", 0.5f));   p.glide.store (f ("glide", 0.0f));
    p.glideMode.store (juce::jlimit (0, Glide::numModes - 1, i ("glideMode", 0)));
}

} // namespace bizgroove
