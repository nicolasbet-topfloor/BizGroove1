// Headless checks for the BizGroove audio engine. Exits non-zero on failure.
// Usage: BizGrooveTests [optional/path/to/render.wav]

#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/AudioEngine.h"
#include "engine/ControlTargets.h"
#include "engine/MidiRouter.h"
#include "engine/Project.h"
#include "engine/Presets.h"
#include "engine/SynthLibrary.h"
#include "engine/SampleTools.h"
#include <iostream>
#include <vector>

using namespace bizgroove;

static int failures = 0;

static void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (! condition)
        ++failures;
}

// The song clock must be sample-exact whatever the audio block size.
static void testSequencerTiming (int blockSize)
{
    constexpr double sampleRate = 48000.0, bpm = 120.0;
    const double samplesPerStep = sampleRate * 60.0 / bpm / 4.0; // 6000

    Song song;
    song.clearScene (0);
    for (int i = 0; i < Song::numSteps; ++i)
        song.scene (0).tracks[0].steps[(size_t) i].on.store (true);
    song.prepare (sampleRate);

    std::array<juce::MidiBuffer, Song::numTracks> out;
    std::array<bool, Song::numTracks> drumsFlags {};
    std::vector<juce::int64> noteOnTimes;
    int noteOffs = 0;
    juce::int64 blockStart = 0;
    const juce::int64 total = (juce::int64) (samplesPerStep * 32); // two bars

    while (blockStart < total)
    {
        for (auto& m : out) m.clear();
        song.process (out, blockSize, bpm, true, drumsFlags);
        for (const auto meta : out[0])
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())  noteOnTimes.push_back (blockStart + meta.samplePosition);
            if (m.isNoteOff()) ++noteOffs;
        }
        blockStart += blockSize;
    }

    bool exact = noteOnTimes.size() >= 32;
    for (size_t i = 0; i < 32 && exact; ++i)
        exact = noteOnTimes[i] == (juce::int64) std::ceil (i * samplesPerStep);

    const auto b = juce::String (blockSize);
    check (noteOnTimes.size() >= 32, "block " + b + ": 32 notes in two bars");
    check (exact, "block " + b + ": every note starts on its exact sample");
    check (noteOffs >= 31, "block " + b + ": notes are released");
}

// Scenes switch on the bar line; the song chain plays scenes with repeats.
static void testScenesAndSong()
{
    constexpr double sr = 48000.0, bpm = 120.0;
    constexpr int block = 480;                 // divides a bar exactly, so bars end on block edges
    const int barSamples = (int) (sr * 2.0);   // one bar at 120 BPM

    Song song;
    for (int s = 0; s < 3; ++s)
    {
        song.clearScene (s);
        // Scene s plays note 60 + s on step 1 of track 1.
        song.scene (s).tracks[0].steps[0].on.store (true);
        song.scene (s).tracks[0].steps[0].note.store (60 + s);
    }
    song.prepare (sr);

    std::array<juce::MidiBuffer, Song::numTracks> out;
    std::array<bool, Song::numTracks> drumsFlags {};

    // Render bar by bar, collecting the note heard at the start of each bar.
    auto playBars = [&] (int bars, std::vector<int>& notes, juce::int64& t, bool& ended) {
        const juce::int64 end = t + (juce::int64) bars * barSamples;
        while (t < end)
        {
            for (auto& m : out) m.clear();
            if (! song.process (out, block, bpm, true, drumsFlags)) { ended = true; return; }
            for (const auto meta : out[0])
                if (meta.getMessage().isNoteOn())
                    notes.push_back (meta.getMessage().getNoteNumber());
            t += block;
        }
    };

    // Scene mode: queue scene 2 mid-bar; it must start only at the next bar.
    std::vector<int> notes;
    juce::int64 t = 0;
    bool ended = false;
    playBars (1, notes, t, ended);
    song.queueScene (1, true);
    check (song.currentScene.load() == 0, "scenes: a queued scene waits for the bar line");
    playBars (1, notes, t, ended);
    check (notes.size() >= 2 && notes[0] == 60 && notes[1] == 61, "scenes: the queued scene starts on the next bar");

    // Song mode: scene 1 twice, then scene 3 once, no loop -> 60, 60, 62, then stop.
    song.chain[0].scene.store (0); song.chain[0].repeats.store (2);
    song.chain[1].scene.store (2); song.chain[1].repeats.store (1);
    song.chainLength.store (2);
    song.songMode.store (true);
    song.loopSong.store (false);
    song.reset();
    notes.clear();
    t = 0;
    ended = false;
    playBars (5, notes, t, ended);
    check (notes.size() == 3 && notes[0] == 60 && notes[1] == 60 && notes[2] == 62,
           "song: the chain plays scenes with their repeat counts");
    check (ended, "song: a song without loop stops at its end");

    // With loop on, it starts again.
    song.loopSong.store (true);
    song.reset();
    notes.clear();
    t = 0;
    ended = false;
    playBars (4, notes, t, ended);
    check (! ended && notes.size() == 4 && notes[3] == 60, "song: a looping song starts again");

    // Copying a scene copies every track's pattern.
    song.copyScene (2, 5);
    check (song.scene (5).tracks[0].steps[0].note.load() == 62 && ! song.sceneIsEmpty (5), "scenes: copy scene");
}

// The 16 drums all sound, stop by themselves, choke and swing work.
static void testDrums()
{
    constexpr double sr = 48000.0;
    DrumKit kit;
    kit.prepare (sr);

    bool allSound = true, allStop = true, allFinite = true;
    for (int d = 0; d < DrumKit::numDrums; ++d)
    {
        kit.prepare (sr);
        juce::AudioBuffer<float> buf (2, (int) (sr * 4.0));
        buf.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, Song::drumBaseNote + d, (juce::uint8) 120), 0);
        kit.render (buf, 0, buf.getNumSamples(), midi);
        const float peak = buf.getMagnitude (0, buf.getNumSamples());
        for (int i = 0; i < buf.getNumSamples() && allFinite; ++i)
            allFinite = std::isfinite (buf.getSample (0, i));
        if (peak < 0.02f || peak > 1.5f)
        {
            std::cout << "  info  " << DrumKit::drumName (d) << " peak " << peak << "\n";
            allSound = false;
        }
        if (kit.isSounding()) allStop = false;
    }
    check (allSound, "drums: all 16 drums make sound at a sensible level");
    check (allStop, "drums: every drum fades out by itself");
    check (allFinite, "drums: no NaN or infinite samples");
    check (DrumKit::drumForNote (36) == 0 && DrumKit::drumForNote (51) == 15 && DrumKit::drumForNote (60) == 0,
           "drums: General MIDI order, kick on C (notes 36 and 60)");

    // Choke: a closed hi-hat cuts the open hi-hat within 50 ms.
    kit.prepare (sr);
    juce::AudioBuffer<float> buf (2, (int) (sr * 0.3));
    buf.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 46, (juce::uint8) 120), 0);       // open hi-hat
    midi.addEvent (juce::MidiMessage::noteOn (1, 42, (juce::uint8) 120), 4800);    // closed at 100 ms
    kit.render (buf, 0, buf.getNumSamples(), midi);
    const float tail = buf.getMagnitude (0, (int) (sr * 0.2), (int) (sr * 0.1));
    check (tail < 0.01f, "drums: closed hi-hat chokes the open hi-hat");

    // Swing on a drum track delays the off-beat hits only.
    Song song;
    song.clearScene (0);
    song.scene (0).tracks[1].drumLanes[0].store (0xffff);   // kick on every step of track 2
    song.swing[1].store (1.0f);
    song.prepare (sr);
    std::array<juce::MidiBuffer, Song::numTracks> out;
    std::array<bool, Song::numTracks> drumsFlags {};
    drumsFlags[1] = true;
    std::vector<juce::int64> hits;
    juce::int64 pos = 0;
    while (pos < 24000)
    {
        for (auto& m : out) m.clear();
        song.process (out, 256, 120.0, true, drumsFlags);
        for (const auto meta : out[1])
            hits.push_back (pos + meta.samplePosition);
        pos += 256;
    }
    // Steps 6000 apart; full swing pushes every second one by half a step (3000).
    check (hits.size() >= 4 && hits[0] == 0 && hits[1] == 9000 && hits[2] == 12000 && hits[3] == 21000,
           "drums: swing pushes the off-beats later");
}

// Tracks: live notes go to the selected track; mute silences a track.
static void testTracks()
{
    AudioEngine engine;
    engine.prepareToPlay (256, 48000.0);
    engine.song.clearScene (0);
    juce::AudioBuffer<float> buf (2, 256);

    auto renderPeak = [&] (int blocks) {
        float peak = 0.0f;
        for (int i = 0; i < blocks; ++i)
        {
            buf.clear();
            engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
            peak = juce::jmax (peak, buf.getMagnitude (0, 256));
        }
        return peak;
    };

    engine.selectedTrack.store (4);
    engine.keyboardState.noteOn (1, 60, 1.0f);
    renderPeak (20);
    check (engine.track (4).level.load() > 0.001f && engine.track (0).level.load() < 1.0e-6f,
           "tracks: the keys play the selected track only");
    engine.track (4).mute.store (true);
    renderPeak (20);
    check (engine.track (4).level.load() < 1.0e-4f, "tracks: mute silences a track");
    engine.keyboardState.noteOff (1, 60, 0.0f);

    // On a drum track a key picks the drum the pads edit.
    engine.selectedTrack.store (1);
    engine.keyboardState.noteOn (1, 38, 1.0f);    // snare
    renderPeak (2);
    engine.keyboardState.noteOff (1, 38, 0.0f);
    check (engine.track (1).selectedDrum.load() == 2, "tracks: a key selects the drum on a drum track");
    engine.toggleStep (3);
    check ((engine.song.scene (0).tracks[1].drumLanes[2].load() & (1u << 3)) != 0,
           "tracks: pads edit the selected drum's steps");
}

