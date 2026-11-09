#include "Sampler.h"

namespace bizgroove
{

void Sampler::setSample (const juce::AudioBuffer<float>& audio, double fileSampleRate, const juce::String& name)
{
    if (audio.getNumSamples() < 2)
        return;

    auto fresh = std::make_unique<Loaded>();
    fresh->audio.setSize (2, audio.getNumSamples());
    for (int ch = 0; ch < 2; ++ch)
        fresh->audio.copyFrom (ch, 0, audio, juce::jmin (ch, audio.getNumChannels() - 1), 0, audio.getNumSamples());
    fresh->sampleRate = fileSampleRate;
    fresh->name = name;

    std::unique_ptr<Loaded> old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (loaded);
        loaded = std::move (fresh);
        for (auto& v : voices)
            v.active = false;
    }
}

void Sampler::clearSample()
{
    std::unique_ptr<Loaded> old;
    const juce::SpinLock::ScopedLockType sl (lock);
    old = std::move (loaded);
    for (auto& v : voices)
        v.active = false;
}

bool Sampler::hasSample() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return loaded != nullptr;
}

juce::String Sampler::getSampleName() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return loaded != nullptr ? loaded->name : juce::String();
}

double Sampler::getSampleSeconds() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return loaded != nullptr ? loaded->audio.getNumSamples() / loaded->sampleRate : 0.0;
}

void Sampler::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (auto& v : voices)
    {
        v.active = false;
        v.env.setSampleRate (sampleRate);
        v.filter.prepare ({ sampleRate, (juce::uint32) juce::jmax (512, maxBlock), 2 });
        v.filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        v.filter.setResonance (0.7f);
    }
}

void Sampler::startVoice (int note, float velocity, bool fromPad)
{
    if (loaded == nullptr)
        return;

    // A free voice, else the oldest one.
    Voice* v = nullptr;
    for (auto& candidate : voices)
        if (! candidate.active) { v = &candidate; break; }
    if (v == nullptr)
        v = &voices[0];

    const double last = loaded->audio.getNumSamples() - 1;
    const double s0 = juce::jlimit (0.0, 1.0, (double) params.start.load()) * last;
    const double s1 = s0 + juce::jlimit (0.01, 1.0, (double) params.length.load()) * (last - s0);
    const int marked = juce::jlimit (0, maxSlices, params.sliceCount.load());
    const int slices = juce::jlimit (1, 16, params.slices.load());

    // Which slice a key or pad plays: keys count up from C4, pads from the first pad.
    auto sliceFor = [note, fromPad] (int count) {
        const int from = fromPad ? padBase : rootNote;
        return ((note - from) % count + count) % count;
    };

    double semitones = params.pitch.load();
    if (marked > 0)
    {
        // The slices marked in the editor.
        const int k = sliceFor (marked);
        const double a = juce::jlimit (0.0, 1.0, (double) params.slicePoints[(size_t) k].load()) * last;
        // The last slice ends where the chosen part ends.
        const double b = k + 1 < marked ? juce::jlimit (0.0, 1.0, (double) params.slicePoints[(size_t) k + 1].load()) * last
                                        : (s1 > a + 1.0 ? s1 : last);
        v->regionStart = a;
        v->regionEnd = juce::jmax (a + 1.0, b);
    }
    else if (slices > 1)
    {
        // Each key plays its own slice, at the sample's pitch.
        const int k = sliceFor (slices);
        const double w = (s1 - s0) / slices;
        v->regionStart = s0 + k * w;
        v->regionEnd = v->regionStart + w;
    }
    else
    {
        v->regionStart = s0;
        v->regionEnd = s1;
        if (! fromPad)
            semitones += note - rootNote;   // the pads play the sample at its own pitch
    }

    // A loop returns to the loop point (only without slices), never before the start.
    v->loopStart = (marked == 0 && slices <= 1)
                       ? juce::jlimit (v->regionStart, juce::jmax (v->regionStart, v->regionEnd - 1.0),
                                       (double) params.loopStart.load() * last)
                       : v->regionStart;

    const bool reverse = params.reverse.load();
    v->step = std::pow (2.0, semitones / 12.0) * (loaded->sampleRate / sr) * (reverse ? -1.0 : 1.0);
    v->pos = reverse ? v->regionEnd - 1.0 : v->regionStart;
    v->note = note;
    v->velocity = velocity;
    v->active = true;
    v->filter.reset();
    v->env.setParameters ({ juce::jmax (0.001f, params.attack.load()), 0.05f,
                            juce::jlimit (0.0f, 1.0f, params.sustain.load()), juce::jmax (0.005f, params.release.load()) });
    v->env.noteOn();
}

void Sampler::stopVoice (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note)
            v.env.noteOff();
}

void Sampler::render (juce::AudioBuffer<float>& out, int startSample, int numSamples, const juce::MidiBuffer& midi)
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked() || loaded == nullptr)
        return;

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        if (at > pos)
        {
            renderVoices (out, startSample + pos, at - pos);
            pos = at;
        }
        const auto m = meta.getMessage();
        if (m.isNoteOn())                         startVoice (m.getNoteNumber(), m.getVelocity() / 127.0f, m.getChannel() == 10);
        else if (m.isNoteOff())                   stopVoice (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) v.env.noteOff();
    }
    if (numSamples > pos)
        renderVoices (out, startSample + pos, numSamples - pos);
}

void Sampler::renderVoices (juce::AudioBuffer<float>& out, int start, int num)
{
    const auto& audio = loaded->audio;
    const float* srcL = audio.getReadPointer (0);
    const float* srcR = audio.getReadPointer (1);
    const int last = audio.getNumSamples() - 1;
    const bool loop = params.loop.load();

    const float bright = juce::jlimit (0.0f, 1.0f, params.brightness.load());
    const bool filterOn = bright < 0.99f;
    const float cutoff = 200.0f * std::pow (100.0f, bright);   // 200 Hz .. 20 kHz

    float* l = out.getWritePointer (0, start);
    float* r = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : l;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        if (filterOn)
            v.filter.setCutoffFrequency (juce::jmin (cutoff, (float) sr * 0.45f));

        for (int i = 0; i < num; ++i)
        {
            // Reached the end of the region: loop or finish.
            if (v.pos >= v.regionEnd || v.pos < v.regionStart)
            {
                if (loop)
                    v.pos = v.step > 0 ? v.loopStart + (v.pos - v.regionEnd) : v.regionEnd - (v.regionStart - v.pos);
                else
                {
                    v.active = false;
                    break;
                }
            }

            const int i0 = juce::jlimit (0, last, (int) v.pos);
            const int i1 = juce::jmin (last, i0 + 1);
            const float frac = (float) (v.pos - (int) v.pos);
            float xl = srcL[i0] + frac * (srcL[i1] - srcL[i0]);
            float xr = srcR[i0] + frac * (srcR[i1] - srcR[i0]);

            if (filterOn)
            {
                xl = v.filter.processSample (0, xl);
                xr = v.filter.processSample (1, xr);
            }

            const float g = v.env.getNextSample() * v.velocity;
            l[i] += xl * g;
            if (r != l) r[i] += xr * g;

            v.pos += v.step;

            if (! v.env.isActive())
            {
                v.active = false;
                break;
            }
        }
    }
}

} // namespace bizgroove
