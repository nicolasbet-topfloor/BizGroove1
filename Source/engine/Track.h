#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"
#include "PolySynth.h"
#include "WavetableSynth.h"
#include "FmSynth.h"
#include "NoiseSynth.h"
#include "VoiceSynth.h"
#include "VocoderSynth.h"
#include "ExternalMidi.h"
#include "Effects.h"
#include "DrumKit.h"
#include "Sampler.h"
#include "Mixer.h"

namespace bizgroove
{

/** One of the 16 tracks: which sound it plays, that sound's settings, and
    its volume and mute. The audio objects inside are used only by the audio
    thread; the atomics are shared with the UI and controllers. */
struct Track
{
    enum Type { testSynth = 0, drums = 1, sampler = 2, poly = 3, wavetable = 4, fm = 5, external = 6, noise = 7, voice = 8, vocoder = 9 };   // index into synthLibrary()
    static constexpr int numTypes = 10;

    /** Each instrument comes out at its own natural level; this evens them out, so
        switching sounds doesn't jump in volume. Measured on the factory sounds
        (a held note, against the test synth): the Poly was about 6 dB quieter,
        the Wavetable 2 dB quieter and the FM synth 2.5 dB louder. */
    static float levelTrim (int instrument)
    {
        switch (instrument)
        {
            case poly:      return 2.0f;    // +6 dB
            case wavetable: return 1.26f;   // +2 dB
            case fm:        return 0.75f;   // -2.5 dB
            default:        return 1.0f;
        }
    }

    static constexpr int numSynthVoices = 16;
    static constexpr int numPolyVoices = 8;
    static constexpr int numWaveVoices = 8;
    static constexpr int numFmVoices = 8;
    static constexpr int numNoiseVoices = 8;
    static constexpr int numSpeechVoices = 4;
    static constexpr int numVocoderVoices = 8;

    Track (const Performance& perf)
    {
        for (int i = 0; i < numSynthVoices; ++i)
            synth.addVoice (new TestSynthVoice (synthParams, perf));
        synth.addSound (new TestSynthSound());
        synth.setNoteStealingEnabled (true);

        for (int i = 0; i < numPolyVoices; ++i)
            polySynth.addVoice (new PolyVoice (polyParams, perf));
        polySynth.addSound (new PolySound());
        polySynth.setNoteStealingEnabled (true);

        for (int i = 0; i < numWaveVoices; ++i)
            waveSynth.addVoice (new WaveVoice (waveParams, perf));
        waveSynth.addSound (new WaveSound());
        waveSynth.setNoteStealingEnabled (true);

        for (int i = 0; i < numFmVoices; ++i)
            fmSynth.addVoice (new FmVoice (fmParams, perf));
        fmSynth.addSound (new FmSound());
        fmSynth.setNoteStealingEnabled (true);

        for (int i = 0; i < numNoiseVoices; ++i)
            noiseSynth.addVoice (new NoiseVoice (noiseParams, perf, (std::uint32_t) (i + 1) * 7919u));
        noiseSynth.addSound (new NoiseSound());
        noiseSynth.setNoteStealingEnabled (true);

        for (int i = 0; i < numSpeechVoices; ++i)
            voiceSynth.addVoice (new SpeechVoice (voiceParams, perf, (std::uint32_t) (i + 1) * 104729u));
        voiceSynth.addSound (new VoiceSound());
        voiceSynth.setNoteStealingEnabled (true);

        for (int i = 0; i < numVocoderVoices; ++i)
            vocoderSynth.addVoice (new VocoderVoice (vocoderParams, perf, (std::uint32_t) (i + 1) * 15485863u));
        vocoderSynth.addSound (new VocoderSound());
        vocoderSynth.setNoteStealingEnabled (true);
    }

    std::atomic<int> type { testSynth };
    std::atomic<float> volume { 0.8f };
    std::atomic<bool> mute { false };
    std::atomic<int> selectedDrum { 0 };     // drum tracks: the drum the knobs and pads edit
    std::atomic<float> level { 0.0f };       // read-out: peak of the last block

    TestSynthParams synthParams;
    PolyParams polyParams;
    WaveParams waveParams;
    FmParams fmParams;
    NoiseParams noiseParams;
    VoiceParams voiceParams;
    VocoderParams vocoderParams;   // the Vocoder instrument; its listening settings also serve the Vocoder effect
    ExternalParams ext;   // External MIDI tracks

    /** External MIDI tracks: the MIDI output they play, by name (kept in
        projects) and identifier (found again on this computer). Message thread. */
    juce::String externalDeviceName, externalDeviceId;

    /** True for tracks whose steps are drum lanes: a Drums track, or an External track set to drums. */
    bool playsDrums() const
    {
        const int t = type.load();
        return t == drums || (t == external && ext.drums.load());
    }
    DrumKit kit;
    Sampler samplerEngine;
    ChannelSettings mix;
    std::array<InsertSettings, 2> inserts;   // two effect slots, after the sound

    /** The sampler's original file, kept so projects can save it (message thread). */
    juce::String sampleFileName;
    juce::MemoryBlock sampleFileData;

    /** Recordings on the drum pads, kept so projects and sounds can save them (message thread). */
    std::array<juce::String, DrumKit::numDrums> padFileNames;
    std::array<juce::MemoryBlock, DrumKit::numDrums> padFileData;

    // ---- audio thread only
    juce::Synthesiser synth;
    juce::Synthesiser polySynth;
    juce::Synthesiser waveSynth;
    juce::Synthesiser fmSynth;
    juce::Synthesiser noiseSynth;
    juce::Synthesiser voiceSynth;
    juce::Synthesiser vocoderSynth;
    VocoderCore vocoderCore;
    juce::SmoothedValue<float> gain;
    ChannelStrip strip;
    std::array<InsertProcessor, 2> insertFx;
};

} // namespace bizgroove
