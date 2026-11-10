#include "AudioEngine.h"
#include <algorithm>

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
    currentRate = sampleRate;

    for (auto& t : tracks)
    {
        t->synth.setCurrentPlaybackSampleRate (sampleRate);
        for (int i = 0; i < t->synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<TestSynthVoice*> (t->synth.getVoice (i)))
                v->prepare (sampleRate, maxBlock);
        t->polySynth.setCurrentPlaybackSampleRate (sampleRate);
        for (int i = 0; i < t->polySynth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<PolyVoice*> (t->polySynth.getVoice (i)))
                v->prepare (sampleRate);
        t->waveSynth.setCurrentPlaybackSampleRate (sampleRate);
        for (int i = 0; i < t->waveSynth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<WaveVoice*> (t->waveSynth.getVoice (i)))
                v->prepare (sampleRate);
        t->fmSynth.setCurrentPlaybackSampleRate (sampleRate);
        for (int i = 0; i < t->fmSynth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<FmVoice*> (t->fmSynth.getVoice (i)))
                v->prepare (sampleRate);
        t->kit.prepare (sampleRate);
        t->samplerEngine.prepare (sampleRate, maxBlock);
        t->strip.prepare (sampleRate);
        for (auto& fx : t->insertFx)
            fx.prepare (sampleRate);
        t->gain.reset (sampleRate, 0.02);
        t->gain.setCurrentAndTargetValue (t->volume.load());
    }

    for (auto& m : trackMidi)
        m.ensureSize (2048);
    liveMidi.ensureSize (2048);
    for (auto& m : externalMidi)
        m.ensureSize (2048);
    liveInput.setSize (8, maxBlock);
    liveInput.clear();
    liveInputSamples = 0;
    trackBuffer.setSize (2, maxBlock);
    reverbBus.setSize (2, maxBlock);
    delayBus.setSize (2, maxBlock);
    effects.prepare (sampleRate, maxBlock);
    performFx.prepare (sampleRate);

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
        tracks[(size_t) lastSelected]->polySynth.allNotesOff (0, true);
        tracks[(size_t) lastSelected]->waveSynth.allNotesOff (0, true);
        tracks[(size_t) lastSelected]->fmSynth.allNotesOff (0, true);
        if (tracks[(size_t) lastSelected]->type.load() == Track::external)
            tracks[(size_t) lastSelected]->ext.panic.store (true);   // no notes left hanging on the gear
        lastSelected = sel;
    }

    // 3. The song clock writes each track's notes for this block.
    bool playing = transport.playing.load();
    const double bpm = transport.bpm.load();
    if (playing && ! wasPlaying)
        transportSamples = 0;
    if (! playing && wasPlaying)
        for (auto& tr : tracks)
            if (tr->type.load() == Track::external)
                tr->ext.panic.store (true);   // Stop silences the outside gear too
    if (! playing && wasPlaying)
        recordNotes.store (false);   // Stop ends a recording pass
    wasPlaying = playing;

    std::array<bool, numTracks> isDrum {};
    for (int t = 0; t < numTracks; ++t)
    {
        const int type = tracks[(size_t) t]->type.load();
        if (type != lastType[(size_t) t])
        {
            tracks[(size_t) t]->synth.allNotesOff (0, false);   // the sound was changed
            tracks[(size_t) t]->polySynth.allNotesOff (0, false);
            tracks[(size_t) t]->waveSynth.allNotesOff (0, false);
            tracks[(size_t) t]->fmSynth.allNotesOff (0, false);
            if (lastType[(size_t) t] == Track::external)
                tracks[(size_t) t]->ext.panic.store (true);
            lastType[(size_t) t] = type;
        }
        isDrum[(size_t) t] = tracks[(size_t) t]->playsDrums();
        trackMidi[(size_t) t].clear();
    }

    if (! song.process (trackMidi, numSamples, bpm, playing, isDrum))
    {
        transport.playing.store (false);   // a non-looping song has ended
        playing = false;
    }

    // 4. Live notes. MIDI channel 10 (the Launchkey pads in Drum mode) plays the
    //    drums: the selected track if it is a drum track, otherwise the first
    //    drum track. Everything else goes to the selected track, so the keys can
    //    play a synth while the pads play the drums. On a drum track a note also
    //    picks the drum that the knobs and pads edit.
    //    On a Sampler track the pads play its slices instead.
    const bool selIsSampler = tracks[(size_t) sel]->type.load() == Track::sampler;
    int drumTarget = (isDrum[(size_t) sel] || selIsSampler) ? sel : -1;
    for (int t = 0; t < numTracks && drumTarget < 0; ++t)
        if (isDrum[(size_t) t])
            drumTarget = t;

    for (const auto meta : liveMidi)
    {
        const auto m = meta.getMessage();
        const int dest = (drumTarget >= 0 && m.getChannel() == 10) ? drumTarget : sel;
        if (isDrum[(size_t) dest] && m.isNoteOn())
        {
            const int drum = DrumKit::drumForNote (m.getNoteNumber());
            if (drum >= 0)
                tracks[(size_t) dest]->selectedDrum.store (drum);
        }
        trackMidi[(size_t) dest].addEvent (m, meta.samplePosition);

        // Recording: the note lands on the nearest step of the pattern playing.
        if (playing && recordNotes.load() && m.isNoteOn() && m.getVelocity() > 0)
            song.recordNote (dest, isDrum[(size_t) dest], m.getNoteNumber(), DrumKit::drumForNote (m.getNoteNumber()),
                             meta.samplePosition, numSamples, bpm);
    }

    // What the effects need: the tempo, the song position, and where the kick drums hit.
    EffectContext fxContext;
    fxContext.bpm = bpm;
    fxContext.beatAtStart = (double) (playing ? transportSamples : freeSamples) / currentRate * juce::jlimit (20.0, 300.0, bpm) / 60.0;
    for (int t = 0; t < numTracks; ++t)
    {
        if (! isDrum[(size_t) t])
            continue;
        for (const auto meta : trackMidi[(size_t) t])
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn() && DrumKit::drumForNote (m.getNoteNumber()) == 0 && fxContext.numKicks < (int) fxContext.kicks.size())
                fxContext.kicks[(size_t) fxContext.numKicks++] = meta.samplePosition;
        }
    }
    std::sort (fxContext.kicks.begin(), fxContext.kicks.begin() + fxContext.numKicks);

    // 5. Render and mix the tracks: sound, effects, volume, EQ, pan, sends.
    bool anySolo = false;
    for (auto& t : tracks)
        anySolo = anySolo || t->mix.solo.load();
    const int only = renderOnlyTrack.load();

    for (int t = 0; t < numTracks; ++t)
    {
        auto& tr = *tracks[(size_t) t];
        trackBuffer.clear (0, numSamples);

        const int type = tr.type.load();
        const bool isExternal = type == Track::external;
        buildExternalMidi (tr.ext, trackMidi[(size_t) t], externalMidi[(size_t) t], isExternal);
        if (isExternal)
        {
            // The gear's sound comes back through the sound card's input, if it is heard.
            if (tr.ext.monitor.load() && liveInputSamples == numSamples)
            {
                const int last = liveInput.getNumChannels() - 1;
                const int first = juce::jlimit (0, last, 2 * tr.ext.inputPair.load());
                trackBuffer.copyFrom (0, 0, liveInput, first, 0, numSamples);
                trackBuffer.copyFrom (1, 0, liveInput, juce::jmin (first + 1, last), 0, numSamples);
            }
        }
        else if (type == Track::drums)
            tr.kit.render (trackBuffer, 0, numSamples, trackMidi[(size_t) t]);
        else if (type == Track::sampler)
            tr.samplerEngine.render (trackBuffer, 0, numSamples, trackMidi[(size_t) t]);
        else
        {
            if (type == Track::fm)
                tr.fmSynth.renderNextBlock (trackBuffer, trackMidi[(size_t) t], 0, numSamples);
            else if (type == Track::wavetable)
                tr.waveSynth.renderNextBlock (trackBuffer, trackMidi[(size_t) t], 0, numSamples);
            else if (type == Track::poly)
                tr.polySynth.renderNextBlock (trackBuffer, trackMidi[(size_t) t], 0, numSamples);
            else
                tr.synth.renderNextBlock (trackBuffer, trackMidi[(size_t) t], 0, numSamples);
        }

        for (size_t slot = 0; slot < tr.insertFx.size(); ++slot)
            tr.insertFx[slot].process (trackBuffer, numSamples, tr.inserts[slot], fxContext);

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
    freeSamples += numSamples;

    // Repeat, stutter and tape stop play on the whole mix.
    performFx.process (*info.buffer, info.startSample, numSamples, perform, bpm);

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
    liveInputSamples = 0;   // the next block brings its own input, or none (offline export)
}

