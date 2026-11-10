#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace bizgroove
{

/** Patterns, scenes and the song chain, and the clock that plays them.

    - A track's pattern is one bar of 16 steps. Synth tracks store a note per
      step; drum tracks store 16 lanes (one per drum) of on/off steps.
    - A scene holds one pattern for each of the 16 tracks.
    - Scene mode loops the current scene. Choosing another scene while playing
      queues it; it starts at the next bar line.
    - Song mode plays the chain: a list of scenes, each repeated N bars.

    All pattern data is atomic: the UI and controllers edit it while the audio
    thread plays it. Timing state belongs to the audio thread alone.
*/
class Song
{
public:
    static constexpr int numTracks = 16;
    static constexpr int numScenes = 16;
    static constexpr int numSteps = 16;
    static constexpr int numDrums = 16;
    static constexpr int maxChain = 64;
    static constexpr int maxBars = 128;   // timeline length limit

    /** First MIDI note of the drum kit: drum d plays on note drumBaseNote + d. */
    static constexpr int drumBaseNote = 36;

    static constexpr int maxChord = 4;   // notes one synth step can hold

    /** One synth step: a note, or a chord of up to four, with how long and how hard it plays. */
    struct Step
    {
        std::atomic<bool> on { false };
        std::atomic<int> note { 48 };                          // the step's first note
        std::array<std::atomic<int>, maxChord - 1> chord {};   // more notes played with it, stored as note + 1 (0 = none)
        std::atomic<float> length { 0.0f };                    // in steps (0.25 .. 16); 0 = the usual short note
        std::atomic<int> velocity { 100 };                     // 1 .. 127

        /** The notes this step plays, the first note first. Returns how many. */
        int getNotes (std::array<int, maxChord>& out) const
        {
            int n = 0;
            out[(size_t) n++] = note.load();
            for (const auto& c : chord)
                if (const int v = c.load(); v > 0)
                    out[(size_t) n++] = v - 1;
            return n;
        }

        /** A single note again: no chord, the usual length. */
        void setSingle (int newNote, int newVelocity = 100)
        {
            note.store (juce::jlimit (0, 127, newNote));
            for (auto& c : chord)
                c.store (0);
            length.store (0.0f);
            velocity.store (juce::jlimit (1, 127, newVelocity));
        }

        /** Adds a note to the chord (the last slot is replaced when it is full). False if it was already there. */
        bool addNote (int newNote)
        {
            newNote = juce::jlimit (0, 127, newNote);
            if (note.load() == newNote)
                return false;
            for (auto& c : chord)
                if (c.load() == newNote + 1)
                    return false;
            for (auto& c : chord)
                if (c.load() == 0)
                {
                    c.store (newNote + 1);
                    return true;
                }
            chord.back().store (newNote + 1);
            return true;
        }

        /** Takes a note out of the step; the step goes off when it was its only note. */
        void removeNote (int oldNote)
        {
            for (auto& c : chord)
                if (c.load() == oldNote + 1)
                {
                    c.store (0);
                    return;
                }
            if (note.load() != oldNote)
                return;
            for (auto& c : chord)   // the first note goes: the next one takes its place
                if (const int v = c.load(); v > 0)
                {
                    note.store (v - 1);
                    c.store (0);
                    return;
                }
            on.store (false);
        }

        /** Moves every note of the step by the same amount. */
        void transpose (int semitones)
        {
            note.store (juce::jlimit (0, 127, note.load() + semitones));
            for (auto& c : chord)
                if (const int v = c.load(); v > 0)
                    c.store (juce::jlimit (0, 127, v - 1 + semitones) + 1);
        }

        void copyFrom (const Step& o)
        {
            on.store (o.on.load());
            note.store (o.note.load());
            for (size_t i = 0; i < chord.size(); ++i)
                chord[i].store (o.chord[i].load());
            length.store (o.length.load());
            velocity.store (o.velocity.load());
        }
    };

    struct TrackPattern
    {
        std::array<Step, numSteps> steps;                     // synth tracks
        std::array<std::atomic<std::uint16_t>, numDrums> drumLanes {};   // drum tracks: bit s = step s
    };

    struct Scene
    {
        std::array<TrackPattern, numTracks> tracks;
    };

    struct ChainEntry
    {
        std::atomic<int> scene { 0 };
        std::atomic<int> repeats { 1 };
    };

    Song();

    // ---------------------------------------------------------- editing (any thread)
    Scene& scene (int index) { return *scenes[(size_t) juce::jlimit (0, numScenes - 1, index)]; }
    void copyScene (int from, int to);
    void clearScene (int index);
    bool sceneIsEmpty (int index) const;

    // Chain
    std::array<ChainEntry, maxChain> chain;
    std::atomic<int> chainLength { 1 };
    std::atomic<bool> songMode { false };     // false = loop the current scene
    std::atomic<bool> loopSong { true };

    // Timeline: for each track and bar, the scene whose pattern it plays (-1 = silent).
    std::array<std::array<std::atomic<int>, maxBars>, numTracks> timeline;
    std::atomic<int> timelineLength { 8 };     // bars
    std::atomic<bool> useTimeline { false };   // song mode plays the timeline instead of the chain
    std::atomic<int> timelineBar { 0 };        // read-out: bar playing

    /** Fills the timeline from the chain: every track plays each chain scene
        for its repeat count. */
    void copyChainToTimeline();
    void clearTimeline();

    /** Length of what Song mode plays, in bars (chain or timeline). */
    int songLengthBars() const;

    /** The scene track `t` plays in bar `bar` (-1 = silent). With playSong
        false it is always the current scene. */
    int sceneAt (int track, int bar, bool playSong) const;

    /** Scene being played and edited. Setting it while stopped takes effect at
        once; while playing in scene mode, use queueScene(). */
    std::atomic<int> currentScene { 0 };
    std::atomic<int> queuedScene { -1 };
    void queueScene (int index, bool playing);

    // Per-track swing (0 = straight .. 1 = heavy shuffle), set by the tracks.
    std::array<std::atomic<float>, numTracks> swing {};

    // ---------------------------------------------------------- read-outs for the UI
    std::atomic<int> playheadStep { -1 };
    std::atomic<int> chainPosition { 0 };
    std::atomic<int> chainRepeat { 0 };

    // ---------------------------------------------------------- audio thread
    void prepare (double sampleRate);

    /** Adds this block's notes for every track. `isDrumTrack[t]` tells the clock
        how to read track t's pattern. Drum hits are note-ons only.
        Returns false when a non-looping song has reached its end. */
    bool process (std::array<juce::MidiBuffer, numTracks>& out, int numSamples, double bpm, bool playing,
                  const std::array<bool, numTracks>& isDrumTrack);

    void reset();

    /** Audio thread, right after process(): records a note played live at `position` in this block onto the
        nearest 1/16 step (quantised) of the pattern playing. Drum tracks set `drum`'s lane; synth tracks set the step's note. */
    void recordNote (int track, bool isDrum, int note, int drum, int position, int numSamples, double bpm, int velocity = 100);

    /** Audio thread, right after process(): a recorded note was let go, so its step
        gets the length it was held (rounded to a quarter of a step). */
    void recordNoteOff (int track, int note, int position, int numSamples, double bpm);

    /** Where the 1/16 steps began in the last block processed (for the metronome). */
    struct StepStart { int position = 0; int step = 0; };
    int getNumStepStarts() const                 { return numStepStarts; }
    const StepStart& getStepStart (int i) const  { return stepStarts[(size_t) juce::jlimit (0, (int) stepStarts.size() - 1, i)]; }

    /** Audio thread: the next start of play begins this many samples into its first block (after a count-in). */
    void setStartOffset (int samples) { startOffset = juce::jmax (0, samples); }

private:
    bool startBar();   // false = song finished
    void emit (std::array<juce::MidiBuffer, numTracks>& out, int track, int sceneIndex, int stepIndex, int position,
               int numSamples, double samplesPerStep, bool isDrum);

    struct PendingOff { bool active = false; int note = 0; double remaining = 0.0; };
    static constexpr int maxPending = 8;   // notes per track still waiting for their note-off

    void addPendingOff (int track, int note, double remaining);
    void flushPending (juce::MidiBuffer& buffer, int track, int note, int position);   // ends `note` now, if it is waiting

    std::array<std::unique_ptr<Scene>, numScenes> scenes;
    std::array<std::array<PendingOff, maxPending>, numTracks> pending;
    bool firstBar = true;

    double sampleRate = 44100.0;
    double samplesUntilNextStep = 0.0;
    int step = -1;
    bool wasPlaying = false;
    float gate = 0.5f;

    // Swung off-beats: per track, a step delayed into the second half of its slot.
    struct Delayed { bool active = false; int stepIndex = 0; int sceneIndex = 0; double remaining = 0.0; };
    std::array<Delayed, numTracks> delayed;

    // Steps just recorded ahead of the playhead: skipped once, so the live note isn't heard twice.
    std::array<std::array<std::uint16_t, numSteps>, numTracks> skipOnce {};
    int playingScene = 0;   // audio-thread copy of currentScene

    // Notes being recorded and still held: where they landed and when they began, for their length.
    struct Held { bool active = false; int scene = 0; int step = 0; double start = 0.0; };
    std::array<std::array<Held, 128>, numTracks> held {};
    double clock = 0.0;   // samples played since play was pressed (to the end of the last block)
    int startOffset = 0;

    std::array<StepStart, 64> stepStarts {};
    int numStepStarts = 0;
    std::array<int, numTracks> trackScene {};   // scene each track plays this bar (-1 = silent)
};

} // namespace bizgroove
