#include "ControlTargets.h"
#include "AudioEngine.h"
#include "SynthLibrary.h"

namespace bizgroove
{

namespace
{
    /** Exponential map, for frequencies and times: v = 0 -> lo, v = 1 -> hi. */
    float expMap (float v, float lo, float hi) { return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, v)); }
    float linMap (float v, float lo, float hi) { return lo + (hi - lo) * juce::jlimit (0.0f, 1.0f, v); }
    float expInv (float x, float lo, float hi) { return juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (lo, x) / lo) / std::log (hi / lo)); }
    float linInv (float x, float lo, float hi) { return juce::jlimit (0.0f, 1.0f, (x - lo) / (hi - lo)); }

    juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    /** Times as people say them: "Instant", "0.25 sec", "1.5 sec". */
    juce::String seconds (float s)
    {
        if (s < 0.01f) return "Instant";
        if (s < 1.0f)  return juce::String (s, 2) + " sec";
        return juce::String (s, 1) + " sec";
    }

    void toggle (std::atomic<bool>& flag) { flag.store (! flag.load()); }
}

ControlTargets::ControlTargets (AudioEngine& e)
{
    // ---- Transport
    add ("transport.playstop", "Play / stop", "Transport", TargetKind::toggle,
         [&e] (float) { toggle (e.transport.playing); });
    add ("transport.play", "Play", "Transport", TargetKind::trigger,
         [&e] (float) { e.transport.playing.store (true); });
    add ("transport.stop", "Stop", "Transport", TargetKind::trigger,
         [&e] (float) { e.transport.playing.store (false); });
    add ("transport.tempo", "Tempo (60-180 BPM)", "Transport", TargetKind::continuous,
         [&e] (float v) { e.transport.bpm.store (std::round (linMap (v, 60.0f, 180.0f) * 10.0f) / 10.0f); },
         [&e] { return linInv ((float) e.transport.bpm.load(), 60.0f, 180.0f); },
         [] (float v) { return juce::String (juce::roundToInt (linMap (v, 60.0f, 180.0f))) + " BPM"; });
    add ("transport.tempoup", "Tempo +1 BPM", "Transport", TargetKind::trigger,
         [&e] (float) { e.transport.bpm.store (juce::jmin (240.0, std::round (e.transport.bpm.load()) + 1.0)); });
    add ("transport.tempodown", "Tempo -1 BPM", "Transport", TargetKind::trigger,
         [&e] (float) { e.transport.bpm.store (juce::jmax (40.0, std::round (e.transport.bpm.load()) - 1.0)); });
    add ("perform.hold", "Hold notes on / off", "Performance", TargetKind::toggle,
         [&e] (float) { toggle (e.hold); });
    add ("transport.metronome", "Metronome on / off", "Transport", TargetKind::toggle,
         [&e] (float) { e.metronomeMode.store (e.metronomeMode.load() == AudioEngine::metronomeOn ? AudioEngine::metronomeOff
                                                                                                  : AudioEngine::metronomeOn); });
    add ("master.volume", "Master volume", "Transport", TargetKind::continuous,
         [&e] (float v) { e.masterGain.store (v); },
         [&e] { return e.masterGain.load(); }, percent);

    // ---- Synth track
    // The selected track's synth settings.
    auto sp = [&e]() -> TestSynthParams& { return e.selected().synthParams; };
    add ("synth.cutoff", "Synth brightness", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().cutoffHz.store (expMap (v, 40.0f, 16000.0f)); },
         [sp] { return expInv (sp().cutoffHz.load(), 40.0f, 16000.0f); }, percent);
    add ("synth.resonance", "Synth edge", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().resonance.store (v); },
         [sp] { return sp().resonance.load(); }, percent);
    add ("synth.envamount", "Synth wah", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().envAmount.store (v); },
         [sp] { return sp().envAmount.load(); }, percent);
    add ("synth.detune", "Synth thickness", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().detuneCents.store (linMap (v, 0.0f, 50.0f)); },
         [sp] { return linInv (sp().detuneCents.load(), 0.0f, 50.0f); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("synth.attack", "Synth fade in", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().attack.store (expMap (v, 0.001f, 2.0f)); },
         [sp] { return expInv (sp().attack.load(), 0.001f, 2.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 2.0f)); });
    add ("synth.decay", "Synth punch", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().decay.store (expMap (v, 0.01f, 2.0f)); },
         [sp] { return expInv (sp().decay.load(), 0.01f, 2.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 2.0f)); });
    add ("synth.sustain", "Synth hold level", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().sustain.store (v); },
         [sp] { return sp().sustain.load(); }, percent);
    add ("synth.release", "Synth fade out", "Synth", TargetKind::continuous,
         [sp] (float v) { sp().release.store (expMap (v, 0.01f, 4.0f)); },
         [sp] { return expInv (sp().release.load(), 0.01f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 4.0f)); });
    add ("synth.pitchbend", "Pitch bend", "Synth", TargetKind::continuous,
         [&e] (float v) { e.performance.pitchBend.store (juce::jlimit (-1.0f, 1.0f, v * 2.0f - 1.0f)); },
         [&e] { return (e.performance.pitchBend.load() + 1.0f) * 0.5f; },
         [] (float v) { return juce::String ((v * 2.0f - 1.0f) * 2.0f, 1) + " semitones"; });
    add ("synth.vibrato", "Vibrato (modulation)", "Synth", TargetKind::continuous,
         [&e] (float v) { e.performance.modWheel.store (v); },
         [&e] { return e.performance.modWheel.load(); }, percent);



    // ---- Poly track: the selected track's Poly settings
    auto pp = [&e]() -> PolyParams& { return e.selected().polyParams; };
    auto offOrPercent = [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); };
    add ("poly.cutoff", "Poly brightness", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().cutoffHz.store (expMap (v, 40.0f, 16000.0f)); },
         [pp] { return expInv (pp().cutoffHz.load(), 40.0f, 16000.0f); }, percent);
    add ("poly.resonance", "Poly edge", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().resonance.store (v); }, [pp] { return pp().resonance.load(); }, percent);
    add ("poly.shape", "Poly filter type", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().shape.store (v); }, [pp] { return pp().shape.load(); },
         [] (float v) {
             if (v < 0.15f) return juce::String ("Warm (low-pass)");
             if (v < 0.40f) return juce::String ("Warm to hollow");
             if (v < 0.60f) return juce::String ("Hollow (notch)");
             if (v < 0.85f) return juce::String ("Hollow to thin");
             return juce::String ("Thin (high-pass)");
         });
    add ("poly.envamount", "Poly wah", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().envAmount.store (v); }, [pp] { return pp().envAmount.load(); }, percent);
    add ("poly.detune", "Poly thickness", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().detuneCents.store (linMap (v, 0.0f, 30.0f)); },
         [pp] { return linInv (pp().detuneCents.load(), 0.0f, 30.0f); }, offOrPercent);
    add ("poly.attack", "Poly fade in", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().attack.store (expMap (v, 0.001f, 4.0f)); },
         [pp] { return expInv (pp().attack.load(), 0.001f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 4.0f)); });
    add ("poly.decay", "Poly punch", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().decay.store (expMap (v, 0.01f, 4.0f)); },
         [pp] { return expInv (pp().decay.load(), 0.01f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 4.0f)); });
    add ("poly.sustain", "Poly hold level", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().sustain.store (v); }, [pp] { return pp().sustain.load(); }, percent);
    add ("poly.release", "Poly fade out", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().release.store (expMap (v, 0.01f, 6.0f)); },
         [pp] { return expInv (pp().release.load(), 0.01f, 6.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 6.0f)); });
    add ("poly.mix", "Poly tone mix", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().mix.store (v); }, [pp] { return pp().mix.load(); },
         [] (float v) {
             if (v < 0.03f) return juce::String ("Saw only");
             if (v > 0.97f) return juce::String ("Square only");
             return "Square " + percent (v);
         });
    add ("poly.pulsewidth", "Poly square width", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().pulseWidth.store (linMap (v, 0.05f, 0.95f)); },
         [pp] { return linInv (pp().pulseWidth.load(), 0.05f, 0.95f); },
         [] (float v) { return std::abs (v - 0.5f) < 0.03f ? juce::String ("Square") : percent (v); });
    add ("poly.octave", "Poly second tone octave", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().octave.store (v < 0.33f ? -1 : (v < 0.67f ? 0 : 1)); },
         [pp] { return (pp().octave.load() + 1) * 0.5f; },
         [] (float v) { return v < 0.33f ? juce::String ("One octave down") : (v < 0.67f ? juce::String ("Same") : juce::String ("One octave up")); });
    add ("poly.unison", "Poly stack", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().unison.store (1 + juce::jlimit (0, 3, (int) (v * 3.999f))); },
         [pp] { return (pp().unison.load() - 1) / 3.0f; },
         [] (float v) { const int n = 1 + juce::jlimit (0, 3, (int) (v * 3.999f)); return n == 1 ? juce::String ("Off") : juce::String (n) + " stacked"; });
    add ("poly.glide", "Poly glide", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().glide.store (v < 0.01f ? 0.0f : expMap (v, 0.005f, 4.0f)); },
         [pp] { return pp().glide.load() <= 0.001f ? 0.0f : expInv (pp().glide.load(), 0.005f, 4.0f); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : seconds (expMap (v, 0.005f, 4.0f)); });
    add ("poly.lforate", "Poly wobble speed", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().lfoRate.store (expMap (v, 0.1f, 20.0f)); },
         [pp] { return expInv (pp().lfoRate.load(), 0.1f, 20.0f); },
         [] (float v) { return juce::String (expMap (v, 0.1f, 20.0f), 1) + " per second"; });
    add ("poly.lfofilter", "Poly wobble on brightness", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().lfoFilter.store (v); }, [pp] { return pp().lfoFilter.load(); }, offOrPercent);
    add ("poly.lfopulse", "Poly wobble on square width", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().lfoPulse.store (v); }, [pp] { return pp().lfoPulse.load(); }, offOrPercent);
    add ("poly.noise", "Poly noise", "Poly", TargetKind::continuous,
         [pp] (float v) { pp().noise.store (v); }, [pp] { return pp().noise.load(); }, offOrPercent);

    // ---- Wavetable track: the selected track's Wavetable settings
    auto wp = [&e]() -> WaveParams& { return e.selected().waveParams; };
    add ("wave.table", "Wavetable wave set", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().table.store (juce::jlimit (0, Wavetables::numTables - 1, (int) (v * (Wavetables::numTables - 0.001f)))); },
         [wp] { return (float) wp().table.load() / (float) (Wavetables::numTables - 1); },
         [] (float v) { return Wavetables::tableName (juce::jlimit (0, Wavetables::numTables - 1, (int) (v * (Wavetables::numTables - 0.001f)))); });
    add ("wave.position", "Wavetable position", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().position.store (v); }, [wp] { return wp().position.load(); }, percent);
    add ("wave.sweep", "Wavetable sweep", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().sweep.store (v); }, [wp] { return wp().sweep.load(); }, offOrPercent);
    add ("wave.cutoff", "Wavetable brightness", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().cutoffHz.store (expMap (v, 40.0f, 18000.0f)); },
         [wp] { return expInv (wp().cutoffHz.load(), 40.0f, 18000.0f); }, percent);
    add ("wave.resonance", "Wavetable edge", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().resonance.store (v); }, [wp] { return wp().resonance.load(); }, percent);
    add ("wave.envamount", "Wavetable wah", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().envAmount.store (v); }, [wp] { return wp().envAmount.load(); }, offOrPercent);
    add ("wave.detune", "Wavetable thickness", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().detuneCents.store (linMap (v, 0.0f, 40.0f)); },
         [wp] { return linInv (wp().detuneCents.load(), 0.0f, 40.0f); }, offOrPercent);
    add ("wave.attack", "Wavetable fade in", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().attack.store (expMap (v, 0.001f, 4.0f)); },
         [wp] { return expInv (wp().attack.load(), 0.001f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 4.0f)); });
    add ("wave.decay", "Wavetable punch", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().decay.store (expMap (v, 0.01f, 6.0f)); },
         [wp] { return expInv (wp().decay.load(), 0.01f, 6.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 6.0f)); });
    add ("wave.sustain", "Wavetable hold level", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().sustain.store (v); }, [wp] { return wp().sustain.load(); }, percent);
    add ("wave.release", "Wavetable fade out", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().release.store (expMap (v, 0.01f, 8.0f)); },
         [wp] { return expInv (wp().release.load(), 0.01f, 8.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 8.0f)); });
    add ("wave.unison", "Wavetable stack", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().unison.store (1 + juce::jlimit (0, 3, (int) (v * 3.999f))); },
         [wp] { return (wp().unison.load() - 1) / 3.0f; },
         [] (float v) { const int n = 1 + juce::jlimit (0, 3, (int) (v * 3.999f)); return n == 1 ? juce::String ("Off") : juce::String (n) + " stacked"; });
    add ("wave.sub", "Wavetable sub", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().sub.store (v); }, [wp] { return wp().sub.load(); }, offOrPercent);
    add ("wave.glide", "Wavetable glide", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().glide.store (v < 0.01f ? 0.0f : expMap (v, 0.005f, 4.0f)); },
         [wp] { return wp().glide.load() <= 0.001f ? 0.0f : expInv (wp().glide.load(), 0.005f, 4.0f); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : seconds (expMap (v, 0.005f, 4.0f)); });
    add ("wave.lforate", "Wavetable wobble speed", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().lfoRate.store (expMap (v, 0.05f, 20.0f)); },
         [wp] { return expInv (wp().lfoRate.load(), 0.05f, 20.0f); },
         [] (float v) { return juce::String (expMap (v, 0.05f, 20.0f), 2) + " per second"; });
    add ("wave.lfoposition", "Wavetable wobble on position", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().lfoPosition.store (v); }, [wp] { return wp().lfoPosition.load(); }, offOrPercent);
    add ("wave.lfofilter", "Wavetable wobble on brightness", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().lfoFilter.store (v); }, [wp] { return wp().lfoFilter.load(); }, offOrPercent);
    add ("wave.noise", "Wavetable noise", "Wavetable", TargetKind::continuous,
         [wp] (float v) { wp().noise.store (v); }, [wp] { return wp().noise.load(); }, offOrPercent);

    // ---- FM track: the selected track's FM settings
    auto fp = [&e]() -> FmParams& { return e.selected().fmParams; };
    auto ratioIndex = [] (float v) { return juce::jlimit (0, (int) fmRatioSteps().size() - 1, (int) (v * ((float) fmRatioSteps().size() - 0.001f))); };
    auto ratioKnob = [ratioIndex] (float ratio) {
        const auto& steps = fmRatioSteps();
        int best = 0;
        for (int i = 1; i < (int) steps.size(); ++i)
            if (std::abs (steps[(size_t) i] - ratio) < std::abs (steps[(size_t) best] - ratio))
                best = i;
        juce::ignoreUnused (ratioIndex);
        return ((float) best + 0.5f) / (float) steps.size();
    };
    add ("fm.algorithm", "FM layout", "FM", TargetKind::continuous,
         [fp] (float v) { fp().algorithm.store (juce::jlimit (0, 7, (int) (v * 7.999f))); },
         [fp] { return ((float) fp().algorithm.load() + 0.5f) / 8.0f; },
         [] (float v) { return juce::String (FmAlgorithms::get (juce::jlimit (0, 7, (int) (v * 7.999f))).name); });
    add ("fm.brightness", "FM brightness", "FM", TargetKind::continuous,
         [fp] (float v) { fp().brightness.store (v); }, [fp] { return fp().brightness.load(); }, percent);
    for (int op = 0; op < FmParams::numOperators; ++op)
    {
        const auto n = juce::String (op + 1);
        add ("fm.ratio" + n, "FM operator " + n + " pitch", "FM", TargetKind::continuous,
             [fp, op, ratioIndex] (float v) { fp().ratio[(size_t) op].store (fmRatioSteps()[(size_t) ratioIndex (v)]); },
             [fp, op, ratioKnob] { return ratioKnob (fp().ratio[(size_t) op].load()); },
             [ratioIndex] (float v) { return "x " + juce::String (fmRatioSteps()[(size_t) ratioIndex (v)], 2).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."); });
        add ("fm.level" + n, "FM operator " + n + " level", "FM", TargetKind::continuous,
             [fp, op] (float v) { fp().level[(size_t) op].store (v); }, [fp, op] { return fp().level[(size_t) op].load(); }, offOrPercent);
    }
    add ("fm.feedback", "FM feedback", "FM", TargetKind::continuous,
         [fp] (float v) { fp().feedback.store (v); }, [fp] { return fp().feedback.load(); }, offOrPercent);
    add ("fm.detune", "FM thickness", "FM", TargetKind::continuous,
         [fp] (float v) { fp().detuneCents.store (linMap (v, 0.0f, 25.0f)); },
         [fp] { return linInv (fp().detuneCents.load(), 0.0f, 25.0f); }, offOrPercent);
    add ("fm.attack", "FM fade in", "FM", TargetKind::continuous,
         [fp] (float v) { fp().attack.store (expMap (v, 0.001f, 4.0f)); },
         [fp] { return expInv (fp().attack.load(), 0.001f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 4.0f)); });
    add ("fm.decay", "FM punch", "FM", TargetKind::continuous,
         [fp] (float v) { fp().decay.store (expMap (v, 0.01f, 8.0f)); },
         [fp] { return expInv (fp().decay.load(), 0.01f, 8.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 8.0f)); });
    add ("fm.sustain", "FM hold level", "FM", TargetKind::continuous,
         [fp] (float v) { fp().sustain.store (v); }, [fp] { return fp().sustain.load(); }, percent);
    add ("fm.release", "FM fade out", "FM", TargetKind::continuous,
         [fp] (float v) { fp().release.store (expMap (v, 0.01f, 8.0f)); },
         [fp] { return expInv (fp().release.load(), 0.01f, 8.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 8.0f)); });
    add ("fm.bitedecay", "FM bite", "FM", TargetKind::continuous,
         [fp] (float v) { fp().biteDecay.store (expMap (v, 0.01f, 6.0f)); },
         [fp] { return expInv (fp().biteDecay.load(), 0.01f, 6.0f); },
         [] (float v) { return seconds (expMap (v, 0.01f, 6.0f)); });
    add ("fm.bitesustain", "FM brightness held", "FM", TargetKind::continuous,
         [fp] (float v) { fp().biteSustain.store (v); }, [fp] { return fp().biteSustain.load(); }, percent);
    add ("fm.velocity", "FM velocity to brightness", "FM", TargetKind::continuous,
         [fp] (float v) { fp().velocityBrightness.store (v); }, [fp] { return fp().velocityBrightness.load(); }, offOrPercent);
    add ("fm.lforate", "FM wobble speed", "FM", TargetKind::continuous,
         [fp] (float v) { fp().lfoRate.store (expMap (v, 0.1f, 20.0f)); },
         [fp] { return expInv (fp().lfoRate.load(), 0.1f, 20.0f); },
         [] (float v) { return juce::String (expMap (v, 0.1f, 20.0f), 1) + " per second"; });
    add ("fm.lfopitch", "FM wobble on pitch", "FM", TargetKind::continuous,
         [fp] (float v) { fp().lfoPitch.store (v); }, [fp] { return fp().lfoPitch.load(); }, offOrPercent);
    add ("fm.lfobrightness", "FM wobble on brightness", "FM", TargetKind::continuous,
         [fp] (float v) { fp().lfoBrightness.store (v); }, [fp] { return fp().lfoBrightness.load(); }, offOrPercent);

    // ---- External MIDI track: the selected track's outside gear
    auto xp = [&e]() -> ExternalParams& { return e.selected().ext; };
    auto onOff = [] (float v) { return v >= 0.5f ? juce::String ("On") : juce::String ("Off"); };
    for (int knob = 0; knob < ExternalParams::numKnobs; ++knob)
    {
        const auto n = juce::String (knob + 1);
        const auto k = (size_t) knob;
        add ("ext.cc" + n, "External knob " + n, "External MIDI", TargetKind::continuous,
             [xp, k] (float v) { xp().ccValue[k].store (v); xp().ccChanged[k].store (true); },
             [xp, k] { return xp().ccValue[k].load(); },
             [xp, k] (float v) { return "CC " + juce::String (xp().ccNumber[k].load()) + ": " + juce::String (juce::roundToInt (v * 127.0f)); });
        add ("ext.ccnum" + n, "External knob " + n + " CC number", "External MIDI", TargetKind::continuous,
             [xp, k] (float v) { xp().ccNumber[k].store (juce::jlimit (0, 127, juce::roundToInt (v * 127.0f))); },
             [xp, k] { return (float) xp().ccNumber[k].load() / 127.0f; },
             [n] (float v) { return "Knob " + n + " sends CC " + juce::String (juce::jlimit (0, 127, juce::roundToInt (v * 127.0f))); });
    }
    add ("ext.program", "External program change", "External MIDI", TargetKind::continuous,
         [xp] (float v) { xp().program.store (juce::jlimit (0, 127, juce::roundToInt (v * 127.0f))); xp().programChanged.store (true); },
         [xp] { return (float) xp().program.load() / 127.0f; },
         [] (float v) { return "Program " + juce::String (juce::jlimit (0, 127, juce::roundToInt (v * 127.0f)) + 1); });
    add ("ext.channel", "External MIDI channel", "External MIDI", TargetKind::continuous,
         [xp] (float v) { xp().channel.store (1 + juce::jlimit (0, 15, (int) (v * 15.999f))); },
         [xp] { return ((float) xp().channel.load() - 0.5f) / 16.0f; },
         [] (float v) { return "Channel " + juce::String (1 + juce::jlimit (0, 15, (int) (v * 15.999f))); });
    add ("ext.drums", "External steps play drums", "External MIDI", TargetKind::continuous,
         [xp] (float v) { xp().drums.store (v >= 0.5f); }, [xp] { return xp().drums.load() ? 1.0f : 0.0f; }, onOff);
    add ("ext.monitor", "External: hear the input", "External MIDI", TargetKind::continuous,
         [xp] (float v) { xp().monitor.store (v >= 0.5f); }, [xp] { return xp().monitor.load() ? 1.0f : 0.0f; }, onOff);
    add ("ext.inputpair", "External: which inputs", "External MIDI", TargetKind::continuous,
         [xp] (float v) { xp().inputPair.store (juce::jlimit (0, 3, (int) (v * 3.999f))); },
         [xp] { return ((float) xp().inputPair.load() + 0.5f) / 4.0f; },
         [] (float v) { const int p = juce::jlimit (0, 3, (int) (v * 3.999f)); return "Inputs " + juce::String (2 * p + 1) + "-" + juce::String (2 * p + 2); });

    // ---- Insert effects: the selected track's two slots
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto n = juce::String (slot + 1);
        const auto sl = (size_t) slot;
        auto ins = [&e, sl]() -> InsertSettings& { return e.selected().inserts[sl]; };
        auto typeOf = [] (float v) { return juce::jlimit (0, InsertEffects::numTypes - 1, (int) (v * ((float) InsertEffects::numTypes - 0.001f))); };
        add ("fx" + n + ".type", "Effect " + n, "Effects", TargetKind::continuous,
             [ins, typeOf] (float v) { ins().choose (typeOf (v)); },
             [ins] { return ((float) ins().type.load() + 0.5f) / (float) InsertEffects::numTypes; },
             [typeOf] (float v) { return InsertEffects::name (typeOf (v)); });
        for (int knob = 0; knob < 4; ++knob)
        {
            const auto kk = (size_t) knob;
            add ("fx" + n + ".k" + juce::String (knob + 1), "Effect " + n + " knob " + juce::String (knob + 1), "Effects", TargetKind::continuous,
                 [ins, kk] (float v) { ins().knob[kk].store (v); },
                 [ins, kk] { return ins().knob[kk].load(); },
                 [ins, knob] (float v) { return InsertEffects::describe (ins().type.load(), knob, v); });
        }
    }

    // ---- Performance effects on the whole mix: press to start, press again to stop
    add ("perform.repeat", "Repeat on/off (whole mix)", "Performance", TargetKind::toggle,
         [&e] (float) { toggle (e.perform.repeat); });
    add ("perform.stutter", "Stutter on/off (whole mix)", "Performance", TargetKind::toggle,
         [&e] (float) { toggle (e.perform.stutter); });
    add ("perform.tapestop", "Tape stop on/off (whole mix)", "Performance", TargetKind::toggle,
         [&e] (float) { toggle (e.perform.tapeStop); });
    add ("perform.division", "Repeat length", "Performance", TargetKind::continuous,
         [&e] (float v) { e.perform.repeatDivision.store (juce::jlimit (0, PerformSettings::numDivisions - 1, (int) (v * ((float) PerformSettings::numDivisions - 0.001f)))); },
         [&e] { return ((float) e.perform.repeatDivision.load() + 0.5f) / (float) PerformSettings::numDivisions; },
         [] (float v) { return PerformSettings::divisionName (juce::jlimit (0, PerformSettings::numDivisions - 1, (int) (v * ((float) PerformSettings::numDivisions - 0.001f)))); });
    add ("perform.stoptime", "Tape stop time", "Performance", TargetKind::continuous,
         [&e] (float v) { e.perform.stopTime.store (v); },
         [&e] { return e.perform.stopTime.load(); },
         [] (float v) { return seconds (PerformSettings::stopSeconds (v)); });

    // ---- Tracks
    add ("track.volume", "Track volume (selected track)", "Tracks", TargetKind::continuous,
         [&e] (float v) { e.selected().volume.store (v); },
         [&e] { return e.selected().volume.load(); }, percent);
    add ("track.mute", "Mute selected track", "Tracks", TargetKind::toggle,
         [&e] (float) { toggle (e.selected().mute); });
    add ("track.next", "Next track", "Tracks", TargetKind::trigger,
         [&e] (float) { e.selectedTrack.store (juce::jmin (AudioEngine::numTracks - 1, e.selectedTrack.load() + 1)); });
    add ("track.prev", "Previous track", "Tracks", TargetKind::trigger,
         [&e] (float) { e.selectedTrack.store (juce::jmax (0, e.selectedTrack.load() - 1)); });
    for (int t = 0; t < AudioEngine::numTracks; ++t)
        add ("track.select." + juce::String (t + 1), "Select track " + juce::String (t + 1), "Tracks", TargetKind::trigger,
             [&e, t] (float) { e.selectedTrack.store (t); });

    auto db = [] (float v) {
        const float d = linMap (v, -12.0f, 12.0f);
        return std::abs (d) < 0.3f ? juce::String ("Flat") : (d > 0 ? "+" : "") + juce::String (d, 1) + " dB";
    };
    add ("track.pan", "Track pan (selected track)", "Tracks", TargetKind::continuous,
         [&e] (float v) { const float b = v * 2.0f - 1.0f; e.selected().mix.pan.store (std::abs (b) < 0.04f ? 0.0f : b); },
         [&e] { return (e.selected().mix.pan.load() + 1.0f) * 0.5f; },
         [] (float v) {
             const float b = v * 2.0f - 1.0f;
             return std::abs (b) < 0.04f ? juce::String ("Centre") : (b < 0 ? "Left " : "Right ") + percent (std::abs (b));
         });
    add ("track.eqlow", "Track bass (EQ low)", "Tracks", TargetKind::continuous,
         [&e] (float v) { e.selected().mix.eqLow.store (linMap (v, -12.0f, 12.0f)); },
         [&e] { return linInv (e.selected().mix.eqLow.load(), -12.0f, 12.0f); }, db);
    add ("track.eqmid", "Track middle (EQ mid)", "Tracks", TargetKind::continuous,
         [&e] (float v) { e.selected().mix.eqMid.store (linMap (v, -12.0f, 12.0f)); },
         [&e] { return linInv (e.selected().mix.eqMid.load(), -12.0f, 12.0f); }, db);
    add ("track.eqhigh", "Track treble (EQ high)", "Tracks", TargetKind::continuous,
         [&e] (float v) { e.selected().mix.eqHigh.store (linMap (v, -12.0f, 12.0f)); },
         [&e] { return linInv (e.selected().mix.eqHigh.load(), -12.0f, 12.0f); }, db);
    add ("track.reverb", "Track reverb amount", "Tracks", TargetKind::continuous,
         [&e] (float v) { e.selected().mix.reverb.store (v); },
         [&e] { return e.selected().mix.reverb.load(); }, percent);
    add ("track.delay", "Track delay amount", "Tracks", TargetKind::continuous,
         [&e] (float v) { e.selected().mix.delay.store (v); },
         [&e] { return e.selected().mix.delay.load(); }, percent);
    add ("track.solo", "Solo selected track", "Tracks", TargetKind::toggle,
         [&e] (float) { toggle (e.selected().mix.solo); });
    add ("master.reverbsize", "Reverb size", "Transport", TargetKind::continuous,
         [&e] (float v) { e.effects.reverbSize.store (v); },
         [&e] { return e.effects.reverbSize.load(); }, percent);
    add ("master.delayfeedback", "Delay repeats", "Transport", TargetKind::continuous,
         [&e] (float v) { e.effects.delayFeedback.store (v * 0.9f); },
         [&e] { return e.effects.delayFeedback.load() / 0.9f; }, percent);

    // ---- Sampler: the selected track's sampler
    auto sm = [&e]() -> Sampler::Params& { return e.selected().samplerEngine.params; };
    add ("sampler.start", "Sampler start", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().start.store (v); }, [sm] { return sm().start.load(); },
         [] (float v) { return v < 0.005f ? juce::String ("Beginning") : percent (v) + " in"; });
    add ("sampler.length", "Sampler length", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().length.store (juce::jmax (0.01f, v)); }, [sm] { return sm().length.load(); },
         [] (float v) { return v > 0.995f ? juce::String ("To the end") : percent (v); });
    add ("sampler.pitch", "Sampler pitch", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().pitch.store (std::round (linMap (v, -24.0f, 24.0f))); },
         [sm] { return linInv (sm().pitch.load(), -24.0f, 24.0f); },
         [] (float v) {
             const int st = juce::roundToInt (linMap (v, -24.0f, 24.0f));
             return st == 0 ? juce::String ("Original") : (st > 0 ? "+" : "") + juce::String (st) + " semitones";
         });
    add ("sampler.brightness", "Sampler brightness", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().brightness.store (v); }, [sm] { return sm().brightness.load(); },
         [] (float v) { return v > 0.99f ? juce::String ("Full") : percent (v); });
    add ("sampler.attack", "Sampler fade in", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().attack.store (expMap (v, 0.001f, 2.0f)); },
         [sm] { return expInv (sm().attack.load(), 0.001f, 2.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 2.0f)); });
    add ("sampler.release", "Sampler fade out", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().release.store (expMap (v, 0.005f, 4.0f)); },
         [sm] { return expInv (sm().release.load(), 0.005f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.005f, 4.0f)); });
    add ("sampler.sustain", "Sampler hold level", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().sustain.store (v); }, [sm] { return sm().sustain.load(); }, percent);
    add ("sampler.loop", "Sampler loop on/off", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().loop.store (v >= 0.5f); }, [sm] { return sm().loop.load() ? 1.0f : 0.0f; },
         [] (float v) { return v >= 0.5f ? juce::String ("On") : juce::String ("Off"); });
    add ("sampler.reverse", "Sampler reverse on/off", "Sampler", TargetKind::continuous,
         [sm] (float v) { sm().reverse.store (v >= 0.5f); }, [sm] { return sm().reverse.load() ? 1.0f : 0.0f; },
         [] (float v) { return v >= 0.5f ? juce::String ("On") : juce::String ("Off"); });
    add ("sampler.slices", "Sampler slices", "Sampler", TargetKind::continuous,
         [sm] (float v) { const int opts[5] = { 1, 2, 4, 8, 16 }; sm().slices.store (opts[juce::jlimit (0, 4, (int) (v * 4.999f))]); },
         [sm] { const int s = sm().slices.load(); return s >= 16 ? 1.0f : s >= 8 ? 0.75f : s >= 4 ? 0.5f : s >= 2 ? 0.25f : 0.0f; },
         [] (float v) { const int opts[5] = { 1, 2, 4, 8, 16 }; const int n = opts[juce::jlimit (0, 4, (int) (v * 4.999f))];
                        return n == 1 ? juce::String ("Off") : juce::String (n) + " slices"; });

    add ("sampler.record", "Sampler record start / stop", "Sampler", TargetKind::trigger,
         [&e] (float) { e.recorder.toggleRequested.store (true); });

    // ---- Scenes and song
    add ("scene.next", "Next scene (starts at the next bar)", "Song", TargetKind::trigger,
         [&e] (float) {
             const int from = e.song.queuedScene.load() >= 0 ? e.song.queuedScene.load() : e.song.currentScene.load();
             e.song.queueScene ((from + 1) % Song::numScenes, e.transport.playing.load());
         });
    add ("scene.prev", "Previous scene (starts at the next bar)", "Song", TargetKind::trigger,
         [&e] (float) {
             const int from = e.song.queuedScene.load() >= 0 ? e.song.queuedScene.load() : e.song.currentScene.load();
             e.song.queueScene ((from + Song::numScenes - 1) % Song::numScenes, e.transport.playing.load());
         });
    add ("song.mode", "Song mode on/off", "Song", TargetKind::toggle,
         [&e] (float) { toggle (e.song.songMode); });

    // ---- Drums: the selected drum of the selected track
    auto dp = [&e]() -> DrumKit::DrumParams& {
        auto& tr = e.selected();
        return tr.kit.drums[(size_t) juce::jlimit (0, DrumKit::numDrums - 1, tr.selectedDrum.load())];
    };
    add ("drum.pitch", "Drum pitch", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().pitch.store (std::round (linMap (v, -12.0f, 12.0f))); },
         [dp] { return linInv (dp().pitch.load(), -12.0f, 12.0f); },
         [] (float v) {
             const int st = juce::roundToInt (linMap (v, -12.0f, 12.0f));
             return st == 0 ? juce::String ("Normal") : (st > 0 ? "+" : "") + juce::String (st) + " semitones";
         });
    add ("drum.length", "Drum length", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().length.store (v); },
         [dp] { return dp().length.load(); },
         [] (float v) {
             const float mult = std::pow (4.0f, (v - 0.5f) * 2.0f);
             return std::abs (mult - 1.0f) < 0.05f ? juce::String ("Normal")
                                                    : (mult < 1.0f ? "Shorter x" : "Longer x") + juce::String (mult < 1.0f ? 1.0f / mult : mult, 1);
         });
    add ("drum.tone", "Drum tone", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().tone.store (v); }, [dp] { return dp().tone.load(); }, percent);
    add ("drum.snap", "Drum snap", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().snap.store (v); }, [dp] { return dp().snap.load(); }, percent);
    add ("drum.drive", "Drum drive", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().drive.store (v); }, [dp] { return dp().drive.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("drum.pan", "Drum pan", "Drums", TargetKind::continuous,
         [dp] (float v) { const float b = v * 2.0f - 1.0f; dp().pan.store (std::abs (b) < 0.04f ? 0.0f : b); },
         [dp] { return (dp().pan.load() + 1.0f) * 0.5f; },
         [] (float v) {
             const float b = v * 2.0f - 1.0f;
             return std::abs (b) < 0.04f ? juce::String ("Centre") : (b < 0 ? "Left " : "Right ") + percent (std::abs (b));
         });
    add ("drum.volume", "Drum volume", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().volume.store (v); }, [dp] { return dp().volume.load(); }, percent);
    add ("drum.swing", "Kit swing", "Drums", TargetKind::continuous,
         [&e] (float v) { e.song.swing[(size_t) e.selectedTrack.load()].store (v); },
         [&e] { return e.song.swing[(size_t) e.selectedTrack.load()].load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Straight") : percent (v); });
    add ("drum.choke", "Hi-hat choke on/off", "Drums", TargetKind::continuous,
         [&e] (float v) { e.selected().kit.choke.store (v >= 0.5f); },
         [&e] { return e.selected().kit.choke.load() ? 1.0f : 0.0f; },
         [] (float v) { return v >= 0.5f ? juce::String ("On") : juce::String ("Off"); });
    add ("drum.velocity", "Kit velocity sensitivity", "Drums", TargetKind::continuous,
         [&e] (float v) { e.selected().kit.velocity.store (v); },
         [&e] { return e.selected().kit.velocity.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("All hits equal") : percent (v); });
    add ("drum.model", "Drum model (analogue / FM / metal)", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().model.store (juce::jlimit (0, 2, (int) (v * 2.999f))); },
         [dp] { return (float) dp().model.load() * 0.5f; },
         [] (float v) {
             const int m = juce::jlimit (0, 2, (int) (v * 2.999f));
             return m == 0 ? juce::String ("Analogue") : (m == 1 ? juce::String ("FM") : juce::String ("Metal"));
         });
    add ("drum.colour", "Drum colour (FM / metal character)", "Drums", TargetKind::continuous,
         [dp] (float v) { dp().colour.store (v); }, [dp] { return dp().colour.load(); }, percent);

    // ---- Noise
    auto np = [&e]() -> NoiseParams& { return e.selected().noiseParams; };
    auto colourName = [] (float v) {
        static const char* names[] = { "Brown", "Brown-pink", "Pink", "Pink-white", "White", "White-blue", "Blue", "Blue-violet", "Violet" };
        return juce::String (names[juce::jlimit (0, 8, juce::roundToInt (v * 8.0f))]);
    };
    add ("noise.colour", "Noise colour", "Noise", TargetKind::continuous,
         [np] (float v) { np().colour.store (v); }, [np] { return np().colour.load(); }, colourName);
    add ("noise.source", "Noise source", "Noise", TargetKind::continuous,
         [np] (float v) { np().source.store (juce::jlimit (0, NoiseParams::numSources - 1, (int) (v * ((float) NoiseParams::numSources - 0.001f)))); },
         [np] { return ((float) np().source.load() + 0.5f) / (float) NoiseParams::numSources; },
         [] (float v) { return NoiseParams::sourceName (juce::jlimit (0, NoiseParams::numSources - 1, (int) (v * ((float) NoiseParams::numSources - 0.001f)))); });
    add ("noise.mod", "Noise modifier", "Noise", TargetKind::continuous,
         [np] (float v) { np().mod.store (v); }, [np] { return np().mod.load(); }, percent);
    add ("noise.grain", "Noise grain", "Noise", TargetKind::continuous,
         [np] (float v) { np().grain.store (v); }, [np] { return np().grain.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Smooth") : percent (v); });
    add ("noise.tone", "Noise pitch (resonator)", "Noise", TargetKind::continuous,
         [np] (float v) { np().tone.store (v); }, [np] { return np().tone.load(); }, percent);
    add ("noise.filtermode", "Noise filter type", "Noise", TargetKind::continuous,
         [np] (float v) { np().filterMode.store (juce::jlimit (0, NoiseParams::numFilterModes - 1, (int) (v * ((float) NoiseParams::numFilterModes - 0.001f)))); },
         [np] { return ((float) np().filterMode.load() + 0.5f) / (float) NoiseParams::numFilterModes; },
         [] (float v) { return NoiseParams::filterName (juce::jlimit (0, NoiseParams::numFilterModes - 1, (int) (v * ((float) NoiseParams::numFilterModes - 0.001f)))); });
    add ("noise.cutoff", "Noise filter", "Noise", TargetKind::continuous,
         [np] (float v) { np().cutoffHz.store (expMap (v, 40.0f, 18000.0f)); },
         [np] { return expInv (np().cutoffHz.load(), 40.0f, 18000.0f); },
         [] (float v) { const float hz = expMap (v, 40.0f, 18000.0f); return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; });
    add ("noise.resonance", "Noise filter edge", "Noise", TargetKind::continuous,
         [np] (float v) { np().resonance.store (v); }, [np] { return np().resonance.load(); }, percent);
    add ("noise.keyfollow", "Noise filter follows the note", "Noise", TargetKind::continuous,
         [np] (float v) { np().keyFollow.store (v); }, [np] { return np().keyFollow.load(); }, percent);
    add ("noise.envamount", "Noise filter sweep", "Noise", TargetKind::continuous,
         [np] (float v) { np().envAmount.store (v * 2.0f - 1.0f); }, [np] { return (np().envAmount.load() + 1.0f) * 0.5f; },
         [] (float v) { const int p = juce::roundToInt ((v * 2.0f - 1.0f) * 100.0f); return p == 0 ? juce::String ("None") : (p > 0 ? "Opens " : "Closes ") + juce::String (std::abs (p)) + "%"; });
    add ("noise.attack", "Noise fade in", "Noise", TargetKind::continuous,
         [np] (float v) { np().attack.store (expMap (v, 0.001f, 4.0f)); },
         [np] { return expInv (np().attack.load(), 0.001f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 4.0f)); });
    add ("noise.decay", "Noise punch", "Noise", TargetKind::continuous,
         [np] (float v) { np().decay.store (expMap (v, 0.005f, 4.0f)); },
         [np] { return expInv (np().decay.load(), 0.005f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.005f, 4.0f)); });
    add ("noise.sustain", "Noise hold level", "Noise", TargetKind::continuous,
         [np] (float v) { np().sustain.store (v); }, [np] { return np().sustain.load(); }, percent);
    add ("noise.release", "Noise fade out", "Noise", TargetKind::continuous,
         [np] (float v) { np().release.store (expMap (v, 0.005f, 8.0f)); },
         [np] { return expInv (np().release.load(), 0.005f, 8.0f); },
         [] (float v) { return seconds (expMap (v, 0.005f, 8.0f)); });
    add ("noise.width", "Noise width", "Noise", TargetKind::continuous,
         [np] (float v) { np().width.store (v); }, [np] { return np().width.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Mono") : percent (v); });
    add ("noise.velocity", "Noise velocity to volume", "Noise", TargetKind::continuous,
         [np] (float v) { np().velocity.store (v); }, [np] { return np().velocity.load(); }, percent);
    add ("noise.lforate", "Noise wobble speed", "Noise", TargetKind::continuous,
         [np] (float v) { np().lfoRate.store (expMap (v, 0.05f, 30.0f)); },
         [np] { return expInv (np().lfoRate.load(), 0.05f, 30.0f); },
         [] (float v) { return juce::String (expMap (v, 0.05f, 30.0f), 2) + " Hz"; });
    add ("noise.lfofilter", "Noise wobble on the filter", "Noise", TargetKind::continuous,
         [np] (float v) { np().lfoFilter.store (v); }, [np] { return np().lfoFilter.load(); }, percent);
    add ("noise.lfocolour", "Noise wobble on the colour", "Noise", TargetKind::continuous,
         [np] (float v) { np().lfoColour.store (v); }, [np] { return np().lfoColour.load(); }, percent);
    add ("noise.level", "Noise level", "Noise", TargetKind::continuous,
         [np] (float v) { np().level.store (expMap (v, 0.25f, 4.0f)); },
         [np] { return expInv (np().level.load(), 0.25f, 4.0f); },
         [] (float v) { const float db = juce::Decibels::gainToDecibels (expMap (v, 0.25f, 4.0f)); return (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB"; });

    // ---- Voice
    auto vp = [&e]() -> VoiceParams& { return e.selected().voiceParams; };
    auto choose = [] (float v, int count) { return juce::jlimit (0, count - 1, (int) (v * ((float) count - 0.001f))); };
    add ("voice.mode", "Voice mode", "Voice", TargetKind::continuous,
         [vp, choose] (float v) { vp().mode.store (choose (v, VoiceParams::numModes)); },
         [vp] { return ((float) vp().mode.load() + 0.5f) / (float) VoiceParams::numModes; },
         [choose] (float v) { return VoiceParams::modeName (choose (v, VoiceParams::numModes)); });
    add ("voice.language", "Voice language", "Voice", TargetKind::continuous,
         [vp, choose] (float v) { vp().language.store (choose (v, speech::numLanguages)); },
         [vp] { return ((float) vp().language.load() + 0.5f) / (float) speech::numLanguages; },
         [choose] (float v) { return speech::languageName (choose (v, speech::numLanguages)); });
    add ("voice.speed", "Voice speed", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().speed.store (expMap (v, 0.4f, 2.5f)); },
         [vp] { return expInv (vp().speed.load(), 0.4f, 2.5f); },
         [] (float v) { return juce::String (expMap (v, 0.4f, 2.5f), 2) + "x"; });
    add ("voice.intonation", "Voice intonation", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().intonation.store (v); }, [vp] { return vp().intonation.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Robot") : percent (v); });
    add ("voice.size", "Voice size", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().size.store (v); }, [vp] { return vp().size.load(); },
         [] (float v) { return v < 0.2f ? juce::String ("Tiny") : v < 0.4f ? juce::String ("Small") : v <= 0.6f ? juce::String ("Normal")
                                       : v <= 0.8f ? juce::String ("Big") : juce::String ("Giant"); });
    add ("voice.buzz", "Voice buzz", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().buzz.store (v); }, [vp] { return vp().buzz.load(); }, percent);
    add ("voice.whisper", "Voice whisper", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().whisper.store (v); }, [vp] { return vp().whisper.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("voice.lofi", "Voice lo-fi", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().lofi.store (v); }, [vp] { return vp().lofi.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Clean") : percent (v); });
    add ("voice.attack", "Voice fade in", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().attack.store (expMap (v, 0.001f, 2.0f)); },
         [vp] { return expInv (vp().attack.load(), 0.001f, 2.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 2.0f)); });
    add ("voice.release", "Voice fade out", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().release.store (expMap (v, 0.005f, 4.0f)); },
         [vp] { return expInv (vp().release.load(), 0.005f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.005f, 4.0f)); });
    add ("voice.level", "Voice level", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().level.store (expMap (v, 0.25f, 4.0f)); },
         [vp] { return expInv (vp().level.load(), 0.25f, 4.0f); },
         [] (float v) { const float db = juce::Decibels::gainToDecibels (expMap (v, 0.25f, 4.0f)); return (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB"; });
    add ("voice.velocity", "Voice velocity to volume", "Voice", TargetKind::continuous,
         [vp] (float v) { vp().velocity.store (v); }, [vp] { return vp().velocity.load(); }, percent);

    // ---- Vocoder: the instrument, and what any track's vocoder (instrument or effect) listens to
    auto vc = [&e]() -> VocoderParams& { return e.selected().vocoderParams; };
    auto vs = [&e]() -> VocoderSettings& { return e.selected().vocoderParams.vocoder; };
    add ("vocoder.voice", "Vocoder listens to the Voice tracks", "Vocoder", TargetKind::continuous,
         [vs] (float v) { vs().voiceLevel.store (v); }, [vs] { return vs().voiceLevel.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("vocoder.mic", "Vocoder listens to the microphone", "Vocoder", TargetKind::continuous,
         [vs] (float v) { vs().micLevel.store (v); }, [vs] { return vs().micLevel.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("vocoder.track", "Vocoder listens to a track", "Vocoder", TargetKind::continuous,
         [vs] (float v) { vs().trackLevel.store (v); }, [vs] { return vs().trackLevel.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("vocoder.source", "Vocoder: which track it listens to", "Vocoder", TargetKind::continuous,
         [vs, choose] (float v) { vs().sourceTrack.store (choose (v, AudioEngine::numTracks)); },
         [vs] { return ((float) vs().sourceTrack.load() + 0.5f) / (float) AudioEngine::numTracks; },
         [choose] (float v) { return "Track " + juce::String (choose (v, AudioEngine::numTracks) + 1); });
    add ("vocoder.bands", "Vocoder bands", "Vocoder", TargetKind::continuous,
         [vs, choose] (float v) { vs().bands.store (choose (v, VocoderSettings::numBandChoices)); },
         [vs] { return ((float) vs().bands.load() + 0.5f) / (float) VocoderSettings::numBandChoices; },
         [choose] (float v) { return juce::String (VocoderSettings::bandCount (choose (v, VocoderSettings::numBandChoices))) + " bands"; });
    add ("vocoder.shift", "Vocoder formant shift", "Vocoder", TargetKind::continuous,
         [vs] (float v) { vs().shift.store (v); }, [vs] { return vs().shift.load(); },
         [] (float v) { const int st = juce::roundToInt (VocoderSettings::shiftSemitones (v)); return st == 0 ? juce::String ("None") : (st > 0 ? "+" : "") + juce::String (st) + " st"; });
    add ("vocoder.response", "Vocoder response", "Vocoder", TargetKind::continuous,
         [vs] (float v) { vs().response.store (v); }, [vs] { return vs().response.load(); },
         [] (float v) { return juce::String (juce::roundToInt (VocoderSettings::releaseMs (v))) + " ms"; });
    add ("vocoder.sibilance", "Vocoder sibilance", "Vocoder", TargetKind::continuous,
         [vs] (float v) { vs().sibilance.store (v); }, [vs] { return vs().sibilance.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("vocoder.wave", "Vocoder carrier", "Vocoder", TargetKind::continuous,
         [vc, choose] (float v) { vc().wave.store (choose (v, VocoderParams::numWaves)); },
         [vc] { return ((float) vc().wave.load() + 0.5f) / (float) VocoderParams::numWaves; },
         [choose] (float v) { return VocoderParams::waveName (choose (v, VocoderParams::numWaves)); });
    add ("vocoder.detune", "Vocoder detune", "Vocoder", TargetKind::continuous,
         [vc] (float v) { vc().detune.store (v); }, [vc] { return vc().detune.load(); }, percent);
    add ("vocoder.noise", "Vocoder breath", "Vocoder", TargetKind::continuous,
         [vc] (float v) { vc().noise.store (v); }, [vc] { return vc().noise.load(); },
         [] (float v) { return v < 0.01f ? juce::String ("Off") : percent (v); });
    add ("vocoder.attack", "Vocoder fade in", "Vocoder", TargetKind::continuous,
         [vc] (float v) { vc().attack.store (expMap (v, 0.001f, 4.0f)); },
         [vc] { return expInv (vc().attack.load(), 0.001f, 4.0f); },
         [] (float v) { return seconds (expMap (v, 0.001f, 4.0f)); });
    add ("vocoder.release", "Vocoder fade out", "Vocoder", TargetKind::continuous,
         [vc] (float v) { vc().release.store (expMap (v, 0.005f, 8.0f)); },
         [vc] { return expInv (vc().release.load(), 0.005f, 8.0f); },
         [] (float v) { return seconds (expMap (v, 0.005f, 8.0f)); });
    add ("vocoder.level", "Vocoder level", "Vocoder", TargetKind::continuous,
         [vc] (float v) { vc().level.store (expMap (v, 0.25f, 4.0f)); },
         [vc] { return expInv (vc().level.load(), 0.25f, 4.0f); },
         [] (float v) { const float db = juce::Decibels::gainToDecibels (expMap (v, 0.25f, 4.0f)); return (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB"; });
    add ("vocoder.velocity", "Vocoder velocity to volume", "Vocoder", TargetKind::continuous,
         [vc] (float v) { vc().velocity.store (v); }, [vc] { return vc().velocity.load(); }, percent);

    // ---- Arpeggiator
    auto pick = [] (float v, int count) { return juce::jlimit (0, count - 1, (int) (v * ((float) count - 0.001f))); };
    add ("arp.on", "Arpeggiator on / off", "Arpeggiator", TargetKind::toggle,
         [&e] (float) { toggle (e.arp.on); });
    add ("arp.mode", "Arpeggiator mode", "Arpeggiator", TargetKind::continuous,
         [&e, pick] (float v) { e.arp.mode.store (pick (v, ArpSettings::numModes)); },
         [&e] { return ((float) e.arp.mode.load() + 0.5f) / (float) ArpSettings::numModes; },
         [pick] (float v) { return ArpSettings::modeName (pick (v, ArpSettings::numModes)); });
    add ("arp.rate", "Arpeggiator rate", "Arpeggiator", TargetKind::continuous,
         [&e, pick] (float v) { e.arp.rate.store (pick (v, ArpSettings::numRates)); },
         [&e] { return ((float) e.arp.rate.load() + 0.5f) / (float) ArpSettings::numRates; },
         [pick] (float v) { return ArpSettings::rateName (pick (v, ArpSettings::numRates)); });
    add ("arp.octaves", "Arpeggiator octaves", "Arpeggiator", TargetKind::continuous,
         [&e, pick] (float v) { e.arp.octaves.store (1 + pick (v, 4)); },
         [&e] { return ((float) (e.arp.octaves.load() - 1) + 0.5f) / 4.0f; },
         [pick] (float v) { const int o = 1 + pick (v, 4); return juce::String (o) + (o == 1 ? " octave" : " octaves"); });
    add ("arp.gate", "Arpeggiator note length", "Arpeggiator", TargetKind::continuous,
         [&e] (float v) { e.arp.gate.store (linMap (v, 0.05f, 1.0f)); },
         [&e] { return linInv (e.arp.gate.load(), 0.05f, 1.0f); },
         [] (float v) { return percent (linMap (v, 0.05f, 1.0f)); });
    add ("arp.swing", "Arpeggiator swing", "Arpeggiator", TargetKind::continuous,
         [&e] (float v) { e.arp.swing.store (v); }, [&e] { return e.arp.swing.load(); }, percent);

    // ---- Glide (portamento) on every synth, and one target for the selected track's synth.
    auto glideSet = [] (std::atomic<float>& g, float v) { g.store (v < 0.01f ? 0.0f : expMap (v, 0.005f, 4.0f)); };
    auto glideGet = [] (const std::atomic<float>& g) { return g.load() <= 0.001f ? 0.0f : expInv (g.load(), 0.005f, 4.0f); };
    auto glideShow = [] (float v) { return v < 0.01f ? juce::String ("Off") : seconds (expMap (v, 0.005f, 4.0f)); };
    auto modeSet = [] (std::atomic<int>& m, float v) { m.store (v < 0.5f ? Glide::fixedTime : Glide::byDistance); };
    auto modeShow = [] (float v) { return Glide::modeName (v < 0.5f ? Glide::fixedTime : Glide::byDistance); };
    add ("synth.glide", "Test synth glide", "Test synth", TargetKind::continuous,
         [&e, glideSet] (float v) { glideSet (e.selected().synthParams.glide, v); },
         [&e, glideGet] { return glideGet (e.selected().synthParams.glide); }, glideShow);
    add ("fm.glide", "FM glide", "FM", TargetKind::continuous,
         [&e, glideSet] (float v) { glideSet (e.selected().fmParams.glide, v); },
         [&e, glideGet] { return glideGet (e.selected().fmParams.glide); }, glideShow);
    add ("noise.glide", "Noise glide", "Noise", TargetKind::continuous,
         [&e, glideSet] (float v) { glideSet (e.selected().noiseParams.glide, v); },
         [&e, glideGet] { return glideGet (e.selected().noiseParams.glide); }, glideShow);
    add ("vocoder.glide", "Vocoder glide", "Vocoder", TargetKind::continuous,
         [&e, glideSet] (float v) { glideSet (e.selected().vocoderParams.glide, v); },
         [&e, glideGet] { return glideGet (e.selected().vocoderParams.glide); }, glideShow);
    add ("voice.glide", "Voice glide", "Voice", TargetKind::continuous,
         [&e, glideSet] (float v) { glideSet (e.selected().voiceParams.glide, v); },
         [&e, glideGet] { return glideGet (e.selected().voiceParams.glide); }, glideShow);
    auto glideOf = [&e]() -> std::atomic<float>* {
        auto& tr = e.selected();
        switch (tr.type.load())
        {
            case Track::testSynth: return &tr.synthParams.glide;
            case Track::poly:      return &tr.polyParams.glide;
            case Track::wavetable: return &tr.waveParams.glide;
            case Track::fm:        return &tr.fmParams.glide;
            case Track::noise:     return &tr.noiseParams.glide;
            case Track::voice:     return &tr.voiceParams.glide;
            case Track::vocoder:   return &tr.vocoderParams.glide;
            default:               return nullptr;
        }
    };
    auto glideModeOf = [&e]() -> std::atomic<int>* {
        auto& tr = e.selected();
        switch (tr.type.load())
        {
            case Track::testSynth: return &tr.synthParams.glideMode;
            case Track::poly:      return &tr.polyParams.glideMode;
            case Track::wavetable: return &tr.waveParams.glideMode;
            case Track::fm:        return &tr.fmParams.glideMode;
            case Track::noise:     return &tr.noiseParams.glideMode;
            case Track::voice:     return &tr.voiceParams.glideMode;
            case Track::vocoder:   return &tr.vocoderParams.glideMode;
            default:               return nullptr;
        }
    };
    add ("track.glide", "Glide time (selected track's synth)", "Tracks", TargetKind::continuous,
         [glideOf, glideSet] (float v) { if (auto* g = glideOf()) glideSet (*g, v); },
         [glideOf, glideGet] { auto* g = glideOf(); return g != nullptr ? glideGet (*g) : 0.0f; }, glideShow);
    for (const char* id : { "synth.glidemode", "poly.glidemode", "wave.glidemode", "fm.glidemode", "noise.glidemode", "voice.glidemode", "vocoder.glidemode", "track.glidemode" })
        add (id, "Glide: fixed time or by distance", "Tracks", TargetKind::continuous,
             [glideModeOf, modeSet] (float v) { if (auto* m = glideModeOf()) modeSet (*m, v); },
             [glideModeOf] { auto* m = glideModeOf(); return m != nullptr && m->load() == Glide::byDistance ? 0.75f : 0.25f; }, modeShow);

    // ---- Oscillators section of the selected track's synth
    auto oscOf = [&e]() -> OscSettings* {
        auto& tr = e.selected();
        switch (tr.type.load())
        {
            case Track::testSynth: return &tr.synthParams.osc;
            case Track::poly:      return &tr.polyParams.osc;
            case Track::wavetable: return &tr.waveParams.osc;
            case Track::fm:        return &tr.fmParams.osc;
            case Track::noise:     return &tr.noiseParams.osc;
            case Track::voice:     return &tr.voiceParams.osc;
            case Track::vocoder:   return &tr.vocoderParams.osc;
            default:               return nullptr;
        }
    };
    add ("osc.main", "Oscillators: the synth's own sound", "Oscillators", TargetKind::continuous,
         [oscOf] (float v) { if (auto* o = oscOf()) o->main.store (v); },
         [oscOf] { auto* o = oscOf(); return o != nullptr ? o->main.load() : 0.0f; }, percent);
    add ("osc.sync", "Oscillators: sync 2 to 1 on / off", "Oscillators", TargetKind::toggle,
         [oscOf] (float) { if (auto* o = oscOf()) toggle (o->sync); });
    add ("osc.ring", "Oscillators: ring 1 x 2 on / off", "Oscillators", TargetKind::toggle,
         [oscOf] (float) { if (auto* o = oscOf()) toggle (o->ring); });
    for (int n = 0; n < 2; ++n)
    {
        const auto id = "osc" + juce::String (n + 1);
        const auto name = "Oscillator " + juce::String (n + 1);
        auto one = [oscOf, n]() -> OscSettings::Osc* { auto* o = oscOf(); return o != nullptr ? &o->osc[(size_t) n] : nullptr; };
        add (id + ".wave", name + " wave", "Oscillators", TargetKind::continuous,
             [one] (float v) { if (auto* o = one()) o->wave.store (juce::jlimit (0, OscSettings::numWaves - 1, (int) (v * ((float) OscSettings::numWaves - 0.001f)))); },
             [one] { auto* o = one(); return o != nullptr ? ((float) o->wave.load() + 0.5f) / (float) OscSettings::numWaves : 0.0f; },
             [] (float v) { return OscSettings::waveName (juce::jlimit (0, OscSettings::numWaves - 1, (int) (v * ((float) OscSettings::numWaves - 0.001f)))); });
        add (id + ".octave", name + " octave", "Oscillators", TargetKind::continuous,
             [one] (float v) { if (auto* o = one()) o->octave.store (juce::jlimit (-2, 2, juce::roundToInt (v * 4.0f) - 2)); },
             [one] { auto* o = one(); return o != nullptr ? (float) (o->octave.load() + 2) / 4.0f : 0.5f; },
             [] (float v) { const int o = juce::jlimit (-2, 2, juce::roundToInt (v * 4.0f) - 2); return o == 0 ? juce::String ("0") : (o > 0 ? "+" : "") + juce::String (o) + " oct"; });
        add (id + ".semi", name + " semitone", "Oscillators", TargetKind::continuous,
             [one] (float v) { if (auto* o = one()) o->semi.store (juce::jlimit (-12, 12, juce::roundToInt (v * 24.0f) - 12)); },
             [one] { auto* o = one(); return o != nullptr ? (float) (o->semi.load() + 12) / 24.0f : 0.5f; },
             [] (float v) { const int s = juce::jlimit (-12, 12, juce::roundToInt (v * 24.0f) - 12); return (s > 0 ? "+" : "") + juce::String (s) + " st"; });
        add (id + ".fine", name + " fine tune", "Oscillators", TargetKind::continuous,
             [one] (float v) { if (auto* o = one()) o->fine.store (linMap (v, -50.0f, 50.0f)); },
             [one] { auto* o = one(); return o != nullptr ? linInv (o->fine.load(), -50.0f, 50.0f) : 0.5f; },
             [] (float v) { const int c = juce::roundToInt (linMap (v, -50.0f, 50.0f)); return (c > 0 ? "+" : "") + juce::String (c) + " cents"; });
        add (id + ".width", name + " pulse width", "Oscillators", TargetKind::continuous,
             [one] (float v) { if (auto* o = one()) o->width.store (linMap (v, 0.05f, 0.95f)); },
             [one] { auto* o = one(); return o != nullptr ? linInv (o->width.load(), 0.05f, 0.95f) : 0.5f; },
             [] (float v) { return percent (linMap (v, 0.05f, 0.95f)); });
        add (id + ".level", name + " level", "Oscillators", TargetKind::continuous,
             [one] (float v) { if (auto* o = one()) o->level.store (v); },
             [one] { auto* o = one(); return o != nullptr ? o->level.load() : 0.0f; },
             [] (float v) { return v < 0.005f ? juce::String ("Off") : percent (v); });
    }

    // ---- Panel knobs 1-8: whatever the loaded synth puts on that knob.
    for (int k = 0; k < 8; ++k)
    {
        auto knobTarget = [this, &e, k]() -> const Target* {
            const auto& library = synthLibrary();
            const auto& def = library[(size_t) juce::jlimit (0, (int) library.size() - 1, e.selected().type.load())];
            // While the More window is open, the knobs turn its highlighted row instead.
            if (const int page = e.knobPage.load(); page > 0)
            {
                const auto i = (size_t) ((page - 1) * 8 + k);
                return i < def.more.size() ? find (def.more[i].target) : nullptr;
            }
            return find (def.knobs[(size_t) k].target);
        };
        add ("panel.knob." + juce::String (k + 1), "Knob " + juce::String (k + 1) + " (loaded synth)", "Panel",
             TargetKind::continuous,
             [knobTarget] (float v) { if (auto* t = knobTarget()) t->set (v); },
             [knobTarget] { auto* t = knobTarget(); return t != nullptr && t->get ? t->get() : 0.0f; },
             [knobTarget] (float v) { auto* t = knobTarget(); return t != nullptr && t->show ? t->show (v) : juce::String(); });
    }

    // ---- Steps
    for (int i = 0; i < Song::numSteps; ++i)
        add ("seq.step." + juce::String (i + 1), "Step " + juce::String (i + 1) + " on/off", "Steps", TargetKind::toggle,
             [&e, i] (float) { e.toggleStep (i); });
    add ("pads.mode", "Pads: play drums / switch steps", "Steps", TargetKind::toggle,
         [&e] (float) { toggle (e.padsPlay); });

    // ---- Stems
    for (int i = 0; i < StemDeck::numStems; ++i)
    {
        auto& s = e.stems.stem (i);
        const auto n = juce::String (i + 1);
        const auto group = "Stem " + n;

        add ("stem." + n + ".volume", "Stem " + n + " volume", group, TargetKind::continuous,
             [&s] (float v) { s.volume.store (v); },
             [&s] { return s.volume.load(); }, percent);
        add ("stem." + n + ".filter", "Stem " + n + " filter (LP/HP)", group, TargetKind::continuous,
             [&s] (float v) {
                 const float bipolar = v * 2.0f - 1.0f;
                 s.filter.store (std::abs (bipolar) < 0.04f ? 0.0f : bipolar); // centre detent
             },
             [&s] { return (s.filter.load() + 1.0f) * 0.5f; },
             [] (float v) {
                 const float b = v * 2.0f - 1.0f;
                 return std::abs (b) < 0.04f ? juce::String ("Off")
                                             : (b < 0 ? "Muffle " : "Thin ") + percent (std::abs (b));
             });
        add ("stem." + n + ".mute", "Stem " + n + " mute", group, TargetKind::toggle,
             [&s] (float) { toggle (s.mute); });
        add ("stem." + n + ".solo", "Stem " + n + " solo", group, TargetKind::toggle,
             [&s] (float) { toggle (s.solo); });
        add ("stem." + n + ".restart", "Stem " + n + " restart (on beat)", group, TargetKind::trigger,
             [&s] (float) { s.restartRequested.store (true); });
        add ("stem." + n + ".roll1", "Stem " + n + " roll 1 beat (hold)", group, TargetKind::momentary,
             [&s] (float v) { if (v > 0.5f) s.roll.store (1); else if (s.roll.load() == 1) s.roll.store (0); });
        add ("stem." + n + ".roll4", "Stem " + n + " roll 1/16 (hold)", group, TargetKind::momentary,
             [&s] (float v) { if (v > 0.5f) s.roll.store (4); else if (s.roll.load() == 4) s.roll.store (0); });
        add ("stem." + n + ".filterreset", "Stem " + n + " filter reset", group, TargetKind::trigger,
             [&s] (float) { s.filter.store (0.0f); });
    }
}

void ControlTargets::add (const juce::String& id, const juce::String& name, const juce::String& group,
                          TargetKind kind, std::function<void (float)> set,
                          std::function<float()> get, std::function<juce::String (float)> show)
{
    index[id] = targets.size();
    targets.push_back ({ id, name, group, kind, std::move (set), std::move (get), std::move (show) });
}

float ControlTargets::getValue (const juce::String& id) const
{
    const auto* t = find (id);
    return t != nullptr && t->get ? t->get() : 0.0f;
}

juce::String ControlTargets::describe (const juce::String& id) const
{
    const auto* t = find (id);
    return t != nullptr && t->show ? t->show (getValue (id)) : juce::String();
}

const ControlTargets::Target* ControlTargets::find (const juce::String& id) const
{
    const auto it = index.find (id);
    return it != index.end() ? &targets[it->second] : nullptr;
}

void ControlTargets::applyValue (const juce::String& id, float value01) const
{
    if (const auto* t = find (id))
    {
        if (t->kind == TargetKind::continuous)
            t->set (value01);
        else
            applyButton (id, value01 >= 0.5f);
    }
}

void ControlTargets::applyButton (const juce::String& id, bool pressed) const
{
    const auto* t = find (id);
    if (t == nullptr)
        return;

    switch (t->kind)
    {
        case TargetKind::momentary:  t->set (pressed ? 1.0f : 0.0f); break;
        case TargetKind::toggle:
        case TargetKind::trigger:    if (pressed) t->set (1.0f); break;
        case TargetKind::continuous: t->set (pressed ? 1.0f : 0.0f); break;
    }
}

} // namespace bizgroove
