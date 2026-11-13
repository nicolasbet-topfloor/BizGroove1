#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TestSynth.h"   // Performance, TestSynthParams, Glide, OscSettings
#include "Speech.h"
#include <array>
#include <atomic>
#include <cmath>

namespace bizgroove
{

/** Settings of the Voice instrument: a speaking synth in the spirit of the
    Speak & Spell and the Monomachine's VO machine. It says a text, in Spanish
    or English, at the pitch of the note you play: the whole text, or one word
    per note, so a sequence can sing it. Shared between the UI and the voices. */
struct VoiceParams
{
    enum Mode { phrase = 0, phraseLoop, wordByWord, singWords, syllableBySyllable, soundBySound, soundsOnKeys, numModes };

    static juce::String modeName (int m)
    {
        static const char* names[numModes] = { "Whole text", "Whole text, looped", "Word by word", "Sing words",
                                               "Syllable by syllable", "Sound by sound", "Sounds on keys" };
        return names[juce::jlimit (0, numModes - 1, m)];
    }

    /** Modes where each note says the next piece of the text. */
    static bool byPiece (int m) { return m == wordByWord || m == singWords || m == syllableBySyllable || m == soundBySound; }

    std::atomic<int>   mode        { phrase };
    std::atomic<int>   language    { speech::spanish };
    std::atomic<float> speed       { 1.0f };    // 0.4..2.5
    std::atomic<float> intonation  { 0.6f };    // 0 monotone (robot, follows the notes only) .. 1 speech-like
    std::atomic<float> size        { 0.5f };    // 0 small (chipmunk) .. 1 giant
    std::atomic<float> buzz        { 0.35f };   // 0 soft human voice .. 1 buzzy machine
    std::atomic<float> whisper     { 0.0f };    // 0..1
    std::atomic<float> lofi        { 0.0f };    // 0 clean .. 1 crunchy old speaking toy
    std::atomic<float> attack      { 0.003f };  // seconds
    std::atomic<float> release     { 0.12f };
    std::atomic<float> level       { 1.0f };    // 0.25..4
    std::atomic<float> velocity    { 0.5f };    // 0..1
    std::atomic<float> glide       { 0.0f };
    std::atomic<int>   glideMode   { Glide::fixedTime };
    std::atomic<int>   keysPitch   { 48 };      // Sounds on keys: the note every sound is said on (the keys choose the sound)

    mutable double lastNoteHz = 0.0;   // audio thread only
    OscSettings osc;                    // the Oscillators section: added to the voice's buzz, so they speak too

    /** The text to say. Message thread: both languages are prepared at once, so switching is instant. */
    void setText (const juce::String& newText)
    {
        text = newText.substring (0, 600);
        auto es = std::make_unique<speech::Script>();
        auto en = std::make_unique<speech::Script>();
        speech::compile (text, speech::spanish, *es);
        speech::compile (text, speech::english, *en);
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            scripts[speech::spanish] = *es;
            scripts[speech::english] = *en;
            ++version;
        }
        nextWord.store (0);
    }

    const juce::String& getText() const { return text; }   // message thread

    /** How many words the text has, in the current language. Message thread. */
    int wordCount() const
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return scripts[(size_t) juce::jlimit (0, 1, language.load())].numWords;
    }

    /** How many pieces each note can say in a mode: words, syllables or sounds. Message thread. */
    int pieceCount (int forMode) const
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        const auto& sc = scripts[(size_t) juce::jlimit (0, 1, language.load())];
        speech::Pieces a {}, b {};
        if (forMode == syllableBySyllable) return speech::syllables (sc, a, b);
        if (forMode == soundBySound)       return speech::sounds (sc, a, b);
        return sc.numWords;
    }

    /** Audio thread: copies the script for a language when it has changed. Never waits. */
    void copyScript (int lang, speech::Script& dest, int& destVersion, int& destLanguage) const
    {
        if (destVersion == version.load() && destLanguage == lang)
            return;
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked())
            return;
        dest = scripts[(size_t) juce::jlimit (0, 1, lang)];
        destVersion = version.load();
        destLanguage = lang;
    }

    /** Audio thread: the word the next note says. Notes that start together (a chord) say the same word. */
    int takeWord (int numWords) const
    {
        if (numWords <= 0)
            return 0;
        if (blockCount != lastWordBlock)
        {
            lastWord = nextWord.load() % numWords;
            nextWord.store (lastWord + 1);
            lastWordBlock = blockCount;
        }
        return lastWord % numWords;
    }

    mutable std::atomic<int> nextWord { 0 };  // back to the first word when the text changes or the song starts
    mutable juce::int64 blockCount = 0;       // audio thread: counted by the engine, one per block
    mutable juce::int64 lastWordBlock = -1;
    mutable int lastWord = 0;

    VoiceParams() { setText (juce::String::fromUTF8 ("hola, soy biz groove")); }

