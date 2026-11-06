# BizGroove

A free, open-source groovebox for macOS and Windows. Licensed under GPLv3.

Runs on macOS 10.13 High Sierra or later (Intel and Apple Silicon) and on
64-bit Windows.

**Version 1.0.** 16 tracks with three sounds (Test synth, Drums, Sampler),
scenes, a song chain and a timeline, a mixer with EQ, reverb and delay,
saving and opening projects, export (WAV mixdown, one WAV per track, MIDI),
a stem deck, and customisable MIDI controllers. The main view is laid out like
a Novation Launchkey Mini MK3. The full user manual is a separate document.

## The window

### Top bar

| Control | What it does |
| --- | --- |
| Track N + Sound menu | Choose the sound of the selected track: *Test synth*, *Drums* or *Sampler* |
| Play, Tempo | Play / stop (also the space bar); speed in BPM |
| File | New, Open, Save, Save as, Export (Cmd/Ctrl + N, O, S, Shift+S, E) |
| Keys & pads / Song / Mixer / Stem deck | Switch between the four views |
| Controllers, Audio / MIDI | Controller mappings; sound card and MIDI devices |

### Tracks and scenes (above the panel and the Song view)

| Row | What it does |
| --- | --- |
| Tracks 1-16 | Click a track to select it: the keys, knobs and pads then play and edit it. **M** mutes it. A light shows when it sounds |
| Scenes 1-16 | A scene holds a one-bar pattern for all 16 tracks. Click a scene to switch to it; while playing, it starts at the next bar (it blinks until then) |
| Copy / Paste / Clear | Act on the current scene |
| Song mode | On: play the song (see the Song view). Off: loop the current scene |

A new project starts with a bass line on track 1 (Test synth) and a beat on
track 2 (Drums), in scene 1.

### Keys & pads (laid out like the Launchkey Mini MK3)

Each on-screen control sits where its twin is on the hardware and moves when
you touch the real one.

| Area | What it does |
| --- | --- |
| Pitch strip | Bends the note up to 2 semitones up or down; springs back when released |
| Modulation strip | Adds vibrato, a gentle wobble in pitch; stays where you leave it |
| 8 knobs | The sound of the selected track (see below) |
| 16 pads | The 16 steps of the selected track in the current scene: top row 1-8, bottom row 9-16. Click to switch a step on or off. On synth tracks, drag up/down or scroll to change its note |
| > | Next scene (at the next bar while playing) |
| Stop/Solo/Mute | Mute or unmute the selected track |
| Track ◀ ▶ | Previous / next track |
| Play | Play / stop |
| Octave - / + | Move the on-screen keys by an octave (synth tracks) |
| 25 keys | Play the selected track; computer keys A-K work too |
| Shift, Transpose, Arp, Fixed Chord | Dimmed: these work inside the Launchkey itself |
| Record | Dimmed: recording arrives later |

### Knob names

