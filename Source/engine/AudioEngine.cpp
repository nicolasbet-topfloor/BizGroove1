#include "AudioEngine.h"

namespace bizgroove
{

AudioEngine::AudioEngine()
{
    for (auto& t : tracks)
        t = std::make_unique<Track> (performance);

    // A new project: track 1 a synth with a bass line, track 2 the drum kit.
    track (1).type.store (Track::drums);
    for (int t = 0; t < numTracks; ++t)
        lastType[(size_t) t] = track (t).type.load();
}

void AudioEngine::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    const int maxBlock = juce::jmax (512, samplesPerBlockExpected);

    for (auto& t : tracks)
    {
        t->synth.setCurrentPlaybackSampleRate (sampleRate);
        for (int i = 0; i < t->synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<TestSynthVoice*> (t->synth.getVoice (i)))
                v->prepare (sampleRate, maxBlock);
        t->kit.prepare (sampleRate);
        t->samplerEngine.prepare (sampleRate, maxBlock);
        t->strip.prepare (sampleRate);
        t->gain.reset (sampleRate, 0.02);
        t->gain.setCurrentAndTargetValue (t->volume.load());
    }

    for (auto& m : trackMidi)
        m.ensureSize (2048);
    liveMidi.ensureSize (2048);
    trackBuffer.setSize (2, maxBlock);
    reverbBus.setSize (2, maxBlock);
    delayBus.setSize (2, maxBlock);
    effects.prepare (sampleRate, maxBlock);

    song.prepare (sampleRate);
    stems.prepare (sampleRate, samplesPerBlockExpected);
    transportSamples = 0;
    wasPlaying = false;
    midiInput.reset (sampleRate);
    keyboardState.reset();

    smoothedGain.reset (sampleRate, 0.02);
    smoothedGain.setCurrentAndTargetValue (masterGain.load());
}

void AudioEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    const int numSamples = info.numSamples;
    if (trackBuffer.getNumSamples() < numSamples)
    {
        trackBuffer.setSize (2, numSamples, false, false, true);
        reverbBus.setSize (2, numSamples, false, false, true);
        delayBus.setSize (2, numSamples, false, false, true);
    }
    reverbBus.clear (0, numSamples);
    delayBus.clear (0, numSamples);

    // 1. Live MIDI: external keyboards, then the on-screen keyboard.
    liveMidi.clear();
    midiInput.removeNextBlockOfMessages (liveMidi, numSamples);
    keyboardState.processNextMidiBuffer (liveMidi, 0, numSamples, true);

    for (const auto meta : liveMidi)
    {
        const auto m = meta.getMessage();
        if (m.isPitchWheel())
            performance.pitchBend.store (juce::jlimit (-1.0f, 1.0f, (m.getPitchWheelValue() - 8192) / 8191.0f));
        else if (m.isController() && m.getControllerNumber() == 1)
            performance.modWheel.store (m.getControllerValue() / 127.0f);
    }

    // 2. Switching tracks releases notes held on the previous one.
    const int sel = juce::jlimit (0, numTracks - 1, selectedTrack.load());
    if (sel != lastSelected)
    {
        tracks[(size_t) lastSelected]->synth.allNotesOff (0, true);
        lastSelected = sel;
    }

    // 3. The song clock writes each track's notes for this block.
    bool playing = transport.playing.load();
    const double bpm = transport.bpm.load();
    if (playing && ! wasPlaying)
        transportSamples = 0;
    wasPlaying = playing;

    std::array<bool, numTracks> isDrum {};
    for (int t = 0; t < numTracks; ++t)
    {
        const int type = tracks[(size_t) t]->type.load();
        if (type != lastType[(size_t) t])
        {
            tracks[(size_t) t]->synth.allNotesOff (0, false);   // the sound was changed
            lastType[(size_t) t] = type;
        }
        isDrum[(size_t) t] = type == Track::drums;
        trackMidi[(size_t) t].clear();
    }

    if (! song.process (trackMidi, numSamples, bpm, playing, isDrum))
    {
        transport.playing.store (false);   // a non-looping song has ended
        playing = false;
    }

    // 4. Live notes go to the selected track. On a drum track a key also picks
    //    the drum that the knobs and pads edit.
    auto& selTrack = *tracks[(size_t) sel];
    for (const auto meta : liveMidi)
    {
        const auto m = meta.getMessage();
        if (isDrum[(size_t) sel] && m.isNoteOn())
        {
            const int drum = DrumKit::drumForNote (m.getNoteNumber());
            if (drum >= 0)
                selTrack.selectedDrum.store (drum);
        }
        trackMidi[(size_t) sel].addEvent (m, meta.samplePosition);
    }

    // 5. Render and mix the tracks: sound, volume, EQ, pan, sends.
    bool anySolo = false;
    for (auto& t : tracks)
        anySolo = anySolo || t->mix.solo.load();
    const int only = renderOnlyTrack.load();

    for (int t = 0; t < numTracks; ++t)
    {
        auto& tr = *tracks[(size_t) t];
        trackBuffer.clear (0, numSamples);

        const int type = tr.type.load();
        if (type == Track::drums)
            tr.kit.render (trackBuffer, 0, numSamples, trackMidi[(size_t) t]);
        else if (type == Track::sampler)
            tr.samplerEngine.render (trackBuffer, 0, numSamples, trackMidi[(size_t) t]);
        else
            tr.synth.renderNextBlock (trackBuffer, trackMidi[(size_t) t], 0, numSamples);

        bool audible = ! tr.mute.load() && (! anySolo || tr.mix.solo.load());
        if (only != -1)
            audible = (t == only);
        tr.gain.setTargetValue (audible ? tr.volume.load() : 0.0f);
        trackBuffer.applyGainRamp (0, 0, numSamples, tr.gain.getCurrentValue(), tr.gain.getTargetValue());
        trackBuffer.applyGainRamp (1, 0, numSamples, tr.gain.getCurrentValue(), tr.gain.getTargetValue());
        tr.gain.skip (numSamples);

        tr.strip.process (trackBuffer, numSamples, tr.mix, reverbBus, delayBus);

        float peak = 0.0f;
        for (int ch = 0; ch < juce::jmin (2, info.buffer->getNumChannels()); ++ch)
        {
            info.buffer->addFrom (ch, info.startSample, trackBuffer, ch, 0, numSamples);
            peak = juce::jmax (peak, trackBuffer.getMagnitude (ch, 0, numSamples));
        }
        tr.level.store (peak);
    }

    // Master effects: reverb and delay returns.
    effects.process (reverbBus, delayBus, *info.buffer, info.startSample, numSamples, bpm);

    // 6. Stems, then master volume.
    if (only == -1 || only == -2)
        stems.process (*info.buffer, info.startSample, numSamples, transportSamples, playing, bpm);
    if (playing)
        transportSamples += numSamples;

    smoothedGain.setTargetValue (masterGain.load());
    const int numChannels = info.buffer->getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = smoothedGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            info.buffer->getWritePointer (ch, info.startSample)[i] *= g;
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        const float peak = ch < numChannels ? info.buffer->getMagnitude (ch, info.startSample, numSamples) : 0.0f;
        peaks[(size_t) ch].store (peak);
    }
}