// Controller profiles route MIDI to targets; unmapped keys still play.
static void testControllerMapping()
{
    AudioEngine engine;
    engine.prepareToPlay (256, 48000.0);
    ControlTargets targets (engine);
    MidiRouter router (engine, targets);

    // Every built-in binding must point at a real target.
    bool allValid = true;
    for (const auto& p : ControllerProfile::builtIns())
        for (const auto& b : p.bindings)
            if (b.target.isNotEmpty() && targets.find (b.target) == nullptr)
            {
                std::cout << "  info  unknown target " << b.target << " in " << p.name << "\n";
                allValid = false;
            }
    check (allValid, "mapping: built-in profiles only use known targets");

    // An unknown device falls back to the Launchkey Mini profile.
    check (router.resolveProfileName ("id-x", "Some USB Keyboard") == ControllerProfile::defaultProfileName,
           "mapping: Launchkey Mini is the default profile");
    check (router.resolveProfileName ("id-mk3", "Launchkey Mini MK3 MIDI 1") == "Novation Launchkey Mini MK3",
           "mapping: a Launchkey Mini MK3 is recognised by name");
    check (router.resolveProfileName ("id-lk1", "Launchkey Mini") == "Novation Launchkey Mini (original)",
           "mapping: an original Launchkey Mini is recognised by name");
    check (router.resolveProfileName ("id-f1", "Traktor Kontrol F1 - 1") == "Native Instruments Traktor Kontrol F1",
           "mapping: a Kontrol F1 is recognised by name");

    // Launchkey knob 1 (CC 21, ch 1) drives the cutoff.
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::controllerEvent (1, 21, 127));
    check (std::abs (engine.track (0).synthParams.cutoffHz.load() - 16000.0f) < 1.0f, "mapping: Launchkey knob 1 sets cutoff");

    // In steps mode, the Launchkey top-left pad (note 40, ch 10) toggles step 1.
    engine.padsPlay.store (false);
    const bool before = engine.isStepOn (0);
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::noteOn (10, 40, (juce::uint8) 100));
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::noteOff (10, 40));
    check (engine.isStepOn (0) != before, "mapping: Launchkey pad toggles a step once per press");
    engine.padsPlay.store (true);
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::noteOn (10, 40, (juce::uint8) 100));
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::noteOff (10, 40));
    check (engine.isStepOn (0) != before, "mapping: in play mode the pads leave the steps alone");

    // Original Launchkey Mini: top round button (CC 108) plays and stops; arrow up nudges tempo.
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::controllerEvent (1, 108, 127));
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::controllerEvent (1, 108, 0));
    const bool roundPlays = engine.transport.playing.load();
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::controllerEvent (1, 108, 127));
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::controllerEvent (1, 108, 0));
    check (roundPlays && ! engine.transport.playing.load(), "mapping: Launchkey round button toggles play");
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::controllerEvent (1, 104, 127));
    check (std::abs (engine.transport.bpm.load() - 121.0) < 0.01, "mapping: Launchkey arrow up adds 1 BPM");

    // A key on channel 1 that shares a pad's note number still plays, not toggles.
    const bool step5 = engine.isStepOn (4);
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100));
    check (engine.isStepOn (4) == step5, "mapping: keys never toggle steps");

    // MIDI Start/Stop drive the transport.
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::midiStart());
    check (engine.transport.playing.load(), "mapping: MIDI Start plays");
    router.route ("lk", "Launchkey Mini", juce::MidiMessage::midiStop());
    check (! engine.transport.playing.load(), "mapping: MIDI Stop stops");

    // Learning: the next CC is stored in the F1 profile, then drives stem 1 volume.
    const juce::String f1 = "Native Instruments Traktor Kontrol F1";
    int faderRow = -1;
    for (const auto& p : router.getProfiles())
        if (p.name == f1)
            for (size_t i = 0; i < p.bindings.size(); ++i)
                if (p.bindings[i].target == "stem.1.volume")
                    faderRow = (int) i;
    router.startLearn (f1, faderRow, [] (MidiTrigger) {});
    router.route ("f1", "Traktor Kontrol F1", juce::MidiMessage::controllerEvent (13, 10, 64)); // learned, not applied
    router.route ("f1", "Traktor Kontrol F1", juce::MidiMessage::controllerEvent (13, 10, 0));
    check (engine.stems.stem (0).volume.load() < 0.01f, "mapping: learned F1 fader drives stem 1 volume");

    // Momentary roll: on while held.
    targets.applyButton ("stem.2.roll4", true);
    const bool held = engine.stems.stem (1).roll.load() == 4;
    targets.applyButton ("stem.2.roll4", false);
    check (held && engine.stems.stem (1).roll.load() == 0, "mapping: roll is on only while held");

    // Pitch bend and modulation reach the synth from any keyboard.
    {
        juce::AudioBuffer<float> buf (2, 256);
        router.route ("mk3", "Launchkey Mini MK3 MIDI 1", juce::MidiMessage::pitchWheel (1, 16383));
        router.route ("mk3", "Launchkey Mini MK3 MIDI 1", juce::MidiMessage::controllerEvent (1, 1, 127));
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
        check (engine.performance.pitchBend.load() > 0.99f, "mapping: pitch bend reaches the synth");
        check (engine.performance.modWheel.load() > 0.99f, "mapping: modulation strip reaches the synth");
    }

    // Panel knob 1 follows the loaded synth (test synth: Brightness = cutoff).
    targets.applyValue ("panel.knob.1", 0.0f);
    check (std::abs (engine.track (0).synthParams.cutoffHz.load() - 40.0f) < 0.5f, "mapping: panel knob 1 drives the loaded synth");
    check (targets.describe ("synth.attack").isNotEmpty() && targets.describe ("synth.attack") != "0.003 s",
           "mapping: values are shown in plain words (" + targets.describe ("synth.attack") + ")");

    // Profiles survive a JSON round trip.
    const auto original = ControllerProfile::kontrolF1();
    const auto copy = ControllerProfile::fromVar (juce::JSON::parse (juce::JSON::toString (original.toVar())));
    check (copy.name == original.name && copy.bindings.size() == original.bindings.size()
               && copy.bindings[5].target == original.bindings[5].target,
           "mapping: profile JSON round trip");
}

// Stems loop in time with the transport and respond to mute, solo and roll.
static void testStemDeck()
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    StemDeck deck;
    deck.prepare (sr, block);

    // A 1-bar click at 120 BPM: one impulse per beat (24000 samples).
    const int barLength = (int) (sr * 2.0);
    juce::AudioBuffer<float> click (1, barLength);
    click.clear();
    for (int beat = 0; beat < 4; ++beat)
        click.setSample (0, beat * 24000, 1.0f);
    deck.setStemAudio (0, click, sr, "click", 120.0);
    check (deck.getStemBars (0) == 1, "stems: a 2-second loop at 120 BPM is 1 bar");
    check (StemDeck::barsForLength (8.1, 120.0) == 4, "stems: lengths snap to musical bar counts");

    auto render = [&] (int numSamples, juce::int64& t, double bpm, std::vector<int>* hits) {
        juce::AudioBuffer<float> out (2, block);
        for (int pos = 0; pos < numSamples; pos += block)
        {
            out.clear();
            deck.process (out, 0, block, t, true, bpm);
            if (hits != nullptr)
                for (int i = 0; i < block; ++i)
                    if (out.getSample (0, i) > 0.1f && (hits->empty() || (int) (t + i) - hits->back() > 100))
                        hits->push_back ((int) (t + i));
            t += block;
        }
    };

    std::vector<int> hits;
    juce::int64 t = 0;
    render (barLength * 2, t, 120.0, &hits);
    bool onBeat = hits.size() == 8;
    if (! onBeat) { std::cout << "  info  hits:"; for (auto h : hits) std::cout << " " << h; std::cout << "\n"; }
    for (size_t i = 0; i < hits.size() && onBeat; ++i)
        onBeat = std::abs (hits[i] - (int) i * 24000) <= 1;
    check (onBeat, "stems: clicks land on every beat over two loops");

    // At 100 BPM the loop is repitched to still fill one bar (28800 samples per beat).
    deck.prepare (sr, block);
    hits.clear();
    t = 0;
    render ((int) (sr * 2.4) * 2, t, 100.0, &hits);
    bool synced = hits.size() >= 8;
    if (! synced) { std::cout << "  info  hits:"; for (auto h : hits) std::cout << " " << h; std::cout << "\n"; }
    for (size_t i = 0; i < 8 && synced; ++i)
        synced = std::abs (hits[i] - (int) i * 28800) <= 2;
    check (synced, "stems: loops stay on the beat at a different tempo");

    // Mute and solo.
    deck.stem (0).mute.store (true);
    hits.clear();
    render (barLength, t, 120.0, &hits);
    check (hits.empty(), "stems: mute silences a stem");
    deck.stem (0).mute.store (false);

    deck.setStemAudio (1, click, sr, "click2", 120.0);
    deck.stem (1).solo.store (true);
    juce::AudioBuffer<float> out (2, block);
    float stem1Level = 1.0f;
    for (int i = 0; i < 400; ++i)
    {
        out.clear();
        deck.process (out, 0, block, t, true, 120.0);
        t += block;
        stem1Level = juce::jmin (stem1Level, deck.stem (0).level.load());
    }
    check (stem1Level < 1.0e-4f, "stems: solo on stem 2 silences stem 1");
    deck.stem (1).solo.store (false);
    deck.clearStem (1);

    // Roll 1/16 at 120 BPM repeats every 6000 samples while held.
    deck.prepare (sr, block);
    t = 0;
    render (24000, t, 120.0, nullptr);        // play just past beat 2 (blocks end at 24064)
    deck.stem (0).roll.store (4);
    hits.clear();
    render (24000, t, 120.0, &hits);          // the 1/16 slice holding beat 2 repeats
    bool rolling = hits.size() == 4;
    if (! rolling) { std::cout << "  info  hits:"; for (auto h : hits) std::cout << " " << h; std::cout << "\n"; }
    for (size_t i = 0; i < hits.size() && rolling; ++i)
        rolling = std::abs (hits[i] - (30000 + (int) i * 6000)) <= 1;
    check (rolling, "stems: 1/16 roll repeats on the grid");
    deck.stem (0).roll.store (0);
    hits.clear();
    render (24000, t, 120.0, &hits);          // after release it rejoins the grid: next beat at 72000
    check (hits.size() == 1 && std::abs (hits[0] - 72000) <= 1, "stems: after a roll the stem is back in sync");
}


