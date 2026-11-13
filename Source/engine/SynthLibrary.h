#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <map>

namespace bizgroove
{

/** One knob as a person sees it: a plain name, what it drives, and a one-line
    explanation shown on hover. */
struct SynthKnob
{
    juce::String label;
    juce::String target;   // a ControlTargets id
    juce::String help;
};

/** A sound engine the track can load from the Sound menu. Its eight main
    knobs line up with the eight knobs on the Launchkey Mini; anything more
    sits behind the "More" button. */
struct SynthDefinition
{
    juce::String id;
    juce::String name;
    juce::String description;
    std::array<SynthKnob, 8> knobs;
    std::vector<SynthKnob> more;
};

/** Every synth BizGroove can load. For now: the test synth only. */
inline const std::vector<SynthDefinition>& synthLibrary()
{
    static const std::vector<SynthDefinition> library {
        {
            "test",
            "Test synth",
            "A simple synth: two slightly detuned buzzy tones through a filter that can be opened and closed.",
            { {
                { "Cutoff",     "synth.cutoff",    "How bright or muffled the sound is. Left is dark, right is bright." },
                { "Resonance",  "synth.resonance", "Adds a whistling, squelchy edge to the sound." },
                { "Env amount", "synth.envamount", "How much each note opens up with a 'wah' when it starts." },
                { "Detune",     "synth.detune",    "Makes the sound fuller by putting its two tones slightly out of tune." },
                { "Attack",     "synth.attack",    "How long a note takes to reach full volume after you press a key." },
                { "Decay",      "synth.decay",     "How long the first hit of each note lasts before it settles." },
                { "Release",    "synth.release",   "How long a note keeps sounding after you let go of the key." },
                { "Sustain",    "synth.sustain",   "How loud a note stays while you keep holding the key." }
            } },
            {
                { "Glide",      "synth.glide",     "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode", "synth.glidemode", "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "Volume",     "track.volume",    "How loud this track is." }
            }
        },
        {
            "drums",
            "Drums",
            "Sixteen synthesised drums in General MIDI order, from the kick on the lowest C. Play a key to choose the drum that the knobs and pads edit.",
            { {
                { "Pitch",  "drum.pitch",  "Tunes the selected drum higher or lower, up to 12 semitones either way." },
                { "Swing",  "drum.swing",  "Shuffles the groove of the whole kit, from straight to swung." },
                { "Decay",  "drum.length", "How long the selected drum rings: a short tick or a long boom." },
                { "Tone",   "drum.tone",   "How bright or dull the selected drum is." },
                { "Snap",   "drum.snap",   "How sharp the click at the start of each hit is." },
                { "Drive",  "drum.drive",  "Adds grit and dirt to the selected drum." },
                { "Pan",    "drum.pan",    "Places the selected drum left or right." },
                { "Volume", "drum.volume", "How loud the selected drum is." }
            } },
            {
                { "Choke",    "drum.choke",    "On: the closed and pedal hi-hats cut off the open hi-hat, as on a real kit." },
                { "Velocity", "drum.velocity", "How much harder hits sound louder. At 0, every hit is equally loud." },
                { "Model",    "drum.model",    "How the selected drum is made: Analogue (classic drum machine), FM (bell-like, digital tones) or Metal (clashing, metallic tones)." },
                { "Colour",   "drum.colour",   "The character of the FM or Metal model: from soft and round to bright and clangy." }
            }
        },
        {
            "sampler",
            "Sampler",
            "Plays a recording you load. On C4 it plays at its own pitch; other keys play it higher or lower, or, with slices, play different parts of it.",
            { {
                { "Start",      "sampler.start",      "Where in the recording playback begins." },
                { "Length",     "sampler.length",     "How much of the recording plays, from the start point." },
                { "Pitch",      "sampler.pitch",      "Plays the recording higher or lower, up to 24 semitones either way." },
                { "Cutoff",     "sampler.brightness", "How bright or muffled it sounds." },
                { "Attack",     "sampler.attack",     "How long each note takes to reach full volume." },
                { "Release",    "sampler.release",    "How long it keeps sounding after you let go of the key." },
                { "Sustain",    "sampler.sustain",    "How loud a note stays while you keep holding the key." },
                { "Volume",     "track.volume",       "How loud this track is." }
            } },
            {
                { "Loop",       "sampler.loop",       "On: the chosen part repeats for as long as you hold the key." },
                { "Reverse",    "sampler.reverse",    "On: plays the recording backwards." },
                { "Slices",     "sampler.slices",     "Cuts the chosen part into equal pieces, one per key upwards from C4." },
                { "Pan",        "track.pan",          "Places this track left or right." }
            }
        },
        {
            "poly",
            "Poly",
            "An eight-voice analogue-style polysynth: two tones per note, a saw and a square, through a filter that sweeps from warm (low-pass) through hollow (notch) to thin (high-pass). For brass, strings, pads and big leads.",
            { {
                { "Cutoff",      "poly.cutoff",    "How bright or muffled the sound is. Left is dark, right is bright." },
                { "Resonance",   "poly.resonance", "Adds a singing, whistling edge where the brightness is set." },
                { "Filter type", "poly.shape",     "Left keeps the lows (warm), the middle scoops out the middle (hollow), right keeps the highs (thin)." },
                { "Env amount",  "poly.envamount", "How much each note opens up when it starts." },
                { "Detune",      "poly.detune",    "Puts the two tones slightly out of tune with each other, for a fuller sound." },
                { "Attack",      "poly.attack",    "How long a note takes to reach full volume after you press a key." },
                { "Release",     "poly.release",   "How long a note keeps sounding after you let go of the key." },
                { "Sustain",     "poly.sustain",   "How loud a note stays while you keep holding the key." }
            } },
            {
                { "Decay",              "poly.decay",      "How long the first hit of each note lasts before it settles." },
                { "Osc mix",            "poly.mix",        "Left: only the bright saw tone. Right: only the hollow square tone. In between: both." },
                { "Pulse width",        "poly.pulsewidth", "Changes the colour of the square tone, from hollow to thin and nasal." },
                { "Osc 2 octave",       "poly.octave",     "Plays the square tone an octave lower, the same, or an octave higher." },
                { "Unison",             "poly.unison",     "Stacks up to four copies of each note, spread in tune, for a huge sound." },
                { "Glide",              "poly.glide",      "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode",         "poly.glidemode",  "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "LFO rate",           "poly.lforate",    "How fast the wobble moves." },
                { "LFO to cutoff",        "poly.lfofilter", "How much the wobble opens and closes the brightness." },
                { "LFO to PW",          "poly.lfopulse",   "How much the wobble moves the square's width, for a lively, chorus-like sound." },
                { "Noise",              "poly.noise",      "Adds a breath of noise, for wind and percussive sounds." },
                { "Volume",             "track.volume",    "How loud this track is." }
            }
        },
        {
            "wavetable",
            "Wavetable",
            "A wavetable synth: each note plays a wave that morphs smoothly through a set of eight (classic shapes, harmonics, vowel-like voices or digital sync sweeps), into a resonant filter. Stack up to four copies across left and right for wide pads and leads.",
            { {
                { "Wave position", "wave.position",  "Moves through the wave set: each step along changes the colour of the sound." },
                { "Env to position", "wave.sweep",     "How far each note travels through the wave set as it starts, before settling back." },
                { "Cutoff",     "wave.cutoff",    "How bright or muffled the sound is. Left is dark, right is bright." },
                { "Resonance",  "wave.resonance", "Adds a singing, whistling edge where the brightness is set." },
                { "Detune",     "wave.detune",    "Puts the stacked copies slightly out of tune with each other, for a fuller sound." },
                { "Attack",     "wave.attack",    "How long a note takes to reach full volume after you press a key." },
                { "Release",    "wave.release",   "How long a note keeps sounding after you let go of the key." },
                { "Sustain",    "wave.sustain",   "How loud a note stays while you keep holding the key." }
            } },
            {
                { "Wavetable",             "wave.table",       "Which set of waves to play: Classic, Harmonics, Voices or Digital." },
                { "Decay",                 "wave.decay",       "How long the first hit of each note lasts before it settles." },
                { "Env amount",            "wave.envamount",   "How much each note opens up when it starts." },
                { "Unison",                "wave.unison",      "Stacks up to four copies of each note, spread in tune and from left to right." },
                { "Sub osc",               "wave.sub",         "Adds a pure tone an octave below, for weight." },
                { "Glide",                 "wave.glide",       "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode",            "wave.glidemode",   "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "LFO rate",              "wave.lforate",     "How fast the wobble moves." },
                { "LFO to position",       "wave.lfoposition", "How much the wobble moves through the wave set, for a sound that keeps changing." },
                { "LFO to cutoff",         "wave.lfofilter",   "How much the wobble opens and closes the brightness." },
                { "Noise",                 "wave.noise",       "Adds a breath of noise, for wind and percussive sounds." },
                { "Volume",                "track.volume",     "How loud this track is." }
            }
        },
        {
            "fm",
            "FM",
            "An FM synth: four pure tones that bend each other's pitch very fast, in one of eight layouts. From electric pianos and bells to glassy pads, punchy basses and metallic hits.",
            { {
                { "Mod index",  "fm.brightness",  "How hard the tones bend each other: left is pure and soft, right is bright and complex." },
                { "Mod decay",  "fm.bitedecay",   "How long the bright attack of each note lasts before it mellows." },
                { "Algorithm",  "fm.algorithm",   "How the four tones are wired: which bend which, and which you hear." },
                { "Ratio 2",    "fm.ratio2",      "The pitch of tone 2, as a multiple of the note. Whole numbers sound musical, others like bells or metal." },
                { "Feedback",   "fm.feedback",    "Tone 4 bends itself: from pure to buzzy and noisy." },
                { "Attack",     "fm.attack",      "How long a note takes to reach full volume after you press a key." },
                { "Release",    "fm.release",     "How long a note keeps sounding after you let go of the key." },
                { "Sustain",    "fm.sustain",     "How loud a note stays while you keep holding the key." }
            } },
            {
                { "Ratio 1",                 "fm.ratio1",       "The pitch of tone 1, as a multiple of the note." },
                { "Ratio 3",                 "fm.ratio3",       "The pitch of tone 3, as a multiple of the note." },
                { "Ratio 4",                 "fm.ratio4",       "The pitch of tone 4, as a multiple of the note." },
                { "Op 1 level",              "fm.level1",       "How loud tone 1 is, or how hard it bends, depending on the layout." },
                { "Op 2 level",              "fm.level2",       "How loud tone 2 is, or how hard it bends, depending on the layout." },
                { "Op 3 level",              "fm.level3",       "How loud tone 3 is, or how hard it bends, depending on the layout." },
                { "Op 4 level",              "fm.level4",       "How loud tone 4 is, or how hard it bends, depending on the layout." },
                { "Mod sustain",             "fm.bitesustain",  "How much of the brightness stays while you hold the key." },
                { "Decay",                   "fm.decay",        "How long the first hit of each note lasts before it settles." },
                { "Detune",                  "fm.detune",       "Puts the tones slightly out of tune with each other, for a chorused, beating sound." },
                { "Velocity to index",       "fm.velocity",     "How much harder hits sound brighter." },
                { "LFO rate",                "fm.lforate",      "How fast the wobble moves." },
                { "LFO to pitch",            "fm.lfopitch",     "Vibrato: how much the wobble moves the pitch." },
                { "LFO to index",            "fm.lfobrightness","How much the wobble moves the brightness." },
                { "Glide",                   "fm.glide",        "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode",              "fm.glidemode",    "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "Volume",                  "track.volume",    "How loud this track is." }
            }
        },
        {
            "external",
            "External MIDI",
            "Plays gear outside BizGroove over MIDI: the M-VAVE FM-1, an app on an iPhone or iPad, or another synth. The steps and keys play it, the knobs send its standard sound controls, and its sound can come back through your sound card's input into the mixer.",
            { {
                { "Cutoff",     "ext.cc1", "Sends CC 74, the filter or brightness on most gear (and on the FM-1 with SLOOP)." },
                { "Resonance",  "ext.cc2", "Sends CC 71, the filter's resonance." },
                { "Attack",     "ext.cc3", "Sends CC 73, the attack." },
                { "Decay",      "ext.cc4", "Sends CC 75, the decay." },
                { "Release",    "ext.cc5", "Sends CC 72, the release." },
                { "Glide",      "ext.cc6", "Sends CC 5, the glide (portamento) time." },
                { "Reverb",     "ext.cc7", "Sends CC 91, the reverb send." },
                { "Delay",      "ext.cc8", "Sends CC 94, the delay send (on the FM-1 with SLOOP; CC 94 is a general effect send elsewhere)." }
            } },
            {
                { "Program",                "ext.program",   "Sends a program change: picks a sound on the gear." },
                { "Channel",                "ext.channel",   "The MIDI channel the gear listens on." },
                { "Drum steps",             "ext.drums",     "On: the steps and pads play drum notes (MIDI notes 36 to 51), as on a Drums track. For the FM-1's drum track or a drum machine." },
                { "Hear input",             "ext.monitor",   "Plays your sound card's input through this track, so the gear's sound goes through the mixer. Turn off if you hear feedback." },
                { "Inputs",                 "ext.inputpair", "Which pair of sound card inputs the gear is connected to." },
                { "Knob 1 CC",           "ext.ccnum1",     "Which CC number knob 1 sends, if your gear uses a different one." },
                { "Knob 2 CC",           "ext.ccnum2",     "Which CC number knob 2 sends, if your gear uses a different one." },
                { "Knob 3 CC",           "ext.ccnum3",     "Which CC number knob 3 sends, if your gear uses a different one." },
                { "Knob 4 CC",           "ext.ccnum4",     "Which CC number knob 4 sends, if your gear uses a different one." },
                { "Knob 5 CC",           "ext.ccnum5",     "Which CC number knob 5 sends, if your gear uses a different one." },
                { "Knob 6 CC",           "ext.ccnum6",     "Which CC number knob 6 sends, if your gear uses a different one." },
                { "Knob 7 CC",           "ext.ccnum7",     "Which CC number knob 7 sends, if your gear uses a different one." },
                { "Knob 8 CC",           "ext.ccnum8",     "Which CC number knob 8 sends, if your gear uses a different one." },
                { "Volume",                 "track.volume",  "How loud the gear's sound is in the mix (when Hear input is on)." }
            }
        },
        {
            "noise",
            "Noise",
            "A noise synth: noise of every colour, from deep brown through pink and white to hissing violet, made into sound in five ways (pure, FM, ring, digital and crackle), then given a pitch, filtered and shaped. For wind, sea, risers, snares and hats, radio static, lasers and metallic textures.",
            { {
                { "Colour",     "noise.colour",    "The colour of the noise: brown (deep, rumbling), pink, white, blue, violet (thin, hissing)." },
                { "Source",     "noise.source",    "How the noise becomes sound: pure coloured noise, FM noise (a tone shaken by noise), ring noise (a band around the note), digital noise (old sound chips) or crackle." },
                { "Modifier",   "noise.mod",       "Changes with the source: how hard the noise shakes the tone (FM), tone or noise (ring), long or metallic loop (digital), how many clicks (crackle)." },
                { "Pitch",      "noise.tone",      "Tunes the noise to the note you play: from plain noise to a singing, whistling tone." },
                { "Cutoff",     "noise.cutoff",    "Where the filter cuts: left is dark, right is open." },
                { "Resonance",  "noise.resonance", "Adds a ringing peak where the filter is set." },
                { "Attack",     "noise.attack",    "How long a note takes to reach full volume after you press a key. At the left, it starts at once: a hit." },
                { "Release",    "noise.release",   "How long a note keeps sounding after you let go of the key." }
            } },
            {
                { "Filter type",            "noise.filtermode", "Low-pass keeps the lows, band-pass a band in the middle, high-pass the highs." },
                { "Env amount",             "noise.envamount",  "How much each note opens (or closes) the filter as it plays: right opens, left closes." },
                { "Key tracking",           "noise.keyfollow",  "How much the filter moves up with higher notes." },
                { "Grain",                  "noise.grain",      "Holds each value for a moment: from smooth to a coarse, steppy, digital noise." },
                { "Decay",                  "noise.decay",      "How long the first hit of each note lasts before it settles to the Sustain level. Short, with Sustain at 0: a drum hit." },
                { "Sustain",                "noise.sustain",    "How loud a note stays while you keep holding the key. At 0 every note is a hit that dies away." },
                { "Width",                  "noise.width",      "Mono, or a different noise on each side for a wide sound." },
                { "Velocity",               "noise.velocity",   "How much harder hits are louder." },
                { "LFO rate",               "noise.lforate",    "How fast the noise's own wobble (LFO) moves." },
                { "LFO to cutoff",          "noise.lfofilter",  "How much the wobble opens and closes the filter: wind and waves." },
                { "LFO to colour",          "noise.lfocolour",  "How much the wobble moves the colour." },
                { "Level",                  "noise.level",      "The sound's own level: turn it up when a narrow filter makes the noise quiet." },
                { "Glide",                  "noise.glide",      "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode",             "noise.glidemode",  "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "Volume",                 "track.volume",     "How loud this track is." }
            }
        },
        {
            "voice",
            "Voice",
            "A speaking synth, like the Speak & Spell and the Monomachine's VO: it says your text, in Spanish or English, at the pitch of the note you play. The whole text on each note, or one word per note so a sequence sings it. Write the text with the Text button.",
            { {
                { "Mode",       "voice.mode",       "What each note says: the whole text, the whole text over and over while you hold the key, the next word, the next word sung (its last vowel lasts as long as the note), the next syllable, the next single sound, or (Sounds on keys) the sound of the key you play: vowels from C2, then consonants." },
                { "Speed",      "voice.speed",      "How fast it talks." },
                { "Intonation", "voice.intonation", "Left: a flat robot voice that only follows your notes. Right: the rise and fall of speech, around your notes." },
                { "Size",       "voice.size",       "The size of the mouth: left is tiny (chipmunk), right is a giant." },
                { "Buzz",       "voice.buzz",       "Left: a soft human voice. Right: a buzzy machine voice." },
                { "Whisper",    "voice.whisper",    "Turns the voice into a whisper: breath instead of buzz." },
                { "Lo-fi",      "voice.lofi",       "Fewer samples and fewer levels, for the crunchy sound of old speaking toys." },
                { "Release",    "voice.release",    "How long the voice takes to fade when it stops." }
            } },
            {
                { "Language",               "voice.language",   "Spanish or English: how the text is read." },
                { "Keys note",              "voice.keyspitch",  "Sounds on keys: the note every sound is said on, since the keys choose the sound." },
                { "Attack",                 "voice.attack",     "How long the voice takes to come in." },
                { "Level",                  "voice.level",      "The voice's own level." },
                { "Velocity",               "voice.velocity",   "How much harder hits are louder." },
                { "Glide",                  "voice.glide",      "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode",             "voice.glidemode",  "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "Volume",                 "track.volume",     "How loud this track is." }
            }
        },
        {
            "vocoder",
            "Vocoder",
            "A vocoder: the synth you play on the keys speaks with another sound's words. It listens to a microphone, to the Voice tracks and to any track, mixed with the first four knobs. Mute the track it listens to if you only want to hear the vocoder.",
            { {
                { "Voice in",   "vocoder.voice",     "How much it listens to the Voice tracks (the speaking synth), all of them mixed." },
                { "Mic in",     "vocoder.mic",       "How much it listens to the sound card's input: sing or talk into a microphone." },
                { "Track in",   "vocoder.track",     "How much it listens to the track chosen with From track (its sound before its effects and volume)." },
                { "From track", "vocoder.source",    "Which track Track in listens to." },
                { "Bands",      "vocoder.bands",     "How many bands it splits the voice into: few for a rough robot, many for clear words." },
                { "Shift",      "vocoder.shift",     "Moves the voice's formants down (a giant) or up (a tiny voice)." },
                { "Sibilance",  "vocoder.sibilance", "Lets the voice's own hiss through (s, t, ch), for clearer words." },
                { "Carrier",    "vocoder.wave",      "The synth that speaks: a saw, a square, a thin pulse or three saws (rich, like the classic string vocoders)." }
            } },
            {
                { "Response",               "vocoder.response", "How quickly the bands follow the voice: tight and choppy, or smooth." },
                { "Detune",                 "vocoder.detune",   "How far apart the carrier's saws are: from one clean tone to a wide chorus." },
                { "Breath",                 "vocoder.noise",    "Adds noise to the carrier, so whispers and hiss come through too." },
                { "Attack",                 "vocoder.attack",   "How long a note takes to come in." },
                { "Release",                "vocoder.release",  "How long a note takes to fade after you let go." },
                { "Level",                  "vocoder.level",    "The vocoder's own level." },
                { "Velocity",               "vocoder.velocity", "How much harder hits are louder." },
                { "Glide",                  "vocoder.glide",    "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode",             "vocoder.glidemode", "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "Volume",                 "track.volume",     "How loud this track is." }
            }
        },
        {
            "granular",
            "Granular",
            "A granular synth: a cloud of tiny grains read from a sound and played at the note's pitch. The sound can be one of its own (glass, choir, strings, noise, metal) or a recording you load. Freeze a moment, smear it into a pad, scatter it into rain.",
            { {
                { "Source",     "gran.source",   "Where the grains come from: Glass, Choir, Strings, Noise, Metal, or Your recording (load one with Load...)." },
                { "Position",   "gran.position", "Where in the sound the grains are read, from its start to its end." },
                { "Grain size", "gran.size",     "How long each grain lasts: tiny for buzzy, glitchy sounds, long for smooth, smeared ones." },
                { "Density",    "gran.density",  "How many grains start every second: a few for rain, many for a thick cloud." },
                { "Spread",     "gran.spread",   "How far around the position the grains are taken: at 0 the sound freezes, higher it blurs." },
                { "Pitch",      "gran.pitch",    "Moves every grain up or down, in semitones." },
                { "Attack",     "gran.attack",   "How long a note takes to reach full volume after you press a key." },
                { "Release",    "gran.release",  "How long a note keeps sounding after you let go of the key." }
            } },
            {
                { "Scan",                   "gran.scan",      "Moves the position by itself as the note plays, forward or back, so the sound keeps changing." },
                { "Detune",                 "gran.detune",    "Puts each grain a little out of tune, at random, for a chorus-like shimmer." },
                { "Reverse",                "gran.reverse",   "How many grains play backwards." },
                { "Stereo spray",           "gran.spray",     "Scatters the grains from the middle out to the left and right." },
                { "Cutoff",                 "gran.cutoff",    "Where the low-pass filter cuts: left is dark, right is open." },
                { "Resonance",              "gran.resonance", "Adds a ringing peak where the filter is set." },
                { "Decay",                  "gran.decay",     "How long the start of each note takes to settle to the Sustain level." },
                { "Sustain",                "gran.sustain",   "How loud a note stays while you keep holding the key." },
                { "Level",                  "gran.level",     "The sound's own level." },
                { "Velocity",               "gran.velocity",  "How much harder hits are louder." },
                { "Glide",                  "gran.glide",     "Slides from one note to the next instead of jumping: how long the slide takes (per octave, when it glides by distance)." },
                { "Glide mode",             "gran.glidemode", "Fixed time: every slide takes the glide time. By distance: wider jumps slide for longer, the glide time per octave." },
                { "Volume",                 "track.volume",   "How loud this track is." }
            }
        }
    };
    return library;
}

/** A section of an instrument's Sound settings window: a title and its knobs.
    The envelope section also draws the envelope's shape. */
struct SoundSection
{
    juce::String title;
    std::vector<SynthKnob> knobs;
    bool envelope = false;   // its first four knobs are Attack, Decay, Sustain and Release
};

/** Every setting of an instrument, by section, for its Sound settings window
    (empty: the instrument still uses the older More window). Each knob is the
    one on the panel or under More, with the same name and help. */
inline const std::vector<SoundSection>& soundSections (int type)
{
    // Each instrument's sections: a title, its knobs (by target), and whether it is the envelope (Attack, Decay, Sustain, Release first).
    struct Plan { const char* title; std::vector<const char*> ids; bool envelope = false; };
    static const std::map<juce::String, std::vector<Plan>> plans {
        { "test", {
            { "Oscillator", { "synth.detune" } },
            { "Filter",     { "synth.cutoff", "synth.resonance", "synth.envamount" } },
            { "Envelope",   { "synth.attack", "synth.decay", "synth.sustain", "synth.release" }, true },
            { "Output",     { "synth.glide", "synth.glidemode", "track.volume" } } } },
        { "drums", {
            { "Drum",       { "drum.model", "drum.pitch", "drum.length", "drum.tone", "drum.snap", "drum.colour" } },
            { "Mix",        { "drum.drive", "drum.pan", "drum.volume" } },
            { "Kit",        { "drum.swing", "drum.choke", "drum.velocity" } } } },
        { "sampler", {
            { "Sample",     { "sampler.start", "sampler.length", "sampler.pitch", "sampler.loop", "sampler.reverse", "sampler.slices" } },
            { "Filter",     { "sampler.brightness" } },
            { "Envelope",   { "sampler.attack", "sampler.sustain", "sampler.release" } },
            { "Output",     { "track.volume", "track.pan" } } } },
        { "poly", {
            { "Oscillators", { "poly.mix", "poly.pulsewidth", "poly.octave", "poly.unison", "poly.detune", "poly.noise" } },
            { "Filter",      { "poly.shape", "poly.cutoff", "poly.resonance", "poly.envamount" } },
            { "Envelope",    { "poly.attack", "poly.decay", "poly.sustain", "poly.release" }, true },
            { "Modulation",  { "poly.lforate", "poly.lfofilter", "poly.lfopulse" } },
            { "Output",      { "poly.glide", "poly.glidemode", "track.volume" } } } },
        { "wavetable", {
            { "Oscillators", { "wave.table", "wave.position", "wave.sweep", "wave.unison", "wave.detune", "wave.sub", "wave.noise" } },
            { "Filter",      { "wave.cutoff", "wave.resonance", "wave.envamount" } },
            { "Envelope",    { "wave.attack", "wave.decay", "wave.sustain", "wave.release" }, true },
            { "Modulation",  { "wave.lforate", "wave.lfoposition", "wave.lfofilter" } },
            { "Output",      { "wave.glide", "wave.glidemode", "track.volume" } } } },
        { "fm", {
            { "Operators",   { "fm.algorithm", "fm.feedback", "fm.ratio1", "fm.ratio2", "fm.ratio3", "fm.ratio4" } },
            { "Levels",      { "fm.level1", "fm.level2", "fm.level3", "fm.level4", "fm.detune" } },
            { "Mod index",   { "fm.brightness", "fm.bitedecay", "fm.bitesustain", "fm.velocity" } },
            { "Envelope",    { "fm.attack", "fm.decay", "fm.sustain", "fm.release" }, true },
            { "Modulation",  { "fm.lforate", "fm.lfopitch", "fm.lfobrightness" } },
            { "Output",      { "fm.glide", "fm.glidemode", "track.volume" } } } },
        { "external", {
            { "Sound (CC)",  { "ext.cc1", "ext.cc2", "ext.cc3", "ext.cc4", "ext.cc5", "ext.cc6", "ext.cc7", "ext.cc8" } },
            { "Setup",       { "ext.program", "ext.channel", "ext.drums", "ext.monitor", "ext.inputpair", "track.volume" } },
            { "CC numbers",  { "ext.ccnum1", "ext.ccnum2", "ext.ccnum3", "ext.ccnum4", "ext.ccnum5", "ext.ccnum6", "ext.ccnum7", "ext.ccnum8" } } } },
        { "noise", {
            { "Source",     { "noise.colour", "noise.source", "noise.mod", "noise.grain", "noise.tone" } },
            { "Filter",     { "noise.filtermode", "noise.cutoff", "noise.resonance", "noise.envamount", "noise.keyfollow" } },
            { "Envelope",   { "noise.attack", "noise.decay", "noise.sustain", "noise.release", "noise.velocity" }, true },
            { "Modulation", { "noise.lforate", "noise.lfofilter", "noise.lfocolour" } },
            { "Output",     { "noise.level", "noise.width", "noise.glide", "noise.glidemode", "track.volume" } } } },
        { "voice", {
            { "Speech",     { "voice.mode", "voice.language", "voice.speed", "voice.intonation", "voice.keyspitch" } },
            { "Voice",      { "voice.size", "voice.buzz", "voice.whisper", "voice.lofi" } },
            { "Envelope",   { "voice.attack", "voice.release" } },
            { "Output",     { "voice.level", "voice.velocity", "voice.glide", "voice.glidemode", "track.volume" } } } },
        { "vocoder", {
            { "Listens to", { "vocoder.voice", "vocoder.mic", "vocoder.track", "vocoder.source" } },
            { "Bands",      { "vocoder.bands", "vocoder.shift", "vocoder.sibilance", "vocoder.response" } },
            { "Carrier",    { "vocoder.wave", "vocoder.detune", "vocoder.noise" } },
            { "Envelope",   { "vocoder.attack", "vocoder.release" } },
            { "Output",     { "vocoder.level", "vocoder.velocity", "vocoder.glide", "vocoder.glidemode", "track.volume" } } } },
        { "granular", {
            { "Grains",     { "gran.source", "gran.position", "gran.size", "gran.density", "gran.spread", "gran.scan" } },
            { "Pitch",      { "gran.pitch", "gran.detune", "gran.reverse", "gran.spray" } },
            { "Filter",     { "gran.cutoff", "gran.resonance" } },
            { "Envelope",   { "gran.attack", "gran.decay", "gran.sustain", "gran.release" }, true },
            { "Output",     { "gran.level", "gran.velocity", "gran.glide", "gran.glidemode", "track.volume" } } } }
    };

    static const std::vector<std::vector<SoundSection>> all = [] {
        std::vector<std::vector<SoundSection>> out;
        for (const auto& def : synthLibrary())
        {
            std::vector<SoundSection> sections;
            const auto it = plans.find (def.id);
            if (it != plans.end())
                for (const auto& plan : it->second)
                {
                    SoundSection section { plan.title, {}, plan.envelope };
                    for (const char* id : plan.ids)
                    {
                        const SynthKnob* found = nullptr;
                        for (const auto& k : def.knobs) if (k.target == id) found = &k;
                        for (const auto& k : def.more)  if (k.target == id) found = &k;
                        jassert (found != nullptr);   // a section names a knob the instrument does not have
                        if (found != nullptr) section.knobs.push_back (*found);
                    }
                    sections.push_back (section);
                }
            out.push_back (sections);
        }
        return out;
    }();
    static const std::vector<SoundSection> none;
    return juce::isPositiveAndBelow (type, (int) all.size()) ? all[(size_t) type] : none;
}

/** The controller's pages in the Sound settings window: neighbouring sections share
    a page while they fit on the eight knobs. The page (from 1) of a section. */
inline int soundPageOfSection (const std::vector<SoundSection>& sections, int section)
{
    int page = 1, used = 0;
    for (int s = 0; s < (int) sections.size(); ++s)
    {
        const int n = juce::jmin (8, (int) sections[(size_t) s].knobs.size());
        if (used > 0 && used + n > 8) { ++page; used = 0; }
        if (s == section) return page;
        used += n;
    }
    return page;
}

inline int soundPageCount (const std::vector<SoundSection>& sections)
{
    return sections.empty() ? 0 : soundPageOfSection (sections, (int) sections.size() - 1);
}

/** Which knob the controller's knob k (0-7) turns on a page; nullptr if none. */
inline const SynthKnob* soundPageKnob (const std::vector<SoundSection>& sections, int page, int k)
{
    int index = 0;
    for (int s = 0; s < (int) sections.size(); ++s)
    {
        if (soundPageOfSection (sections, s) != page)
            continue;
        const auto& knobs = sections[(size_t) s].knobs;
        for (int i = 0; i < juce::jmin (8, (int) knobs.size()); ++i, ++index)
            if (index == k)
                return &knobs[(size_t) i];
    }
    return nullptr;
}

} // namespace bizgroove