// ============================================================== editing helpers

bool AudioEngine::isStepOn (int step)
{
    auto& tr = selected();
    auto& pattern = song.scene (song.currentScene.load()).tracks[(size_t) selectedTrack.load()];
    if (tr.type.load() == Track::drums)
        return ((pattern.drumLanes[(size_t) tr.selectedDrum.load()].load() >> step) & 1u) != 0;
    return pattern.steps[(size_t) step].on.load();
}

void AudioEngine::toggleStep (int step)
{
    if (! juce::isPositiveAndBelow (step, Song::numSteps))
        return;
    auto& tr = selected();
    auto& pattern = song.scene (song.currentScene.load()).tracks[(size_t) selectedTrack.load()];
    if (tr.type.load() == Track::drums)
    {
        auto& lane = pattern.drumLanes[(size_t) tr.selectedDrum.load()];
        lane.store ((std::uint16_t) (lane.load() ^ (1u << step)));
    }
    else
    {
        auto& s = pattern.steps[(size_t) step];
        s.on.store (! s.on.load());
    }
}

int AudioEngine::getStepNote (int step)
{
    return song.scene (song.currentScene.load()).tracks[(size_t) selectedTrack.load()].steps[(size_t) step].note.load();
}

void AudioEngine::setStepNote (int step, int note)
{
    song.scene (song.currentScene.load()).tracks[(size_t) selectedTrack.load()].steps[(size_t) step].note.store (juce::jlimit (0, 127, note));
}

} // namespace bizgroove

namespace bizgroove
{

bool AudioEngine::trackHasContent (int t)
{
    for (int s = 0; s < Song::numScenes; ++s)
    {
        const auto& p = song.scene (s).tracks[(size_t) t];
        for (const auto& st : p.steps)
            if (st.on.load()) return true;
        for (const auto& l : p.drumLanes)
            if (l.load() != 0) return true;
    }
    return false;
}

bool AudioEngine::renderOffline (const RenderJob& job, juce::AudioBuffer<float>& result,
                                 std::function<bool (float)> keepGoing)
{
    constexpr int block = 512;
    const double secondsPerBar = 4.0 * 60.0 / juce::jlimit (20.0, 300.0, transport.bpm.load());
    const int musicSamples = (int) std::ceil (job.bars * secondsPerBar * job.sampleRate);
    const int total = musicSamples + (int) (job.tailSeconds * job.sampleRate);

    // Remember what we change, to put it back afterwards.
    const bool songModeBefore = song.songMode.load();
    const int sceneBefore = song.currentScene.load();

    result.setSize (2, total);
    result.clear();
    prepareToPlay (block, job.sampleRate);
    song.songMode.store (job.playSong);
    song.reset();
    renderOnlyTrack.store (job.onlyTrack);
    keyboardState.allNotesOff (0);
    transport.playing.store (true);

    juce::AudioBuffer<float> chunk (2, block);
    bool completed = true;
    for (int pos = 0; pos < total; pos += block)
    {
        if (pos >= musicSamples && transport.playing.load())
            transport.playing.store (false);   // the music is done: let reverb and delay ring out

        const int n = juce::jmin (block, total - pos);
        chunk.clear();
        getNextAudioBlock (juce::AudioSourceChannelInfo (&chunk, 0, n));
        for (int ch = 0; ch < 2; ++ch)
            result.copyFrom (ch, pos, chunk, ch, 0, n);

        if (keepGoing && (pos / block) % 64 == 0 && ! keepGoing ((float) pos / (float) total))
        {
            completed = false;
            break;
        }
    }

    transport.playing.store (false);
    renderOnlyTrack.store (-1);
    song.songMode.store (songModeBefore);
    song.currentScene.store (sceneBefore);
    song.reset();
    return completed;
}

} // namespace bizgroove
