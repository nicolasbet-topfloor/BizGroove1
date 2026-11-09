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
        }
    };
    return library;
}

} // namespace bizgroove