Each knob shows the word printed beside it on the Launchkey (small, grey; it
belongs to the Launchkey's own arpeggiator) and, in bold, what it does in
BizGroove with the selected track's sound.

| Knob | Printed | Test synth | Drums |
| --- | --- | --- | --- |
| 1 | Tempo | Brightness | Pitch: tune the drum up or down, up to 12 semitones |
| 2 | Swing | Edge | Swing: shuffle the whole kit, straight to swung |
| 3 | Gate | Wah | Length: a short tick or a long boom |
| 4 | Mutate | Thickness | Tone: dull to bright |
| 5 | Deviate | Fade in | Snap: how sharp the click at the start is |
| 6 | (none) | Punch | Drive: grit and distortion |
| 7 | (none) | Fade out | Pan: left or right |
| 8 | (none) | Volume (track) | Volume of the drum |
| More | | Hold level | Choke (closed and pedal hi-hats cut the open hi-hat), Velocity (harder hits louder) |

On a drum track, the drum knobs and the pads apply to **the drum you last
played**; the panel shows which ("editing Snare").

### The drum kit

Sixteen synthesised drums in General MIDI order, on the 16 keys from the
lowest C (on screen: C2, MIDI note 36):

| Key | Drum | Key | Drum |
| --- | --- | --- | --- |
| C | Kick | G# | Pedal hi-hat |
| C# | Side stick | A | Low tom |
| D | Snare | A# | Open hi-hat |
| D# | Clap | B | Low-mid tom |
| E | Snare 2 | C (next) | Hi-mid tom |
| F | Low floor tom | C# | Crash |
| F# | Closed hi-hat | D | High tom |
| G | High floor tom | D# | Ride |

The kit repeats every two octaves, so it also starts on C4 (note 60). On the
Launchkey, if the kick is not on your lowest key, press its **Octave** button
once.

### Sampler

Choose *Sampler* in the Sound menu, then **Load sample...** (or drop a WAV,
AIFF, FLAC or OGG file on the panel). C4 plays the recording at its own pitch.

| Knob | Sampler |
| --- | --- |
| 1 Start | Where playback begins |
| 2 Length | How much of the recording plays |
| 3 Pitch | Higher or lower, up to 24 semitones |
| 4 Brightness | Bright to muffled |
| 5 Fade in / 6 Fade out | How a note starts and ends |
| 7 Pan / 8 Volume | Of the track |
| More | Loop, Reverse, Slices (2-16 pieces, one per key from C4), Hold level |

### Song view

Two ways to arrange: **Chain** (a list of scenes, each for 1-16 bars) and
**Timeline** (a grid of tracks by bars; paint a scene into each cell, right-
click to clear; *Copy the song chain here* fills it from the chain). Choose
which one **Song mode** plays with *Song mode plays*. **Loop the song** repeats
it or stops at the end.

### Mixer

One channel per track: volume fader and level meter, M (mute), S (solo), Pan,
Bass/Middle/Treble (plus or minus 12 dB at 120 Hz, 1 kHz and 8 kHz), and
Reverb and Delay amounts. Master: reverb size, delay time (1/16, 1/8, dotted
1/8, 1/4, in time with the tempo), delay repeats and master volume.

### Projects and export

A project (`.bizgroove`) holds everything, including the Sampler and stem
deck recordings, so one file can be copied to another computer. BizGroove
asks before closing a project with unsaved changes.

**File > Export** renders faster than real time: the whole song or the current
scene repeated, as a stereo **mixdown** WAV, **one WAV per track** (empty tracks
skipped; the stem deck as its own file), and/or a **MIDI file**. 44.1 or 48 kHz,
24-bit or 32-bit float, with 0, 2 or 5 seconds of tail for reverb and echoes.
No limiting or other mastering is applied.

## Other features

| Area | Now |
| --- | --- |
| Stem deck | Four looping stems (WAV, AIFF, FLAC, OGG). Each loop is sized to 1, 2, 4, 8 or 16 bars and repitched to stay on the beat at any tempo. Volume, low/high-pass filter, mute, solo, restart on the beat, beat roll |
| Tracks | 16 tracks, each a Test synth, Drums or Sampler, with volume, mute and solo |
| Controllers | Any MIDI controller can be mapped. Built-in profiles: **Launchkey Mini MK3** (default), **original Launchkey Mini**, **NI Traktor Kontrol F1** (stems), and a generic one |
| MIDI | Every connected MIDI controller is switched on automatically, including ones plugged in later |
| Audio | Sound card, sample rate and buffer size under **Audio / MIDI**; settings are remembered |

## Controllers

Open **Controllers** in the top bar. Each connected MIDI device gets a
profile: picked automatically by its name, or chosen from the list. A device
BizGroove doesn't recognise uses the Launchkey Mini MK3 profile.

To change a mapping, pick the profile under *Edit profile*, choose what a row
drives, press **Learn** and move the control. **Learn all** walks through every
row that drives something. **Add control** adds a row, **Duplicate** makes
your own profile from a built-in one, and **Reset** restores a built-in
profile. The MIDI monitor at the bottom shows what each control sends.
Everything is saved at once to `Controllers.json` in the BizGroove settings
folder.

### Novation Launchkey Mini MK3 (default)

The keys, pitch strip and modulation strip work as soon as it is plugged in.

Novation does not publish the MIDI messages the MK3's pads, knobs and buttons
send outside a DAW, so BizGroove does not guess them. Teach it once:

1. On the Launchkey, put the pads in **Drum** mode (Shift + the *Drum* pad) and
   the knobs in **Custom** mode (hold Shift: the top pads are labelled with
   modes; *Drum* is the second pad, and the knobs' *Custom* is the eighth).
2. In BizGroove, open **Controllers**, choose *Novation Launchkey Mini MK3*
   under *Edit profile*, and press **Learn all**.
3. Touch each control when its name appears: knobs 1-8, the 16 pads, Play,
   then the buttons you want for previous/next track, next scene (>) and
   mute (Stop/Solo/Mute).

The result is saved and used every time.

### Novation Launchkey Mini, original model

Keep **InControl** off (its light unlit). Knobs CC 21-28, pads notes 36-51,
round buttons CC 108-109, arrows CC 104-105 and track buttons CC 106-107, from
Novation's MIDI table for that model. The top round button plays/stops, the
bottom one stops, the arrows nudge the tempo, and the track buttons select the
previous/next track.

### Native Instruments Traktor Kontrol F1 (stems)

| F1 control | Stem deck |
| --- | --- |
| Faders 1-4 | Stem volume |
| Knobs 1-4 | Filter: left low-pass, right high-pass |
| Stop buttons 1-4 | Mute |
| Pad row 1 | Solo |
| Pad row 2 | Restart on the next beat |
| Pad row 3 | Roll 1 beat while held |
| Pad row 4 | Roll 1/16 while held |
| Browse push / Sync | Play-stop / play |

Each pad column is one stem. The F1 is a Traktor controller and only sends
MIDI once it is in MIDI mode (set up with NI's Controller Editor). Its MIDI
numbers depend on that template, so the first time: open **Controllers**,
choose the F1 profile, press **Learn all** and move each control when it is
named. Pad lights are not driven yet.

## Getting the app

### Option A: download a build (no tools needed)

1. Put this folder in a GitHub repository and push it.
2. GitHub Actions builds macOS and Windows versions automatically
   (**Actions** tab → latest run → *Artifacts*).
3. To publish a release, push a tag such as `v0.1.0`; the zips appear under
   **Releases**.

The step-by-step guide *BizGroove: Building the App on GitHub* explains every
click. In short: the macOS build is not notarised by Apple, so the first time
macOS blocks it, open **System Settings > Privacy & Security**, scroll down and
click **Open Anyway** (on macOS 12 and earlier: **System Preferences > Security &
Privacy > General**). On Windows, if SmartScreen warns, click **More info >
Run anyway**.

### Option B: build it yourself

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

They check that the sampler plays at the right pitch and from the chosen
start, that pan, solo and the reverb send work, that the timeline and the
song chain play the right scenes, that a saved project opens with every
setting and recording intact, that export renders the right length and
writes WAV and MIDI files with every note, that the song clock is
sample-accurate at any buffer size, that
scenes switch on the bar line and the song chain plays its repeats (and stops
or loops), that all 16 drums sound and fade, choke and swing work, that keys
play only the selected track and mute silences a track, that
the engine renders clean, unclipped audio that falls silent after stop, that
controller profiles route MIDI correctly (including learning), and that stems
stay on the beat at any tempo, through mute, solo and rolls.

## Layout

```
Source/engine/   real-time audio and MIDI: sequencer, synth, stem deck,
                 control targets, controller profiles, MIDI router (no GUI code)
Source/ui/       window, step grid, knobs, stem panel, Controllers window
Tests/           headless engine tests
```

Design rule: nothing on the audio thread may allocate, wait on a lock or touch
the GUI. The UI and controllers talk to the engine through atomics; the stem
deck only ever *tries* a lock and skips a block rather than wait.

## Next

Planned for version 2: the five vintage-style synths (Mono, Jove, Pro, Wave,
Op6); longer patterns with chords, piano roll, live recording and per-step
parameter changes; audio tracks; samples on drum pads; group buses,
compressor, sidechain and automation.