// A test recording: a click every 1000 samples, 48 kHz, 1 second, rising in level.
static juce::MemoryBlock makeTestWav (double sr = 48000.0)
{
    juce::AudioBuffer<float> b (1, (int) sr);
    b.clear();
    for (int i = 0; i < b.getNumSamples(); i += 1000)
        b.setSample (0, i, 0.2f + 0.8f * (float) i / (float) b.getNumSamples());
    juce::MemoryBlock block;
    {
        auto* stream = new juce::MemoryOutputStream (block, false);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream, sr, 1, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    return block;
}

static void testSamplerAndMixer()
{
    constexpr double sr = 48000.0;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    AudioEngine engine;
    engine.prepareToPlay (256, sr);
    engine.song.clearScene (0);
    auto& tr = engine.track (3);
    tr.type.store (Track::sampler);
    juce::String err;
    const bool loaded = Project::loadSampleIntoTrack (engine, 3, "clicks.wav", makeTestWav(), formats, err);
    check (loaded && tr.samplerEngine.hasSample(), "sampler: loads a WAV recording" + (err.isEmpty() ? juce::String() : " (" + err + ")"));

    // Render the sampler alone for 0.5 s, return the click positions.
    float firstPeak = 0.0f;
    auto clicks = [&] (int note) {
        Sampler s;
        s.params.start.store (tr.samplerEngine.params.start.load());
        s.params.reverse.store (tr.samplerEngine.params.reverse.load());
        s.params.slices.store (tr.samplerEngine.params.slices.load());
        juce::AudioBuffer<float> a; double rate = 0; juce::String e2;
        Project::decodeAudio (makeTestWav(), formats, a, rate, e2);
        s.setSample (a, rate, "t");
        s.prepare (sr, 512);
        juce::AudioBuffer<float> out (2, 24000);
        out.clear();
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
        s.render (out, 0, out.getNumSamples(), m);
        std::vector<int> hits;
        for (int i = 0; i < out.getNumSamples(); ++i)
            if (std::abs (out.getSample (0, i)) > 0.1f && (hits.empty() || i - hits.back() > 50))
                hits.push_back (i);
        firstPeak = hits.empty() ? 0.0f : std::abs (out.getSample (0, hits[0]));
        return hits;
    };

    auto root = clicks (60);
    check (root.size() >= 3 && std::abs ((root[1] - root[0]) - 1000) <= 1, "sampler: C4 plays at the original pitch");
    auto octave = clicks (72);
    check (octave.size() >= 3 && std::abs ((octave[1] - octave[0]) - 500) <= 1, "sampler: an octave up plays twice as fast");

    const float earlyLevel = firstPeak;               // clicks get louder through the recording
    tr.samplerEngine.params.start.store (24000.0f / 47999.0f);   // exactly sample 24000, half-way
    clicks (60);
    std::cout << "  info  first click level: from the start " << earlyLevel << ", from half-way " << firstPeak << "\n";
    check (earlyLevel < 0.3f && firstPeak > 0.5f, "sampler: Start skips into the recording");
    tr.samplerEngine.params.start.store (0.0f);

    // Mixer: pan hard left silences the right channel.
    engine.selectedTrack.store (0);
    engine.track (0).mix.pan.store (-1.0f);
    engine.keyboardState.noteOn (1, 60, 1.0f);
    juce::AudioBuffer<float> buf (2, 256);
    float left = 0, right = 0;
    for (int i = 0; i < 20; ++i)   // the pan glides for 20 ms; measure after that
    {
        buf.clear();
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
        left = buf.getMagnitude (0, 0, 256);
        right = buf.getMagnitude (1, 0, 256);
    }
    check (left > 0.01f && right < 1.0e-4f, "mixer: pan hard left puts the sound on the left only");

    // Solo: soloing track 4 silences track 1.
    engine.track (0).mix.pan.store (0.0f);
    engine.track (3).mix.solo.store (true);
    for (int i = 0; i < 20; ++i)
    {
        buf.clear();
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
    }
    check (engine.track (0).level.load() < 1.0e-4f, "mixer: solo on another track silences this one");
    engine.track (3).mix.solo.store (false);
    engine.keyboardState.noteOff (1, 60, 0.0f);

    // Reverb send: sound continues after the note when reverb is up.
    engine.prepareToPlay (256, sr);
    engine.track (0).mix.reverb.store (1.0f);
    engine.keyboardState.noteOn (1, 60, 1.0f);
    for (int i = 0; i < 20; ++i) { buf.clear(); engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256)); }
    engine.keyboardState.noteOff (1, 60, 0.0f);
    float tail = 0;
    for (int i = 0; i < 150; ++i)   // 0.8 s later: the synth has stopped, the reverb has not
    {
        buf.clear();
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
        if (i > 140) tail = juce::jmax (tail, buf.getMagnitude (0, 0, 256));
    }
    check (tail > 1.0e-4f, "mixer: the reverb send rings on after the note");
}

static void testTimelineProjectExport()
{
    constexpr double sr = 48000.0;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    // Timeline: track 1 plays scene 2 in bar 2 only.
    Song song;
    for (int s = 0; s < 2; ++s)
    {
        song.clearScene (s);
        song.scene (s).tracks[0].steps[0].on.store (true);
        song.scene (s).tracks[0].steps[0].note.store (60 + s);
    }
    song.clearTimeline();
    song.timeline[0][0].store (0);
    song.timeline[0][1].store (1);
    song.timeline[0][2].store (-1);
    song.timelineLength.store (3);
    song.useTimeline.store (true);
    song.songMode.store (true);
    song.loopSong.store (false);
    song.prepare (sr);
    std::array<juce::MidiBuffer, Song::numTracks> out;
    std::array<bool, Song::numTracks> flags {};
    std::vector<int> notes;
    bool ended = false;
    for (int i = 0; i < 1000 && ! ended; ++i)
    {
        for (auto& m : out) m.clear();
        ended = ! song.process (out, 480, 120.0, true, flags);
        for (const auto meta : out[0])
            if (meta.getMessage().isNoteOn()) notes.push_back (meta.getMessage().getNoteNumber());
    }
    check (notes.size() == 2 && notes[0] == 60 && notes[1] == 61 && ended,
           "timeline: tracks play the scene placed in each bar, silent bars stay silent");

    song.chain[0].scene.store (1); song.chain[0].repeats.store (2);
    song.chain[1].scene.store (0); song.chain[1].repeats.store (1);
    song.chainLength.store (2);
    song.copyChainToTimeline();
    check (song.timelineLength.load() == 3 && song.timeline[5][0].load() == 1 && song.timeline[5][2].load() == 0,
           "timeline: copying the song chain fills the timeline");

    // Project: save, change, open -> the same settings and the sample come back.
    AudioEngine a;
    a.prepareToPlay (256, sr);
    a.transport.bpm.store (97.0);
    a.track (5).type.store (Track::sampler);
    a.track (5).mix.eqHigh.store (4.5f);
    a.track (1).kit.drums[2].tone.store (0.9f);
    a.song.scene (3).tracks[7].steps[9].on.store (true);
    a.song.chainLength.store (2);
    juce::String err;
    Project::loadSampleIntoTrack (a, 5, "clicks.wav", makeTestWav(), formats, err);
    const auto before = Project::fingerprint (a);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bizgroove-test.bizgroove");
    const bool saved = Project::save (a, file, err);

    AudioEngine b;
    b.prepareToPlay (256, sr);
    const bool opened = Project::open (b, file, formats, err);
    check (saved && opened && Project::fingerprint (b) == before, "project: save and open bring back every setting");
    check (b.track (5).samplerEngine.hasSample() && b.track (5).sampleFileName == "clicks.wav",
           "project: the sampler's recording is stored inside the project file");
    file.deleteFile();

    // Export: an offline render of 2 bars at 120 BPM with a 1 s tail is 5 s long and not silent.
    AudioEngine c;
    juce::AudioBuffer<float> mix;
    AudioEngine::RenderJob job;
    job.sampleRate = sr; job.bars = 2; job.tailSeconds = 1.0; job.playSong = false;
    const bool rendered = c.renderOffline (job, mix);
    check (rendered && mix.getNumSamples() == (int) (5.0 * sr) && mix.getRMSLevel (0, 0, mix.getNumSamples()) > 0.005f,
           "export: offline render has the right length and makes sound");

    // One track alone: track 2 (drums) without track 1.
    juce::AudioBuffer<float> drumsOnly;
    job.onlyTrack = 1;
    c.renderOffline (job, drumsOnly);
    check (drumsOnly.getRMSLevel (0, 0, drumsOnly.getNumSamples()) > 0.001f
               && drumsOnly.getRMSLevel (0, 0, drumsOnly.getNumSamples()) < mix.getRMSLevel (0, 0, mix.getNumSamples()),
           "export: a single track renders on its own");

    const auto wavFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bizgroove-test.wav");
    check (Exporter::writeWav (mix, sr, 24, wavFile, err) && wavFile.getSize() > 100000, "export: writes a 24-bit WAV");
    wavFile.deleteFile();

    const auto midiFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bizgroove-test.mid");
    const bool midiOk = Exporter::writeMidi (c, false, 2, midiFile, err);
    juce::MidiFile readBack;
    juce::FileInputStream in (midiFile);
    const bool parsed = in.openedOk() && readBack.readFrom (in);
    int noteOns = 0;
    for (int t = 0; t < readBack.getNumTracks(); ++t)
        for (const auto* ev : *readBack.getTrack (t))
            if (ev->message.isNoteOn()) ++noteOns;
    // Default scene: 10 bass notes + 4 kicks + 2 snares + 4 hats per bar, two bars.
    check (midiOk && parsed && noteOns == 2 * (10 + 4 + 2 + 4), "export: MIDI file holds every note (" + juce::String (noteOns) + ")");
    midiFile.deleteFile();
}

static void testRender (const juce::File& wavOut)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    const int totalSamples = (int) (sampleRate * 8.0); // 4 bars at 120 BPM

    AudioEngine engine;
    engine.prepareToPlay (blockSize, sampleRate);
    engine.transport.playing.store (true);

    juce::AudioBuffer<float> out (2, totalSamples);
    out.clear();
    juce::AudioBuffer<float> block (2, blockSize);

    for (int pos = 0; pos < totalSamples; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, totalSamples - pos);
        block.clear();
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&block, 0, n));
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, block, ch, 0, n);
    }

    bool finite = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < totalSamples && finite; ++i)
            finite = std::isfinite (out.getSample (ch, i));

    const float peak = out.getMagnitude (0, totalSamples);
    const float rms = out.getRMSLevel (0, 0, totalSamples);
    std::cout << "  info  peak " << juce::Decibels::gainToDecibels (peak) << " dBFS, rms "
              << juce::Decibels::gainToDecibels (rms) << " dBFS\n";

    check (finite, "render: no NaN or infinite samples");
    check (rms > 0.005f, "render: default pattern makes sound");
    check (peak < 1.0f, "render: output does not clip");

    // Stop: after the release tail the output must fall silent.
    engine.transport.playing.store (false);
    float tailPeak = 0.0f;
    for (int i = 0; i < (int) (sampleRate * 1.5) / blockSize; ++i)
    {
        block.clear();
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&block, 0, blockSize));
        tailPeak = block.getMagnitude (0, blockSize);
    }
    check (tailPeak < 1.0e-4f, "render: silence after stop");

    if (wavOut != juce::File())
    {
        wavOut.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = wavOut.createOutputStream())
        {
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0));
            if (writer != nullptr)
            {
                stream.release(); // the writer owns it now
                writer->writeFromAudioSampleBuffer (out, 0, totalSamples);
                std::cout << "  info  wrote " << wavOut.getFullPathName() << "\n";
            }
        }
    }
}


