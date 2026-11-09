#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // PolyBlepSaw, Performance
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** Settings of the Poly: an eight-voice analogue-style polysynth in the manner
    of the classic late-1970s polys built from synth modules. Shared between
    the UI and the voices. */
struct PolyParams
{
    std::atomic<float> cutoffHz    { 1800.0f };
    std::atomic<float> resonance   { 0.25f };   // 0..1
    std::atomic<float> shape       { 0.0f };    // filter: 0 low-pass, 0.5 notch, 1 high-pass
    std::atomic<float> envAmount   { 0.45f };   // 0..1
    std::atomic<float> attack      { 0.005f };  // seconds
    std::atomic<float> decay       { 0.45f };
    std::atomic<float> sustain     { 0.7f };
    std::atomic<float> release     { 0.35f };
    std::atomic<float> detuneCents { 7.0f };    // between the stacked tones
    std::atomic<float> pulseWidth  { 0.5f };    // 0.05..0.95
    std::atomic<float> mix         { 0.5f };    // 0 = saw tone only, 1 = pulse tone only
    std::atomic<int>   octave      { 0 };       // second tone: -1, 0 or +1 octave
    std::atomic<int>   unison      { 1 };       // tones stacked per note: 1..4
    std::atomic<float> glide       { 0.0f };    // seconds
    std::atomic<float> lfoRate     { 5.0f };    // Hz
    std::atomic<float> lfoFilter   { 0.0f };    // 0..1
    std::atomic<float> lfoPulse    { 0.0f };    // 0..1
    std::atomic<float> noise       { 0.0f };    // 0..1

    double lastNoteHz = 0.0;   // audio thread only: where a glide starts
};

/** Band-limited pulse wave (PolyBLEP), with a variable width. */
class PolyBlepPulse
{
public:
    void setFrequency (double hz, double sampleRate) { increment = hz / sampleRate; }
    void reset (double startPhase = 0.0)             { phase = startPhase; }

