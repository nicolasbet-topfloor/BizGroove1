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

    struct Step
    {
        std::atomic<bool> on { false };
        std::atomic<int> note { 48 };
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

private:
    bool startBar();   // false = song finished
    void emit (std::array<juce::MidiBuffer, numTracks>& out, int track, int sceneIndex, int stepIndex, int position,
               int numSamples, double samplesPerStep, bool isDrum);

    struct PendingOff { bool active = false; int note = 0; double remaining = 0.0; };

    std::array<std::unique_ptr<Scene>, numScenes> scenes;
    std::array<PendingOff, numTracks> pending;
    bool firstBar = true;

    double sampleRate = 44100.0;
    double samplesUntilNextStep = 0.0;
    int step = -1;
    bool wasPlaying = false;
    float gate = 0.5f;

    // Swung off-beats: per track, a step delayed into the second half of its slot.
    struct Delayed { bool active = false; int stepIndex = 0; int sceneIndex = 0; double remaining = 0.0; };
    std::array<Delayed, numTracks> delayed;
    int playingScene = 0;   // audio-thread copy of currentScene
    std::array<int, numTracks> trackScene {};   // scene each track plays this bar (-1 = silent)
};

} // namespace bizgroove
