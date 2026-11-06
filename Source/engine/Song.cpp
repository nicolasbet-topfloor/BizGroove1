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
        {
            b.tracks[(size_t) t].steps[(size_t) i].on.store (a.tracks[(size_t) t].steps[(size_t) i].on.load());
            b.tracks[(size_t) t].steps[(size_t) i].note.store (a.tracks[(size_t) t].steps[(size_t) i].note.load());
        }
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
    for (auto& p : pending) p.active = false;
    for (auto& d : delayed) d.active = false;
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

    if (isDrum)
    {
        for (int d = 0; d < numDrums; ++d)
            if ((pattern.drumLanes[(size_t) d].load() >> stepIndex) & 1u)
                buffer.addEvent (juce::MidiMessage::noteOn (1, drumBaseNote + d, (juce::uint8) 100), position);
        return;
    }

    const auto& st = pattern.steps[(size_t) stepIndex];
    if (! st.on.load())
        return;

    auto& p = pending[(size_t) track];
    if (p.active)
    {
        buffer.addEvent (juce::MidiMessage::noteOff (1, p.note), position);
        p.active = false;
    }

    const int note = st.note.load();
    buffer.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), position);

    const double offAt = position + gate * samplesPerStep;
    if (offAt < numSamples)
        buffer.addEvent (juce::MidiMessage::noteOff (1, note), juce::jmax (position, (int) offAt));
    else
        p = { true, note, offAt - numSamples };
}

bool Song::process (std::array<juce::MidiBuffer, numTracks>& out, int numSamples, double bpm, bool playing,
                    const std::array<bool, numTracks>& isDrumTrack)
{
    const double samplesPerStep = sampleRate * 60.0 / juce::jlimit (20.0, 300.0, bpm) / 4.0;   // 1/16 notes

    // Note-offs and swung hits carried over from the last block.
    for (int t = 0; t < numTracks; ++t)
    {
        auto& p = pending[(size_t) t];
        if (p.active)
        {
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
    return true;
}

} // namespace bizgroove
