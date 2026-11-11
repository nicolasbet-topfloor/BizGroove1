#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Song.h"
#include "Track.h"
#include "StemDeck.h"
#include "Mixer.h"
#include "SampleRecorder.h"
#include "PerformFx.h"
#include "Arpeggiator.h"
#include <functional>

namespace bizgroove
{

class ControlTargets;

/** Transport state shared between UI and audio thread. */
struct Transport
{
    std::atomic<bool> playing { false };
    std::atomic<double> bpm { 120.0 };
};

/** The real-time core: live MIDI and the song clock drive 16 tracks, which
    are mixed with the stem deck into the master output.

    It is an AudioSource so the same object serves live playback through an
    AudioSourcePlayer and, later, offline export by calling getNextAudioBlock
    in a loop.
*/
class AudioEngine final : public juce::AudioSource
{
public:
    static constexpr int numTracks = Song::numTracks;

    AudioEngine();

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;

    Transport transport;
    Song song;
    Performance performance;
    StemDeck stems;
    MasterEffects effects;
    PerformSettings perform;   // repeat, stutter and tape stop on the whole mix

    /** Offline rendering for export. While exporting, the live audio device
        must be detached. `onlyTrack` >= 0 renders that track alone (no stem
        deck); -1 renders the full mix; -2 renders only the stem deck. */
    struct RenderJob
    {
        double sampleRate = 48000.0;
        int bars = 4;
        double tailSeconds = 2.0;
        int onlyTrack = -1;
        bool playSong = true;   // false = loop the current scene
    };
    bool renderOffline (const RenderJob& job, juce::AudioBuffer<float>& result,
                        std::function<bool (float progress)> keepGoing = {});

    /** Original files of the four stems, kept for saving projects (message thread). */
    struct SampleFile { juce::String name; juce::MemoryBlock data; };
    std::array<SampleFile, StemDeck::numStems> stemFiles;

    /** True if a track has any steps in any scene (used to skip empty stems). */
    bool trackHasContent (int track);

    Track& track (int index)            { return *tracks[(size_t) juce::jlimit (0, numTracks - 1, index)]; }
    Track& selected()                   { return track (trackOverride >= 0 ? trackOverride : selectedTrack.load()); }

    /** While the LFOs move a track's knobs, "the selected track" means that track, on that thread only. */
    static inline thread_local int trackOverride = -1;

    /** The knobs the LFOs can move: set once by the app (and the tests). */
    std::atomic<const ControlTargets*> lfoTargets { nullptr };

    /** The track the keys, knobs and pads play and edit. */
    std::atomic<int> selectedTrack { 0 };

    /** Feeds external MIDI keyboards (from AudioDeviceManager). */
    juce::MidiMessageCollector midiInput;

    /** Shared with the on-screen keyboard so it lights up on external notes. */
    juce::MidiKeyboardState keyboardState;

    std::atomic<float> masterGain { 0.8f };

    /** The Launchkey pads: true = play the drums (or a Sampler's slices), false = switch steps on and off. */
    std::atomic<bool> padsPlay { true };

    /** Recording what is played into the steps, quantised to 1/16, while the song plays. Stop switches it off. */
    std::atomic<bool> recordNotes { false };
    /** What the controller's eight panel knobs turn: 0 = the synth's eight main knobs;
        1, 2... = that row of eight in the More window, while it is open. */
    std::atomic<int> knobPage { 0 };

    /** Which open window the eight knobs follow (with knobPage as its page), and how many
        pages it has: while one is open, the controller's track arrows turn its pages. */
    enum KnobOverlay { overlayNone = 0, overlayMore, overlayOsc, overlayFx, overlayArp, overlayLfo };
    std::atomic<int> knobOverlay { overlayNone };
    std::atomic<int> knobPages { 0 };

    /** The selected track's sound, after its effects and before its volume, for the
        oscilloscope and spectrum: a ring of the latest samples (audio thread writes). */
    static constexpr int scopeSize = 8192;
    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    double getSampleRate() const { return currentRate; }

    /** Hold: notes keep sounding after you let go of the keys, for long, evolving sounds.
        Playing again once every key is up starts a new held chord; switching it off lets them go. */
    std::atomic<bool> hold { false };

    /** The arpeggiator, on the selected track: Arp on the panel. */
    ArpSettings arp;

