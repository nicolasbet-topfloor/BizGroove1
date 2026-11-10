#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

namespace bizgroove
{

/** A tape echo in the manner of the classic multi-head units: a loop of tape
    passing a record head and three playback heads. The head selector picks
    which heads are heard (each one repeats later than the last). Intensity
    feeds the echoes back onto the tape; past about three quarters it builds up
    and, near the top, runs away into self-oscillation, held in check by the
    tape's own saturation. Wow and flutter make the pitch sway; wear dulls the
    sound, adds hiss and the odd dropout. Audio thread. */
class TapeEcho
{
public:
    static constexpr int numModes = 7;

    static juce::String modeName (int mode)
    {
        static const char* names[numModes] = { "Head 1", "Head 2", "Head 3", "Heads 1+2", "Heads 2+3", "Heads 1+3", "Heads 1+2+3" };
        return names[juce::jlimit (0, numModes - 1, mode)];
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int size = juce::nextPowerOfTwo ((int) (sampleRate * 4.2));
        line.assign ((size_t) size, 0.0f);
        mask = size - 1;
        reset();
    }

    void reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
        write = 0;
        wowPhase = flutterPhase = 0.0;
        drift = driftTarget = 0.0f;
        loopLow = loopHigh = 0.0f;
        outLowL = outLowR = 0.0f;
        dropGain = 1.0f;
        smoothedHead = -1.0;
    }

