#include "StemDeck.h"

namespace bizgroove
{

StemDeck::StemDeck() = default;

int StemDeck::barsForLength (double seconds, double bpm)
{
    const double bars = seconds * bpm / 240.0; // 4 beats per bar
    // Snap to a musical length: 1, 2, 4, 8, 16... bars, or the nearest whole bar beyond 16.
    if (bars <= 0.75) return 1;
    for (int b : { 1, 2, 4, 8, 16 })
        if (bars < b * 1.25)
            return b;
    return juce::jmax (1, (int) std::round (bars));
}

void StemDeck::setStemAudio (int index, const juce::AudioBuffer<float>& audio, double fileSampleRate,
                             const juce::String& name, double projectBpm)
{
    if (! juce::isPositiveAndBelow (index, numStems) || audio.getNumSamples() == 0)
        return;

    auto fresh = std::make_unique<Loaded>();
    fresh->audio.setSize (2, audio.getNumSamples());
    for (int ch = 0; ch < 2; ++ch)
        fresh->audio.copyFrom (ch, 0, audio, juce::jmin (ch, audio.getNumChannels() - 1), 0, audio.getNumSamples());
    fresh->sampleRate = fileSampleRate;
    fresh->name = name;
    fresh->bars = barsForLength (audio.getNumSamples() / fileSampleRate, projectBpm);

    std::unique_ptr<Loaded> old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (voices[(size_t) index].loaded);
        voices[(size_t) index].loaded = std::move (fresh);
    }
    // `old` is freed here, on the message thread, never on the audio thread.
}

void StemDeck::clearStem (int index)
{
    std::unique_ptr<Loaded> old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (voices[(size_t) index].loaded);
    }
    stems[(size_t) index].playFraction.store (0.0f);
    stems[(size_t) index].level.store (0.0f);
}

bool StemDeck::hasAudio (int index) const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return voices[(size_t) index].loaded != nullptr;
}

juce::String StemDeck::getStemName (int index) const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    const auto& l = voices[(size_t) index].loaded;
    return l != nullptr ? l->name : juce::String();
}

int StemDeck::getStemBars (int index) const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    const auto& l = voices[(size_t) index].loaded;
    return l != nullptr ? l->bars : 0;
}

void StemDeck::prepare (double deviceSampleRate, int maxBlockSize)
{
    deviceRate = deviceSampleRate;
    scratch.setSize (2, juce::jmax (512, maxBlockSize));

    const juce::dsp::ProcessSpec spec { deviceSampleRate, (juce::uint32) juce::jmax (512, maxBlockSize), 2 };
    for (auto& v : voices)
    {
        v.lowPass.prepare (spec);
        v.lowPass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        v.highPass.prepare (spec);
        v.highPass.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        v.gain.reset (deviceSampleRate, 0.02);
        v.offset = 0;
        v.pendingRestartAt = -1;
        v.activeRoll = 0;
    }
}

void StemDeck::process (juce::AudioBuffer<float>& out, int startSample, int numSamples,
                        juce::int64 transportSample, bool playing, double bpm)
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked())
        return; // a stem is being swapped this instant; skip one block

    if (! playing)
    {
        for (auto& v : voices)
        {
            v.offset = 0;
            v.pendingRestartAt = -1;
            v.activeRoll = 0;
            v.snapGain = true;
            v.lowPass.reset();
            v.highPass.reset();
        }
        for (auto& s : stems)
            s.level.store (0.0f);
        return;
    }

    bool anySolo = false;
    for (auto& s : stems)
        anySolo = anySolo || s.solo.load();

    for (int i = 0; i < numStems; ++i)
    {
        auto& s = stems[(size_t) i];
        const bool audible = ! s.mute.load() && (! anySolo || s.solo.load());
        renderVoice (voices[(size_t) i], s, out, startSample, numSamples, transportSample, bpm, audible);
    }
}

