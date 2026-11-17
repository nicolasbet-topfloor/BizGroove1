#pragma once

#include "engine/AudioEngine.h"
#include "engine/SampleTools.h"
#include <cmath>
#include <vector>

namespace bizgroove::lite
{

/** Drum hits and instrument notes made sample by sample, for the light apps'
    sounds: a dusty boom-bap kit (with the grit of an old 12-bit sampler), a crisp
    trap kit, an 808, a round bass and an electric piano. They are made once, at
    48 kHz, and put on drum pads or in a Sampler like any recording, so they are
    saved with the project. */
class OneShots
{
public:
    static constexpr double rate = 48000.0;

    // ---------------------------------------------------------------- what the apps load
    enum Kit { boomBap = 0, trap };

    /** Puts a kit on a Drums track's pads. Pads the kit leaves empty play their synthesised drum. */
    static void loadKit (Track& t, Kit kit)
    {
        for (int d = 0; d < DrumKit::numDrums; ++d)
        {
            t.kit.clearPadSample (d);
            t.padFileNames[(size_t) d] = {};
            t.padFileData[(size_t) d].reset();
        }
        auto put = [&t] (int d, const juce::String& name, const std::vector<float>& x, float volume) {
            pad (t, d, name, x);
            auto& p = t.kit.drums[(size_t) d];
            p.pitch.store (0.0f); p.length.store (0.5f); p.tone.store (1.0f);
            p.snap.store (0.0f); p.drive.store (0.0f); p.pan.store (0.0f); p.volume.store (volume);
        };
        const bool dusty = kit == boomBap;
        const auto& k = dusty ? dustyKit() : trapKit();
        put (0, "Kick", k.kick, 0.95f);
        put (2, "Snare", k.snare, dusty ? 0.8f : 0.75f);
        put (4, "Ghost snare", k.snare, 0.28f);
        put (3, "Clap", k.clap, 0.75f);
        put (1, "Rim", k.rim, 0.6f);
        put (6, "Hat", k.hat, dusty ? 0.5f : 0.45f);
        put (8, "Soft hat", k.hat, dusty ? 0.22f : 0.25f);
        put (10, "Open hat", k.openHat, 0.4f);
        put (9, "Shaker", k.shaker, 0.35f);
        t.kit.drums[10].pan.store (0.15f);
        t.kit.drums[9].pan.store (-0.2f);
        t.kit.velocity.store (0.3f);
    }

    enum Instrument { eightOhEight = 0, dustyBass, electricPiano };

    /** Puts an instrument in a Sampler track, tuned so every note plays at its own pitch. */
    static void loadInstrument (Track& t, Instrument which)
    {
        const auto& x = which == eightOhEight ? tr808() : which == dustyBass ? roundBass() : piano();
        const juce::String name = which == eightOhEight ? "808" : which == dustyBass ? "Dusty bass" : "Dusty piano";
        juce::AudioBuffer<float> audio (2, (int) x.size());
        audio.copyFrom (0, 0, x.data(), (int) x.size());
        audio.copyFrom (1, 0, x.data(), (int) x.size());
        t.type.store (Track::sampler);
        t.samplerEngine.setSample (audio, rate, name);
        t.sampleFileName = name + ".wav";
        t.sampleFileData = SampleTools::encodeWav (audio, rate);
        auto& p = t.samplerEngine.params;
        // The bass notes were made at C2 (MIDI 36), the piano at C4 (60, the Sampler's own pitch).
        p.pitch.store (which == electricPiano ? 0.0f : 24.0f);
        p.start.store (0.0f); p.length.store (1.0f); p.brightness.store (1.0f);
        p.attack.store (0.001f); p.sustain.store (1.0f); p.loop.store (false); p.reverse.store (false);
        p.slices.store (1);
        p.release.store (which == eightOhEight ? 0.12f : which == dustyBass ? 0.08f : 0.35f);
    }

    // ---------------------------------------------------------------- the sounds
    struct DrumSet { std::vector<float> kick, snare, clap, rim, hat, openHat, shaker; };

