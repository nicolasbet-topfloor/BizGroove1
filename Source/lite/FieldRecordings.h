#pragma once

#include "engine/Track.h"
#include "engine/SampleTools.h"
#include <cmath>

namespace bizgroove::lite
{

/** A field recording each light app starts with, so its Sampler sounds before you load
    your own: made here, never recorded from anyone. Twenty seconds of a place: rain on a
    roof, birds, a far-off street and wind, mixed differently for each app. */
class FieldRecordings
{
public:
    static constexpr double rate = 48000.0;

    struct Mix { const char* name; float rain, birds, street, wind; std::uint32_t seed; };

    static Mix mixFor (int style)
    {
        static const Mix mixes[] = {
            { "Street at night", 0.25f, 0.05f, 0.9f, 0.3f, 11u },   // BizHop
            { "Seaside wind",    0.1f,  0.3f,  0.2f, 1.0f, 23u },   // BizWave
            { "Rain on a roof",  1.0f,  0.0f,  0.35f, 0.2f, 37u },  // BizTechno
            { "Forest rain",     0.7f,  0.8f,  0.0f, 0.5f, 41u },   // BizNoise
            { "Park morning",    0.15f, 1.0f,  0.45f, 0.3f, 53u }   // BizSampler
        };
        return mixes[juce::jlimit (0, 4, style)];
    }

    static juce::AudioBuffer<float> make (const Mix& m, double seconds = 20.0)
    {
        const int n = (int) (seconds * rate);
        juce::AudioBuffer<float> out (2, n);
        out.clear();
        juce::Random rng ((juce::int64) m.seed);
        float* l = out.getWritePointer (0);
        float* r = out.getWritePointer (1);

        // Wind and the street: noise through slow-moving low-pass filters, swelling and fading.
        float wl = 0.0f, wr = 0.0f, sl = 0.0f, sr = 0.0f, sl2 = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / rate;
            const float gust = 0.5f + 0.5f * (float) std::sin (t * 0.31 + std::sin (t * 0.07) * 2.0);
            const float a = 0.002f + 0.01f * gust;
            wl += a * (rng.nextFloat() * 2.0f - 1.0f - wl);
            wr += a * (rng.nextFloat() * 2.0f - 1.0f - wr);
            l[i] += m.wind * 6.0f * gust * wl;
            r[i] += m.wind * 6.0f * gust * wr;

            const float hum = 0.6f + 0.4f * (float) std::sin (t * 0.19);
            sl += 0.02f * (rng.nextFloat() * 2.0f - 1.0f - sl);
            sl2 += 0.05f * (sl - sl2);
            sr += 0.02f * (rng.nextFloat() * 2.0f - 1.0f - sr);
            l[i] += m.street * 2.5f * hum * sl2;
            r[i] += m.street * 2.5f * hum * (0.7f * sl2 + 0.3f * sr);
        }

        // A car passing now and then: a rising and falling hiss.
        if (m.street > 0.0f)
            for (double at = 2.0 + rng.nextDouble() * 3.0; at < seconds - 4.0; at += 5.0 + rng.nextDouble() * 6.0)
            {
                const int start = (int) (at * rate), len = (int) (3.5 * rate);
                float lp = 0.0f;
                const float pan = rng.nextFloat();
                for (int k = 0; k < len && start + k < n; ++k)
                {
                    const float x = (float) k / (float) len;
                    const float env = std::sin (juce::MathConstants<float>::pi * x);
                    lp += 0.08f * (rng.nextFloat() * 2.0f - 1.0f - lp);
                    const float v = m.street * 0.6f * env * env * lp;
                    l[start + k] += v * (1.0f - pan * x);
                    r[start + k] += v * (0.3f + pan * x);
                }
            }

        // Rain: thousands of tiny drops, each a short click that rings a little.
        if (m.rain > 0.0f)
        {
            const int drops = (int) (seconds * 900.0 * m.rain);
            for (int d = 0; d < drops; ++d)
            {
                const int at = rng.nextInt (juce::jmax (1, n - 2000));
                const float amp = m.rain * (0.02f + 0.1f * rng.nextFloat() * rng.nextFloat());
                const float f = 1500.0f + 5000.0f * rng.nextFloat();
                const float pan = rng.nextFloat();
                const int len = 200 + rng.nextInt (600);
                for (int k = 0; k < len; ++k)
                {
                    const float v = amp * std::exp (-(float) k / (len * 0.25f))
                                  * (float) std::sin (juce::MathConstants<double>::twoPi * f * k / rate);
                    l[at + k] += v * (1.0f - pan);
                    r[at + k] += v * pan;
                }
            }
        }

        // Birds: short whistles that sweep and warble, a few in a row.
        if (m.birds > 0.0f)
            for (double at = 0.5; at < seconds - 1.0; at += 0.6 + rng.nextDouble() * 2.5 / m.birds)
            {
                const int calls = 1 + rng.nextInt (4);
                const float base = 2200.0f + 2500.0f * rng.nextFloat();
                const float pan = rng.nextFloat();
                double ph = 0.0;
                for (int c = 0; c < calls; ++c)
                {
                    const int start = (int) ((at + c * 0.14) * rate), len = (int) ((0.06 + 0.08 * rng.nextDouble()) * rate);
                    const float sweep = (rng.nextFloat() - 0.3f) * 1500.0f;
                    for (int k = 0; k < len && start + k < n; ++k)
                    {
                        const float x = (float) k / (float) len;
                        const float env = std::sin (juce::MathConstants<float>::pi * x);
                        const float f = base + sweep * x + 300.0f * (float) std::sin (k * 0.004);
                        ph += juce::MathConstants<double>::twoPi * f / rate;
                        const float v = m.birds * 0.25f * env * (float) std::sin (ph);
                        l[start + k] += v * (1.0f - 0.6f * pan);
                        r[start + k] += v * (0.4f + 0.6f * pan);
                    }
                }
            }

        const float peak = juce::jmax (1.0e-4f, out.getMagnitude (0, n));
        out.applyGain (0.8f / peak);
        const int fade = (int) (0.05 * rate);
        out.applyGainRamp (0, fade, 0.0f, 1.0f);
        out.applyGainRamp (n - fade, fade, 1.0f, 0.0f);
        return out;
    }

    /** Puts the app's field recording on a Sampler track (and keeps it, so projects save it). */
    static void load (Track& t, int style)
    {
        const auto m = mixFor (style);
        const auto audio = make (m);
        t.samplerEngine.setSample (audio, rate, m.name);
        t.sampleFileName = juce::String (m.name) + ".wav";
        t.sampleFileData = SampleTools::encodeWav (audio, rate);
    }
};

} // namespace bizgroove::lite