void AudioEngine::setLiveInput (const juce::AudioBuffer<float>& input, int startSample, int numSamples)
{
    if (numSamples > liveInput.getNumSamples())
    {
        liveInputSamples = 0;
        return;
    }
    const int channels = juce::jmin (liveInput.getNumChannels(), input.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
        liveInput.copyFrom (ch, 0, input, ch, startSample, numSamples);
    for (int ch = channels; ch < liveInput.getNumChannels(); ++ch)
        liveInput.clear (ch, 0, numSamples);
    liveInputSamples = numSamples;
}

// ============================================================== editing helpers

bool AudioEngine::isStepOn (int step)
{
    auto& tr = selected();
    auto& pattern = song.scene (song.currentScene.load()).tracks[(size_t) selectedTrack.load()];
    if (tr.playsDrums())
        return ((pattern.drumLanes[(size_t) tr.selectedDrum.load()].load() >> step) & 1u) != 0;
    return pattern.steps[(size_t) step].on.load();
}

void AudioEngine::toggleStep (int step)
{
    if (! juce::isPositiveAndBelow (step, Song::numSteps))
        return;
    auto& tr = selected();
    auto& pattern = song.scene (song.currentScene.load()).tracks[(size_t) selectedTrack.load()];
    if (tr.playsDrums())
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