    static const DrumSet& dustyKit()
    {
        static const DrumSet k = [] {
            DrumSet s;
            s.kick    = lofi (kick (52.0f, 150.0f, 0.032f, 0.20f, 0.45f, 1.8f, 0.45f));
            s.snare   = lofi (snare (190.0f, 0.09f, 0.24f, 1.8f, 0.8f, 1.0f));   // a fat body and a bright crack
            s.clap    = lofi (clap (0.14f));
            s.rim     = lofi (rim());
            s.hat     = lofi (hat (0.05f, 0.08f));
            s.openHat = lofi (hat (0.30f, 0.45f));
            s.shaker  = lofi (shaker());
            return s;
        }();
        return k;
    }

    static const DrumSet& trapKit()
    {
        static const DrumSet k = [] {
            DrumSet s;
            s.kick    = kick (55.0f, 230.0f, 0.018f, 0.11f, 0.30f, 2.6f, 0.8f);
            s.snare   = snare (200.0f, 0.10f, 0.22f, 1.3f, 0.6f, 1.2f);
            s.clap    = clap (0.16f);
            s.rim     = rim();
            s.hat     = hat (0.028f, 0.05f);
            s.openHat = hat (0.25f, 0.35f);
            s.shaker  = shaker();
            return s;
        }();
        return k;
    }

    /** An 808: a sine at C2 that drops a little at the start, rings long, and is driven so it is heard on small speakers. */
    static const std::vector<float>& tr808()
    {
        static const std::vector<float> x = [] {
            const float f0 = 65.406f;
            auto y = make (2.6f, [f0] (float t, double& phase, juce::Random& rng) {
                const float f = f0 * (1.0f + 0.45f * std::exp (-t / 0.02f));
                phase += (double) f / rate;
                const float amp = (1.0f - std::exp (-t / 0.002f)) * std::exp (-t / 0.9f);
                const float click = (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t / 0.0015f) * 0.15f;
                return std::tanh (2.4f * (float) std::sin (juce::MathConstants<double>::twoPi * phase) * amp) / std::tanh (2.4f) + click;
            });
            return finish (y, 0.5f, 0.01f);
        }();
        return x;
    }

    /** A round, short bass at C2, plucked: a sine with a little triangle, through a filter that closes. */
    static const std::vector<float>& roundBass()
    {
        static const std::vector<float> x = [] {
            const float f0 = 65.406f;
            auto y = make (1.6f, [f0] (float t, double& phase, juce::Random&) {
                phase += (double) f0 / rate;
                const double p = phase - std::floor (phase);
                const float sine = (float) std::sin (juce::MathConstants<double>::twoPi * p);
                const float tri = (float) (4.0 * std::abs (p - 0.5) - 1.0);
                const float second = (float) std::sin (2.0 * juce::MathConstants<double>::twoPi * p) * std::exp (-t / 0.15f);
                const float amp = (1.0f - std::exp (-t / 0.003f)) * std::exp (-t / 0.5f);
                return (sine + 0.5f * tri + 0.3f * second) * amp;   // the triangle and the octave carry it on small speakers
            });
            y = lowpass (y, 900.0f, 0.6f);
            return finish (lofi (y, 0.6f), 0.5f, 0.02f);
        }();
        return x;
    }

    /** An electric piano at C4: the tone, a second partial that fades, the bell of the tine, and a soft bark. */
    static const std::vector<float>& piano()
    {
        static const std::vector<float> x = [] {
            const float f0 = 261.626f;
            auto y = make (3.2f, [f0] (float t, double& phase, juce::Random&) {
                phase += (double) f0 / rate;
                const double w = juce::MathConstants<double>::twoPi * phase;
                const float tone = (float) std::sin (w) + 0.5f * (float) std::sin (2.0 * w) * std::exp (-t / 0.7f)
                                   + 0.22f * (float) std::sin (3.0 * w) * std::exp (-t / 0.35f)
                                   + 0.10f * (float) std::sin (4.0 * w) * std::exp (-t / 0.2f)
                                   + 0.15f * (float) std::sin (7.0 * w) * std::exp (-t / 0.03f);
                const float amp = (1.0f - std::exp (-t / 0.002f)) * std::exp (-t / 1.3f);
                const float bark = std::tanh (1.6f * tone * amp * (1.0f + 0.6f * std::exp (-t / 0.08f)));
                return bark;
            });
            y = lowpass (y, 5000.0f, 0.7f);
            return finish (lofi (y, 0.5f), 0.25f, 0.05f);
        }();
        return x;
    }

private:
    // ---------------------------------------------------------------- drum recipes
    static std::vector<float> kick (float low, float high, float drop, float decay, float length, float drive, float click)
    {
        auto y = make (length, [=] (float t, double& phase, juce::Random& rng) {
            const float f = low + (high - low) * std::exp (-t / drop);
            phase += (double) f / rate;
            const double w = juce::MathConstants<double>::twoPi * phase;
            const float amp = std::exp (-t / decay);
            const float body = (float) std::sin (w) * amp + 0.15f * (float) std::sin (2.0 * w) * std::exp (-t / 0.04f);
            const float c = (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t / 0.0025f) * click;
            return std::tanh (drive * (body + c)) / std::tanh (drive);
        });
        return finish (lowpass (y, 6000.0f, 0.7f), 0.95f, 0.004f);
    }

