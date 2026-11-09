#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "AudioEngine.h"
#include <vector>

namespace bizgroove
{

/** The program bank: sounds saved one track at a time.

    A sound is a track's instrument and its settings: the test synth's knobs,
    the drum kit's sixteen drums, or the sampler's settings plus its
    recording. The track's volume, pan, EQ and sends belong to the mix and are
    not part of a sound.

    Factory sounds are built in and cannot be changed. Your own sounds are
    .bgsound files (a zip holding sound.json and, for the sampler, the
    recording) in Documents/BizGroove/Sounds/<instrument>, so they can be
    copied, backed up and shared.
*/
namespace Presets
{
    constexpr const char* fileExtension = ".bgsound";
    constexpr int formatVersion = 1;

    struct FactorySound
    {
        juce::String name;
        juce::var sound;   // as soundToVar()
    };

    /** The instrument's name, as in the Sound menu: "Test synth", "Drums", "Sampler". */
    juce::String instrumentName (int type);

    /** Documents/BizGroove/Sounds/<instrument>, created if missing. */
    juce::File userFolder (int type);

    /** Your saved sounds for an instrument, sorted by name. */
    juce::Array<juce::File> userSounds (int type);

    /** The built-in sounds for an instrument. */
    const std::vector<FactorySound>& factorySounds (int type);

    /** A track's sound as JSON (no audio data). */
    juce::var soundToVar (const Track& track);

    /** Sets a track's instrument and its settings from soundToVar(). The
        sampler's recording is left as it is. */
    void applySoundVar (Track& track, const juce::var& sound);

    /** Saves the track's sound, with the sampler's recording if it has one. */
    bool save (const Track& track, const juce::File& file, juce::String& error);

    /** Loads a .bgsound file into a track, with its recording if it has one. */
    bool load (AudioEngine& engine, int trackIndex, const juce::File& file,
               juce::AudioFormatManager& formats, juce::String& error);

    /** The file for a sound called `name` in the instrument's folder. Unless
        `allowExisting`, adds " 2", " 3"... when the name is taken. */
    juce::File fileForName (int type, const juce::String& name, bool allowExisting);
}

} // namespace bizgroove