    /** Adds the echoes of inL/inR into outL/outR.
        headSeconds: the time to the first playback head; heads 2 and 3 repeat at twice and three times that.
        intensity 0..1 (above about 0.85 it self-oscillates); wowFlutter, tone and wear 0..1. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  double headSeconds, int mode, float intensity, float wowFlutter, float tone, float wear)
    {
        static constexpr int headsFor[numModes] = { 1, 2, 4, 3, 6, 5, 7 };
        const int heads = headsFor[juce::jlimit (0, numModes - 1, mode)];
        int count = 0;
        for (int h = 0; h < 3; ++h)
            count += (heads >> h) & 1;
        const float headGain = 1.0f / std::sqrt ((float) juce::jmax (1, count));

        const double maxHead = (double) (mask - 8) / 3.0;
        const double target = juce::jlimit (8.0, maxHead, headSeconds * sr);
        if (smoothedHead < 0.0)
            smoothedHead = target;
        // Changing the time slides the tape speed, as on the real thing.
        const double slide = 1.0 - std::exp (-1.0 / (0.25 * sr));

        const float feedback = 1.4f * juce::jlimit (0.0f, 1.0f, intensity);
        const float lowCoef = std::exp (-juce::MathConstants<float>::twoPi * (1500.0f * std::pow (8.0f, tone) * (1.0f - 0.6f * wear)) / (float) sr);
        const float highCoef = std::exp (-juce::MathConstants<float>::twoPi * 120.0f / (float) sr);
        const double wowStep = juce::MathConstants<double>::twoPi * 0.55 / sr;
        const double flutterStep = juce::MathConstants<double>::twoPi * 6.5 / sr;
        const float wowDepth = (float) sr * (0.0022f * wowFlutter + 0.0012f * wear);
        const float flutterDepth = (float) sr * 0.00035f * wowFlutter;
        const float hiss = 0.0025f * wear * wear;
        static constexpr float panL[3] = { 1.0f, 0.45f, 0.75f };
        static constexpr float panR[3] = { 0.45f, 1.0f, 0.75f };

        for (int i = 0; i < n; ++i)
        {
            smoothedHead += slide * (target - smoothedHead);
            wowPhase += wowStep;
            if (wowPhase > juce::MathConstants<double>::twoPi) wowPhase -= juce::MathConstants<double>::twoPi;
            flutterPhase += flutterStep;
            if (flutterPhase > juce::MathConstants<double>::twoPi) flutterPhase -= juce::MathConstants<double>::twoPi;

            // A slow random drift, worse on worn tape.
            if ((i & 4095) == 0)
                driftTarget = random.nextFloat() * 2.0f - 1.0f;
            drift += 0.00004f * (driftTarget - drift);

            // Worn tape drops out now and then.
            if (wear > 0.3f && random.nextFloat() < wear * 1.5f / (float) sr)
                dropGain = 0.25f + 0.4f * random.nextFloat();
            dropGain += 0.0004f * (1.0f - dropGain);

            const float sway = wowDepth * (float) std::sin (wowPhase) + flutterDepth * (float) std::sin (flutterPhase) + wowDepth * 0.4f * drift;

            float sum = 0.0f, yl = 0.0f, yr = 0.0f;
            for (int h = 0; h < 3; ++h)
            {
                if (((heads >> h) & 1) == 0)
                    continue;
                const double delay = (double) (h + 1) * (smoothedHead + (double) sway);
                const float y = read (delay) * dropGain;
                sum += y;
                yl += y * panL[h];
                yr += y * panR[h];
            }
            sum *= headGain;

            // The loop: the replayed echo, darkened and thinned like tape, back onto the record head.
            loopLow = sum + lowCoef * (loopLow - sum);
            loopHigh = loopLow + highCoef * (loopHigh - loopLow);
            const float replay = loopLow - loopHigh;
            const float input = 0.5f * (inL[i] + inR[i]) + hiss * (random.nextFloat() * 2.0f - 1.0f);
            line[(size_t) write] = std::tanh (1.1f * (input + feedback * replay)) / 1.1f;   // tape saturation
            write = (write + 1) & mask;

            yl *= headGain;
            yr *= headGain;
            outLowL = yl + lowCoef * (outLowL - yl);
            outLowR = yr + lowCoef * (outLowR - yr);
            outL[i] += outLowL;
            outR[i] += outLowR;
        }
    }

private:
    float read (double delay) const
    {
        double pos = (double) write - delay;
        const double size = (double) (mask + 1);
        while (pos < 0.0) pos += size;
        const int i0 = (int) pos & mask;
        const int i1 = (i0 + 1) & mask;
        const float frac = (float) (pos - std::floor (pos));
        return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
    }

    double sr = 44100.0;
    std::vector<float> line;
    int write = 0, mask = 0;
    double wowPhase = 0.0, flutterPhase = 0.0, smoothedHead = -1.0;
    float drift = 0.0f, driftTarget = 0.0f;
    float loopLow = 0.0f, loopHigh = 0.0f, outLowL = 0.0f, outLowR = 0.0f, dropGain = 1.0f;
    juce::Random random { 0x5eed };
};

/** A spring reverb: three springs, each a run of all-pass stages (which smear
    a click into the springy "boing") inside a feedback loop. Decay sets how
    long it rings, tone how bright it is, drip how springy the chirp is.
    Audio thread. */
class SpringReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        static constexpr double seconds[numSprings] = { 0.0331, 0.0419, 0.0537 };
        for (int s = 0; s < numSprings; ++s)
        {
            auto& spring = springs[(size_t) s];
            const int size = juce::nextPowerOfTwo ((int) (seconds[s] * sampleRate) + 4);
            spring.line.assign ((size_t) size, 0.0f);
            spring.mask = size - 1;
            spring.length = (int) (seconds[s] * sampleRate);
        }
        reset();
    }

    void reset()
    {
        for (auto& spring : springs)
        {
            std::fill (spring.line.begin(), spring.line.end(), 0.0f);
            spring.write = 0;
            spring.low = 0.0f;
            spring.stages.fill (0.0f);
        }
        inLow = 0.0f;
    }

    /** Adds the reverb of inL/inR into outL/outR. decay, tone and drip 0..1. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, float decay, float tone, float drip)
    {
        const float feedback = 0.45f + 0.5f * juce::jlimit (0.0f, 1.0f, decay);
        const float lowCoef = std::exp (-juce::MathConstants<float>::twoPi * (1800.0f + 5200.0f * tone) / (float) sr);
        const float highCoef = std::exp (-juce::MathConstants<float>::twoPi * 180.0f / (float) sr);
        const float a = 0.5f + 0.3f * juce::jlimit (0.0f, 1.0f, drip);

        for (int i = 0; i < n; ++i)
        {
            // Springs don't carry the lows: take them out before they go in.
            const float x = 0.5f * (inL[i] + inR[i]);
            inLow = x + highCoef * (inLow - x);
            const float driven = std::tanh (1.5f * (x - inLow));

            float out[numSprings] {};
            for (size_t s = 0; s < springs.size(); ++s)
            {
                auto& spring = springs[s];
                int read = spring.write - spring.length;
                if (read < 0) read += spring.mask + 1;
                const float delayed = spring.line[(size_t) read];
                spring.low = delayed + lowCoef * (spring.low - delayed);

                float v = driven + feedback * spring.low;
                for (auto& z : spring.stages)
                {
                    const float y = a * v + z;   // first-order all-pass: smears the click over time
                    z = v - a * y;
                    v = y;
                }
                spring.line[(size_t) spring.write] = v;
                spring.write = (spring.write + 1) & spring.mask;
                out[s] = delayed;
            }
            outL[i] += 0.6f * (out[0] + 0.7f * out[2]);
            outR[i] += 0.6f * (out[1] + 0.7f * out[2]);
        }
    }

private:
    static constexpr int numSprings = 3;
    struct Spring
    {
        std::vector<float> line;
        int write = 0, mask = 0, length = 1;
        float low = 0.0f;
        std::array<float, 14> stages {};
    };

    double sr = 44100.0;
    std::array<Spring, numSprings> springs;
    float inLow = 0.0f;
};

} // namespace bizgroove
