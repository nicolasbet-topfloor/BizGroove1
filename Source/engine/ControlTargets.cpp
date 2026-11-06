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

    // ---- Panel knobs 1-8: whatever the loaded synth puts on that knob.
    for (int k = 0; k < 8; ++k)
    {
        auto knobTarget = [this, &e, k]() -> const Target* {
            const auto& library = synthLibrary();
            const auto& def = library[(size_t) juce::jlimit (0, (int) library.size() - 1, e.selected().type.load())];
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