// Builds a demo project (used for the manual's screenshots): bass, drums,
// a sampler chord, two scenes on the timeline, and two stems.
static juce::MemoryBlock wavOf (const juce::AudioBuffer<float>& b, double sr)
{
    juce::MemoryBlock block;
    auto* stream = new juce::MemoryOutputStream (block, false);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream, sr, (unsigned int) b.getNumChannels(), 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    w.reset();
    return block;
}

static int makeDemo (const juce::File& file)
{
    constexpr double sr = 48000.0;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    AudioEngine e;
    e.transport.bpm.store (120.0);

    // Track 3: Sampler with a soft chord (C, E, G), two seconds.
    juce::AudioBuffer<float> chord (2, (int) (sr * 2.0));
    for (int i = 0; i < chord.getNumSamples(); ++i)
    {
        const double t = i / sr, env = std::exp (-t * 1.5);
        const float v = (float) (0.2 * env * (std::sin (juce::MathConstants<double>::twoPi * 261.63 * t)
                                              + std::sin (juce::MathConstants<double>::twoPi * 329.63 * t)
                                              + std::sin (juce::MathConstants<double>::twoPi * 392.0 * t)));
        chord.setSample (0, i, v);
        chord.setSample (1, i, v);
    }
    e.track (2).type.store (Track::sampler);
    juce::String err;
    Project::loadSampleIntoTrack (e, 2, "soft chord.wav", wavOf (chord, sr), formats, err);
    for (int st : { 0, 8 })
    {
        e.song.scene (0).tracks[2].steps[(size_t) st].on.store (true);
        e.song.scene (0).tracks[2].steps[(size_t) st].note.store (60);
    }
    e.track (2).mix.reverb.store (0.45f);
    e.track (2).mix.pan.store (-0.25f);

    // Scene 2: scene 1 plus more hi-hats and a clap.
    e.song.copyScene (0, 1);
    e.song.scene (1).tracks[1].drumLanes[6].store (0x5555);
    e.song.scene (1).tracks[1].drumLanes[3].store ((std::uint16_t) ((1u << 4) | (1u << 12)));
    e.song.scene (1).tracks[2].steps[4].on.store (true);
    e.song.scene (1).tracks[2].steps[4].note.store (67);

    // The song: scene 1 twice, scene 2 twice, then on the timeline.
    e.song.chain[0].scene.store (0); e.song.chain[0].repeats.store (2);
    e.song.chain[1].scene.store (1); e.song.chain[1].repeats.store (2);
    e.song.chainLength.store (2);
    e.song.copyChainToTimeline();
    e.song.timeline[2][0].store (-1);   // the sampler waits a bar
    e.song.useTimeline.store (true);

    e.track (0).mix.eqLow.store (3.0f);
    e.track (1).mix.delay.store (0.15f);
    e.track (1).mix.reverb.store (0.2f);

    // Two stems: a pad loop and a shaker loop, one bar each.
    juce::AudioBuffer<float> pad (2, (int) (sr * 2.0)), shaker (2, (int) (sr * 2.0));
    juce::Random rnd (7);
    for (int i = 0; i < pad.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const float p = (float) (0.15 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * t) * (0.6 + 0.4 * std::sin (t * 3.0)));
        pad.setSample (0, i, p); pad.setSample (1, i, p);
        const double beat = std::fmod (t, 0.25);
        const float sh = (float) (0.2 * std::exp (-beat * 40.0) * (rnd.nextFloat() * 2.0 - 1.0));
        shaker.setSample (0, i, sh); shaker.setSample (1, i, sh);
    }
    Project::loadStem (e, 0, "pad loop.wav", wavOf (pad, sr), formats, err);
    Project::loadStem (e, 1, "shaker loop.wav", wavOf (shaker, sr), formats, err);

    if (! Project::save (e, file, err))
    {
        std::cout << "demo: " << err << "\n";
        return 1;
    }
    std::cout << "demo written: " << file.getFullPathName() << "\n";
    return 0;
}

// The program bank: a sound saved from one track loads into another unchanged.
static void testPresets()
{
    std::cout << "Program bank\n";
    AudioEngine e;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::String err;

    auto& synthTrack = e.track (0);
    synthTrack.type.store (Track::testSynth);
    synthTrack.synthParams.cutoffHz.store (321.0f);
    synthTrack.synthParams.sustain.store (0.77f);
    juce::TemporaryFile synthFile (Presets::fileExtension);
    check (Presets::save (synthTrack, synthFile.getFile(), err), "a synth sound saves");
    check (Presets::load (e, 1, synthFile.getFile(), formats, err), "it loads into another track");
    check (e.track (1).type.load() == Track::testSynth
           && std::abs (e.track (1).synthParams.cutoffHz.load() - 321.0f) < 0.01f
           && std::abs (e.track (1).synthParams.sustain.load() - 0.77f) < 0.001f, "with the same settings");

    auto& kitTrack = e.track (2);
    kitTrack.type.store (Track::drums);
    kitTrack.kit.drums[3].tone.store (0.12f);
    Presets::applySoundVar (e.track (3), Presets::soundToVar (kitTrack));
    check (e.track (3).type.load() == Track::drums && std::abs (e.track (3).kit.drums[3].tone.load() - 0.12f) < 0.001f,
           "a drum kit carries over");

    constexpr double sr = 48000.0;
    juce::AudioBuffer<float> tone (2, 24000);
    for (int i = 0; i < tone.getNumSamples(); ++i)
    {
        const float s = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / sr);
        tone.setSample (0, i, s);
        tone.setSample (1, i, s);
    }
    check (Project::loadSampleIntoTrack (e, 4, "tone.wav", wavOf (tone, sr), formats, err), "a recording loads into a sampler");
    e.track (4).type.store (Track::sampler);
    e.track (4).samplerEngine.params.slices.store (8);
    juce::TemporaryFile samplerFile (Presets::fileExtension);
    check (Presets::save (e.track (4), samplerFile.getFile(), err), "a sampler sound saves with its recording");
    check (Presets::load (e, 5, samplerFile.getFile(), formats, err) && e.track (5).samplerEngine.hasSample()
           && e.track (5).type.load() == Track::sampler && e.track (5).samplerEngine.params.slices.load() == 8,
           "and brings the recording with it");

    for (int type = Track::testSynth; type <= Track::sampler; ++type)
        check (! Presets::factorySounds (type).empty(), Presets::instrumentName (type) + " has factory sounds");
}

// Recording, trimming, chopping and the pads for the Sampler.
static void testSampling()
{
    std::cout << "Sampling\n";
    constexpr double sr = 48000.0;

    // The recorder copies blocks only while recording, and only from the chosen source.
    SampleRecorder rec;
    rec.deviceRate.store (sr);
    rec.start();
    juce::AudioBuffer<float> block (2, 480);
    block.clear();
    block.setSample (0, 100, 0.5f);
    rec.write (block, 0, 480, SampleRecorder::input);
    rec.write (block, 0, 480, SampleRecorder::output);   // the other source: ignored
    juce::AudioBuffer<float> got;
    double gotRate = 0.0;
    rec.stop (got, gotRate);
    check (got.getNumSamples() == 480 && gotRate == sr && got.getSample (0, 100) == 0.5f,
           "recorder: records the chosen source only");

    // Silence is trimmed away; two clicks are found as hits.
    juce::AudioBuffer<float> clicks (2, (int) sr * 2);
    clicks.clear();
    for (int c : { 24000, 48000 })
        for (int i = 0; i < 200; ++i)
        {
            clicks.setSample (0, c + i, 0.8f);
            clicks.setSample (1, c + i, 0.8f);
        }
    juce::AudioBuffer<float> trimmed (clicks);
    check (SampleTools::trimSilence (trimmed, sr) && trimmed.getNumSamples() < clicks.getNumSamples() - 20000,
           "trim: the silence before and after is cut");
    const auto hits = SampleTools::findHits (clicks, sr, 0.5f, 16);
    check (hits.size() == 3, "chop: finds the start and the two hits (" + juce::String ((int) hits.size()) + ")");

    // A pad plays its marked slice: pad 2 starts at the second marker, on the second click.
    Sampler s;
    s.setSample (clicks, sr, "clicks");
    s.prepare (sr, 512);
    s.params.sliceCount.store (2);
    s.params.slicePoints[0].store (0.0f);
    s.params.slicePoints[1].store (48000.0f / (float) (clicks.getNumSamples() - 1));
    juce::AudioBuffer<float> out (2, 4800);
    out.clear();
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (10, Sampler::padBase + 1, (juce::uint8) 127), 0);
    s.render (out, 0, out.getNumSamples(), m);
    check (out.getMagnitude (0, 0, 200) > 0.1f, "pads: pad 2 plays the second slice");
}

// The Poly: plays, stays clean, fades out, and its settings survive a save.
static void testPoly()
{
    std::cout << "Poly\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::poly);
    e.track (0).polyParams.unison.store (4);
    e.track (0).polyParams.resonance.store (0.9f);
    e.keyboardState.noteOn (1, 60, 1.0f);
    e.keyboardState.noteOn (1, 64, 1.0f);

    juce::AudioBuffer<float> buf (2, 256);
    float peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 100; ++i)
    {
        buf.clear();
        e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
        peak = juce::jmax (peak, buf.getMagnitude (0, 0, 256));
        for (int s = 0; s < 256; ++s)
            finite = finite && std::isfinite (buf.getSample (0, s));
    }
    std::cout << "  info  poly peak " << peak << "\n";
    check (peak > 0.01f && finite, "poly: a chord makes sound, with no NaN");
    check (peak < 1.5f, "poly: a resonant stacked chord stays at a sane level");

    e.keyboardState.noteOff (1, 60, 0.0f);
    e.keyboardState.noteOff (1, 64, 0.0f);
    for (int i = 0; i < 1200; ++i)
    {
        buf.clear();
        e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
    }
    check (e.track (0).level.load() < 1.0e-4f, "poly: notes fade out after release");

    e.track (0).polyParams.shape.store (0.5f);
    const auto v = Presets::soundToVar (e.track (0));
    e.track (2).type.store (Track::sampler);
    Presets::applySoundVar (e.track (2), v);
    check (e.track (2).type.load() == Track::poly && std::abs (e.track (2).polyParams.shape.load() - 0.5f) < 0.001f
           && e.track (2).polyParams.unison.load() == 4, "poly: its sound carries over to another track");
    check (Presets::factorySounds (Track::poly).size() >= 8, "poly: has factory sounds");
}

