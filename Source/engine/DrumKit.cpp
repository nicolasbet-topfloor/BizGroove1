#include "DrumKit.h"

namespace bizgroove
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // Coefficient for an exponential fade to -60 dB over `seconds`.
    float fadeCoef (float seconds, double sr)
    {
        return std::exp (std::log (0.001f) / (float) (juce::jmax (0.001f, seconds) * sr));
    }

    // Inharmonic square-wave ratios for metallic hi-hats and cymbals (808-style).
    constexpr double metalHz[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
}

void DrumKit::Filter::set (float cutoff, float q, double sr)
{
    g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, (float) sr * 0.45f, cutoff) / (float) sr);
    k = 1.0f / juce::jmax (0.1f, q);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void DrumKit::Filter::process (float x, float& low, float& band, float& high)
{
    const float v3 = x - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    low = v2;
    band = v1;
    high = x - k * v1 - v2;
}

DrumKit::DrumKit()
{
    // General MIDI drum notes 36-51, in order.
    specs = { {
        { Model::kick,   52.0f,  0.45f },   // 36 Kick
        { Model::rim,    1700.0f, 0.06f },  // 37 Side stick
        { Model::snare,  190.0f, 0.22f },   // 38 Snare
        { Model::clap,   1100.0f, 0.30f },  // 39 Clap
        { Model::snare,  235.0f, 0.18f },   // 40 Snare 2
        { Model::tom,    82.0f,  0.45f },   // 41 Low floor tom
        { Model::hat,    7000.0f, 0.05f },  // 42 Closed hi-hat
        { Model::tom,    98.0f,  0.42f },   // 43 High floor tom
        { Model::hat,    7000.0f, 0.10f },  // 44 Pedal hi-hat
        { Model::tom,    110.0f, 0.38f },   // 45 Low tom
        { Model::hat,    7000.0f, 0.55f },  // 46 Open hi-hat
        { Model::tom,    131.0f, 0.35f },   // 47 Low-mid tom
        { Model::tom,    147.0f, 0.32f },   // 48 Hi-mid tom
        { Model::cymbal, 6000.0f, 1.60f },  // 49 Crash
        { Model::tom,    175.0f, 0.30f },   // 50 High tom
        { Model::cymbal, 3200.0f, 1.20f, true }  // 51 Ride
    } };

    // Gentle default stereo spread, as on most kits.
    drums[6].pan.store (0.25f);   drums[8].pan.store (0.25f);   drums[10].pan.store (0.25f);
    drums[5].pan.store (-0.3f);   drums[7].pan.store (-0.15f);  drums[9].pan.store (0.0f);
    drums[11].pan.store (0.15f);  drums[12].pan.store (0.25f);  drums[14].pan.store (0.35f);
    drums[13].pan.store (-0.3f);  drums[15].pan.store (0.3f);
}

juce::String DrumKit::drumName (int drum)
{
    static const char* names[numDrums] = { "Kick", "Side stick", "Snare", "Clap", "Snare 2", "Low floor tom",
                                           "Closed hi-hat", "High floor tom", "Pedal hi-hat", "Low tom",
                                           "Open hi-hat", "Low-mid tom", "Hi-mid tom", "Crash", "High tom", "Ride" };
    return juce::isPositiveAndBelow (drum, numDrums) ? juce::String (names[drum]) : juce::String();
}

int DrumKit::drumForNote (int note)
{
    const int index = ((note - 36) % 24 + 24) % 24;
    return index < numDrums ? index : -1;
}

void DrumKit::prepare (double sr)
{
    sampleRate = sr;
    for (auto& v : voices)
    {
        v = Voice();
    }
}

float DrumKit::noise()
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (float) (rng & 0xffffff) / (float) 0x7fffff - 1.0f;
}

