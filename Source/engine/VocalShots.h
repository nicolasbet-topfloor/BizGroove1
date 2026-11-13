#pragma once

#include "Track.h"
#include "SampleTools.h"

namespace bizgroove
{

/** Shouts and ad-libs in the manner of hip-hop and hip-house records ("Yeah!",
    "Come on", "Jack"), made by the Voice itself (no one's recording), driven a
    little and given some room, and put on a kit's pads so the steps can play
    them like drums. Made once, the first time they are asked for. */
class VocalShots
{
public:
    static constexpr double rate = 48000.0;

    struct Shot { const char* text; int language; int note; float size; float buzz; };

    /** The sixteen shots, one per pad, in the kit's order. */
    static const std::array<Shot, DrumKit::numDrums>& list()
    {
        static const std::array<Shot, DrumKit::numDrums> shots { {
            { "yeah!",     speech::english, 50, 0.65f, 0.45f },
            { "uh.",       speech::english, 45, 0.75f, 0.35f },
            { "come on!",  speech::english, 50, 0.65f, 0.45f },
            { "hey!",      speech::english, 53, 0.6f,  0.5f },
            { "ho!",       speech::english, 50, 0.65f, 0.5f },
            { "jack!",     speech::english, 50, 0.65f, 0.5f },
            { "check it.", speech::english, 47, 0.7f,  0.4f },
            { "let's go!", speech::english, 52, 0.6f,  0.45f },
            { "yo.",       speech::english, 47, 0.7f,  0.4f },
            { "say what?", speech::english, 50, 0.65f, 0.45f },
            { "dale!",     speech::spanish, 50, 0.65f, 0.45f },
            { "vamos!",    speech::spanish, 50, 0.65f, 0.45f },
            { "eso!",      speech::spanish, 52, 0.6f,  0.45f },
            { "one, two.", speech::english, 47, 0.7f,  0.4f },
            { "drop it!",  speech::english, 48, 0.7f,  0.45f },
            { "aw yeah.",  speech::english, 46, 0.75f, 0.4f }
        } };
        return shots;
    }

    static juce::String name (int pad)
    {
        auto t = juce::String::fromUTF8 (list()[(size_t) juce::jlimit (0, DrumKit::numDrums - 1, pad)].text);
        t = t.trimCharactersAtEnd ("!.?,");
        return t.substring (0, 1).toUpperCase() + t.substring (1);
    }

    /** Puts every shot on the pads of a Drums track (and keeps them, so projects save them). */
    static void load (Track& t)
    {
        for (int d = 0; d < DrumKit::numDrums; ++d)
            loadOne (t, d);
        t.kit.choke.store (false);
    }

    /** Puts a shot on a pad: its own shot, or another one ('shot'). */
    static void loadOne (Track& t, int d, int shot = -1)
    {
        if (! juce::isPositiveAndBelow (d, DrumKit::numDrums))
            return;
        if (! juce::isPositiveAndBelow (shot, DrumKit::numDrums))
            shot = d;
        const auto& audio = rendered()[(size_t) shot];
        t.kit.setPadSample (d, audio, rate, name (shot));
        t.padFileNames[(size_t) d] = name (shot) + ".wav";
        t.padFileData[(size_t) d] = SampleTools::encodeWav (audio, rate);
        auto& p = t.kit.drums[(size_t) d];
        p.pitch.store (0.0f); p.length.store (1.0f); p.tone.store (1.0f); p.snap.store (0.0f); p.drive.store (0.0f);
        p.volume.store (0.8f); p.pan.store (0.0f);
    }

    /** Says a text with the Voice, offline, and gives back the sound. For the shots, and for tests. */
    static juce::AudioBuffer<float> say (const juce::String& text, int language, int note, float size, float buzz, double seconds = 1.6)
    {
        VoiceParams vp;
        vp.language.store (language);
        vp.setText (text);
        vp.mode.store (VoiceParams::phrase);
        vp.size.store (size); vp.buzz.store (buzz); vp.intonation.store (0.9f); vp.speed.store (1.1f);
        vp.level.store (2.0f); vp.attack.store (0.002f); vp.release.store (0.08f);
        Performance perf;
        juce::Synthesiser synth;
        auto* voice = new SpeechVoice (vp, perf, 4242u);
        synth.addVoice (voice);
        synth.addSound (new VoiceSound());
        synth.setCurrentPlaybackSampleRate (rate);
        voice->prepare (rate);

        const int total = (int) (seconds * rate), block = 512;
        juce::AudioBuffer<float> out (2, total);
        out.clear();
        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            midi.clear();
            if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.95f), 0);
            if (pos <= (int) (0.25 * rate) && pos + n > (int) (0.25 * rate)) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            synth.renderNextBlock (out, midi, pos, n);
            ++vp.blockCount;
        }
        return out;
    }

private:
    static const std::array<juce::AudioBuffer<float>, DrumKit::numDrums>& rendered()
    {
        static const std::array<juce::AudioBuffer<float>, DrumKit::numDrums> all = [] {
            std::array<juce::AudioBuffer<float>, DrumKit::numDrums> a;
            for (int d = 0; d < DrumKit::numDrums; ++d)
            {
                const auto& s = list()[(size_t) d];
                auto audio = say (juce::String::fromUTF8 (s.text), s.language, s.note, s.size, s.buzz);
                if (! SampleTools::trimSilence (audio, rate, -45.0f))
                    audio.setSize (2, 64);
                shape (audio);
                a[(size_t) d] = std::move (audio);
            }
            return a;
        }();
        return all;
    }

    /** As a record's shout sounds: pushed into a little drive, with a short slap of room behind it. */
    static void shape (juce::AudioBuffer<float>& a)
    {
        const int n = a.getNumSamples();
        const int slap = (int) (0.045 * rate), slap2 = (int) (0.083 * rate);
        juce::AudioBuffer<float> out (2, n + slap2 + (int) (0.05 * rate));
        out.clear();
        const float peak = juce::jmax (1.0e-4f, a.getMagnitude (0, 0, n));
        for (int i = 0; i < n; ++i)
        {
            const float x = std::tanh (2.2f * a.getSample (0, i) / peak);
            out.addSample (0, i, x);
            out.addSample (1, i, x);
            out.addSample (0, i + slap, 0.18f * x);    // the slap: a little later on each side
            out.addSample (1, i + slap2, 0.14f * x);
        }
        const float m = juce::jmax (1.0e-4f, out.getMagnitude (0, out.getNumSamples()));
        out.applyGain (0.9f / m);
        const int fade = juce::jmin (out.getNumSamples(), (int) (0.02 * rate));
        out.applyGainRamp (out.getNumSamples() - fade, fade, 1.0f, 0.0f);
        a = std::move (out);
    }
};

} // namespace bizgroove