// Recordings on the drum pads: they play, they stop, and they are saved.
static void testDrumPads()
{
    std::cout << "Drum pads\n";
    constexpr double sr = 48000.0;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    AudioEngine e;
    e.prepareToPlay (256, sr);

    juce::AudioBuffer<float> beep (2, 4800);   // 0.1 s
    for (int i = 0; i < beep.getNumSamples(); ++i)
    {
        const float s = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / sr);
        beep.setSample (0, i, s);
        beep.setSample (1, i, s);
    }
    juce::String err;
    check (Project::loadSampleIntoPad (e, 1, 2, "beep.wav", wavOf (beep, sr), formats, err) && e.track (1).kit.hasPadSample (2),
           "pads: a recording loads onto pad 3");

    auto& kit = e.track (1).kit;
    kit.prepare (sr);
    juce::AudioBuffer<float> out (2, 9600);
    out.clear();
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 127), 0);
    kit.render (out, 0, out.getNumSamples(), m);
    check (out.getMagnitude (0, 0, 4000) > 0.1f && out.getMagnitude (0, 5500, 4000) < 1.0e-3f,
           "pads: the pad plays its recording, then stops");

    juce::TemporaryFile project (Project::fileExtension);
    check (Project::save (e, project.getFile(), err), "pads: a project with a pad recording saves");
    AudioEngine e2;
    check (Project::open (e2, project.getFile(), formats, err) && e2.track (1).kit.hasPadSample (2) && ! e2.track (1).kit.hasPadSample (3),
           "pads: the recording comes back on the same pad");

    juce::TemporaryFile kitFile (Presets::fileExtension);
    check (Presets::save (e.track (1), kitFile.getFile(), err) && Presets::load (e2, 5, kitFile.getFile(), formats, err)
           && e2.track (5).type.load() == Track::drums && e2.track (5).kit.hasPadSample (2),
           "pads: a saved kit brings its recordings");

    Project::clearPad (e, 1, 2);
    check (! e.track (1).kit.hasPadSample (2) && e.track (1).padFileData[2].getSize() == 0, "pads: a pad can be cleared");
}

// The FM and metal drum models: every drum sounds, stays finite and fades out.
static void testDrumModels()
{
    std::cout << "Drum models\n";
    constexpr double sr = 48000.0;
    for (int model : { 1, 2 })
    {
        bool allSound = true, allFinite = true, allFade = true;
        for (int d = 0; d < DrumKit::numDrums; ++d)
        {
            DrumKit kit;
            kit.prepare (sr);
            for (auto& p : kit.drums)
                p.model.store (model);
            juce::AudioBuffer<float> out (2, (int) (sr * 4.0));
            out.clear();
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::noteOn (10, 36 + d, (juce::uint8) 110), 0);
            kit.render (out, 0, out.getNumSamples(), m);
            const float* l = out.getReadPointer (0);
            for (int i = 0; i < out.getNumSamples(); ++i)
                if (! std::isfinite (l[i])) { allFinite = false; break; }
            if (out.getMagnitude (0, 0, 4800) <= 0.01f)
            {
                allSound = false;
                std::cout << "    silent: model " << model << " drum " << d << "\n";
            }
            if (out.getMagnitude (0, out.getNumSamples() - 4800, 4800) >= 1.0e-3f)
                allFade = false;
        }
        const juce::String name = model == 1 ? "FM" : "metal";
        check (allSound, name + " model: every drum sounds");
        check (allFinite, name + " model: no broken samples");
        check (allFade, name + " model: every drum fades out");
    }
    check (Presets::factorySounds (Track::drums).size() >= 6, "drum models: FM and Metal kits are in the factory sounds");
}

// The Wavetable synth: every wave set plays cleanly, notes fade out, and its settings survive saving.
static void testWavetable()
{
    std::cout << "Wavetable\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::wavetable);
    auto& wp = e.track (0).waveParams;
    wp.unison.store (4);
    wp.resonance.store (0.9f);
    wp.sub.store (0.5f);
    wp.lfoPosition.store (0.5f);

    juce::AudioBuffer<float> buf (2, 256);
    bool allSound = true, allFinite = true, allSane = true, allFade = true;
    for (int table = 0; table < Wavetables::numTables; ++table)
    {
        wp.table.store (table);
        e.keyboardState.noteOn (1, 48, 1.0f);
        e.keyboardState.noteOn (1, 64, 1.0f);
        e.keyboardState.noteOn (1, 100, 1.0f);
        float peak = 0.0f;
        for (int i = 0; i < 100; ++i)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
            peak = juce::jmax (peak, buf.getMagnitude (0, 0, 256), buf.getMagnitude (1, 0, 256));
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 256; ++s)
                    allFinite = allFinite && std::isfinite (buf.getSample (ch, s));
        }
        std::cout << "  info  " << Wavetables::tableName (table) << " peak " << peak << "\n";
        allSound = allSound && peak > 0.01f;
        allSane = allSane && peak < 1.5f;

        e.keyboardState.noteOff (1, 48, 0.0f);
        e.keyboardState.noteOff (1, 64, 0.0f);
        e.keyboardState.noteOff (1, 100, 0.0f);
        for (int i = 0; i < 1200; ++i)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
        }
        allFade = allFade && e.track (0).level.load() < 1.0e-4f;
    }
    check (allSound && allFinite, "wavetable: every wave set makes sound, with no NaN");
    check (allSane, "wavetable: a resonant stacked chord stays at a sane level");
    check (allFade, "wavetable: notes fade out after release");

    wp.table.store (2);
    wp.position.store (0.65f);
    const auto v = Presets::soundToVar (e.track (0));
    e.track (2).type.store (Track::sampler);
    Presets::applySoundVar (e.track (2), v);
    check (e.track (2).type.load() == Track::wavetable && e.track (2).waveParams.table.load() == 2
           && std::abs (e.track (2).waveParams.position.load() - 0.65f) < 0.001f && e.track (2).waveParams.unison.load() == 4,
           "wavetable: its sound carries over to another track");

    juce::String err;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::TemporaryFile project (Project::fileExtension);
    AudioEngine e2;
    check (Project::save (e, project.getFile(), err) && Project::open (e2, project.getFile(), formats, err)
           && e2.track (0).type.load() == Track::wavetable && e2.track (0).waveParams.table.load() == 2,
           "wavetable: a project keeps the Wavetable track and its settings");
    check (Presets::factorySounds (Track::wavetable).size() >= 8, "wavetable: has factory sounds");
}

// The FM synth: every layout plays cleanly, notes fade out, and its settings survive saving.
static void testFm()
{
    std::cout << "FM\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::fm);
    auto& fp = e.track (0).fmParams;
    fp.brightness.store (1.0f);
    fp.feedback.store (1.0f);
    fp.ratio[1].store (15.0f);

    juce::AudioBuffer<float> buf (2, 256);
    bool allSound = true, allFinite = true, allSane = true, allFade = true;
    for (int alg = 0; alg < 8; ++alg)
    {
        fp.algorithm.store (alg);
        for (int note : { 36, 60, 64, 108 })
            e.keyboardState.noteOn (1, note, 1.0f);
        float peak = 0.0f;
        for (int i = 0; i < 100; ++i)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
            peak = juce::jmax (peak, buf.getMagnitude (0, 0, 256));
            for (int s = 0; s < 256; ++s)
                allFinite = allFinite && std::isfinite (buf.getSample (0, s));
        }
        std::cout << "  info  layout " << (alg + 1) << " peak " << peak << "\n";
        allSound = allSound && peak > 0.01f;
        allSane = allSane && peak < 1.5f;

        for (int note : { 36, 60, 64, 108 })
            e.keyboardState.noteOff (1, note, 0.0f);
        for (int i = 0; i < 1200; ++i)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
        }
        allFade = allFade && e.track (0).level.load() < 1.0e-4f;
    }
    check (allSound && allFinite, "fm: every layout makes sound, with no NaN");
    check (allSane, "fm: full brightness and feedback stays at a sane level");
    check (allFade, "fm: notes fade out after release");

    fp.algorithm.store (5);
    fp.ratio[2].store (7.0f);
    const auto v = Presets::soundToVar (e.track (0));
    e.track (2).type.store (Track::sampler);
    Presets::applySoundVar (e.track (2), v);
    check (e.track (2).type.load() == Track::fm && e.track (2).fmParams.algorithm.load() == 5
           && std::abs (e.track (2).fmParams.ratio[2].load() - 7.0f) < 0.001f,
           "fm: its sound carries over to another track");

    juce::String err;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::TemporaryFile project (Project::fileExtension);
    AudioEngine e2;
    check (Project::save (e, project.getFile(), err) && Project::open (e2, project.getFile(), formats, err)
           && e2.track (0).type.load() == Track::fm && e2.track (0).fmParams.algorithm.load() == 5,
           "fm: a project keeps the FM track and its settings");
    check (Presets::factorySounds (Track::fm).size() >= 8, "fm: has factory sounds");
}

static bool hasMessage (const juce::MidiBuffer& b, std::function<bool (const juce::MidiMessage&)> test)
{
    for (const auto m : b)
        if (test (m.getMessage()))
            return true;
    return false;
}