void DrumKit::trigger (int drum, float velocity01)
{
    if (! juce::isPositiveAndBelow (drum, numDrums))
        return;

    const auto& p = drums[(size_t) drum];
    const auto& spec = specs[(size_t) drum];
    auto& v = voices[(size_t) drum];

    const float lengthMult = std::pow (4.0f, (p.length.load() - 0.5f) * 2.0f);   // 0.25x .. 4x
    const float sens = juce::jlimit (0.0f, 1.0f, velocity.load());

    v.active = true;
    v.choking = false;
    v.sampleCount = 0;
    v.amp = 1.0f;
    v.ampCoef = fadeCoef (spec.baseDecay * lengthMult, sampleRate);
    v.pitchEnv = 1.0f;
    v.pitchCoef = fadeCoef (spec.model == Model::kick ? 0.05f : 0.08f, sampleRate);
    v.click = 1.0f;
    v.clickCoef = fadeCoef (0.004f, sampleRate);
    v.body = 1.0f;
    v.bodyCoef = fadeCoef (spec.baseDecay * lengthMult * 0.5f, sampleRate);
    v.velocityGain = (1.0f - sens) + sens * juce::jlimit (0.0f, 1.0f, velocity01);
    v.phase = 0.0;
    v.metalPhase = {};
    v.f1.reset();
    v.f2.reset();

    // Choke: closed (42) and pedal (44) hi-hats cut the open hi-hat (46).
    if (choke.load() && (drum == 6 || drum == 8) && voices[10].active)
    {
        voices[10].choking = true;
        voices[10].ampCoef = fadeCoef (0.012f, sampleRate);
    }
}

bool DrumKit::isSounding() const
{
    for (const auto& v : voices)
        if (v.active)
            return true;
    return false;
}

void DrumKit::render (juce::AudioBuffer<float>& out, int startSample, int numSamples, const juce::MidiBuffer& midi)
{
    float* left = out.getWritePointer (0, startSample);
    float* right = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : left;

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        const auto m = meta.getMessage();
        if (! m.isNoteOn())
            continue;

        for (int d = 0; d < numDrums; ++d)
            if (voices[(size_t) d].active && at > pos)
                renderVoice (d, voices[(size_t) d], left + pos, right + pos, at - pos);
        pos = juce::jmax (pos, at);
        trigger (drumForNote (m.getNoteNumber()), m.getVelocity() / 127.0f);
    }

    for (int d = 0; d < numDrums; ++d)
        if (voices[(size_t) d].active && numSamples > pos)
            renderVoice (d, voices[(size_t) d], left + pos, right + pos, numSamples - pos);
}