    static std::vector<float> snare (float body, float bodyDecay, float noiseDecay, float drive, float noiseLevel, float room)
    {
        juce::Random rng (3);
        std::vector<float> n ((size_t) (rate * 0.6));
        for (auto& v : n) v = rng.nextFloat() * 2.0f - 1.0f;
        auto bright = highpass (bandpass (n, 2600.0f, 0.6f), 700.0f, 0.7f);
        auto dark = lowpass (n, 2500.0f, 0.7f);
        std::vector<float> y (n.size());
        double p1 = 0, p2 = 0;
        for (size_t i = 0; i < y.size(); ++i)
        {
            const float t = (float) i / (float) rate;
            p1 += (double) body * (1.0 + 0.3 * std::exp (-t / 0.01)) / rate;
            p2 += (double) body * 1.78 / rate;
            const float tone = 0.7f * (float) std::sin (juce::MathConstants<double>::twoPi * p1) * std::exp (-t / bodyDecay)
                               + 0.35f * (float) std::sin (juce::MathConstants<double>::twoPi * p2) * std::exp (-t / (bodyDecay * 0.6f));
            const float noise = bright[i] * noiseLevel * 2.2f * std::exp (-t / (noiseDecay * 0.6f))
                                + dark[i] * 0.12f * room * std::exp (-t / 0.22f);
            y[i] = std::tanh (drive * (tone + noise)) / std::tanh (drive);
        }
        return finish (y, 0.9f, 0.005f);
    }

    static std::vector<float> clap (float tail)
    {
        juce::Random rng (5);
        std::vector<float> y ((size_t) (rate * 0.5));
        const float bursts[4] = { 0.0f, 0.010f, 0.021f, 0.033f };
        for (size_t i = 0; i < y.size(); ++i)
        {
            const float t = (float) i / (float) rate;
            float env = 0.0f;
            for (float b : bursts)
                if (t >= b) env = juce::jmax (env, std::exp (-(t - b) / 0.005f));
            if (t >= bursts[3]) env = juce::jmax (env, 0.6f * std::exp (-(t - bursts[3]) / tail));
            y[i] = (rng.nextFloat() * 2.0f - 1.0f) * env;
        }
        return finish (highpass (bandpass (y, 1300.0f, 1.0f), 500.0f, 0.7f), 0.85f, 0.002f);
    }

    static std::vector<float> rim()
    {
        auto y = make (0.12f, [] (float t, double& phase, juce::Random& rng) {
            phase += 1700.0 / rate;
            return (float) std::sin (juce::MathConstants<double>::twoPi * phase) * std::exp (-t / 0.012f)
                   + (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t / 0.004f) * 0.4f;
        });
        return finish (highpass (y, 400.0f, 0.7f), 0.8f, 0.002f);
    }