// External MIDI tracks: notes and knobs go out on the right channel, nothing hangs, the input can be heard, and it all saves.
static void testExternal()
{
    std::cout << "External MIDI\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    e.selectedTrack.store (0);
    auto& tr = e.track (0);
    tr.type.store (Track::external);
    tr.ext.channel.store (5);
    juce::AudioBuffer<float> buf (2, 256);
    auto block = [&] { buf.clear(); e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256)); };

    block();
    e.keyboardState.noteOn (1, 60, 1.0f);
    block();
    check (hasMessage (e.externalMidiFor (0), [] (const juce::MidiMessage& m) { return m.isNoteOn() && m.getNoteNumber() == 60 && m.getChannel() == 5; }),
           "external: a key plays the gear on its channel");
    check (buf.getMagnitude (0, 0, 256) < 1.0e-6f, "external: makes no sound of its own");

    ControlTargets targets (e);
    targets.applyValue ("ext.cc1", 1.0f);
    block();
    check (hasMessage (e.externalMidiFor (0), [] (const juce::MidiMessage& m) {
               return m.isController() && m.getControllerNumber() == 74 && m.getControllerValue() == 127 && m.getChannel() == 5; }),
           "external: knob 1 sends CC 74");
    block();
    check (e.externalMidiFor (0).isEmpty(), "external: a knob sends only when it moves");

    tr.ext.channel.store (6);
    block();
    check (hasMessage (e.externalMidiFor (0), [] (const juce::MidiMessage& m) { return m.isAllNotesOff() && m.getChannel() == 5; }),
           "external: changing the channel stops the notes on the old one");

    tr.type.store (Track::testSynth);
    block();
    check (hasMessage (e.externalMidiFor (0), [] (const juce::MidiMessage& m) { return m.isAllNotesOff(); }),
           "external: changing the sound stops the gear's notes");
    e.keyboardState.noteOff (1, 60, 0.0f);
    tr.type.store (Track::external);
    block();

    juce::AudioBuffer<float> input (2, 256);
    for (int i = 0; i < 256; ++i)
    {
        const float s = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / 48000.0);
        input.setSample (0, i, s);
        input.setSample (1, i, s);
    }
    float heard = 0.0f;
    block();
    check (buf.getMagnitude (0, 0, 256) < 1.0e-6f, "external: the input is silent while Hear input is off");
    tr.ext.monitor.store (true);
    for (int i = 0; i < 20; ++i)
    {
        e.setLiveInput (input, 0, 256);
        block();
        heard = juce::jmax (heard, buf.getMagnitude (0, 0, 256));
    }
    std::cout << "  info  heard " << heard << "\n";
    check (heard > 0.05f, "external: Hear input plays the sound card's input through the track");
    tr.ext.monitor.store (false);
    for (int i = 0; i < 20; ++i)
    {
        e.setLiveInput (input, 0, 256);
        block();
    }
    check (buf.getMagnitude (0, 0, 256) < 1.0e-3f, "external: Hear input off silences it again");

    tr.ext.drums.store (true);
    check (tr.playsDrums(), "external: can play drum steps");

    tr.externalDeviceName = "FM-1";
    tr.ext.ccNumber[2].store (20);
    juce::String err;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::TemporaryFile project (Project::fileExtension);
    AudioEngine e2;
    check (Project::save (e, project.getFile(), err) && Project::open (e2, project.getFile(), formats, err)
           && e2.track (0).type.load() == Track::external && e2.track (0).externalDeviceName == "FM-1"
           && e2.track (0).ext.channel.load() == 6 && e2.track (0).ext.ccNumber[2].load() == 20 && e2.track (0).ext.drums.load(),
           "external: a project keeps the track, its MIDI out and its knobs");
    check (Presets::factorySounds (Track::external).size() >= 4, "external: has starting sounds");
}

// Insert effects: each one keeps the sound clean and at a sane level, the duck follows the kick, and they save.
static void testInserts()
{
    std::cout << "Insert effects\n";
    juce::AudioBuffer<float> buf (2, 256);
    bool allSound = true, allFinite = true, allSane = true;
    for (int type = 1; type < InsertEffects::numTypes; ++type)
    {
        AudioEngine e;
        e.prepareToPlay (256, 48000.0);
        e.selectedTrack.store (0);
        e.track (0).type.store (Track::poly);
        e.track (0).inserts[0].choose (type);
        e.track (0).inserts[1].choose (type == InsertEffects::drive ? InsertEffects::compressor : InsertEffects::off);
        e.keyboardState.noteOn (1, 48, 1.0f);
        e.keyboardState.noteOn (1, 60, 1.0f);
        float peak = 0.0f;
        for (int i = 0; i < 150; ++i)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
            peak = juce::jmax (peak, buf.getMagnitude (0, 0, 256), buf.getMagnitude (1, 0, 256));
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 256; ++s)
                    allFinite = allFinite && std::isfinite (buf.getSample (ch, s));
        }
        std::cout << "  info  " << InsertEffects::name (type) << " peak " << peak << "\n";
        allSound = allSound && peak > 0.01f;
        allSane = allSane && peak < 2.0f;
    }
    check (allSound && allFinite, "inserts: every effect lets the sound through, with no NaN");
    check (allSane, "inserts: no effect blows up the level");

    // The duck dips right after a kick and comes back up.
    InsertProcessor duck;
    duck.prepare (48000.0);
    InsertSettings s;
    s.choose (InsertEffects::duck);
    s.knob[0].store (1.0f);
    EffectContext ctx;
    ctx.kicks[0] = 0;
    ctx.numKicks = 1;
    juce::AudioBuffer<float> ones (2, 4800);
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::fill (ones.getWritePointer (ch), 1.0f, 4800);
    duck.process (ones, 4800, s, ctx);
    check (ones.getSample (0, 960) < 0.3f && ones.getSample (0, 4799) > ones.getSample (0, 960),
           "inserts: the duck dips on the kick, then swells back");

    AudioEngine e;
    e.track (3).inserts[0].choose (InsertEffects::phaser);
    e.track (3).inserts[1].choose (InsertEffects::lofi);
    e.track (3).inserts[1].knob[1].store (0.8f);
    juce::String err;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::TemporaryFile project (Project::fileExtension);
    AudioEngine e2;
    check (Project::save (e, project.getFile(), err) && Project::open (e2, project.getFile(), formats, err)
           && e2.track (3).inserts[0].type.load() == InsertEffects::phaser && e2.track (3).inserts[1].type.load() == InsertEffects::lofi
           && std::abs (e2.track (3).inserts[1].knob[1].load() - 0.8f) < 0.001f,
           "inserts: a project keeps each track's effects");
}

// Reverb and delay kinds: each pairing sounds, stays sane (even the tape echo at full intensity), and saves.
static void testSendEffects()
{
    std::cout << "Reverb and delay\n";
    juce::AudioBuffer<float> buf (2, 256);
    bool allSound = true, allFinite = true, allSane = true;
    for (int rType = 0; rType < MasterEffects::numReverbTypes; ++rType)
    {
        for (int dType = 0; dType < MasterEffects::numDelayTypes; ++dType)
        {
            AudioEngine e;
            e.prepareToPlay (256, 48000.0);
            e.selectedTrack.store (0);
            e.track (0).type.store (Track::poly);
            e.track (0).mix.reverb.store (1.0f);
            e.track (0).mix.delay.store (1.0f);
            e.effects.reverbType.store (rType);
            e.effects.delayType.store (dType);
            e.effects.delayFeedback.store (0.9f);   // the top: the tape echo self-oscillates
            e.keyboardState.noteOn (1, 60, 1.0f);
            float peak = 0.0f, tail = 0.0f;
            for (int i = 0; i < 1000; ++i)
            {
                if (i == 40)
                    e.keyboardState.noteOff (1, 60, 0.0f);
                buf.clear();
                e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
                peak = juce::jmax (peak, buf.getMagnitude (0, 0, 256), buf.getMagnitude (1, 0, 256));
                if (i >= 200)
                    tail = juce::jmax (tail, buf.getMagnitude (0, 0, 256));
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < 256; ++s)
                        allFinite = allFinite && std::isfinite (buf.getSample (ch, s));
            }
            std::cout << "  info  " << MasterEffects::reverbTypeName (rType) << " + " << MasterEffects::delayTypeName (dType)
                      << " peak " << peak << " tail " << tail << "\n";
            allSound = allSound && tail > 1.0e-4f;   // echoes or reverb still there after the note has gone
            allSane = allSane && peak < 2.5f;
        }
    }
    check (allSound && allFinite, "send effects: every reverb and delay kind sounds, with no NaN");
    check (allSane, "send effects: even at full intensity they stay at a sane level");

    AudioEngine e;
    e.effects.reverbType.store (MasterEffects::spring);
    e.effects.delayType.store (MasterEffects::tape);
    e.effects.tapeHeads.store (4);
    e.effects.tapeWear.store (0.8f);
    juce::String err;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::TemporaryFile project (Project::fileExtension);
    AudioEngine e2;
    check (Project::save (e, project.getFile(), err) && Project::open (e2, project.getFile(), formats, err)
           && e2.effects.reverbType.load() == MasterEffects::spring && e2.effects.delayType.load() == MasterEffects::tape
           && e2.effects.tapeHeads.load() == 4 && std::abs (e2.effects.tapeWear.load() - 0.8f) < 0.001f,
           "send effects: a project keeps the reverb and delay kinds");
}

// Repeat, stutter and tape stop on the whole mix: clean, the tape stop really stops, and the music comes back.
static void testPerform()
{
    std::cout << "Performance effects\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::poly);
    e.track (0).polyParams.sustain.store (1.0f);
    e.keyboardState.noteOn (1, 57, 1.0f);
    juce::AudioBuffer<float> buf (2, 256);
    bool finite = true;
    auto run = [&] (int blocks) {
        float last = 0.0f;
        for (int i = 0; i < blocks; ++i)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256));
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 256; ++s)
                    finite = finite && std::isfinite (buf.getSample (ch, s));
            last = buf.getMagnitude (0, 0, 256);
        }
        return last;
    };

    const float playing = run (60);
    e.perform.repeat.store (true);
    const float repeating = run (60);
    e.perform.repeat.store (false);
    e.perform.stutter.store (true);
    run (60);
    e.perform.stutter.store (false);
    e.perform.stopTime.store (0.0f);   // the quickest stop
    e.perform.tapeStop.store (true);
    const float stopped = run (120);
    e.perform.tapeStop.store (false);
    const float back = run (20);
    std::cout << "  info  playing " << playing << " repeat " << repeating << " stopped " << stopped << " back " << back << "\n";
    check (finite && repeating > 0.01f, "perform: repeat keeps the music going, with no NaN");
    check (stopped < 1.0e-3f, "perform: tape stop slows the mix to silence");
    check (back > 0.5f * playing, "perform: letting go brings the music back");
}