    /** The metronome: a click on every beat, a higher one on the first beat of the bar.
        Only heard through the sound card: never in exports or in resampling. */
    enum MetronomeMode { metronomeOff = 0, metronomeOn = 1, metronomeWhileRecording = 2 };
    std::atomic<int> metronomeMode { metronomeWhileRecording };
    std::atomic<float> metronomeLevel { 0.5f };   // 0..1
    std::atomic<int> countInBars { 1 };           // clicks before recording from a stop: 0, 1 or 2 bars

    /** Message thread: starts play after the count-in (at once when it is 0 bars). */
    void startWithCountIn();
    void cancelCountIn() { countInCancel.store (true); }

    /** Read-out: beats left in the count-in; 0 when not counting in. */
    std::atomic<int> countInBeatsLeft { 0 };

    /** Live playback, audio thread, after the output was recorded: adds the metronome's clicks for the block just rendered. */
    void renderMetronome (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    /** Message thread: record from the input (or resample) onto the selected drum pad. */
    std::atomic<bool> recordPadRequested { false };

    /** Records into a Sampler, from the sound card's input or from BizGroove itself. */
    SampleRecorder recorder;

    /** The MIDI each External track sent in the last block, for LiveOutput to pass on to its device. */
    const juce::MidiBuffer& externalMidiFor (int t) const { return externalMidi[(size_t) juce::jlimit (0, numTracks - 1, t)]; }

    /** Live playback, audio thread: the sound card's input for the coming block,
        heard through External tracks that have Hear input on. */
    void setLiveInput (const juce::AudioBuffer<float>& input, int startSample, int numSamples);

    /** Peak level of the last block per channel, for meters. */
    float getPeak (int channel) const { return peaks[(size_t) juce::jlimit (0, 1, channel)].load(); }

    // ---- editing helpers for the selected track, in the current scene
    bool isStepOn (int step);
    void toggleStep (int step);
    int  getStepNote (int step);
    void setStepNote (int step, int note);

private:
    std::array<std::unique_ptr<Track>, numTracks> tracks;
    std::array<juce::MidiBuffer, numTracks> trackMidi;
    juce::AudioBuffer<float> trackBuffer, reverbBus, delayBus;
    std::atomic<int> renderOnlyTrack { -1 };
    juce::MidiBuffer liveMidi;
    std::array<juce::MidiBuffer, numTracks> externalMidi;
    juce::AudioBuffer<float> liveInput;
    int liveInputSamples = 0;   // samples of liveInput valid for this block (0: none)

    // What the vocoders listen to: each track's own sound (before its effects and volume) and
    // every Voice track mixed, kept from the last block, so any track can feed any other.
    juce::AudioBuffer<float> tapNow, tapPrev, modulatorBuffer;
    int tapPrevSamples = 0;
    const float* buildModulator (const VocoderSettings& s, int numSamples);
    void runLfos (int numSamples, bool playing, double bpm);
    double currentRate = 44100.0;
    PerformProcessor performFx;
    juce::int64 freeSamples = 0;   // always running, for effects that move with the beat while stopped

    juce::SmoothedValue<float> smoothedGain;

    // Metronome and count-in (audio thread).
    std::atomic<bool> countInRequest { false }, countInCancel { false };
    double countInRemaining = 0.0, countInNextBeat = 0.0;
    int countInBeat = 0;
    struct Click { int position = 0; bool accent = false; };
    std::array<Click, 32> clicks {};
    int numClicks = 0;
    double clickPhase = 0.0, clickFreq = 1000.0;
    float clickAmp = 0.0f;
    void addClick (int position, bool accent);

    // Hold (audio thread).
    std::array<bool, 128> latched {}, keyDown {};
    int keysDown = 0;
    bool wasHold = false;
    void releaseLatched (juce::MidiBuffer& midi, int position);

    // Arpeggiator (audio thread).
    Arpeggiator arpeggiator;
    juce::MidiBuffer selInput, arpOut, arpScratch;
    bool wasArp = false;
    juce::int64 transportSamples = 0;   // samples since play was pressed
    bool wasPlaying = false;
    int lastSelected = 0;
    std::array<int, numTracks> lastType {};
    std::array<std::atomic<float>, 2> peaks {};
};

} // namespace bizgroove