    float next (double width)
    {
        width = juce::jlimit (0.05, 0.95, width);
        double value = phase < width ? 1.0 : -1.0;
        value += blep (phase, increment);
        value -= blep (std::fmod (phase + 1.0 - width, 1.0), increment);
        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
        return (float) value;
    }

private:
    static double blep (double t, double dt)
    {
        if (t < dt)        { t /= dt;            return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    double phase = 0.0, increment = 0.0;
};

struct PolySound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** One voice: up to four stacked pairs of tones (saw and pulse), noise, a
    12 dB state-variable filter that sweeps from low-pass through notch to
    high-pass, its own envelope, and a wobble (LFO) for the filter and pulse. */
class PolyVoice final : public juce::SynthesiserVoice
{
public:
    static constexpr int maxUnison = 4;

    PolyVoice (PolyParams& p, const Performance& perf) : params (p), performance (perf) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PolySound*> (s) != nullptr; }

    void prepare (double sampleRate)
    {
        ampEnv.setSampleRate (sampleRate);
        filterEnv.setSampleRate (sampleRate);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        targetHz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        const float glide = params.glide.load();
        currentHz = (glide > 0.001f && params.lastNoteHz > 0.0) ? params.lastNoteHz : targetHz;
        params.lastNoteHz = targetHz;

        unison = juce::jlimit (1, maxUnison, params.unison.load());
        if (! ampEnv.isActive())
        {
            for (int i = 0; i < maxUnison; ++i)
            {
                saws[(size_t) i].reset (0.13 * i);
                pulses[(size_t) i].reset (0.29 * i + 0.5);
            }
            ic1 = ic2 = 0.0f;
        }

        level = 0.2f * velocity / std::sqrt ((float) unison);
        ampEnv.setParameters ({ params.attack.load(), params.decay.load(), params.sustain.load(), params.release.load() });
        filterEnv.setParameters ({ params.attack.load(), params.decay.load(), params.sustain.load() * 0.6f, params.release.load() });
        ampEnv.noteOn();
        filterEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            filterEnv.noteOff();
        }
        else
        {
            ampEnv.reset();
            filterEnv.reset();
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
        const float glide = params.glide.load();
        const double glideCoef = glide > 0.001f ? std::exp (-1.0 / (glide * sr)) : 0.0;

        const float baseCutoff = params.cutoffHz.load();
        const float envDepth = params.envAmount.load();
        const float lfoFilter = params.lfoFilter.load();
        const float lfoPulse = params.lfoPulse.load();
        const float mix = juce::jlimit (0.0f, 1.0f, params.mix.load());
        const float noiseLevel = juce::jlimit (0.0f, 1.0f, params.noise.load());
        const float shape = juce::jlimit (0.0f, 1.0f, params.shape.load());
        const float k = 2.0f - 1.94f * juce::jlimit (0.0f, 1.0f, params.resonance.load());   // damping: lower = more resonance
        const double width = params.pulseWidth.load();
        const float nyquistLimit = (float) sr * 0.45f;
        const int numChannels = output.getNumChannels();

        constexpr int controlInterval = 16;
        float g = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float lfo = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float fEnv = filterEnv.getNextSample();
            vibratoPhase += vibratoStep;
            if (vibratoPhase > twoPi) vibratoPhase -= twoPi;
            lfoPhase += lfoStep;
            if (lfoPhase > twoPi) lfoPhase -= twoPi;

            if ((i % controlInterval) == 0)
            {
                lfo = (float) std::sin (lfoPhase);
                currentHz = targetHz + (currentHz - targetHz) * std::pow (glideCoef, (double) controlInterval);
                updatePitch (sr);

                // The envelope opens the filter up to five octaves; the wobble up to two either way.
                const float cutoff = baseCutoff * std::pow (2.0f, 5.0f * envDepth * fEnv + 2.0f * lfoFilter * lfo);
                g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, nyquistLimit, cutoff) / (float) sr);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            // The tones.
            const double w = width + 0.4 * lfoPulse * lfo;
            float sawSum = 0.0f, pulseSum = 0.0f;
            for (int u = 0; u < unison; ++u)
            {
                sawSum += saws[(size_t) u].next();
                pulseSum += pulses[(size_t) u].next (w);
            }
            float x = (1.0f - mix) * sawSum + mix * pulseSum;
            if (noiseLevel > 0.0f)
                x += noiseLevel * (random.nextFloat() * 2.0f - 1.0f) * (float) unison;

            // A gentle drive before the filter, then the state-variable filter (TPT form).
            x = std::tanh (0.6f * x);
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            const float low = v2, high = x - k * v1 - v2, notch = low + high;
            const float filtered = shape < 0.5f ? low + (notch - low) * (shape * 2.0f)
                                                : notch + (high - notch) * ((shape - 0.5f) * 2.0f);

            const float sample = filtered * ampEnv.getNextSample() * level;
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
    /** Note pitch with glide, pitch bend and vibrato; the stacked tones spread around it. */
    void updatePitch (double sampleRate)
    {
        const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                               + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (vibratoPhase);
        const double hz = currentHz * std::pow (2.0, semitones / 12.0);
        const double octave = std::pow (2.0, (double) juce::jlimit (-1, 1, params.octave.load()));
        const double cents = params.detuneCents.load();

        for (int u = 0; u < unison; ++u)
        {
            // Spread: the first pair sits on the note; the others fan out either side.
            const double offset = unison == 1 ? 0.0 : ((double) u / (unison - 1) - 0.5) * 2.0 * cents;
            const double sawHz = hz * std::pow (2.0, (offset - 0.5 * cents) / 1200.0);
            const double pulseHz = hz * octave * std::pow (2.0, (offset + 0.5 * cents) / 1200.0);
            saws[(size_t) u].setFrequency (sawHz, sampleRate);
            pulses[(size_t) u].setFrequency (pulseHz, sampleRate);
        }
    }

    PolyParams& params;
    const Performance& performance;
    double targetHz = 440.0, currentHz = 440.0;
    double vibratoPhase = 0.0, lfoPhase = 0.0;
    int unison = 1;
    std::array<PolyBlepSaw, maxUnison> saws;
    std::array<PolyBlepPulse, maxUnison> pulses;
    float ic1 = 0.0f, ic2 = 0.0f;
    juce::ADSR ampEnv, filterEnv;
    juce::Random random;
    float level = 0.0f;
};

} // namespace bizgroove