// Chords, lengths and how hard: a step plays all its notes, for as long as it says.
static void testChordSteps()
{
    std::cout << "Chords and note lengths\n";
    constexpr double sr = 48000.0, bpm = 120.0;
    constexpr int block = 256;
    const double samplesPerStep = sr * 60.0 / bpm / 4.0;   // 6000

    Song song;
    song.clearScene (0);
    auto& st = song.scene (0).tracks[0].steps[0];
    st.setSingle (60, 90);
    st.addNote (64);
    st.addNote (67);
    st.length.store (2.0f);
    st.on.store (true);
    song.prepare (sr);

    std::array<juce::MidiBuffer, Song::numTracks> out;
    std::array<bool, Song::numTracks> drums {};
    std::vector<std::pair<juce::int64, juce::MidiMessage>> events;
    for (juce::int64 pos = 0; pos < (juce::int64) (samplesPerStep * 4); pos += block)
    {
        for (auto& m : out) m.clear();
        song.process (out, block, bpm, true, drums);
        for (const auto meta : out[0])
            events.emplace_back (pos + meta.samplePosition, meta.getMessage());
    }
    int ons = 0, offs = 0;
    bool velocityOk = true, offTimeOk = true;
    for (const auto& [time, m] : events)
    {
        if (m.isNoteOn()) { ++ons; velocityOk = velocityOk && m.getVelocity() == 90; }
        if (m.isNoteOff())
        {
            ++offs;
            offTimeOk = offTimeOk && std::abs ((double) time - 2.0 * samplesPerStep) < 0.05 * samplesPerStep;
        }
    }
    check (ons == 3, "chord: a step plays its three notes");
    check (velocityOk, "chord: at the step's velocity");
    check (offs == 3 && offTimeOk, "chord: they last the step's length (2 steps)");

    std::array<int, Song::maxChord> notes {};
    st.removeNote (60);
    check (st.getNotes (notes) == 2 && notes[0] == 64 && st.on.load(), "chord: taking out the first note keeps the others");
    st.removeNote (64);
    st.removeNote (67);
    check (! st.on.load(), "chord: taking out the last note switches the step off");
}

// Recording a chord, held for a while: one step gets every note and the time it was held.
static void testRecordChord()
{
    std::cout << "Recording chords and lengths\n";
    AudioEngine e;
    constexpr int block = 256;
    e.prepareToPlay (block, 48000.0);
    e.song.clearScene (0);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::poly);
    juce::AudioBuffer<float> buf (2, block);
    auto run = [&] { buf.clear(); e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, block)); };

    e.transport.bpm.store (120.0);
    e.transport.playing.store (true);
    e.recordNotes.store (true);
    for (int i = 0; i < 30; ++i) run();
    for (int n : { 60, 64, 67 }) e.keyboardState.noteOn (1, n, 0.8f);
    run();
    // Hold for about two steps (12000 samples at 120 BPM).
    for (int i = 0; i < 12000 / block; ++i) run();
    for (int n : { 60, 64, 67 }) e.keyboardState.noteOff (1, n, 0.0f);
    run();

    int stepsOn = 0, chordNotes = 0;
    float length = 0.0f;
    int velocity = 0;
    for (auto& step : e.song.scene (0).tracks[0].steps)
        if (step.on.load())
        {
            ++stepsOn;
            std::array<int, Song::maxChord> notes {};
            chordNotes = step.getNotes (notes);
            length = step.length.load();
            velocity = step.velocity.load();
        }
    std::cout << "  info  recorded " << stepsOn << " step(s), " << chordNotes << " notes, length " << length << ", velocity " << velocity << "\n";
    check (stepsOn == 1 && chordNotes == 3, "record: a chord lands on one step, all three notes");
    check (length >= 1.75f && length <= 2.25f, "record: the step keeps how long the chord was held");
    check (velocity > 90 && velocity < 110, "record: and how hard it was played");
    e.transport.playing.store (false);
    run();

    // Projects keep chords, lengths and velocities.
    const auto saved = Project::toVar (e);
    e.song.clearScene (0);
    for (auto& step : e.song.scene (0).tracks[0].steps)
        step.setSingle (48);
    Project::applyVar (e, saved);
    int keptNotes = 0;
    float keptLength = 0.0f;
    for (auto& step : e.song.scene (0).tracks[0].steps)
        if (step.on.load())
        {
            std::array<int, Song::maxChord> notes {};
            keptNotes = step.getNotes (notes);
            keptLength = step.length.load();
        }
    check (keptNotes == 3 && std::abs (keptLength - length) < 1.0e-4f, "record: a saved project keeps the chord and its length");
}

// The metronome clicks on the beats; the count-in clicks a bar, then play starts exactly on time.
static void testMetronome()
{
    std::cout << "Metronome and count-in\n";
    AudioEngine e;
    constexpr int block = 256;
    constexpr double rate = 48000.0;
    e.prepareToPlay (block, rate);
    e.metronomeLevel.store (1.0f);
    juce::AudioBuffer<float> buf (2, block), click (2, block);
    int onsets = 0;
    bool quiet = true;
    juce::int64 sinceLoud = 100000;
    auto run = [&] {
        buf.clear();
        e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, block));
        click.clear();
        e.renderMetronome (click, 0, block);
        for (int i = 0; i < block; ++i)
        {
            const bool loud = std::abs (click.getSample (0, i)) > 0.05f;
            if (loud && quiet && sinceLoud > 4000) ++onsets;
            sinceLoud = loud ? 0 : sinceLoud + 1;
            quiet = ! loud;
        }
    };

    const int bar = (int) (rate * 2.0);   // 120 BPM
    e.transport.bpm.store (120.0);
    e.metronomeMode.store (AudioEngine::metronomeOn);
    e.transport.playing.store (true);
    for (int i = 0; i < bar / block; ++i) run();
    check (onsets == 4, "metronome: four clicks in a bar (" + juce::String (onsets) + ")");
    e.transport.playing.store (false);
    run();

    e.metronomeMode.store (AudioEngine::metronomeOff);
    e.countInBars.store (1);
    onsets = 0;
    e.startWithCountIn();
    int blocks = 0;
    while (! e.transport.playing.load() && blocks < 2 * bar / block)
    {
        run();
        ++blocks;
    }
    const int startedBlock = blocks - 1;   // the block in which play began
    check (onsets == 4, "count-in: four clicks before play (" + juce::String (onsets) + ")");
    check (startedBlock == bar / block, "count-in: play starts after exactly one bar");
    check (e.song.playheadStep.load() == 0, "count-in: on the first step");
    e.transport.playing.store (false);
    run();
}

// While the More window is open, the controller's panel knobs turn its row instead of the main knobs.
static void testKnobPage()
{
    std::cout << "More window knobs\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    ControlTargets targets (e);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::poly);
    const auto& def = synthLibrary()[(size_t) Track::poly];
    check (! def.more.empty(), "knob page: the poly synth has More settings");
    if (def.more.empty())
        return;

    const auto mainId = def.knobs[0].target, moreId = def.more[0].target;
    targets.applyValue ("panel.knob.1", 0.7f);
    const float moreBefore = targets.getValue (moreId);
    check (std::abs (targets.getValue (mainId) - 0.7f) < 0.03f, "knob page: knob 1 turns the first main knob");

    e.knobPage.store (1);
    const float target = moreBefore > 0.5f ? 0.2f : 0.8f;
    targets.applyValue ("panel.knob.1", target);
    check (std::abs (targets.getValue (moreId) - target) < 0.03f && std::abs (targets.getValue (mainId) - 0.7f) < 0.03f,
           "knob page: with More open, knob 1 turns the first More setting and leaves the main knob");
    e.knobPage.store (0);
}

// Levels: how loud each factory sound plays the same notes, to even them out. Information only.
static void testLevels()
{
    std::cout << "Levels\n";
    AudioEngine e;
    constexpr int block = 512;
    constexpr double rate = 48000.0;
    e.prepareToPlay (block, rate);
    e.selectedTrack.store (0);
    juce::AudioBuffer<float> buf (2, block);
    auto render = [&] (double seconds) {
        double sum = 0.0;
        float peak = 0.0f;
        const int blocks = (int) (seconds * rate / block);
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, block));
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float x = buf.getSample (ch, i);
                    sum += (double) x * x;
                    peak = juce::jmax (peak, std::abs (x));
                }
        }
        const double rms = std::sqrt (sum / juce::jmax (1.0, (double) blocks * block * 2.0));
        return std::make_pair (juce::Decibels::gainToDecibels ((float) rms, -120.0f), juce::Decibels::gainToDecibels (peak, -120.0f));
    };

    for (int type : { (int) Track::testSynth, (int) Track::poly, (int) Track::wavetable, (int) Track::fm, (int) Track::noise, (int) Track::drums })
    {
        for (const auto& s : Presets::factorySounds (type))
        {
            Presets::applySoundVar (e.track (0), s.sound);
            render (1.0);   // let the last sound die away
            std::pair<float, float> level;
            if (type == Track::drums)
            {
                for (int note : { 36, 38, 42, 36 })
                {
                    e.keyboardState.noteOn (1, note, 100.0f / 127.0f);
                    e.keyboardState.noteOff (1, note, 0.0f);
                }
                level = render (1.0);
            }
            else
            {
                e.keyboardState.noteOn (1, 48, 100.0f / 127.0f);
                level = render (1.5);
                e.keyboardState.noteOff (1, 48, 0.0f);
            }
            std::cout << "  info  level " << Presets::instrumentName (type) << " / " << s.name
                      << ": rms " << juce::String (level.first, 1) << " dB, peak " << juce::String (level.second, 1) << " dB\n";
        }
    }
    e.keyboardState.allNotesOff (1);
}

