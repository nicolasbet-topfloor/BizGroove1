#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace bizgroove
{

/** The effects a track can put in its two insert slots, after its sound and
    before its volume, pan and sends. */
struct InsertEffects
{
    enum Type { off = 0, chorus, flanger, phaser, ensemble, drive, lofi, autoFilter, compressor, duck, numTypes };

    static juce::String name (int type)
    {
        switch (type)
        {
            case chorus:     return "Chorus";
            case flanger:    return "Flanger";
            case phaser:     return "Phaser";
            case ensemble:   return "Ensemble";
            case drive:      return "Drive";
            case lofi:       return "Lo-fi";
            case autoFilter: return "Moving filter";
            case compressor: return "Compressor";
            case duck:       return "Duck (sidechain)";
            default:         return "Off";
        }
    }

    /** What the four knobs of an effect do. */
    static std::array<const char*, 4> knobNames (int type)
    {
        switch (type)
        {
            case chorus:     return { { "Speed", "Depth", "Width", "Mix" } };
            case flanger:    return { { "Speed", "Depth", "Resonance", "Mix" } };
            case phaser:     return { { "Speed", "Depth", "Resonance", "Mix" } };
            case ensemble:   return { { "Speed", "Depth", "Tone", "Mix" } };
            case drive:      return { { "Drive", "Tone", "Character", "Mix" } };
            case lofi:       return { { "Bits", "Rate", "Tone", "Mix" } };
            case autoFilter: return { { "Brightness", "Edge", "Movement", "Speed" } };
            case compressor: return { { "Threshold", "Ratio", "Speed", "Mix" } };
            case duck:       return { { "Amount", "Release", "Trigger", "Smooth" } };
            default:         return { { "-", "-", "-", "-" } };
        }
    }

    /** Where the knobs start when the effect is chosen. */
    static std::array<float, 4> defaults (int type)
    {
        switch (type)
        {
            case chorus:     return { { 0.35f, 0.5f, 0.7f, 0.5f } };
            case flanger:    return { { 0.25f, 0.6f, 0.5f, 0.5f } };
            case phaser:     return { { 0.3f, 0.7f, 0.4f, 0.5f } };
            case ensemble:   return { { 0.4f, 0.6f, 0.6f, 0.6f } };
            case drive:      return { { 0.4f, 0.7f, 0.2f, 1.0f } };
            case lofi:       return { { 0.4f, 0.3f, 0.7f, 1.0f } };
            case autoFilter: return { { 0.45f, 0.4f, 0.5f, 0.5f } };
            case compressor: return { { 0.5f, 0.3f, 0.5f, 1.0f } };
            case duck:       return { { 0.7f, 0.35f, 0.0f, 0.2f } };
            default:         return { { 0.5f, 0.5f, 0.5f, 0.5f } };
        }
    }

    /** The Speed knob of the moving filter, in note lengths that follow the tempo. */
    static constexpr int numSyncedRates = 7;
    static double syncedBeats (float v)
    {
        static constexpr double beats[numSyncedRates] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
        return beats[juce::jlimit (0, numSyncedRates - 1, (int) (v * (numSyncedRates - 0.001f)))];
    }
    static juce::String syncedName (float v)
    {
        static const char* names[numSyncedRates] = { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };
        return names[juce::jlimit (0, numSyncedRates - 1, (int) (v * (numSyncedRates - 0.001f)))];
    }

    /** How a knob's value reads, for each effect. */
    static juce::String describe (int type, int knob, float v)
    {
        auto pct = [] (float x) { return juce::String (juce::roundToInt (x * 100.0f)) + "%"; };
        auto hz = [] (float x) { return juce::String (x, x < 1.0f ? 2 : 1) + " Hz"; };
        switch (type)
        {
            case chorus: case flanger: case phaser: case ensemble:
                if (knob == 0) return hz (rateHz (v));
                break;
            case drive:
                if (knob == 0) return "x " + juce::String (driveGain (v), 1);
                break;
            case lofi:
                if (knob == 0) return juce::String (bits (v), 1) + " bits";
                if (knob == 1) return v < 0.01f ? juce::String ("Full rate") : "1/" + juce::String (holdFactor (v));
                break;
            case autoFilter:
                if (knob == 3) return syncedName (v);
                break;
            case compressor:
                if (knob == 0) return juce::String (threshold (v), 1) + " dB";
                if (knob == 1) return juce::String (ratio (v), 1) + ":1";
                break;
            case duck:
                if (knob == 1) return juce::String (juce::roundToInt (1000.0f * duckRelease (v))) + " ms";
                if (knob == 2) return v < 0.5f ? juce::String ("Kick drum") : juce::String ("Every beat");
                break;
            default:
                return "-";
        }
        return pct (v);
    }

    // Value maps shared by the knobs and the sound.
    static float rateHz (float v)      { return 0.05f * std::pow (160.0f, juce::jlimit (0.0f, 1.0f, v)); }   // 0.05 .. 8 Hz
    static float driveGain (float v)   { return std::pow (40.0f, juce::jlimit (0.0f, 1.0f, v)); }             // 1 .. 40
    static float bits (float v)        { return 16.0f - 14.0f * juce::jlimit (0.0f, 1.0f, v); }               // 16 .. 2
    static int   holdFactor (float v)  { return 1 + (int) (39.0f * std::pow (juce::jlimit (0.0f, 1.0f, v), 1.5f)); }
    static float threshold (float v)   { return -40.0f * (1.0f - juce::jlimit (0.0f, 1.0f, v)); }             // -40 .. 0 dB
    static float ratio (float v)       { return 1.0f + 19.0f * juce::jlimit (0.0f, 1.0f, v) * juce::jlimit (0.0f, 1.0f, v); }
    static float duckRelease (float v) { return 0.04f * std::pow (25.0f, juce::jlimit (0.0f, 1.0f, v)); }      // 40 ms .. 1 s
};

/** One insert slot's settings, shared with the UI and controllers. */
struct InsertSettings
{
    std::atomic<int> type { InsertEffects::off };
    std::array<std::atomic<float>, 4> knob { { 0.5f, 0.5f, 0.5f, 0.5f } };

    /** Message thread: choose an effect, with its knobs at their starting places. */
    void choose (int newType)
    {
        newType = juce::jlimit (0, InsertEffects::numTypes - 1, newType);
        if (newType == type.load())
            return;
        const auto d = InsertEffects::defaults (newType);
        for (size_t k = 0; k < 4; ++k)
            knob[k].store (d[k]);
        type.store (newType);
    }
};

/** What the effects need to know about the music in this block. */
struct EffectContext
{
    double bpm = 120.0;
    double beatAtStart = 0.0;            // song position in beats at the block's first sample
    std::array<int, 32> kicks {};        // sample positions of kick drum hits in this block
    int numKicks = 0;
};

/** The sound of one insert slot. Audio thread; keeps the state of every
    effect so switching is instant. */
class InsertProcessor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int size = juce::nextPowerOfTwo ((int) (sampleRate * 0.06) + 4);   // 60 ms
        for (auto& line : lines)
            line.assign ((size_t) size, 0.0f);
        mask = size - 1;
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);
        write = 0;
        lfoPhase = slowPhase = fastPhase = 0.0;
        feedbackL = feedbackR = 0.0f;
        for (auto& stage : allpass) stage = {};
        toneL = toneR = 0.0f;
        holdCount = 0; heldL = heldR = 0.0f;
        svf = {};
        gainDb = 0.0f;
        duckEnv = 0.0f; duckTarget = 0.0f;
        lastType = -1;
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples, const InsertSettings& s, const EffectContext& ctx)
    {
        const int type = juce::jlimit (0, InsertEffects::numTypes - 1, s.type.load());
        if (type == InsertEffects::off)
        {
            lastType = type;
            return;
        }
        if (type != lastType)
        {
            reset();   // a new effect starts clean, with no old echoes
            lastType = type;
        }

        std::array<float, 4> k {};
        for (size_t i = 0; i < 4; ++i)
            k[i] = juce::jlimit (0.0f, 1.0f, s.knob[i].load());

        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);

        switch (type)
        {
            case InsertEffects::chorus:     modulatedDelay (l, r, numSamples, k, 0.012, 0.007, false); break;
            case InsertEffects::flanger:    modulatedDelay (l, r, numSamples, k, 0.0005, 0.005, true); break;
            case InsertEffects::phaser:     phaserProcess (l, r, numSamples, k); break;
            case InsertEffects::ensemble:   ensembleProcess (l, r, numSamples, k); break;
            case InsertEffects::drive:      driveProcess (l, r, numSamples, k); break;
            case InsertEffects::lofi:       lofiProcess (l, r, numSamples, k); break;
            case InsertEffects::autoFilter: filterProcess (l, r, numSamples, k, ctx); break;
            case InsertEffects::compressor: compressorProcess (l, r, numSamples, k); break;
            case InsertEffects::duck:       duckProcess (l, r, numSamples, k, ctx); break;
            default: break;
        }
    }

