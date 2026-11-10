#include "Song.h"

namespace bizgroove
{

Song::Song()
{
    for (auto& s : scenes)
        s = std::make_unique<Scene>();

    // A starting groove in scene 1 so a new project makes sound at once.
    auto& first = scene (0);

    // Track 1 (synth): a bass line.
    constexpr int bassNotes[numSteps] = { 36, 36, 48, 36, 39, 36, 46, 36, 36, 36, 48, 36, 43, 41, 39, 34 };
    constexpr bool bassOn[numSteps]   = { true, false, true, false, true, false, true, true,
                                          true, false, true, false, true, true, true, false };
    for (int i = 0; i < numSteps; ++i)
    {
        first.tracks[0].steps[(size_t) i].on.store (bassOn[i]);
        first.tracks[0].steps[(size_t) i].note.store (bassNotes[i]);
    }

    // Track 2 (drums): kick on the beats, snare on 2 and 4, closed hi-hat on the off-beats.
    auto lane = [&first] (int drum, std::uint16_t bits) { first.tracks[1].drumLanes[(size_t) drum].store (bits); };
    auto bitsFor = [] (std::initializer_list<int> stepsOn) {
        std::uint16_t b = 0;
        for (int st : stepsOn) b = (std::uint16_t) (b | (1u << st));
        return b;
    };
    lane (0, bitsFor ({ 0, 4, 8, 12 }));              // kick
    lane (2, bitsFor ({ 4, 12 }));                     // snare
    lane (6, bitsFor ({ 2, 6, 10, 14 }));              // closed hi-hat

    chain[0].scene.store (0);
    chain[0].repeats.store (1);
    clearTimeline();
}

void Song::clearTimeline()
{
    for (auto& row : timeline)
        for (auto& cell : row)
            cell.store (-1);
}

void Song::copyChainToTimeline()
{
    clearTimeline();
    int bar = 0;
    const int n = juce::jlimit (1, maxChain, chainLength.load());
    for (int i = 0; i < n && bar < maxBars; ++i)
        for (int r = 0; r < juce::jmax (1, chain[(size_t) i].repeats.load()) && bar < maxBars; ++r, ++bar)
            for (auto& row : timeline)
                row[(size_t) bar].store (chain[(size_t) i].scene.load());
    timelineLength.store (juce::jmax (1, bar));
}

int Song::sceneAt (int track, int bar, bool playSong) const
{
    if (! playSong)
        return currentScene.load();
    if (useTimeline.load())
        return bar < timelineLength.load() && bar < maxBars ? timeline[(size_t) track][(size_t) bar].load() : -1;

    const int n = juce::jlimit (1, maxChain, chainLength.load());
    int b = 0;
    for (int i = 0; i < n; ++i)
    {
        b += juce::jmax (1, chain[(size_t) i].repeats.load());
        if (bar < b)
            return chain[(size_t) i].scene.load();
    }
    return -1;
}

int Song::songLengthBars() const
{
    if (useTimeline.load())
        return juce::jlimit (1, maxBars, timelineLength.load());
    int bars = 0;
    const int n = juce::jlimit (1, maxChain, chainLength.load());
    for (int i = 0; i < n; ++i)
        bars += juce::jmax (1, chain[(size_t) i].repeats.load());
    return bars;
}

void Song::copyScene (int from, int to)
{
    if (from == to)
        return;
    auto& a = scene (from);
    auto& b = scene (to);
    for (int t = 0; t < numTracks; ++t)
    {
        for (int i = 0; i < numSteps; ++i)
            b.tracks[(size_t) t].steps[(size_t) i].copyFrom (a.tracks[(size_t) t].steps[(size_t) i]);
        for (int d = 0; d < numDrums; ++d)
            b.tracks[(size_t) t].drumLanes[(size_t) d].store (a.tracks[(size_t) t].drumLanes[(size_t) d].load());
    }
}

void Song::clearScene (int index)
{
    auto& s = scene (index);
    for (auto& t : s.tracks)
    {
        for (auto& st : t.steps)
            st.on.store (false);
        for (auto& l : t.drumLanes)
            l.store (0);
    }
}

bool Song::sceneIsEmpty (int index) const
{
    const auto& s = *scenes[(size_t) juce::jlimit (0, numScenes - 1, index)];
    for (const auto& t : s.tracks)
    {
        for (const auto& st : t.steps)
            if (st.on.load()) return false;
        for (const auto& l : t.drumLanes)
            if (l.load() != 0) return false;
    }
    return true;
}

void Song::queueScene (int index, bool playing)
{
    index = juce::jlimit (0, numScenes - 1, index);
    if (playing && ! songMode.load())
        queuedScene.store (index);
    else
    {
        currentScene.store (index);
        queuedScene.store (-1);
    }
}

void Song::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void Song::reset()
{
    wasPlaying = false;
    trackScene.fill (currentScene.load());
    firstBar = true;
    step = -1;
    samplesUntilNextStep = 0.0;
    playheadStep.store (-1);
    for (auto& row : pending)
        for (auto& p : row) p.active = false;
    for (auto& d : delayed) d.active = false;
    for (auto& row : skipOnce) row.fill (0);
    for (auto& row : held)
        for (auto& h : row) h.active = false;
    clock = 0.0;
    numStepStarts = 0;
}

void Song::addPendingOff (int track, int note, double remaining)
{
    auto& row = pending[(size_t) track];
    for (auto& p : row)
        if (! p.active)
        {
            p = { true, note, remaining };
            return;
        }
    // All in use: replace the one that ends soonest.
    auto* soonest = &row[0];
    for (auto& p : row)
        if (p.remaining < soonest->remaining)
            soonest = &p;
    *soonest = { true, note, remaining };
}

void Song::flushPending (juce::MidiBuffer& buffer, int track, int note, int position)
{
    for (auto& p : pending[(size_t) track])
        if (p.active && p.note == note)
        {
            buffer.addEvent (juce::MidiMessage::noteOff (1, note), position);
            p.active = false;
        }
}

bool Song::startBar()
{
    if (songMode.load() && useTimeline.load())
    {
        const int length = juce::jlimit (1, maxBars, timelineLength.load());
        int bar = firstBar ? 0 : timelineBar.load() + 1;
        if (bar >= length)
        {
            if (! loopSong.load())
                return false;
            bar = 0;
        }
        timelineBar.store (bar);
        for (int t = 0; t < numTracks; ++t)
            trackScene[(size_t) t] = juce::jlimit (-1, numScenes - 1, timeline[(size_t) t][(size_t) bar].load());

        // Show (and edit) the scene of the first track that plays this bar.
        for (int t = 0; t < numTracks; ++t)
            if (trackScene[(size_t) t] >= 0) { currentScene.store (trackScene[(size_t) t]); break; }
        firstBar = false;
        playingScene = currentScene.load();
        return true;
    }

    if (songMode.load())
    {
        const int length = juce::jlimit (1, maxChain, chainLength.load());
        int pos = chainPosition.load();
        int rep = chainRepeat.load();

        if (firstBar)
        {
            pos = 0;
            rep = 0;
        }
        else if (++rep >= juce::jmax (1, chain[(size_t) pos].repeats.load()))
        {
            rep = 0;
            if (++pos >= length)
            {
                if (! loopSong.load())
                    return false;
                pos = 0;
            }
        }

        chainPosition.store (pos);
        chainRepeat.store (rep);
        currentScene.store (juce::jlimit (0, numScenes - 1, chain[(size_t) pos].scene.load()));
        queuedScene.store (-1);
    }
    else
    {
        const int q = queuedScene.exchange (-1);
        if (q >= 0)
            currentScene.store (q);
    }

    firstBar = false;
    playingScene = currentScene.load();
    trackScene.fill (playingScene);
    return true;
}

void Song::emit (std::array<juce::MidiBuffer, numTracks>& out, int track, int sceneIndex, int stepIndex, int position,
                 int numSamples, double samplesPerStep, bool isDrum)
{
    if (! juce::isPositiveAndBelow (sceneIndex, numScenes))
        return;   // this track is silent in this bar
    auto& pattern = scenes[(size_t) sceneIndex]->tracks[(size_t) track];
    auto& buffer = out[(size_t) track];
    const std::uint16_t skip = skipOnce[(size_t) track][(size_t) stepIndex];   // played live just now
    skipOnce[(size_t) track][(size_t) stepIndex] = 0;

    if (isDrum)
    {
        for (int d = 0; d < numDrums; ++d)
            if (((pattern.drumLanes[(size_t) d].load() >> stepIndex) & 1u) && ((skip >> d) & 1u) == 0)
                buffer.addEvent (juce::MidiMessage::noteOn (1, drumBaseNote + d, (juce::uint8) 100), position);
        return;
    }

    const auto& st = pattern.steps[(size_t) stepIndex];
    if (! st.on.load() || (skip & 1u) != 0)
        return;

    std::array<int, maxChord> notes {};
    const int count = st.getNotes (notes);
    const auto velocity = (juce::uint8) juce::jlimit (1, 127, st.velocity.load());
    const float length = st.length.load();
    const double duration = (length > 0.0f ? juce::jlimit (0.25, 16.0, (double) length) - 0.02 : (double) gate) * samplesPerStep;

    for (int i = 0; i < count; ++i)
    {
        const int note = notes[(size_t) i];
        flushPending (buffer, track, note, position);   // the same note still sounding: end it first
        buffer.addEvent (juce::MidiMessage::noteOn (1, note, velocity), position);

        const double offAt = position + duration;
        if (offAt < numSamples)
            buffer.addEvent (juce::MidiMessage::noteOff (1, note), juce::jmax (position, (int) offAt));
        else
            addPendingOff (track, note, offAt - numSamples);
    }
}

bool Song::process (std::array<juce::MidiBuffer, numTracks>& out, int numSamples, double bpm, bool playing,
                    const std::array<bool, numTracks>& isDrumTrack)
{
    const double samplesPerStep = sampleRate * 60.0 / juce::jlimit (20.0, 300.0, bpm) / 4.0;   // 1/16 notes

    // Note-offs and swung hits carried over from the last block.
    numStepStarts = 0;
    for (int t = 0; t < numTracks; ++t)
    {
        for (auto& p : pending[(size_t) t])
        {
            if (! p.active)
                continue;
            if (p.remaining < numSamples)
            {
                out[(size_t) t].addEvent (juce::MidiMessage::noteOff (1, p.note), juce::jmax (0, (int) p.remaining));
                p.active = false;
            }
            else p.remaining -= numSamples;
        }

        auto& d = delayed[(size_t) t];
        if (d.active && playing)
        {
            if (d.remaining < numSamples)
            {
                d.active = false;
                emit (out, t, d.sceneIndex, d.stepIndex, juce::jmax (0, (int) d.remaining), numSamples, samplesPerStep, isDrumTrack[(size_t) t]);
            }
            else d.remaining -= numSamples;
        }
    }

    if (! playing)
    {
        if (wasPlaying)
        {
            for (int t = 0; t < numTracks; ++t)
                out[(size_t) t].addEvent (juce::MidiMessage::allNotesOff (1), 0);
            reset();
        }
        return true;
    }

    if (! wasPlaying)
    {
        reset();
        wasPlaying = true;
        samplesUntilNextStep = (double) juce::jmin (startOffset, numSamples - 1);   // after a count-in, the bar starts mid-block
        startOffset = 0;
    }

    for (;;)
    {
        const auto position = (int) std::ceil (samplesUntilNextStep);
        if (position >= numSamples)
            break;

        step = (step + 1) % numSteps;
        if (step == 0 && ! startBar())
            return false;   // the song has ended
        playheadStep.store (step);
        if (numStepStarts < (int) stepStarts.size())
            stepStarts[(size_t) numStepStarts++] = { position, step };

        for (int t = 0; t < numTracks; ++t)
        {
            // Swing pushes every second 1/16 later, by up to half a step.
            const double push = (step % 2 == 1) ? swing[(size_t) t].load() * 0.5 * samplesPerStep : 0.0;
            const double at = position + push;
            const int sc = trackScene[(size_t) t];
            if (at < numSamples)
                emit (out, t, sc, step, (int) at, numSamples, samplesPerStep, isDrumTrack[(size_t) t]);
            else
                delayed[(size_t) t] = { true, step, sc, at - numSamples };
        }

        samplesUntilNextStep += samplesPerStep;
    }

    samplesUntilNextStep -= numSamples;
    clock += numSamples;
    return true;
}

void Song::recordNote (int track, bool isDrum, int note, int drum, int position, int numSamples, double bpm, int velocity)
{
    if (step < 0 || ! juce::isPositiveAndBelow (track, numTracks) || (isDrum && ! juce::isPositiveAndBelow (drum, numDrums)))
        return;

    // Where the note falls between the steps: process() has just run, so the last step
    // started at (numSamples + samplesUntilNextStep - samplesPerStep) in this block.
    const double samplesPerStep = sampleRate * 60.0 / juce::jlimit (20.0, 300.0, bpm) / 4.0;
    const double lastStepStart = (double) numSamples + samplesUntilNextStep - samplesPerStep;
    const double offset = ((double) position - lastStepStart) / samplesPerStep;   // in steps from the last step
    const int whole = (int) std::floor (offset);
    const int nearest = whole + ((offset - whole) >= 0.5 ? 1 : 0);              // quantise to the nearest 1/16
    const int target = ((step + nearest) % numSteps + numSteps) % numSteps;
    const bool ahead = nearest > 0;   // the playhead hasn't reached it yet

    const int sc = trackScene[(size_t) track] >= 0 ? trackScene[(size_t) track] : playingScene;
    auto& pattern = scenes[(size_t) juce::jlimit (0, numScenes - 1, sc)]->tracks[(size_t) track];
    if (isDrum)
    {
        auto& lane = pattern.drumLanes[(size_t) drum];
        lane.store ((std::uint16_t) (lane.load() | (1u << target)));
        if (ahead)
            skipOnce[(size_t) track][(size_t) target] = (std::uint16_t) (skipOnce[(size_t) track][(size_t) target] | (1u << drum));
    }
    else
    {
        // An empty step takes the note; a step that already plays gets it added, as a chord.
        auto& st = pattern.steps[(size_t) target];
        if (! st.on.load())
            st.setSingle (note, velocity);
        else if (! st.addNote (note))
            st.velocity.store (juce::jlimit (1, 127, velocity));   // the same note again: just how hard
        st.on.store (true);
        if (ahead)
            skipOnce[(size_t) track][(size_t) target] = (std::uint16_t) (skipOnce[(size_t) track][(size_t) target] | 1u);

        // Remember it until the key is let go, for its length.
        if (juce::isPositiveAndBelow (note, 128))
            held[(size_t) track][(size_t) note] = { true, juce::jlimit (0, numScenes - 1, sc), target,
                                                    clock - (double) numSamples + (double) position };
    }
}

void Song::recordNoteOff (int track, int note, int position, int numSamples, double bpm)
{
    if (! juce::isPositiveAndBelow (track, numTracks) || ! juce::isPositiveAndBelow (note, 128))
        return;
    auto& h = held[(size_t) track][(size_t) note];
    if (! h.active)
        return;
    h.active = false;

    const double samplesPerStep = sampleRate * 60.0 / juce::jlimit (20.0, 300.0, bpm) / 4.0;
    const double heldSteps = (clock - (double) numSamples + (double) position - h.start) / samplesPerStep;
    const float length = (float) juce::jlimit (0.25, 16.0, std::round (heldSteps * 4.0) / 4.0);

    auto& st = scenes[(size_t) h.scene]->tracks[(size_t) track].steps[(size_t) h.step];
    std::array<int, maxChord> notes {};
    const bool chord = st.getNotes (notes) > 1;
    st.length.store (chord ? juce::jmax (st.length.load(), length) : length);   // a chord lasts as long as its longest note
}

} // namespace bizgroove