    /** A hi-hat: six square tones far apart (the 808's way) and noise, high-passed. */
    static std::vector<float> hat (float decay, float length)
    {
        static const float freqs[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        juce::Random rng (9);
        std::vector<float> y ((size_t) (rate * length));
        std::array<double, 6> ph {};
        for (size_t i = 0; i < y.size(); ++i)
        {
            const float t = (float) i / (float) rate;
            float metal = 0.0f;
            for (int k = 0; k < 6; ++k)
            {
                ph[(size_t) k] += (double) freqs[k] * 2.3 / rate;
                metal += ph[(size_t) k] - std::floor (ph[(size_t) k]) < 0.5 ? 1.0f : -1.0f;
            }
            const float env = std::exp (-t / decay) * (1.0f - std::exp (-t / 0.0005f));
            y[i] = (metal / 6.0f * 0.6f + (rng.nextFloat() * 2.0f - 1.0f) * 0.7f) * env;
        }
        return finish (highpass (bandpass (y, 9500.0f, 0.5f), 6500.0f, 0.7f), 0.8f, 0.003f);
    }

    static std::vector<float> shaker()
    {
        juce::Random rng (13);
        std::vector<float> y ((size_t) (rate * 0.15));
        for (size_t i = 0; i < y.size(); ++i)
        {
            const float t = (float) i / (float) rate;
            y[i] = (rng.nextFloat() * 2.0f - 1.0f) * (1.0f - std::exp (-t / 0.008f)) * std::exp (-t / 0.035f);
        }
        return finish (bandpass (y, 5500.0f, 1.2f), 0.7f, 0.003f);
    }

    // ---------------------------------------------------------------- tools
    template <typename F>
    static std::vector<float> make (float seconds, F f)
    {
        std::vector<float> y ((size_t) (rate * seconds));
        double phase = 0.0;
        juce::Random rng (1);
        for (size_t i = 0; i < y.size(); ++i)
            y[i] = f ((float) i / (float) rate, phase, rng);
        return y;
    }

    /** A biquad (RBJ), run over the whole sound. */
    static std::vector<float> biquad (const std::vector<float>& x, int type, float freq, float q)
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (20.0, rate * 0.45, (double) freq) / rate;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        double b0, b1, b2;
        if (type == 0)      { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; }      // low-pass
        else if (type == 1) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; }   // high-pass
        else                { b0 = alpha; b1 = 0; b2 = -alpha; }                         // band-pass
        const double a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
        std::vector<float> y (x.size());
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double v = (b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
            x2 = x1; x1 = x[i]; y2 = y1; y1 = v;
            y[i] = (float) v;
        }
        return y;
    }

    static std::vector<float> lowpass  (const std::vector<float>& x, float f, float q) { return biquad (x, 0, f, q); }
    static std::vector<float> highpass (const std::vector<float>& x, float f, float q) { return biquad (x, 1, f, q); }
    static std::vector<float> bandpass (const std::vector<float>& x, float f, float q) { return biquad (x, 2, f, q); }

    /** The old sampler's grit: held at 26 kHz, 12 bits, with its filter. 'amount' 1 is the full effect. */
    static std::vector<float> lofi (const std::vector<float>& x, float amount = 1.0f)
    {
        const double hold = rate / 26040.0;
        const float levels = 2048.0f;   // 12 bits
        std::vector<float> y (x.size());
        double counter = 0.0;
        float held = 0.0f;
        for (size_t i = 0; i < x.size(); ++i)
        {
            counter += 1.0;
            if (counter >= hold) { counter -= hold; held = std::round (x[i] * levels) / levels; }
            y[i] = x[i] + amount * (held - x[i]);
        }
        auto f = lowpass (y, 11000.0f, 0.7f);
        for (size_t i = 0; i < f.size(); ++i)
            f[i] = std::tanh (1.2f * f[i]) / std::tanh (1.2f);   // a little warmth from the converters
        return f;
    }

    /** Peak level, a short fade at the end, and a tiny one at the start so it never clicks. */
    static std::vector<float> finish (std::vector<float> y, float peak, float fadeOutSeconds)
    {
        float m = 0.0f;
        for (float v : y) m = juce::jmax (m, std::abs (v));
        const float g = m > 0.0f ? peak / m : 0.0f;
        const size_t fadeIn = 24, fadeOut = (size_t) (rate * fadeOutSeconds);
        for (size_t i = 0; i < y.size(); ++i)
        {
            float e = g;
            if (i < fadeIn) e *= (float) i / (float) fadeIn;
            if (i + fadeOut > y.size()) e *= (float) (y.size() - i) / (float) juce::jmax ((size_t) 1, fadeOut);
            y[i] *= e;
        }
        return y;
    }

    static void pad (Track& t, int d, const juce::String& name, const std::vector<float>& x)
    {
        juce::AudioBuffer<float> audio (2, (int) x.size());
        audio.copyFrom (0, 0, x.data(), (int) x.size());
        audio.copyFrom (1, 0, x.data(), (int) x.size());
        t.kit.setPadSample (d, audio, rate, name);
        t.padFileNames[(size_t) d] = name + ".wav";
        t.padFileData[(size_t) d] = SampleTools::encodeWav (audio, rate);
    }
};

} // namespace bizgroove::lite
