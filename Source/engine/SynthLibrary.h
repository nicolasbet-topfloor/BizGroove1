#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

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
                { "Brightness", "synth.cutoff",    "How bright or muffled the sound is. Left is dark, right is bright." },
                { "Edge",       "synth.resonance", "Adds a whistling, squelchy edge to the sound." },
                { "Wah",        "synth.envamount", "How much each note opens up with a 'wah' when it starts." },
                { "Thickness",  "synth.detune",    "Makes the sound fuller by putting its two tones slightly out of tune." },
                { "Fade in",    "synth.attack",    "How long a note takes to reach full volume after you press a key." },
                { "Punch",      "synth.decay",     "How long the first hit of each note lasts before it settles." },
                { "Fade out",   "synth.release",   "How long a note keeps sounding after you let go of the key." },
                { "Hold level", "synth.sustain",   "How loud a note stays while you keep holding the key." }
            } },
            {
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
                { "Length", "drum.length", "How long the selected drum rings: a short tick or a long boom." },
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
                { "Brightness", "sampler.brightness", "How bright or muffled it sounds." },
                { "Fade in",    "sampler.attack",     "How long each note takes to reach full volume." },
                { "Fade out",   "sampler.release",    "How long it keeps sounding after you let go of the key." },
                { "Hold level", "sampler.sustain",    "How loud a note stays while you keep holding the key." },
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
                { "Brightness",  "poly.cutoff",    "How bright or muffled the sound is. Left is dark, right is bright." },
                { "Edge",        "poly.resonance", "Adds a singing, whistling edge where the brightness is set." },
                { "Filter type", "poly.shape",     "Left keeps the lows (warm), the middle scoops out the middle (hollow), right keeps the highs (thin)." },
                { "Wah",         "poly.envamount", "How much each note opens up when it starts." },
                { "Thickness",   "poly.detune",    "Puts the two tones slightly out of tune with each other, for a fuller sound." },
                { "Fade in",     "poly.attack",    "How long a note takes to reach full volume after you press a key." },
                { "Fade out",    "poly.release",   "How long a note keeps sounding after you let go of the key." },
                { "Hold level",  "poly.sustain",   "How loud a note stays while you keep holding the key." }
            } },
            {
                { "Punch",              "poly.decay",      "How long the first hit of each note lasts before it settles." },
                { "Tone mix",           "poly.mix",        "Left: only the bright saw tone. Right: only the hollow square tone. In between: both." },
                { "Square width",       "poly.pulsewidth", "Changes the colour of the square tone, from hollow to thin and nasal." },
                { "Second tone octave", "poly.octave",     "Plays the square tone an octave lower, the same, or an octave higher." },
                { "Stack",              "poly.unison",     "Stacks up to four copies of each note, spread in tune, for a huge sound." },
                { "Glide",              "poly.glide",      "Slides from one note to the next instead of jumping." },
                { "Wobble speed",       "poly.lforate",    "How fast the wobble moves." },
                { "Wobble on brightness", "poly.lfofilter", "How much the wobble opens and closes the brightness." },
                { "Wobble on square",   "poly.lfopulse",   "How much the wobble moves the square's width, for a lively, chorus-like sound." },
                { "Noise",              "poly.noise",      "Adds a breath of noise, for wind and percussive sounds." },
                { "Volume",             "track.volume",    "How loud this track is." }
            }
        },
        {
            "wavetable",
            "Wavetable",
            "A wavetable synth: each note plays a wave that morphs smoothly through a set of eight (classic shapes, harmonics, vowel-like voices or digital sync sweeps), into a resonant filter. Stack up to four copies across left and right for wide pads and leads.",
            { {
                { "Position",   "wave.position",  "Moves through the wave set: each step along changes the colour of the sound." },
                { "Sweep",      "wave.sweep",     "How far each note travels through the wave set as it starts, before settling back." },
                { "Brightness", "wave.cutoff",    "How bright or muffled the sound is. Left is dark, right is bright." },
                { "Edge",       "wave.resonance", "Adds a singing, whistling edge where the brightness is set." },
                { "Thickness",  "wave.detune",    "Puts the stacked copies slightly out of tune with each other, for a fuller sound." },
                { "Fade in",    "wave.attack",    "How long a note takes to reach full volume after you press a key." },
                { "Fade out",   "wave.release",   "How long a note keeps sounding after you let go of the key." },
                { "Hold level", "wave.sustain",   "How loud a note stays while you keep holding the key." }
            } },
            {
                { "Wave set",              "wave.table",       "Which set of waves to play: Classic, Harmonics, Voices or Digital." },
                { "Punch",                 "wave.decay",       "How long the first hit of each note lasts before it settles." },
                { "Wah",                   "wave.envamount",   "How much each note opens up when it starts." },
                { "Stack",                 "wave.unison",      "Stacks up to four copies of each note, spread in tune and from left to right." },
                { "Sub",                   "wave.sub",         "Adds a pure tone an octave below, for weight." },
                { "Glide",                 "wave.glide",       "Slides from one note to the next instead of jumping." },
                { "Wobble speed",          "wave.lforate",     "How fast the wobble moves." },
                { "Wobble on position",    "wave.lfoposition", "How much the wobble moves through the wave set, for a sound that keeps changing." },
                { "Wobble on brightness",  "wave.lfofilter",   "How much the wobble opens and closes the brightness." },
                { "Noise",                 "wave.noise",       "Adds a breath of noise, for wind and percussive sounds." },
                { "Volume",                "track.volume",     "How loud this track is." }
            }
        },
        {
            "fm",
            "FM",
            "An FM synth: four pure tones that bend each other's pitch very fast, in one of eight layouts. From electric pianos and bells to glassy pads, punchy basses and metallic hits.",
            { {
                { "Brightness", "fm.brightness",  "How hard the tones bend each other: left is pure and soft, right is bright and complex." },
                { "Bite",       "fm.bitedecay",   "How long the bright attack of each note lasts before it mellows." },
                { "Layout",     "fm.algorithm",   "How the four tones are wired: which bend which, and which you hear." },
                { "Pitch 2",    "fm.ratio2",      "The pitch of tone 2, as a multiple of the note. Whole numbers sound musical, others like bells or metal." },
                { "Feedback",   "fm.feedback",    "Tone 4 bends itself: from pure to buzzy and noisy." },
                { "Fade in",    "fm.attack",      "How long a note takes to reach full volume after you press a key." },
                { "Fade out",   "fm.release",     "How long a note keeps sounding after you let go of the key." },
                { "Hold level", "fm.sustain",     "How loud a note stays while you keep holding the key." }
            } },
            {
                { "Pitch 1",                 "fm.ratio1",       "The pitch of tone 1, as a multiple of the note." },
                { "Pitch 3",                 "fm.ratio3",       "The pitch of tone 3, as a multiple of the note." },
                { "Pitch 4",                 "fm.ratio4",       "The pitch of tone 4, as a multiple of the note." },
                { "Level 1",                 "fm.level1",       "How loud tone 1 is, or how hard it bends, depending on the layout." },
                { "Level 2",                 "fm.level2",       "How loud tone 2 is, or how hard it bends, depending on the layout." },
                { "Level 3",                 "fm.level3",       "How loud tone 3 is, or how hard it bends, depending on the layout." },
                { "Level 4",                 "fm.level4",       "How loud tone 4 is, or how hard it bends, depending on the layout." },
                { "Brightness held",         "fm.bitesustain",  "How much of the brightness stays while you hold the key." },
                { "Punch",                   "fm.decay",        "How long the first hit of each note lasts before it settles." },
                { "Thickness",               "fm.detune",       "Puts the tones slightly out of tune with each other, for a chorused, beating sound." },
                { "Velocity to brightness",  "fm.velocity",     "How much harder hits sound brighter." },
                { "Wobble speed",            "fm.lforate",      "How fast the wobble moves." },
                { "Wobble on pitch",         "fm.lfopitch",     "Vibrato: how much the wobble moves the pitch." },
                { "Wobble on brightness",    "fm.lfobrightness","How much the wobble moves the brightness." },
                { "Volume",                  "track.volume",    "How loud this track is." }
            }
        },
        {
            "external",
            "External MIDI",
            "Plays gear outside BizGroove over MIDI: the M-VAVE FM-1, an app on an iPhone or iPad, or another synth. The steps and keys play it, the knobs send its standard sound controls, and its sound can come back through your sound card's input into the mixer.",
            { {
                { "Brightness", "ext.cc1", "Sends CC 74, the filter or brightness on most gear (and on the FM-1 with SLOOP)." },
                { "Edge",       "ext.cc2", "Sends CC 71, the filter's resonance." },
                { "Fade in",    "ext.cc3", "Sends CC 73, the attack." },
                { "Punch",      "ext.cc4", "Sends CC 75, the decay." },
                { "Fade out",   "ext.cc5", "Sends CC 72, the release." },
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
                { "Filter",     "noise.cutoff",    "Where the filter cuts: left is dark, right is open." },
                { "Edge",       "noise.resonance", "Adds a ringing peak where the filter is set." },
                { "Fade in",    "noise.attack",    "How long a note takes to reach full volume after you press a key." },
                { "Fade out",   "noise.release",   "How long a note keeps sounding after you let go of the key." }
            } },
            {
                { "Filter type",            "noise.filtermode", "Low-pass keeps the lows, band-pass a band in the middle, high-pass the highs." },
                { "Filter sweep",           "noise.envamount",  "How much each note opens (or closes) the filter as it plays: right opens, left closes." },
                { "Filter follows note",    "noise.keyfollow",  "How much the filter moves up with higher notes." },
                { "Grain",                  "noise.grain",      "Holds each value for a moment: from smooth to a coarse, steppy, digital noise." },
                { "Punch",                  "noise.decay",      "How long the first hit of each note lasts before it settles." },
                { "Hold level",             "noise.sustain",    "How loud a note stays while you keep holding the key." },
                { "Width",                  "noise.width",      "Mono, or a different noise on each side for a wide sound." },
                { "Velocity to volume",     "noise.velocity",   "How much harder hits are louder." },
                { "Wobble speed",           "noise.lforate",    "How fast the wobble moves." },
                { "Wobble on filter",       "noise.lfofilter",  "How much the wobble opens and closes the filter: wind and waves." },
                { "Wobble on colour",       "noise.lfocolour",  "How much the wobble moves the colour." },
                { "Level",                  "noise.level",      "The sound's own level: turn it up when a narrow filter makes the noise quiet." },
                { "Volume",                 "track.volume",     "How loud this track is." }
            }
        }
    };
    return library;
}

} // namespace bizgroove
