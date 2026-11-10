#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Glide.h"
#include "OscLayer.h"
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** Parameters shared between the UI and the voices. */
struct TestSynthParams
{
    std::atomic<float> cutoffHz   { 1200.0f };
    std::atomic<float> resonance  { 0.35f };   // 0..1
    std::atomic<float> envAmount  { 0.6f };    // 0..1, filter envelope depth
    std::atomic<float> attack     { 0.003f };  // seconds
    std::atomic<float> decay      { 0.25f };
    std::atomic<float> sustain    { 0.4f };
    std::atomic<float> release    { 0.2f };
    std::atomic<float> detuneCents{ 8.0f };
    std::atomic<float> glide      { 0.0f };    // seconds (per octave when gliding by distance)
    std::atomic<int>   glideMode  { Glide::fixedTime };

    mutable double lastNoteHz = 0.0;   // audio thread only: where a glide starts
    OscSettings osc;                    // the Oscillators section

    static constexpr float bendRangeSemitones = 2.0f;
    static constexpr float vibratoMaxSemitones = 0.5f;
    static constexpr float vibratoRateHz = 5.5f;
};

/** Pitch bend and modulation: one set for the whole instrument, from the
    keyboard's strips or the on-screen ones. */
struct Performance
{
    std::atomic<float> pitchBend { 0.0f };   // -1..1 = -2..+2 semitones
    std::atomic<float> modWheel  { 0.0f };   // 0..1 = vibrato depth, up to +/-50 cents
};

/** Band-limited sawtooth using a PolyBLEP correction, so high notes don't alias. */
class PolyBlepSaw
{
public:
    void setFrequency (double hz, double sampleRate) { increment = hz / sampleRate; }
    void reset (double startPhase = 0.0)             { phase = startPhase; }

    float next()
    {
        double value = 2.0 * phase - 1.0;
        value -= polyBlep (phase, increment);
        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
        return (float) value;
    }

private:
    static double polyBlep (double t, double dt)
    {
        if (t < dt)        { t /= dt;               return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt;    return t * t + t + t + 1.0; }
        return 0.0;
    }

    double phase = 0.0, increment = 0.0;
};

struct TestSynthSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** Two detuned saws into a resonant low-pass filter with its own envelope. */
class TestSynthVoice final : public juce::SynthesiserVoice
{
public:
    TestSynthVoice (const TestSynthParams& p, const Performance& perf) : params (p), performance (perf) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<TestSynthSound*> (s) != nullptr; }

    void prepare (double sampleRate, int maxBlockSize)
    {
        filter.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        ampEnv.setSampleRate (sampleRate);
        filterEnv.setSampleRate (sampleRate);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        const auto sr = getSampleRate();
        const double hz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        glide.start (params.lastNoteHz, hz, params.glide.load(), params.glideMode.load(), sr);
        params.lastNoteHz = hz;
        baseHz = glide.hz();
        updatePitch (sr);
        if (! ampEnv.isActive())
        {
            oscA.reset (0.0);
            oscB.reset (0.37);
            layer.reset();
            filter.reset();
        }

        level = 0.25f * velocity;

        ampEnv.setParameters ({ params.attack.load(), params.decay.load(), params.sustain.load(), params.release.load() });
        filterEnv.setParameters ({ 0.001f, params.decay.load(), 0.0f, params.release.load() });
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
        const double lfoStep = juce::MathConstants<double>::twoPi * TestSynthParams::vibratoRateHz / sr;

        const float baseCutoff = params.cutoffHz.load();
        const float envDepth = params.envAmount.load();
        const float nyquistLimit = (float) getSampleRate() * 0.45f;
        filter.setResonance (juce::jmap (params.resonance.load(), 0.7f, 8.0f));

        constexpr int controlInterval = 16; // update the filter every 16 samples
        const int numChannels = output.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            const float env = filterEnv.getNextSample();
            lfoPhase += lfoStep;
            if (lfoPhase > juce::MathConstants<double>::twoPi)
                lfoPhase -= juce::MathConstants<double>::twoPi;

            if ((i % controlInterval) == 0)
            {
                baseHz = glide.advance (controlInterval);
                updatePitch (sr);

                // Envelope sweeps the cutoff up to four octaves above the knob.
                const float cutoff = baseCutoff * std::pow (2.0f, 4.0f * envDepth * env);
                filter.setCutoffFrequency (juce::jlimit (20.0f, nyquistLimit, cutoff));
            }

            const float raw = params.osc.main.load() * 0.5f * (oscA.next() + oscB.next()) + 2.0f * layer.next (params.osc);
            const float sample = filter.processSample (0, raw) * ampEnv.getNextSample() * level;

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
    /** Note pitch, plus pitch bend and vibrato, plus the detune spread. */
    void updatePitch (double sampleRate)
    {
        const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                               + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (lfoPhase);
        const double hz = baseHz * std::pow (2.0, semitones / 12.0);
        const double spread = std::pow (2.0, params.detuneCents.load() / 1200.0);
        oscA.setFrequency (hz * spread, sampleRate);
        oscB.setFrequency (hz / spread, sampleRate);
        layer.setPitch (hz, sampleRate, params.osc);
    }

    const TestSynthParams& params;
    const Performance& performance;
    double baseHz = 440.0;
    double lfoPhase = 0.0;
    Glide glide;
    PolyBlepSaw oscA, oscB;
    OscLayer layer;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR ampEnv, filterEnv;
    float level = 0.0f;
};

} // namespace bizgroove
