#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "AudioEngine.h"

namespace bizgroove
{

/** Saving and opening BizGroove projects.

    A project file (.bizgroove) is a zip archive holding project.json (every
    setting, pattern, scene, the song and the mixer) and a samples folder with
    the sampler and stem-deck recordings, so one file carries everything.
*/
namespace Project
{
    constexpr const char* fileExtension = ".bizgroove";
    constexpr int formatVersion = 1;

    /** Everything except audio data, as JSON. */
    juce::var toVar (AudioEngine& engine);

    /** Applies settings from toVar(). Audio data is loaded separately. */
    void applyVar (AudioEngine& engine, const juce::var& state);

    /** The settings as one string, to tell whether anything changed. */
    juce::String fingerprint (AudioEngine& engine);

    bool save (AudioEngine& engine, const juce::File& file, juce::String& error);
    bool open (AudioEngine& engine, const juce::File& file, juce::AudioFormatManager& formats, juce::String& error);

    /** Reads an audio file's bytes into a buffer. */
    bool decodeAudio (const juce::MemoryBlock& data, juce::AudioFormatManager& formats,
                      juce::AudioBuffer<float>& out, double& sampleRate, juce::String& error);

    /** Loads a recording into a track's sampler (keeps the bytes for saving). */
    bool loadSampleIntoTrack (AudioEngine& engine, int track, const juce::String& name, const juce::MemoryBlock& data,
                              juce::AudioFormatManager& formats, juce::String& error);

    /** Puts a recording on a drum pad of a track (keeps the bytes for saving). */
    bool loadSampleIntoPad (AudioEngine& engine, int track, int drum, const juce::String& name, const juce::MemoryBlock& data,
                            juce::AudioFormatManager& formats, juce::String& error);

    /** Takes the recording off a drum pad, so it plays its synthesised drum again. */
    void clearPad (AudioEngine& engine, int track, int drum);

    /** Loads a recording into a stem slot (keeps the bytes for saving). */
    bool loadStem (AudioEngine& engine, int slot, const juce::String& name, const juce::MemoryBlock& data,
                   juce::AudioFormatManager& formats, juce::String& error);
}

/** Export to files for mastering and for other software. */
namespace Exporter
{
    bool writeWav (const juce::AudioBuffer<float>& audio, double sampleRate, int bitDepth,
                   const juce::File& file, juce::String& error);

    /** A type-1 MIDI file: one track per BizGroove track that has notes,
        drums on channel 10 with General MIDI notes, the tempo included. */
    bool writeMidi (AudioEngine& engine, bool playSong, int bars, const juce::File& file, juce::String& error);
}

} // namespace bizgroove