// The Noise synth: every colour at about the same loudness, brown deep and violet thin;
// every source makes sound; the resonator gives it a pitch.
static void testNoise()
{
    std::cout << "Noise\n";
    // Colours, straight from the generator.
    std::array<double, 5> rms {}, lowShare {};
    for (int c = 0; c < 5; ++c)
    {
        ColourNoise n (12345u);
        double sum = 0.0, low = 0.0;
        float lp = 0.0f;
        for (int i = 0; i < 96000; ++i)
        {
            const float x = n.next ((float) c * 0.25f);
            lp += 0.0645f * (x - lp);   // about 500 Hz at 48 kHz
            sum += (double) x * x;
            low += (double) lp * lp;
        }
        rms[(size_t) c] = std::sqrt (sum / 96000.0);
        lowShare[(size_t) c] = low / juce::jmax (1.0e-12, sum);
    }
    std::cout << "  info  rms brown " << rms[0] << " pink " << rms[1] << " white " << rms[2] << " blue " << rms[3] << " violet " << rms[4] << "\n";
    bool even = true;
    for (auto r : rms)
        even = even && std::abs (juce::Decibels::gainToDecibels ((float) (r / rms[2]))) < 3.0f;
    check (even, "noise: every colour is about as loud as white");
    check (lowShare[0] > 0.6 && lowShare[1] > lowShare[2] && lowShare[2] > lowShare[3] && lowShare[4] < 0.05,
           "noise: brown is deep, violet thin, and the colours in between in order");

    AudioEngine e;
    constexpr int block = 512;
    e.prepareToPlay (block, 48000.0);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::noise);
    auto& np = e.track (0).noiseParams;
    juce::AudioBuffer<float> buf (2, block);
    auto play = [&] (int note, int blocks) {
        e.keyboardState.noteOn (1, note, 0.8f);
        double sum = 0.0;
        bool finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, block));
            for (int i = 0; i < block; ++i)
            {
                const float x = buf.getSample (0, i);
                finite = finite && std::isfinite (x);
                sum += (double) x * x;
            }
        }
        e.keyboardState.noteOff (1, note, 0.0f);
        for (int b = 0; b < 40; ++b)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, block));
        }
        return std::make_pair (std::sqrt (sum / (double) (blocks * block)), finite);
    };

    bool allSound = true, allFinite = true;
    for (int s = 0; s < NoiseParams::numSources; ++s)
    {
        np.source.store (s);
        for (float grain : { 0.0f, 0.6f })
            for (float tone : { 0.0f, 0.8f })
            {
                np.grain.store (grain);
                np.tone.store (tone);
                const auto [level, finite] = play (57, 20);
                allSound = allSound && level > 1.0e-3;
                allFinite = allFinite && finite;
            }
    }
    check (allSound, "noise: every source makes sound, with and without grain and pitch");
    check (allFinite, "noise: with no NaN");

    // The resonator tunes the noise: the sound repeats at the note's period.
    np.source.store (NoiseParams::pure);
    np.grain.store (0.0f);
    np.tone.store (0.95f);
    np.cutoffHz.store (18000.0f);
    e.keyboardState.noteOn (1, 57, 0.8f);   // A3, 220 Hz: a period of about 218 samples
    std::vector<float> tail;
    for (int b = 0; b < 40; ++b)
    {
        buf.clear();
        e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, block));
        if (b >= 20)
            for (int i = 0; i < block; ++i)
                tail.push_back (buf.getSample (0, i));
    }
    e.keyboardState.noteOff (1, 57, 0.0f);
    auto correlation = [&tail] (int lag) {
        double ab = 0.0, aa = 0.0, bb = 0.0;
        for (size_t i = (size_t) lag; i < tail.size(); ++i)
        {
            ab += (double) tail[i] * tail[i - (size_t) lag];
            aa += (double) tail[i] * tail[i];
            bb += (double) tail[i - (size_t) lag] * tail[i - (size_t) lag];
        }
        return ab / juce::jmax (1.0e-12, std::sqrt (aa * bb));
    };
    const int period = juce::roundToInt (48000.0 / 220.0);
    double best = 0.0;
    for (int lag = period - 2; lag <= period + 2; ++lag)
        best = juce::jmax (best, correlation (lag));
    std::cout << "  info  correlation at the note's period " << best << "\n";
    check (best > 0.5, "noise: the pitch control tunes the noise to the note");

    // Projects and sounds keep the settings; there are factory sounds.
    np.colour.store (0.83f);
    np.source.store (NoiseParams::crackle);
    const auto v = Presets::soundToVar (e.track (0));
    Presets::applySoundVar (e.track (2), v);
    check (e.track (2).type.load() == Track::noise && std::abs (e.track (2).noiseParams.colour.load() - 0.83f) < 1.0e-6f
               && e.track (2).noiseParams.source.load() == NoiseParams::crackle, "noise: a sound keeps its settings");
    check (Presets::factorySounds (Track::noise).size() >= 10, "noise: has factory sounds");
}

// Hold: notes keep sounding after the keys are let go, until Hold is switched off.
static void testHold()
{
    std::cout << "Hold\n";
    AudioEngine e;
    constexpr int block = 512;
    e.prepareToPlay (block, 48000.0);
    e.selectedTrack.store (0);
    e.track (0).type.store (Track::poly);
    juce::AudioBuffer<float> buf (2, block);
    auto level = [&] (int blocks) {
        float peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, block));
            peak = juce::jmax (peak, buf.getMagnitude (0, 0, block));
        }
        return peak;
    };

    e.hold.store (true);
    e.keyboardState.noteOn (1, 60, 0.8f);
    level (4);
    e.keyboardState.noteOff (1, 60, 0.0f);
    level (100);   // about a second after letting go
    check (level (4) > 1.0e-3f, "hold: the note keeps sounding after the key is let go");
    e.hold.store (false);
    level (300);
    check (level (4) < 1.0e-4f, "hold: switching Hold off lets it go");
}

// Kits of your own: one drum taken from another kit, factory or saved, onto the same pad.
static void testCopyDrum()
{
    std::cout << "Kits of your own\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::String err;
    auto& kit = e.track (1).kit;   // track 2 is the drums
    check (e.track (1).type.load() == Track::drums, "kit: track 2 is a drum track");

    const auto& kits = Presets::factorySounds (Track::drums);
    const auto& source = kits[4].sound;   // the FM kit
    const float kickPitch = kit.drums[0].pitch.load();
    const int kickModel = kit.drums[0].model.load();
    Presets::copyDrum (e, 1, 2, source);
    const auto want = source["kit"]["drums"][2];
    check (std::abs (kit.drums[2].pitch.load() - (float) (double) want[0]) < 1.0e-6f
               && kit.drums[2].model.load() == (int) want[7]
               && std::abs (kit.drums[2].colour.load() - (float) (double) want[8]) < 1.0e-6f,
           "kit: the snare comes from the FM kit");
    check (kit.drums[0].pitch.load() == kickPitch && kit.drums[0].model.load() == kickModel,
           "kit: the other drums stay as they were");

    kit.drums[5].pitch.store (7.0f);
    kit.drums[5].model.store (2);
    juce::TemporaryFile saved (Presets::fileExtension);
    check (Presets::save (e.track (1), saved.getFile(), err), "kit: a kit saves");
    kit.drums[5].pitch.store (0.0f);
    kit.drums[5].model.store (0);
    check (Presets::copyDrumFromFile (e, 1, 5, saved.getFile(), formats, err)
               && kit.drums[5].pitch.load() == 7.0f && kit.drums[5].model.load() == 2,
           "kit: a drum comes from a saved kit");

    juce::TemporaryFile synthSound (Presets::fileExtension);
    Presets::save (e.track (0), synthSound.getFile(), err);
    check (! Presets::copyDrumFromFile (e, 1, 5, synthSound.getFile(), formats, err) && kit.drums[5].pitch.load() == 7.0f,
           "kit: a sound that is not a kit is refused");
}

// Live recording: notes played while the song plays land on the nearest step, on synth and drum tracks.
static void testLiveRecord()
{
    std::cout << "Live recording\n";
    AudioEngine e;
    e.prepareToPlay (256, 48000.0);
    e.song.clearScene (0);
    e.selectedTrack.store (0);   // track 1 is a synth
    juce::AudioBuffer<float> buf (2, 256);
    auto block = [&] { buf.clear(); e.getNextAudioBlock (juce::AudioSourceChannelInfo (&buf, 0, 256)); };
    auto& pattern = e.song.scene (0).tracks[0];
    auto stepsWith = [&pattern] (int note) {
        std::vector<int> found;
        for (int s = 0; s < Song::numSteps; ++s)
            if (pattern.steps[(size_t) s].on.load() && pattern.steps[(size_t) s].note.load() == note)
                found.push_back (s);
        return found;
    };

    e.recordNotes.store (true);
    e.keyboardState.noteOn (1, 62, 1.0f);
    block();
    e.keyboardState.noteOff (1, 62, 0.0f);
    block();
    check (stepsWith (62).empty(), "record: nothing is recorded while stopped");

    e.transport.bpm.store (120.0);
    e.transport.playing.store (true);
    for (int i = 0; i < 40; ++i)
        block();
    const int playhead = e.song.playheadStep.load();
    e.keyboardState.noteOn (1, 64, 1.0f);
    block();
    e.keyboardState.noteOff (1, 64, 0.0f);
    block();
    const auto found = stepsWith (64);
    const int distance = found.size() == 1 ? (found[0] - playhead + Song::numSteps) % Song::numSteps : 99;
    std::cout << "  info  playhead " << playhead << " recorded on " << (found.empty() ? -1 : found[0]) << "\n";
    check (found.size() == 1 && (distance <= 1 || distance == Song::numSteps - 1),
           "record: a played note lands on the nearest step, once");

    e.selectedTrack.store (1);   // track 2 is the drums
    e.keyboardState.noteOn (1, 38, 1.0f);   // the snare
    block();
    e.keyboardState.noteOff (1, 38, 0.0f);
    block();
    check (e.song.scene (0).tracks[1].drumLanes[2].load() != 0 && e.song.scene (0).tracks[1].drumLanes[0].load() == 0,
           "record: a drum hit lands in that drum's lane");

    e.transport.playing.store (false);
    block();
    check (! e.recordNotes.load(), "record: Stop ends the recording");
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init; // needed by some JUCE singletons

    if (argc > 2 && juce::String (argv[1]) == "--make-demo")
        return makeDemo (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    std::cout << "BizGroove engine tests\n";
    for (int blockSize : { 64, 333, 480, 1024 })
        testSequencerTiming (blockSize);

    testScenesAndSong();
    testDrums();
    testTracks();
    testSamplerAndMixer();
    testTimelineProjectExport();
    testControllerMapping();
    testStemDeck();
    testPresets();
    testSampling();
    testPoly();
    testDrumPads();
    testDrumModels();
    testWavetable();
    testFm();
    testExternal();
    testInserts();
    testSendEffects();
    testPerform();
    testLiveRecord();
    testCopyDrum();
    testNoise();
    testHold();
    testKnobPage();
    testChordSteps();
    testRecordChord();
    testMetronome();
    testLevels();
    testRender (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File());

    std::cout << (failures == 0 ? "All tests passed\n" : juce::String (failures) + " test(s) failed\n");
    return failures == 0 ? 0 : 1;
}