void StemDeck::renderVoice (Voice& v, Stem& s, juce::AudioBuffer<float>& out, int startSample, int numSamples,
                            juce::int64 transportSample, double bpm, bool audible)
{
    if (v.loaded == nullptr)
        return;

    const auto& audio = v.loaded->audio;
    const double fileLength = audio.getNumSamples();
    const double samplesPerBeat = 60.0 / juce::jlimit (20.0, 300.0, bpm) * deviceRate;
    const double loopLength = samplesPerBeat * 4.0 * v.loaded->bars;   // in device samples

    // Restart: wait for the next beat so the stem stays on the grid.
    if (s.restartRequested.exchange (false))
        v.pendingRestartAt = (juce::int64) (std::ceil ((double) transportSample / samplesPerBeat) * samplesPerBeat);

    const int requestedRoll = s.roll.load();

    // Filter settings for this block.
    const float filterAmount = s.filter.load();
    const bool useLowPass = filterAmount < -0.02f;
    const bool useHighPass = filterAmount > 0.02f;
    if (useLowPass)
        v.lowPass.setCutoffFrequency (20000.0f * std::pow (0.01f, -filterAmount));   // 20 kHz -> 200 Hz
    if (useHighPass)
        v.highPass.setCutoffFrequency (20.0f * std::pow (250.0f, filterAmount));     // 20 Hz -> 5 kHz
    if (useLowPass != (v.lastFilter < -0.02f)) v.lowPass.reset();
    if (useHighPass != (v.lastFilter > 0.02f)) v.highPass.reset();
    v.lastFilter = filterAmount;

    const float targetGain = audible ? s.volume.load() : 0.0f;
    if (v.snapGain)
    {
        v.gain.setCurrentAndTargetValue (targetGain);
        v.snapGain = false;
    }
    v.gain.setTargetValue (targetGain);

    float peak = 0.0f;
    double phase = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const juce::int64 t = transportSample + i;

        if (v.pendingRestartAt >= 0 && t >= v.pendingRestartAt)
        {
            v.offset = v.pendingRestartAt;
            v.pendingRestartAt = -1;
        }

        // Position on the stem's own timeline, in device samples.
        double rel = (double) (t - v.offset);

        if (requestedRoll != v.activeRoll)
        {
            v.activeRoll = requestedRoll;
            if (requestedRoll > 0)
            {
                const double rollLength = samplesPerBeat / requestedRoll;
                // Loop the grid slice we are in, without jumping: the roll
                // stays phase-locked to the beat.
                v.rollStartPos = std::floor (rel / rollLength) * rollLength;
                v.rollStartedAt = t - (juce::int64) std::llround (rel - v.rollStartPos);
            }
        }

        if (v.activeRoll > 0)
        {
            const double rollLength = samplesPerBeat / v.activeRoll;
            rel = v.rollStartPos + std::fmod ((double) (t - v.rollStartedAt), rollLength);
        }

        phase = std::fmod (rel, loopLength);
        if (phase < 0.0)
            phase += loopLength;

        // Repitch: map the loop onto the whole file, with linear interpolation.
        const double filePos = phase / loopLength * fileLength;
        const int i0 = (int) filePos;
        const int i1 = (i0 + 1) % (int) fileLength;
        const float frac = (float) (filePos - i0);
        const float g = v.gain.getNextValue();

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* src = audio.getReadPointer (ch);
            float x = src[i0] + frac * (src[i1] - src[i0]);
            if (useLowPass)  x = v.lowPass.processSample (ch, x);
            if (useHighPass) x = v.highPass.processSample (ch, x);
            x *= g;
            peak = juce::jmax (peak, std::abs (x));
            if (ch < out.getNumChannels())
                out.addSample (ch, startSample + i, x);
        }
    }

    s.playFraction.store ((float) (phase / loopLength));
    s.level.store (peak);
}

} // namespace bizgroove
