# BizGroove

An open-source groovebox for macOS and Windows, licensed under GPLv3.
Ready-to-run versions for macOS and Windows are sold at
[bizarinternational.com](https://bizarinternational.com); the source code
here is free software.

Runs on macOS 10.13 High Sierra or later (Intel and Apple Silicon) and on
64-bit Windows. Works with any MIDI keyboard or controller; the main view is
laid out like a Novation Launchkey Mini MK3, the controller it was designed
with. Everything can also be played with the mouse and the computer keyboard.

The [user manual](https://bizarinternational.com/assets/BizGroove_User_Manual.pdf)
(PDF) explains every control. This page is an overview.

## What it does

| Area | What it does |
| --- | --- |
| Tracks and scenes | 16 tracks, each playing one instrument; 16 scenes, each holding a pattern for every track. Scenes switch on the next bar |
| Instruments | Twelve (below), each with factory sounds and your own saved sounds, two extra oscillators, two LFOs, an arpeggiator and glide |
| Sequencer | Patterns of 1 to 64 steps per track, each looping on its own length; chords, note length and velocity per step; live recording onto the nearest 1/16 with a count-in; up to eight parameter locks per step |
| Pattern view | The whole pattern as a grid of notes, with a lock lane below. Move the pattern a step earlier or later, select steps and clear them, clear the track, and undo (Ctrl/Cmd + Z) |
| Swing | A Swing knob for the whole song, on every second 1/16 or on the off-beat 1/8. Heard in exports, written into exported MIDI, saved with the project. Drum tracks add their own |
| Song | A chain of scenes, or a timeline of tracks by bars; loop it or stop at the end |
| Sampler | Load or record (mic, line in, or BizGroove itself); slices, loop, reverse; an MPC-style sample editor; Grain mode for clouds of grains, with a live input |
| Mixer and effects | Volume, pan, three-band EQ, reverb and delay sends per track; two insert effects per track; room, plate or spring reverb; digital or tape echo; Repeat, Stutter and Tape stop to play live |
| Stem deck | Four looping stems kept in time at any tempo, with volume, filter, mute, solo, restart and beat roll |
| Controllers | Any MIDI controller, taught with Learn. Built-in profiles for the Launchkey Mini MK3, the original Launchkey Mini, the NI Traktor Kontrol F1 and a generic one. Sends MIDI clock |
| Projects and export | One `.bizgroove` file holds everything, recordings included. Export a WAV mixdown, one WAV per track and/or a MIDI file, faster than real time |

### The twelve instruments

| Instrument | What it is |
| --- | --- |
| Test synth | A simple synth: two detuned buzzy tones through a filter |
| Drums | Sixteen drums in General MIDI order, each analogue, FM or metal, or your own sample on its pad |
| Sampler | Plays a recording at any pitch or in slices; also Grain mode |
| Poly | An eight-voice analogue-style polysynth |
| Wavetable | Morphing waves through a resonant filter, up to four copies spread in stereo |
| FM | Four operators in eight layouts: electric pianos, bells, basses, metallic hits |
| External MIDI | Plays hardware synths or apps over MIDI; their sound can return through the sound card |
| Noise | Noise of every colour in five forms (pure, FM, ring, digital, crackle) |
| Voice | A speaking synth in the spirit of the Speak & Spell, in English or Spanish |
| Vocoder | The synth on the keys speaks with another sound's words |
| Granular | Clouds of grains from built-in sounds or a recording |
| Macro Synth | Thirteen kinds of sound on three knobs (Harmonics, Timbre, Morph): waveshaper, phase distortion, additive, formant, wave terrain, chords, string machine, swarm, six-operator DX FM, chiptune, plucked string, modal and particles |

## Getting the app

Ready-to-run versions: [bizarinternational.com](https://bizarinternational.com).

GitHub Actions builds macOS and Windows versions of every push (**Actions**
tab → latest run → *Artifacts*). The macOS build is not notarised by Apple, so
the first time macOS blocks it: open **System Settings > Privacy & Security**,
scroll down and click **Open Anyway** (on macOS 12 and earlier: **System
Preferences > Security & Privacy > General**). On Windows, if SmartScreen
warns, click **More info > Run anyway**.

### Building it yourself

Requirements: CMake 3.22+, and Xcode (macOS) or Visual Studio 2022 with
"Desktop development with C++" (Windows). JUCE 8 is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The app lands in `build/BizGroove_artefacts/Release/`.

## Tests

```
ctest --test-dir build -C Release --output-on-failure
```

Headless engine tests check, among other things, that the sequencer and the
song clock are sample-accurate at any buffer size, that scenes, the song chain
and the timeline play the right patterns, that swing moves the right steps on
1/16 and 1/8, that pattern editing (move, clear, undo) and live recording
work, that every instrument (the Macro Synth included) sounds at a sensible
level and falls silent after stop, that projects reopen with every setting and
recording intact, that export writes WAV and MIDI files with every note, that
controller profiles route MIDI correctly, and that stems stay on the beat.

## Layout

```
Source/engine/   real-time audio and MIDI: sequencer, instruments, effects, stem deck,
                 control targets, controller profiles, MIDI router (no GUI code)
Source/ui/       window, step grid, knobs, stem panel, Controllers window
Tests/           headless engine tests
ThirdParty/eurorack/  the Macro Synth's sound engines (MIT licence, see below)
```

Design rule: nothing on the audio thread may allocate, wait on a lock or touch
the GUI. The UI and controllers talk to the engine through atomics; the stem
deck only ever *tries* a lock and skips a block rather than wait.

## Credits

The Macro Synth (instrument 12) plays sound engines written by Emilie Gillet
and published under the MIT License, from her repositories
[pichenettes/eurorack](https://github.com/pichenettes/eurorack) and
[pichenettes/stmlib](https://github.com/pichenettes/stmlib). Their code, with
its copyright notices and licence, is in `ThirdParty/eurorack`. BizGroove is
not affiliated with or endorsed by their author.

BizGroove is made with [JUCE](https://juce.com).

BizGroove is a creation of Bizarinternational.