private:
    juce::String text;
    mutable juce::SpinLock lock;
    std::array<speech::Script, speech::numLanguages> scripts;
    std::atomic<int> version { 0 };
};

struct VoiceSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

/** One speaking voice: a buzz at the note's pitch (the vocal folds) and a hiss
    (breath), shaped by four resonances that move from sound to sound (the
    mouth), as in the formant synthesisers of the speaking machines. */
class SpeechVoice final : public juce::SynthesiserVoice
{
public:
    SpeechVoice (const VoiceParams& p, const Performance& perf, std::uint32_t seed)
        : params (p), performance (perf), noiseState (seed | 1u)
    {
        script = std::make_unique<speech::Script>();
    }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<VoiceSound*> (s) != nullptr; }

    void prepare (double sampleRate) { ampEnv.setSampleRate (sampleRate); }

    void startNote (int midiNote, float vel, juce::SynthesiserSound*, int) override
    {
        mode = juce::jlimit (0, VoiceParams::numModes - 1, params.mode.load());
        // Sounds on keys: the key picks the sound; every sound is said on the same note.
        noteHz = juce::MidiMessage::getMidiNoteInHertz (mode == VoiceParams::soundsOnKeys ? juce::jlimit (24, 96, params.keysPitch.load()) : midiNote);
        glide.start (params.lastNoteHz, noteHz, params.glide.load(), params.glideMode.load(), getSampleRate());
        params.lastNoteHz = noteHz;

        if (mode == VoiceParams::soundsOnKeys)
        {
            // A script of one sound (a stop gets its short silence first, so it pops).
            const int p = speech::keySoundFor (midiNote);
            script->numSegs = 0;
            auto add = [this] (int ph, float ms) {
                speech::Seg s;
                s.phone = (std::uint8_t) ph;
                s.ms = (std::uint16_t) juce::roundToInt (ms);
                script->segs[(size_t) script->numSegs++] = s;
            };
            if (speech::phone (p).kind == speech::burst)
                add (p - 1, 8.0f);
            add (p, speech::phone (p).ms * (speech::phone (p).kind == speech::burst ? 1.0f : 1.5f));
            script->numWords = 1;
            script->wordStart[0] = 0;
            script->wordStart[1] = (std::uint16_t) script->numSegs;
            scriptVersion = -1;   // the text's script must be copied again before it is spoken
            from = 0;
            to = script->numSegs;
        }
        else
        {
            params.copyScript (params.language.load(), *script, scriptVersion, scriptLanguage);
        }

        if (mode == VoiceParams::soundsOnKeys)
        {
            // from and to are set above
        }
        else if (mode == VoiceParams::syllableBySyllable || mode == VoiceParams::soundBySound)
        {
            const int count = mode == VoiceParams::syllableBySyllable ? speech::syllables (*script, pieceStart, pieceEnd)
                                                                      : speech::sounds (*script, pieceStart, pieceEnd);
            const int k = params.takeWord (count);
            from = count > 0 ? pieceStart[(size_t) k] : 0;
            to = count > 0 ? pieceEnd[(size_t) k] : 0;
        }
        else if (mode == VoiceParams::wordByWord || mode == VoiceParams::singWords)
        {
            const int w = params.takeWord (script->numWords);
            from = script->wordStart[(size_t) w];
            to = script->wordStart[(size_t) w + 1];
            while (to > from && speech::phone (script->segs[(size_t) to - 1].phone).kind == speech::pause)
                --to;   // a word on its own: without the pause after it
        }
        else
        {
            from = 0;
            to = script->numSegs;
        }

        const bool fresh = ! ampEnv.isActive();
        seg = from;
        segLeft = segSamples (seg);
        finished = to <= from;
        keyDown = true;
        envReleased = false;
        if (fresh)
        {
            for (auto& r : res) r = {};
            fricIc1 = fricIc2 = nasalLow = 0.0f;
            phase = 0.0;
            lastFlow = 0.0f;
            held = 0.0f;
            holdCount = 0.0;
            layer.reset();
            snapToTargets();
        }
        const float vs = juce::jlimit (0.0f, 1.0f, params.velocity.load());
        gainNow = 0.5f * juce::jlimit (0.25f, 4.0f, params.level.load()) * ((1.0f - vs) + vs * vel);
        ampEnv.setParameters ({ params.attack.load(), 0.05f, 1.0f, params.release.load() });
        ampEnv.noteOn();
        trillPhase = 0.0;
    }

    void stopNote (float, bool allowTailOff) override
    {
        keyDown = false;
        if (allowTailOff)
        {
            // The text (or word) is said to its end, even after a short note; a loop stops when you let go.
            if (mode == VoiceParams::phraseLoop || finished)
                release();
        }
        else
        {
            ampEnv.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const double sr = getSampleRate();
        const double twoPi = juce::MathConstants<double>::twoPi;
        const float buzz = juce::jlimit (0.0f, 1.0f, params.buzz.load());
        const float whisper = juce::jlimit (0.0f, 1.0f, params.whisper.load());
        const float lofi = juce::jlimit (0.0f, 1.0f, params.lofi.load());
        const float intonation = juce::jlimit (0.0f, 1.0f, params.intonation.load());
        const float shift = std::pow (2.0f, (0.5f - juce::jlimit (0.0f, 1.0f, params.size.load())) * 0.8f);
        const float main = params.osc.main.load();
        const int numChannels = output.getNumChannels();
        constexpr int controlInterval = 16;

        // Lo-fi: fewer samples a second and fewer levels, like the speaking toys' tiny chips.
        const double lofiRate = lofi < 0.01f ? sr : juce::jlimit (3000.0, sr, 22050.0 * std::pow (3000.0 / 22050.0, (double) lofi));
        const double holdLength = sr / lofiRate;
        const float steps = lofi < 0.01f ? 0.0f : std::pow (2.0f, 12.0f - 8.0f * lofi);

        for (int i = 0; i < numSamples; ++i)
        {
            if ((i % controlInterval) == 0)
                control (controlInterval, sr, shift, whisper, intonation);

            // Where we are in the text.
            if (! finished)
            {
                const bool holdHere = keyDown
                                   && ((mode == VoiceParams::singWords && (script->segs[(size_t) seg].flags & speech::Seg::lastVowel) != 0)
                                       || ((mode == VoiceParams::soundBySound || mode == VoiceParams::soundsOnKeys) && seg == to - 1 && lasts (seg)));
                if (! holdHere)
                    segLeft -= 1.0;
                if (segLeft <= 0.0)
                {
                    ++seg;
                    if (seg >= to)
                    {
                        if (mode == VoiceParams::phraseLoop && keyDown && to > from)
                            seg = from;   // again, while the key is held
                        else
                        {
                            finished = true;
                            seg = juce::jmax (from, to - 1);
                        }
                    }
                    if (! finished)
                        segLeft += segSamples (seg);
                }
            }
            if (finished && ! envReleased && (mode != VoiceParams::singWords || ! keyDown))
                release();

            // The buzz of the voice: a smooth glottal pulse, or a buzzy saw.
            const double inc = juce::jlimit (1.0e-5, 0.45, f0 / sr);
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            const float flow = glottalFlow (phase);
            const float pulse = (flow - lastFlow) / (float) (inc * 5.0);
            lastFlow = flow;
            const float saw = (float) (2.0 * phase - 1.0 - blep (phase, inc));
            const float voiced = main * ((1.0f - buzz) * pulse + buzz * 0.7f * saw) + 2.0f * layer.next (params.osc);

            const float noise = white();
            float av = avNow;
            if (inTrill)
            {
                trillPhase += twoPi * 26.0 / sr;
                av *= 0.55f + 0.45f * (float) std::cos (trillPhase);
            }
            const float source = av * ((1.0f - whisper) * voiced + whisper * 1.3f * noise)
                               + (aspNow + 0.04f * avNow) * 0.6f * noise;

            // The mouth: four resonances in a row.
            float x = source;
            for (auto& r : res)
            {
                const float y = r.a * x + r.b * r.y1 + r.c * r.y2;
                r.y2 = r.y1;
                r.y1 = y;
                x = y;
            }
            // Nasal sounds: the sound comes out through the nose, dull and low.
            nasalLow += 0.08f * (x - nasalLow);
            x = x * (1.0f - 0.65f * nasalNow) + 1.3f * nasalNow * nasalLow;

            // The hiss of s, sh, f... : noise through its own band.
            const float v3 = noise - fricIc2;
            const float v1 = fa1 * fricIc1 + fa2 * v3;
            const float v2 = fricIc2 + fa2 * fricIc1 + fa3 * v3;
            fricIc1 = 2.0f * v1 - fricIc1;
            fricIc2 = 2.0f * v2 - fricIc2;
            const float hiss = afNow * v1 * fricK * hissGain;

            float out = 0.11f * x + hiss;

            if (steps > 0.0f)
            {
                holdCount -= 1.0;
                if (holdCount <= 0.0)
                {
                    held = std::round (juce::jlimit (-1.0f, 1.0f, out) * steps) / steps;
                    holdCount += holdLength;
                }
                out = held;
            }

            const float amp = ampEnv.getNextSample();
            out *= amp * gainNow;
            for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
                output.addSample (ch, startSample + i, out);

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    struct Resonator { float a = 1.0f, b = 0.0f, c = 0.0f, y1 = 0.0f, y2 = 0.0f; };

    /** A sound that can go on for as long as the key is held: a vowel, a hum, a hiss. */
    bool lasts (int s) const
    {
        const auto k = speech::phone (script->segs[(size_t) juce::jlimit (0, juce::jmax (0, script->numSegs - 1), s)].phone).kind;
        return k == speech::vowel || k == speech::nasal || k == speech::fricative || k == speech::liquid || k == speech::trill || k == speech::glide;
    }

    void release()
    {
        ampEnv.noteOff();
        envReleased = true;
    }

    double segSamples (int s) const
    {
        if (s < 0 || s >= script->numSegs)
            return 1.0;
        const double speed = juce::jlimit (0.4, 2.5, (double) params.speed.load());
        return juce::jmax (1.0, (double) script->segs[(size_t) s].ms * 0.001 * getSampleRate() / speed);
    }

    /** The targets of the current sound: formants and how much buzz, hiss and breath. */
    void targets (float& f1, float& f2, float& f3, float& av, float& af, float& fhz, float& fbw, float& asp, float& nasal, int& kind) const
    {
        const auto& s = script->segs[(size_t) juce::jlimit (0, juce::jmax (0, script->numSegs - 1), seg)];
        const auto& d = speech::phone (script->numSegs > 0 ? s.phone : speech::Gap);
        kind = d.kind;
        f1 = d.f1; f2 = d.f2; f3 = d.f3;
        av = d.voice; af = d.hiss; fhz = d.hissHz; fbw = d.hissWidth;
        asp = 0.0f;
        nasal = d.kind == speech::nasal ? 1.0f : 0.0f;
        if (d.kind == speech::tap) av *= 0.6f;
        if (finished)
            av = af = 0.0f;   // said: the voice falls silent
        if (d.kind == speech::aspirate || d.kind == speech::pause || d.kind == speech::closure)
        {
            // Breath and silences keep the mouth where the next sound will be.
            const int nextSeg = juce::jmin (script->numSegs - 1, seg + 1);
            if (nextSeg >= 0)
            {
                const auto& n = speech::phone (script->segs[(size_t) nextSeg].phone);
                if (d.kind != speech::closure && n.kind != speech::pause)
                {
                    f1 = n.f1; f2 = n.f2; f3 = n.f3;
                }
            }
            if (d.kind == speech::aspirate) asp = 0.55f;
        }
    }

    void snapToTargets()
    {
        int kind = 0;
        targets (f1Now, f2Now, f3Now, avNow, afNow, fhzNow, fbwNow, aspNow, nasalNow, kind);
        avNow = afNow = aspNow = 0.0f;   // start from silence, the mouth already in place
        f0 = noteHz;
    }

    void control (int interval, double sr, float shift, float whisper, float intonation)
    {
        // Pitch: the note (with glide, bend and vibrato) and the intonation of the text.
        vibratoPhase += juce::MathConstants<double>::twoPi * TestSynthParams::vibratoRateHz * interval / sr;
        if (vibratoPhase > juce::MathConstants<double>::twoPi) vibratoPhase -= juce::MathConstants<double>::twoPi;
        const double semitones = performance.pitchBend.load() * TestSynthParams::bendRangeSemitones
                               + performance.modWheel.load() * TestSynthParams::vibratoMaxSemitones * std::sin (vibratoPhase);
        const double base = glide.advance (interval) * std::pow (2.0, semitones / 12.0);
        const int s = juce::jlimit (0, juce::jmax (0, script->numSegs - 1), seg);
        const double cents = script->numSegs > 0 ? (double) script->segs[(size_t) s].cents * intonation : 0.0;
        const double target = base * std::pow (2.0, cents / 1200.0);
        f0 += (target - f0) * (1.0 - std::exp (-(double) interval / (0.03 * sr)));
        layer.setPitch (f0, sr, params.osc);

        float f1, f2, f3, av, af, fhz, fbw, asp, nasal;
        int kind = 0;
        targets (f1, f2, f3, av, af, fhz, fbw, asp, nasal, kind);
        inTrill = kind == speech::trill && ! finished;

        hissGain = kind == speech::burst ? 0.3f : 0.5f;   // bursts are short pops, not hisses
        const bool quick = kind == speech::burst || kind == speech::closure || kind == speech::tap;
        auto glideTo = [interval, sr] (float& now, float goal, double ms)
        {
            now += (goal - now) * (float) (1.0 - std::exp (-(double) interval / (ms * 0.001 * sr)));
        };
        const double formantMs = quick ? 7.0 : 16.0;
        glideTo (f1Now, f1, formantMs); glideTo (f2Now, f2, formantMs); glideTo (f3Now, f3, formantMs);
        glideTo (avNow, av, quick ? 3.0 : 7.0);
        glideTo (afNow, af, kind == speech::burst ? 1.0 : 4.0);
        glideTo (aspNow, asp, 5.0);
        glideTo (nasalNow, nasal, 10.0);
        glideTo (fhzNow, fhz > 0.0f ? fhz : fhzNow, 2.0);
        glideTo (fbwNow, fbw > 0.0f ? fbw : fbwNow, 2.0);

        // The resonators (Klatt's): each formant with its bandwidth, wider when whispering.
        const float widen = 1.0f + 1.5f * whisper;
        const std::array<float, 4> freqs { f1Now * shift, f2Now * shift, f3Now * shift, 3500.0f * shift };
        const std::array<float, 4> widths { 70.0f * widen, 100.0f * widen, 160.0f * widen, 250.0f * widen };
        for (size_t k = 0; k < 4; ++k)
        {
            const double f = juce::jlimit (60.0, 0.45 * sr, (double) freqs[k]);
            const double r = std::exp (-juce::MathConstants<double>::pi * widths[k] / sr);
            res[k].c = (float) (-r * r);
            res[k].b = (float) (2.0 * r * std::cos (juce::MathConstants<double>::twoPi * f / sr));
            res[k].a = 1.0f - res[k].b - res[k].c;
        }

        // The hiss band.
        const double fc = juce::jlimit (200.0, 0.45 * sr, (double) (fhzNow * shift));
        const double q = juce::jlimit (0.3, 8.0, fc / juce::jmax (100.0, (double) fbwNow));
        fricK = (float) (1.0 / q);
        const float g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
        fa1 = 1.0f / (1.0f + g * (g + fricK));
        fa2 = g * fa1;
        fa3 = g * fa2;
    }

    /** The flow of air through the vocal folds over one cycle (Rosenberg): opening, closing, shut. */
    static float glottalFlow (double p)
    {
        if (p < 0.4)  return (float) (0.5 * (1.0 - std::cos (juce::MathConstants<double>::pi * p / 0.4)));
        if (p < 0.56) return (float) std::cos (juce::MathConstants<double>::halfPi * (p - 0.4) / 0.16);
        return 0.0f;
    }

    static double blep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    float white()
    {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return (float) (std::int32_t) noiseState * (1.0f / 2147483648.0f);
    }

    const VoiceParams& params;
    const Performance& performance;
    std::unique_ptr<speech::Script> script;
    int scriptVersion = -1, scriptLanguage = -1;
    speech::Pieces pieceStart {}, pieceEnd {};   // the syllables or sounds of the text, found at each note
    int mode = VoiceParams::phrase, seg = 0, from = 0, to = 0;
    double segLeft = 0.0;
    bool finished = true, keyDown = false, inTrill = false, envReleased = false;

    double noteHz = 220.0, f0 = 220.0, phase = 0.0, vibratoPhase = 0.0, trillPhase = 0.0;
    float lastFlow = 0.0f;
    Glide glide;
    OscLayer layer;

    float f1Now = 500, f2Now = 1500, f3Now = 2500, avNow = 0, afNow = 0, fhzNow = 4000, fbwNow = 2000, aspNow = 0, nasalNow = 0;
    std::array<Resonator, 4> res {};
    float fricIc1 = 0.0f, fricIc2 = 0.0f, fricK = 1.0f, fa1 = 0.0f, fa2 = 0.0f, fa3 = 0.0f, nasalLow = 0.0f;
    float held = 0.0f, gainNow = 0.5f, hissGain = 0.5f;
    double holdCount = 0.0;
    std::uint32_t noiseState;
    juce::ADSR ampEnv;
};

/** The Voice settings as JSON, for projects and saved sounds: the text too. */
inline juce::var voiceParamsToVar (const VoiceParams& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("text", p.getText());
    o->setProperty ("mode", p.mode.load());              o->setProperty ("language", p.language.load());
    o->setProperty ("speed", p.speed.load());            o->setProperty ("intonation", p.intonation.load());
    o->setProperty ("size", p.size.load());              o->setProperty ("buzz", p.buzz.load());
    o->setProperty ("whisper", p.whisper.load());        o->setProperty ("lofi", p.lofi.load());
    o->setProperty ("attack", p.attack.load());          o->setProperty ("release", p.release.load());
    o->setProperty ("level", p.level.load());            o->setProperty ("velocity", p.velocity.load());
    o->setProperty ("glide", p.glide.load());            o->setProperty ("glideMode", p.glideMode.load());
    o->setProperty ("keysPitch", p.keysPitch.load());
    o->setProperty ("osc", oscToVar (p.osc));
    return juce::var (o);
}

/** Reads settings written by voiceParamsToVar(); anything missing gets its default. Message thread (it prepares the text). */
inline void applyVoiceParams (VoiceParams& p, const juce::var& v)
{
    auto f = [&v] (const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; };
    auto i = [&v] (const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; };
    p.mode.store (juce::jlimit (0, VoiceParams::numModes - 1, i ("mode", 0)));
    p.language.store (juce::jlimit (0, speech::numLanguages - 1, i ("language", 0)));
    p.speed.store (f ("speed", 1.0f));                p.intonation.store (f ("intonation", 0.6f));
    p.size.store (f ("size", 0.5f));                  p.buzz.store (f ("buzz", 0.35f));
    p.whisper.store (f ("whisper", 0.0f));            p.lofi.store (f ("lofi", 0.0f));
    p.attack.store (f ("attack", 0.003f));            p.release.store (f ("release", 0.12f));
    p.level.store (f ("level", 1.0f));                p.velocity.store (f ("velocity", 0.5f));
    p.glide.store (f ("glide", 0.0f));
    p.glideMode.store (juce::jlimit (0, Glide::numModes - 1, i ("glideMode", 0)));
    p.keysPitch.store (juce::jlimit (24, 96, i ("keysPitch", 48)));
    applyOsc (p.osc, v["osc"]);
    p.setText (v.hasProperty ("text") ? v["text"].toString() : juce::String::fromUTF8 ("hola, soy biz groove"));
}

} // namespace bizgroove