void DrumKit::renderVoice (int drum, Voice& v, float* left, float* right, int numSamples)
{
    const auto& p = drums[(size_t) drum];
    const auto& spec = specs[(size_t) drum];
    const float sr = (float) sampleRate;

    const float ratio = std::pow (2.0f, juce::jlimit (-12.0f, 12.0f, p.pitch.load()) / 12.0f);
    const float tone = juce::jlimit (0.0f, 1.0f, p.tone.load());
    const float snap = juce::jlimit (0.0f, 1.0f, p.snap.load());
    const float drive = juce::jlimit (0.0f, 1.0f, p.drive.load());
    const float driveK = 1.0f + 9.0f * drive;
    const float driveNorm = 1.0f / std::tanh (driveK);
    const float pan = juce::jlimit (-1.0f, 1.0f, p.pan.load());
    const float panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
    const float panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
    const float level = juce::jlimit (0.0f, 1.0f, p.volume.load()) * v.velocityGain * 0.7f;

    // Filter settings per block.
    switch (spec.model)
    {
        case Model::snare:  v.f1.set (1000.0f + tone * 6000.0f, 0.7f, sampleRate); break;
        case Model::clap:   v.f1.set (spec.baseHz * ratio * (0.6f + tone), 1.8f, sampleRate); break;
        case Model::rim:    v.f1.set (spec.baseHz * ratio * 1.5f, 1.0f, sampleRate); break;
        case Model::hat:    v.f1.set (spec.baseHz * (0.6f + tone * 0.8f), 0.8f, sampleRate);
                            v.f2.set (11000.0f, 0.7f, sampleRate); break;
        case Model::cymbal: v.f1.set (spec.baseHz * (0.6f + tone * 0.8f), spec.ride ? 2.5f : 0.9f, sampleRate); break;
        case Model::kick:
        case Model::tom:    v.f1.set (2000.0f + tone * 8000.0f, 0.7f, sampleRate); break;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float x = 0.0f;
        const float n = noise();
        float lo, bp, hp;

        switch (spec.model)
        {
            case Model::kick:
            {
                // Sine with a fast pitch drop; Tone sets how far the pitch falls.
                const float hz = spec.baseHz * ratio * (1.0f + (1.5f + 3.0f * tone) * v.pitchEnv);
                v.phase += twoPi * hz / sr;
                x = std::sin ((float) v.phase);
                break;
            }
            case Model::tom:
            {
                const float hz = spec.baseHz * ratio * (1.0f + 0.4f * v.pitchEnv);
                v.phase += twoPi * hz / sr;
                v.f1.process (n, lo, bp, hp);
                x = std::sin ((float) v.phase) + 0.15f * tone * lo * v.body;
                break;
            }
            case Model::snare:
            {
                const float hz = spec.baseHz * ratio * (1.0f + 0.5f * v.pitchEnv);
                v.phase += twoPi * hz / sr;
                v.f1.process (n, lo, bp, hp);
                x = 0.7f * std::sin ((float) v.phase) * v.body + (0.5f + 0.5f * tone) * hp;
                break;
            }
            case Model::rim:
            {
                v.phase += twoPi * spec.baseHz * ratio / sr;
                v.f1.process (n, lo, bp, hp);
                x = 0.8f * std::sin ((float) v.phase) + 0.6f * bp;
                break;
            }
            case Model::clap:
            {
                // Three quick bursts, then the tail.
                const float t = (float) v.sampleCount / sr;
                float bursts = 0.0f;
                for (int b = 0; b < 3; ++b)
                {
                    const float tb = t - 0.011f * (float) b;
                    if (tb >= 0.0f)
                        bursts += std::exp (-tb / 0.004f);
                }
                v.f1.process (n, lo, bp, hp);
                x = 2.0f * bp * (0.6f * bursts + (t > 0.022f ? 1.0f : 0.0f));
                break;
            }
            case Model::hat:
            case Model::cymbal:
            {
                float metal = 0.0f;
                for (size_t k = 0; k < 6; ++k)
                {
                    v.metalPhase[k] += metalHz[k] * ratio * (spec.model == Model::cymbal ? 1.6 : 1.0) / sampleRate;
                    if (v.metalPhase[k] >= 1.0) v.metalPhase[k] -= 1.0;
                    metal += v.metalPhase[k] < 0.5 ? 1.0f : -1.0f;
                }
                const float src = metal * (spec.ride ? 0.22f : 0.16f) + n * (spec.ride ? 0.15f : 0.5f);
                v.f1.process (src, lo, bp, hp);
                x = spec.ride ? (bp + 0.5f * hp) : hp;
                if (spec.model == Model::hat)
                {
                    float lo2, bp2, hp2;
                    v.f2.process (x, lo2, bp2, hp2);
                    x = lo2 * 1.6f;
                }
                break;
            }
        }

        // Snap: a short click of noise at the start of the hit.
        x += snap * v.click * n * 0.9f;

        x *= v.amp;
        if (drive > 0.01f)
            x = std::tanh (driveK * x) * driveNorm;
        x *= level;

        left[i] += x * panL;
        right[i] += x * panR;

        v.amp *= v.ampCoef;
        v.pitchEnv *= v.pitchCoef;
        v.click *= v.clickCoef;
        v.body *= v.bodyCoef;
        ++v.sampleCount;

        if (v.amp < 1.0e-4f)
        {
            v.active = false;
            break;
        }
    }

    if (v.phase > 1.0e6) v.phase = std::fmod (v.phase, (double) twoPi);
}

} // namespace bizgroove