private:
    static constexpr double twoPi = juce::MathConstants<double>::twoPi;

    float readLine (const std::vector<float>& line, double delaySamples) const
    {
        double pos = (double) write - delaySamples;
        const double size = (double) (mask + 1);
        while (pos < 0.0) pos += size;
        const int i0 = (int) pos & mask;
        const int i1 = (i0 + 1) & mask;
        const float frac = (float) (pos - std::floor (pos));
        return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
    }

    void advance (float inL, float inR)
    {
        lines[0][(size_t) write] = inL;
        lines[1][(size_t) write] = inR;
        write = (write + 1) & mask;
    }

    /** Chorus and flanger: a short delay whose time sways, mixed with the dry sound. */
    void modulatedDelay (float* l, float* r, int n, const std::array<float, 4>& k, double baseSeconds, double depthSeconds, bool flange)
    {
        const double step = twoPi * InsertEffects::rateHz (k[0]) / sr;
        const double depth = depthSeconds * k[1] * sr;
        const double base = baseSeconds * sr + 2.0;
        const double spread = flange ? 0.25 * juce::MathConstants<double>::pi : k[2] * juce::MathConstants<double>::pi;
        const float fb = flange ? 0.9f * k[2] : 0.0f;
        const float mix = k[3];
        for (int i = 0; i < n; ++i)
        {
            lfoPhase += step;
            if (lfoPhase > twoPi) lfoPhase -= twoPi;
            const double dL = base + depth * 0.5 * (1.0 + std::sin (lfoPhase));
            const double dR = base + depth * 0.5 * (1.0 + std::sin (lfoPhase + spread));
            const float wetL = readLine (lines[0], dL);
            const float wetR = readLine (lines[1], dR);
            advance (l[i] + std::tanh (fb * wetL), r[i] + std::tanh (fb * wetR));   // the feedback saturates instead of running away
            l[i] = l[i] * (1.0f - 0.5f * mix) + wetL * mix * (flange ? 0.7f : 1.0f);
            r[i] = r[i] * (1.0f - 0.5f * mix) + wetR * mix * (flange ? 0.7f : 1.0f);
        }
    }

    /** Ensemble: three swaying copies per side, a slow and a fast sway, as in string machines. */
    void ensembleProcess (float* l, float* r, int n, const std::array<float, 4>& k)
    {
        const double speed = 0.3 + 1.7 * k[0];
        const double slowStep = twoPi * 0.6 * speed / sr, fastStep = twoPi * 6.0 * speed / sr;
        const double slowDepth = 0.002 * k[1] * sr, fastDepth = 0.0003 * k[1] * sr;
        const double base = 0.007 * sr;
        const float toneCoef = std::exp (-2.0f * juce::MathConstants<float>::pi * (1500.0f + 10000.0f * k[2]) / (float) sr);
        const float mix = k[3];
        const double third = twoPi / 3.0;
        for (int i = 0; i < n; ++i)
        {
            slowPhase += slowStep; if (slowPhase > twoPi) slowPhase -= twoPi;
            fastPhase += fastStep; if (fastPhase > twoPi) fastPhase -= twoPi;
            float wetL = 0.0f, wetR = 0.0f;
            for (int v = 0; v < 3; ++v)
            {
                const double ph = third * v;
                const double dL = base + slowDepth * (1.0 + std::sin (slowPhase + ph)) + fastDepth * (1.0 + std::sin (fastPhase + ph));
                const double dR = base + slowDepth * (1.0 + std::sin (slowPhase + ph + 0.5 * third)) + fastDepth * (1.0 + std::sin (fastPhase + ph + 0.5 * third));
                wetL += readLine (lines[0], dL);
                wetR += readLine (lines[1], dR);
            }
            advance (l[i], r[i]);
            toneL = wetL / 3.0f + toneCoef * (toneL - wetL / 3.0f);
            toneR = wetR / 3.0f + toneCoef * (toneR - wetR / 3.0f);
            l[i] = l[i] * (1.0f - 0.6f * mix) + toneL * mix * 1.2f;
            r[i] = r[i] * (1.0f - 0.6f * mix) + toneR * mix * 1.2f;
        }
    }

    /** Phaser: six all-pass stages swept by a slow sway, with feedback. */
    void phaserProcess (float* l, float* r, int n, const std::array<float, 4>& k)
    {
        const double step = twoPi * InsertEffects::rateHz (k[0]) / sr;
        const float fb = 0.75f * k[2];
        const float mix = k[3];
        float coef = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            lfoPhase += step;
            if (lfoPhase > twoPi) lfoPhase -= twoPi;
            if ((i & 7) == 0)
            {
                const float sweep = 0.5f + 0.5f * (float) std::sin (lfoPhase);
                const float hz = 200.0f * std::pow (2.0f, 4.5f * k[1] * sweep);
                const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (hz, (float) sr * 0.45f) / (float) sr);
                coef = (t - 1.0f) / (t + 1.0f);
            }
            float xl = l[i] + std::tanh (fb * feedbackL), xr = r[i] + std::tanh (fb * feedbackR);
            for (size_t st = 0; st < 6; ++st)
            {
                auto& a = allpass[st];
                const float yl = coef * xl + a.zl;  a.zl = xl - coef * yl;  xl = yl;
                const float yr = coef * xr + a.zr;  a.zr = xr - coef * yr;  xr = yr;
            }
            feedbackL = xl;
            feedbackR = xr;
            l[i] = l[i] * (1.0f - 0.5f * mix) + xl * mix * 0.7f;
            r[i] = r[i] * (1.0f - 0.5f * mix) + xr * mix * 0.7f;
        }
    }

    /** Drive: from warm soft saturation to hard clipping, then a tone control. */
    void driveProcess (float* l, float* r, int n, const std::array<float, 4>& k)
    {
        const float g = InsertEffects::driveGain (k[0]);
        const float makeup = 0.7f + 0.3f / std::sqrt (g);
        const float hard = k[2];
        const float toneCoef = std::exp (-2.0f * juce::MathConstants<float>::pi * (600.0f * std::pow (30.0f, k[1])) / (float) sr);
        const float mix = k[3];
        auto shape = [hard] (float x) {
            const float soft = std::tanh (x + 0.15f) - std::tanh (0.15f);   // a little lopsided, for warmth
            const float clipped = juce::jlimit (-1.0f, 1.0f, x);
            return soft + hard * (clipped - soft);
        };
        for (int i = 0; i < n; ++i)
        {
            const float yl = shape (l[i] * g) * makeup, yr = shape (r[i] * g) * makeup;
            toneL = yl + toneCoef * (toneL - yl);
            toneR = yr + toneCoef * (toneR - yr);
            l[i] += mix * (toneL - l[i]);
            r[i] += mix * (toneR - r[i]);
        }
    }

    /** Lo-fi: fewer bits and a lower sample rate, like an old sampler. */
    void lofiProcess (float* l, float* r, int n, const std::array<float, 4>& k)
    {
        const float levels = std::pow (2.0f, InsertEffects::bits (k[0]) - 1.0f);
        const int factor = InsertEffects::holdFactor (k[1]);
        const float toneCoef = std::exp (-2.0f * juce::MathConstants<float>::pi * (800.0f * std::pow (25.0f, k[2])) / (float) sr);
        const float mix = k[3];
        for (int i = 0; i < n; ++i)
        {
            if (holdCount <= 0)
            {
                heldL = std::round (l[i] * levels) / levels;
                heldR = std::round (r[i] * levels) / levels;
                holdCount = factor;
            }
            --holdCount;
            toneL = heldL + toneCoef * (toneL - heldL);
            toneR = heldR + toneCoef * (toneR - heldR);
            l[i] += mix * (toneL - l[i]);
            r[i] += mix * (toneR - r[i]);
        }
    }

    /** Moving filter: a resonant low-pass that opens and closes in time with the music. */
    void filterProcess (float* l, float* r, int n, const std::array<float, 4>& k, const EffectContext& ctx)
    {
        const float base = 60.0f * std::pow (250.0f, k[0]);
        const float damping = 2.0f - 1.75f * k[1];
        const float octaves = 4.0f * k[2];
        const double beats = InsertEffects::syncedBeats (k[3]);
        const double beatsPerSample = juce::jlimit (20.0, 300.0, ctx.bpm) / 60.0 / sr;
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0)
            {
                // The sway follows the song position, so it lines up with the bar.
                const double beat = ctx.beatAtStart + (double) i * beatsPerSample;
                const float lfo = (float) std::sin (twoPi * beat / beats);
                const float hz = juce::jlimit (20.0f, (float) sr * 0.45f, base * std::pow (2.0f, octaves * lfo));
                const float g = std::tan (juce::MathConstants<float>::pi * hz / (float) sr);
                a1 = 1.0f / (1.0f + g * (g + damping));
                a2 = g * a1;
                a3 = g * a2;
            }
            l[i] = svfLow (svf.l1, svf.l2, l[i], a1, a2, a3);
            r[i] = svfLow (svf.r1, svf.r2, r[i], a1, a2, a3);
        }
    }

    static float svfLow (float& ic1, float& ic2, float x, float a1, float a2, float a3)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    /** Compressor: evens out the level; the gain lost above the threshold is made up. */
    void compressorProcess (float* l, float* r, int n, const std::array<float, 4>& k)
    {
        const float thr = InsertEffects::threshold (k[0]);
        const float rat = InsertEffects::ratio (k[1]);
        const float attack = 0.0002f * std::pow (150.0f, 1.0f - k[2]);   // fast .. 30 ms
        const float release = 0.03f * std::pow (25.0f, 1.0f - k[2]);    // 30 .. 750 ms
        const float attackCoef = std::exp (-1.0f / (attack * (float) sr));
        const float releaseCoef = std::exp (-1.0f / (release * (float) sr));
        const float makeupDb = -thr * (1.0f - 1.0f / rat) * 0.5f;
        const float mix = k[3];
        for (int i = 0; i < n; ++i)
        {
            const float peak = juce::jmax (std::abs (l[i]), std::abs (r[i]));
            const float inDb = peak > 1.0e-6f ? 20.0f * std::log10 (peak) : -120.0f;
            const float over = inDb - thr;
            const float wantDb = over > 0.0f ? -over * (1.0f - 1.0f / rat) : 0.0f;
            const float coef = wantDb < gainDb ? attackCoef : releaseCoef;
            gainDb = wantDb + coef * (gainDb - wantDb);
            const float g = std::pow (10.0f, (gainDb + makeupDb) / 20.0f);
            l[i] += mix * (l[i] * g - l[i]);
            r[i] += mix * (r[i] * g - r[i]);
        }
    }

    /** Duck: dips the sound on every kick (or every beat) and lets it swell back: the pumping of dance music. */
    void duckProcess (float* l, float* r, int n, const std::array<float, 4>& k, const EffectContext& ctx)
    {
        const float amount = k[0];
        const float releaseCoef = std::exp (-1.0f / (InsertEffects::duckRelease (k[1]) * (float) sr));
        const bool everyBeat = k[2] >= 0.5f;
        const float smooth = 0.0005f + 0.02f * k[3];
        const float attackCoef = std::exp (-1.0f / (smooth * (float) sr));
        const double beatsPerSample = juce::jlimit (20.0, 300.0, ctx.bpm) / 60.0 / sr;
        int nextKick = 0;
        for (int i = 0; i < n; ++i)
        {
            bool hit = false;
            if (everyBeat)
            {
                const double before = ctx.beatAtStart + (double) (i - 1) * beatsPerSample;
                const double now = ctx.beatAtStart + (double) i * beatsPerSample;
                hit = std::floor (now) != std::floor (before);
            }
            else
            {
                while (nextKick < ctx.numKicks && ctx.kicks[(size_t) nextKick] <= i)
                {
                    hit = hit || ctx.kicks[(size_t) nextKick] == i;
                    ++nextKick;
                }
            }
            if (hit)
                duckTarget = 1.0f;
            duckEnv = duckTarget + attackCoef * (duckEnv - duckTarget);   // dips quickly, without a click
            duckTarget *= releaseCoef;
            const float g = 1.0f - amount * juce::jlimit (0.0f, 1.0f, duckEnv);
            l[i] *= g;
            r[i] *= g;
        }
    }

    double sr = 44100.0;
    std::array<std::vector<float>, 2> lines;
    int write = 0, mask = 0;
    double lfoPhase = 0.0, slowPhase = 0.0, fastPhase = 0.0;
    float feedbackL = 0.0f, feedbackR = 0.0f;
    struct AllpassState { float zl = 0.0f, zr = 0.0f; };
    std::array<AllpassState, 6> allpass {};
    float toneL = 0.0f, toneR = 0.0f;
    int holdCount = 0;
    float heldL = 0.0f, heldR = 0.0f;
    struct SvfState { float l1 = 0.0f, l2 = 0.0f, r1 = 0.0f, r2 = 0.0f; } svf;
    float gainDb = 0.0f;
    float duckEnv = 0.0f, duckTarget = 0.0f;
    int lastType = -1;
};

/** The insert settings as JSON, for projects. */
inline juce::var insertsToVar (const std::array<InsertSettings, 2>& slots)
{
    juce::Array<juce::var> list;
    for (const auto& s : slots)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", s.type.load());
        juce::Array<juce::var> knobs;
        for (const auto& k : s.knob)
            knobs.add (k.load());
        o->setProperty ("knobs", knobs);
        list.add (juce::var (o));
    }
    return list;
}

inline void applyInserts (std::array<InsertSettings, 2>& slots, const juce::var& v)
{
    const auto* list = v.getArray();
    for (int i = 0; i < 2; ++i)
    {
        auto& s = slots[(size_t) i];
        const juce::var o = list != nullptr && i < list->size() ? (*list)[i] : juce::var();
        const int type = juce::jlimit (0, InsertEffects::numTypes - 1, o.hasProperty ("type") ? (int) o["type"] : 0);
        const auto defaults = InsertEffects::defaults (type);
        const auto* knobs = o["knobs"].getArray();
        for (int k = 0; k < 4; ++k)
            s.knob[(size_t) k].store (knobs != nullptr && k < knobs->size() ? (float) (double) (*knobs)[k] : defaults[(size_t) k]);
        s.type.store (type);
    }
}

} // namespace bizgroove
