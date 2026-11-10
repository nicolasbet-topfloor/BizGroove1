#include "Presets.h"
#include "Project.h"
#include "SynthLibrary.h"

namespace bizgroove
{

namespace
{
    using Obj = juce::DynamicObject;

    float getF (const juce::var& v, const char* key, float fallback) { return v.hasProperty (key) ? (float) (double) v[key] : fallback; }
    int   getI (const juce::var& v, const char* key, int fallback)   { return v.hasProperty (key) ? (int) v[key] : fallback; }
    bool  getB (const juce::var& v, const char* key, bool fallback)  { return v.hasProperty (key) ? (bool) v[key] : fallback; }

    int clampType (int type) { return juce::jlimit (0, Track::numTypes - 1, type); }

    // ------------------------------------------------------------ to JSON
    juce::var synthToVar (const TestSynthParams& sp)
    {
        auto* s = new Obj();
        s->setProperty ("cutoff", sp.cutoffHz.load());     s->setProperty ("resonance", sp.resonance.load());
        s->setProperty ("envAmount", sp.envAmount.load()); s->setProperty ("detune", sp.detuneCents.load());
        s->setProperty ("attack", sp.attack.load());       s->setProperty ("decay", sp.decay.load());
        s->setProperty ("sustain", sp.sustain.load());     s->setProperty ("release", sp.release.load());
        return juce::var (s);
    }

    juce::var kitToVar (const DrumKit& kit)
    {
        auto* k = new Obj();
        k->setProperty ("choke", kit.choke.load());
        k->setProperty ("velocity", kit.velocity.load());
        juce::Array<juce::var> drums;
        for (const auto& d : kit.drums)
        {
            juce::Array<juce::var> a;
            a.add (d.pitch.load()); a.add (d.length.load()); a.add (d.tone.load()); a.add (d.snap.load());
            a.add (d.drive.load()); a.add (d.pan.load());    a.add (d.volume.load());
            a.add (d.model.load()); a.add (d.colour.load());
            drums.add (a);
        }
        k->setProperty ("drums", drums);
        return juce::var (k);
    }

    juce::var samplerToVar (const Sampler::Params& sm, const juce::String& fileName)
    {
        auto* m = new Obj();
        m->setProperty ("start", sm.start.load());           m->setProperty ("length", sm.length.load());
        m->setProperty ("pitch", sm.pitch.load());           m->setProperty ("brightness", sm.brightness.load());
        m->setProperty ("attack", sm.attack.load());         m->setProperty ("release", sm.release.load());
        m->setProperty ("sustain", sm.sustain.load());       m->setProperty ("loop", sm.loop.load());
        m->setProperty ("reverse", sm.reverse.load());       m->setProperty ("slices", sm.slices.load());
        m->setProperty ("file", fileName);
        m->setProperty ("loopStart", sm.loopStart.load());
        juce::Array<juce::var> marks;
        for (int i = 0; i < juce::jlimit (0, Sampler::maxSlices, sm.sliceCount.load()); ++i)
            marks.add (sm.slicePoints[(size_t) i].load());
        m->setProperty ("sliceMarks", marks);
        return juce::var (m);
    }

    juce::var soundOf (int type, const char* key, const juce::var& settings)
    {
        auto* o = new Obj();
        o->setProperty ("app", "BizGroove");
        o->setProperty ("kind", "sound");
        o->setProperty ("formatVersion", Presets::formatVersion);
        o->setProperty ("type", type);
        o->setProperty ("instrument", Presets::instrumentName (type));
        o->setProperty (key, settings);
        return juce::var (o);
    }

    // ------------------------------------------------------------ factory sounds
    Presets::FactorySound synthPreset (const char* name, float cutoffHz, float resonance, float envAmount, float detuneCents,
                                       float attack, float decay, float sustain, float release)
    {
        TestSynthParams p;
        p.cutoffHz.store (cutoffHz);   p.resonance.store (resonance);
        p.envAmount.store (envAmount); p.detuneCents.store (detuneCents);
        p.attack.store (attack);       p.decay.store (decay);
        p.sustain.store (sustain);     p.release.store (release);
        return { name, soundOf (Track::testSynth, "synth", synthToVar (p)) };
    }

    template <typename Tweak>
    Presets::FactorySound kitPreset (const char* name, Tweak&& tweak)
    {
        DrumKit kit;   // starts from the standard kit
        for (int d = 0; d < DrumKit::numDrums; ++d)
        {
            auto& p = kit.drums[(size_t) d];
            tweak (d, p);
            p.pitch.store  (juce::jlimit (-12.0f, 12.0f, p.pitch.load()));
            p.length.store (juce::jlimit (0.0f, 1.0f, p.length.load()));
            p.tone.store   (juce::jlimit (0.0f, 1.0f, p.tone.load()));
            p.snap.store   (juce::jlimit (0.0f, 1.0f, p.snap.load()));
            p.drive.store  (juce::jlimit (0.0f, 1.0f, p.drive.load()));
        }
        return { name, soundOf (Track::drums, "kit", kitToVar (kit)) };
    }

    Presets::FactorySound samplerPreset (const char* name, float attack, float release, float sustain,
                                         bool loop, bool reverse, int slices)
    {
        Sampler::Params p;
        p.attack.store (attack);   p.release.store (release); p.sustain.store (sustain);
        p.loop.store (loop);       p.reverse.store (reverse); p.slices.store (slices);
        return { name, soundOf (Track::sampler, "sampler", samplerToVar (p, {})) };
    }

    juce::var polyToVar (const PolyParams& pp)
    {
        auto* p = new Obj();
        p->setProperty ("cutoff", pp.cutoffHz.load());    p->setProperty ("resonance", pp.resonance.load());
        p->setProperty ("shape", pp.shape.load());        p->setProperty ("envAmount", pp.envAmount.load());
        p->setProperty ("attack", pp.attack.load());      p->setProperty ("decay", pp.decay.load());
        p->setProperty ("sustain", pp.sustain.load());    p->setProperty ("release", pp.release.load());
        p->setProperty ("detune", pp.detuneCents.load()); p->setProperty ("pulseWidth", pp.pulseWidth.load());
        p->setProperty ("mix", pp.mix.load());            p->setProperty ("octave", pp.octave.load());
        p->setProperty ("unison", pp.unison.load());      p->setProperty ("glide", pp.glide.load());
        p->setProperty ("lfoRate", pp.lfoRate.load());    p->setProperty ("lfoFilter", pp.lfoFilter.load());
        p->setProperty ("lfoPulse", pp.lfoPulse.load());  p->setProperty ("noise", pp.noise.load());
        return juce::var (p);
    }

    void applyPoly (PolyParams& pp, const juce::var& p)
    {
        pp.cutoffHz.store (getF (p, "cutoff", 1800.0f));  pp.resonance.store (getF (p, "resonance", 0.25f));
        pp.shape.store (getF (p, "shape", 0.0f));         pp.envAmount.store (getF (p, "envAmount", 0.45f));
        pp.attack.store (getF (p, "attack", 0.005f));     pp.decay.store (getF (p, "decay", 0.45f));
        pp.sustain.store (getF (p, "sustain", 0.7f));     pp.release.store (getF (p, "release", 0.35f));
        pp.detuneCents.store (getF (p, "detune", 7.0f));  pp.pulseWidth.store (getF (p, "pulseWidth", 0.5f));
        pp.mix.store (getF (p, "mix", 0.5f));             pp.octave.store (juce::jlimit (-1, 1, getI (p, "octave", 0)));
        pp.unison.store (juce::jlimit (1, 4, getI (p, "unison", 1)));
        pp.glide.store (getF (p, "glide", 0.0f));
        pp.lfoRate.store (getF (p, "lfoRate", 5.0f));     pp.lfoFilter.store (getF (p, "lfoFilter", 0.0f));
        pp.lfoPulse.store (getF (p, "lfoPulse", 0.0f));   pp.noise.store (getF (p, "noise", 0.0f));
    }

    template <typename Tweak>
    Presets::FactorySound polyPreset (const char* name, Tweak&& tweak)
    {
        PolyParams p;   // starts from the Poly's own settings
        tweak (p);
        return { name, soundOf (Track::poly, "poly", polyToVar (p)) };
    }

    template <typename Tweak>
    Presets::FactorySound wavePreset (const char* name, Tweak&& tweak)
    {
        WaveParams p;   // starts from the Wavetable's own settings
        tweak (p);
        return { name, soundOf (Track::wavetable, "wave", waveParamsToVar (p)) };
    }

    template <typename Tweak>
    Presets::FactorySound fmPreset (const char* name, Tweak&& tweak)
    {
        FmParams p;   // starts from the FM synth's own settings
        tweak (p);
        return { name, soundOf (Track::fm, "fm", fmParamsToVar (p)) };
    }

    void setOperators (FmParams& p, std::array<float, 4> ratios, std::array<float, 4> levels)
    {
        for (size_t op = 0; op < 4; ++op)
        {
            p.ratio[op].store (ratios[op]);
            p.level[op].store (levels[op]);
        }
    }

}

// ================================================================ names and folders

juce::String Presets::instrumentName (int type)
{
    const auto& library = synthLibrary();
    return juce::isPositiveAndBelow (type, (int) library.size()) ? library[(size_t) type].name : juce::String ("Sound");
}

juce::File Presets::userFolder (int type)
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("BizGroove")
                      .getChildFile ("Sounds")
                      .getChildFile (juce::File::createLegalFileName (instrumentName (type)));
    folder.createDirectory();
    return folder;
}

juce::Array<juce::File> Presets::userSounds (int type)
{
    auto files = userFolder (type).findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);

    struct ByName
    {
        int compareElements (const juce::File& a, const juce::File& b) const
        {
            return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension());
        }
    };
    ByName byName;
    files.sort (byName);
    return files;
}

juce::File Presets::fileForName (int type, const juce::String& name, bool allowExisting)
{
    auto base = juce::File::createLegalFileName (name.trim());
    if (base.isEmpty())
        base = "My sound";

    const auto folder = userFolder (type);
    auto file = folder.getChildFile (base + fileExtension);
    for (int n = 2; ! allowExisting && file.exists(); ++n)
        file = folder.getChildFile (base + " " + juce::String (n) + fileExtension);
    return file;
}

// ================================================================ factory sounds

const std::vector<Presets::FactorySound>& Presets::factorySounds (int type)
{
    static const std::vector<FactorySound> synth {
        //            name           cutoff   edge   wah   thick  fade in  punch  hold  fade out
        synthPreset ("Init",          1200.0f, 0.35f, 0.6f, 8.0f,  0.003f,  0.25f, 0.4f, 0.2f),
        synthPreset ("Round bass",     380.0f, 0.20f, 0.5f, 4.0f,  0.002f,  0.30f, 0.6f, 0.08f),
        synthPreset ("Acid line",      260.0f, 0.85f, 0.9f, 0.0f,  0.002f,  0.18f, 0.0f, 0.10f),
        synthPreset ("Saw lead",      3200.0f, 0.30f, 0.3f, 12.0f, 0.005f,  0.40f, 0.8f, 0.25f),
        synthPreset ("Soft pad",       900.0f, 0.15f, 0.2f, 18.0f, 0.80f,   1.50f, 0.8f, 1.8f),
        synthPreset ("Pluck",          600.0f, 0.40f, 0.8f, 6.0f,  0.001f,  0.20f, 0.0f, 0.30f),
        synthPreset ("Brass stab",     700.0f, 0.25f, 0.7f, 10.0f, 0.06f,   0.50f, 0.7f, 0.30f),
        synthPreset ("Slow sweep",     200.0f, 0.60f, 1.0f, 15.0f, 0.50f,   2.00f, 0.5f, 2.0f)
    };

    static const std::vector<FactorySound> kits {
        kitPreset ("Standard", [] (int, DrumKit::DrumParams&) {}),
        kitPreset ("Tight", [] (int, DrumKit::DrumParams& p) {
            p.length.store (p.length.load() * 0.6f);
            p.snap.store (p.snap.load() + 0.25f);
        }),
        kitPreset ("Boomy", [] (int d, DrumKit::DrumParams& p) {
            if (d == 0) { p.pitch.store (p.pitch.load() - 3.0f); p.length.store (0.8f); }
            else        { p.length.store (p.length.load() + 0.15f); }
            p.tone.store (p.tone.load() - 0.1f);
        }),
        kitPreset ("Dirty", [] (int, DrumKit::DrumParams& p) {
            p.drive.store (0.55f);
            p.tone.store (p.tone.load() + 0.1f);
        }),
        kitPreset ("FM kit", [] (int d, DrumKit::DrumParams& p) {
            const bool metal = d == 6 || d == 8 || d == 10 || d == 13 || d == 15;
            p.model.store (1);
            p.tone.store (d == 0 ? 0.25f : 0.5f);
            p.colour.store (d == 0 ? 0.15f : (metal ? 0.9f : 0.45f));
        }),
        kitPreset ("Metal kit", [] (int d, DrumKit::DrumParams& p) {
            if (d != 0) { p.model.store (2); p.colour.store (0.6f); }
        })
    };

    static const std::vector<FactorySound> sampler {
        //              name             fade in  fade out hold  loop   reverse slices
        samplerPreset ("One-shot",       0.002f,  0.25f,   1.0f, false, false,  1),
        samplerPreset ("Loop",           0.01f,   0.30f,   1.0f, true,  false,  1),
        samplerPreset ("Reversed",       0.05f,   0.40f,   1.0f, false, true,   1),
        samplerPreset ("Chop into 8",    0.002f,  0.05f,   1.0f, false, false,  8)
    };

    static const std::vector<FactorySound> polys {
        polyPreset ("Init", [] (PolyParams&) {}),
        polyPreset ("Brass", [] (PolyParams& p) {
            p.cutoffHz.store (600.0f); p.resonance.store (0.15f); p.envAmount.store (0.7f);
            p.attack.store (0.06f); p.decay.store (0.6f); p.sustain.store (0.6f); p.release.store (0.25f);
            p.detuneCents.store (8.0f); p.mix.store (0.3f);
        }),
        polyPreset ("Strings", [] (PolyParams& p) {
            p.cutoffHz.store (2500.0f); p.resonance.store (0.1f); p.envAmount.store (0.1f);
            p.attack.store (0.5f); p.decay.store (1.0f); p.sustain.store (0.9f); p.release.store (1.2f);
            p.detuneCents.store (14.0f); p.mix.store (0.2f); p.unison.store (2);
            p.lfoRate.store (0.4f); p.lfoPulse.store (0.3f);
        }),
        polyPreset ("Warm pad", [] (PolyParams& p) {
            p.cutoffHz.store (900.0f); p.resonance.store (0.2f); p.envAmount.store (0.25f);
            p.attack.store (1.2f); p.decay.store (2.0f); p.sustain.store (0.8f); p.release.store (2.5f);
            p.detuneCents.store (10.0f); p.unison.store (2); p.lfoRate.store (0.3f); p.lfoFilter.store (0.15f);
        }),
        polyPreset ("Hollow notch", [] (PolyParams& p) {
            p.cutoffHz.store (1200.0f); p.resonance.store (0.5f); p.shape.store (0.5f); p.envAmount.store (0.3f);
            p.attack.store (0.02f); p.decay.store (0.8f); p.sustain.store (0.6f); p.release.store (0.6f);
            p.mix.store (0.6f);
        }),
        polyPreset ("Unison lead", [] (PolyParams& p) {
            p.cutoffHz.store (3000.0f); p.resonance.store (0.3f); p.envAmount.store (0.3f);
            p.attack.store (0.003f); p.decay.store (0.3f); p.sustain.store (0.8f); p.release.store (0.2f);
            p.detuneCents.store (18.0f); p.unison.store (4); p.glide.store (0.06f); p.mix.store (0.4f);
        }),
        polyPreset ("Fat bass", [] (PolyParams& p) {
            p.cutoffHz.store (300.0f); p.resonance.store (0.3f); p.envAmount.store (0.6f);
            p.attack.store (0.002f); p.decay.store (0.25f); p.sustain.store (0.4f); p.release.store (0.1f);
            p.detuneCents.store (5.0f); p.octave.store (-1);
        }),
        polyPreset ("Square keys", [] (PolyParams& p) {
            p.cutoffHz.store (1500.0f); p.resonance.store (0.1f); p.envAmount.store (0.4f);
            p.attack.store (0.003f); p.decay.store (0.8f); p.sustain.store (0.3f); p.release.store (0.4f);
            p.mix.store (1.0f); p.pulseWidth.store (0.35f);
        }),
        polyPreset ("Thin sweep", [] (PolyParams& p) {
            p.cutoffHz.store (200.0f); p.shape.store (1.0f); p.envAmount.store (0.9f);
            p.attack.store (0.5f); p.decay.store (3.0f); p.sustain.store (0.3f); p.release.store (2.0f);
            p.lfoRate.store (0.2f); p.lfoFilter.store (0.3f);
        })
    };

    static const std::vector<FactorySound> waves {
        wavePreset ("Init", [] (WaveParams&) {}),
        wavePreset ("Glass keys", [] (WaveParams& p) {
            p.table.store (1); p.position.store (0.3f); p.sweep.store (0.4f); p.cutoffHz.store (9000.0f);
            p.decay.store (1.2f); p.sustain.store (0.3f); p.release.store (0.8f);
        }),
        wavePreset ("Vowel pad", [] (WaveParams& p) {
            p.table.store (2); p.position.store (0.0f); p.sweep.store (0.0f);
            p.lfoPosition.store (0.6f); p.lfoRate.store (0.15f); p.unison.store (3); p.detuneCents.store (12.0f);
            p.attack.store (0.8f); p.decay.store (2.0f); p.sustain.store (0.9f); p.release.store (2.0f);
        }),
        wavePreset ("Digital lead", [] (WaveParams& p) {
            p.table.store (3); p.position.store (0.3f); p.sweep.store (0.5f); p.unison.store (2); p.detuneCents.store (8.0f);
            p.glide.store (0.05f); p.cutoffHz.store (6000.0f); p.resonance.store (0.3f);
        }),
        wavePreset ("Morph bass", [] (WaveParams& p) {
            p.table.store (0); p.position.store (0.45f); p.sweep.store (0.4f); p.cutoffHz.store (900.0f); p.envAmount.store (0.5f);
            p.decay.store (0.3f); p.sustain.store (0.5f); p.release.store (0.15f); p.sub.store (0.6f);
        }),
        wavePreset ("Evolving", [] (WaveParams& p) {
            p.table.store (1); p.position.store (0.5f); p.sweep.store (0.0f); p.lfoPosition.store (0.8f); p.lfoRate.store (0.08f);
            p.lfoFilter.store (0.2f); p.unison.store (4); p.detuneCents.store (18.0f); p.cutoffHz.store (4000.0f);
            p.attack.store (1.5f); p.decay.store (2.0f); p.sustain.store (0.9f); p.release.store (3.0f);
        }),
        wavePreset ("Talking bass", [] (WaveParams& p) {
            p.table.store (2); p.position.store (0.0f); p.sweep.store (0.9f); p.cutoffHz.store (3000.0f); p.resonance.store (0.4f);
            p.decay.store (0.35f); p.sustain.store (0.2f); p.release.store (0.2f); p.sub.store (0.4f);
        }),
        wavePreset ("Sync stab", [] (WaveParams& p) {
            p.table.store (3); p.position.store (0.1f); p.sweep.store (0.8f); p.unison.store (2);
            p.decay.store (0.25f); p.sustain.store (0.0f); p.release.store (0.2f);
        })
    };

    static const std::vector<FactorySound> fms {
        fmPreset ("Init", [] (FmParams&) {}),
        fmPreset ("Electric piano", [] (FmParams& p) {
            p.algorithm.store (3); setOperators (p, { 1.0f, 1.0f, 1.0f, 15.0f }, { 1.0f, 0.5f, 0.5f, 0.25f });
            p.brightness.store (0.5f); p.biteDecay.store (0.8f); p.biteSustain.store (0.1f);
            p.decay.store (2.5f); p.sustain.store (0.2f); p.release.store (0.5f); p.velocityBrightness.store (0.8f); p.detuneCents.store (3.0f);
        }),
        fmPreset ("Bell", [] (FmParams& p) {
            p.algorithm.store (2); setOperators (p, { 1.0f, 3.5f, 1.0f, 11.0f }, { 1.0f, 0.6f, 0.4f, 0.3f });
            p.brightness.store (0.6f); p.biteDecay.store (2.0f); p.biteSustain.store (0.2f);
            p.decay.store (4.0f); p.sustain.store (0.0f); p.release.store (3.0f);
        }),
        fmPreset ("FM bass", [] (FmParams& p) {
            p.algorithm.store (0); setOperators (p, { 1.0f, 1.0f, 2.0f, 1.0f }, { 1.0f, 0.7f, 0.3f, 0.2f });
            p.feedback.store (0.3f); p.brightness.store (0.7f); p.biteDecay.store (0.25f); p.biteSustain.store (0.1f);
            p.decay.store (0.4f); p.sustain.store (0.6f); p.release.store (0.1f);
        }),
        fmPreset ("Glass pad", [] (FmParams& p) {
            p.algorithm.store (4); setOperators (p, { 1.0f, 2.0f, 4.0f, 3.5f }, { 1.0f, 0.5f, 0.3f, 0.3f });
            p.brightness.store (0.4f); p.biteDecay.store (1.5f); p.biteSustain.store (0.5f);
            p.attack.store (1.0f); p.decay.store (2.0f); p.sustain.store (0.9f); p.release.store (2.5f); p.detuneCents.store (8.0f);
        }),
        fmPreset ("Brass", [] (FmParams& p) {
            p.algorithm.store (6); setOperators (p, { 1.0f, 1.0f, 2.0f, 3.0f }, { 1.0f, 0.7f, 0.3f, 0.2f });
            p.attack.store (0.06f); p.biteDecay.store (0.3f); p.biteSustain.store (0.6f);
            p.sustain.store (0.8f); p.brightness.store (0.6f); p.velocityBrightness.store (0.6f);
        }),
        fmPreset ("Organ", [] (FmParams& p) {
            p.algorithm.store (7); setOperators (p, { 0.5f, 1.0f, 2.0f, 4.0f }, { 0.8f, 0.7f, 0.5f, 0.3f });
            p.attack.store (0.005f); p.decay.store (1.0f); p.sustain.store (1.0f); p.release.store (0.1f);
            p.lfoPitch.store (0.1f); p.lfoRate.store (6.0f);
        }),
        fmPreset ("Metal hit", [] (FmParams& p) {
            p.algorithm.store (0); setOperators (p, { 1.0f, 1.41f, 3.5f, 7.0f }, { 1.0f, 0.8f, 0.7f, 0.6f });
            p.feedback.store (0.5f); p.brightness.store (0.9f); p.biteDecay.store (0.4f); p.biteSustain.store (0.0f);
            p.decay.store (0.6f); p.sustain.store (0.0f); p.release.store (0.4f);
        }),
        fmPreset ("Pluck", [] (FmParams& p) {
            p.algorithm.store (1); setOperators (p, { 1.0f, 3.0f, 1.0f, 5.0f }, { 1.0f, 0.6f, 0.4f, 0.3f });
            p.brightness.store (0.7f); p.biteDecay.store (0.08f); p.biteSustain.store (0.0f);
            p.decay.store (0.5f); p.sustain.store (0.0f); p.release.store (0.3f);
        })
    };

    switch (clampType (type))
    {
        case Track::fm:      return fms;
        case Track::wavetable: return waves;
        case Track::poly:    return polys;
        case Track::drums:   return kits;
        case Track::sampler: return sampler;
        default:             return synth;
    }
}

// ================================================================ sound <-> JSON

juce::var Presets::soundToVar (const Track& tr)
{
    const int type = clampType (tr.type.load());
    if (type == Track::drums)
        return soundOf (type, "kit", kitToVar (tr.kit));
    if (type == Track::sampler)
        return soundOf (type, "sampler", samplerToVar (tr.samplerEngine.params, tr.sampleFileName));
    if (type == Track::poly)
        return soundOf (type, "poly", polyToVar (tr.polyParams));
    if (type == Track::wavetable)
        return soundOf (type, "wave", waveParamsToVar (tr.waveParams));
    if (type == Track::fm)
        return soundOf (type, "fm", fmParamsToVar (tr.fmParams));
    return soundOf (type, "synth", synthToVar (tr.synthParams));
}

void Presets::applySoundVar (Track& tr, const juce::var& v)
{
    const int type = clampType (getI (v, "type", tr.type.load()));

    if (type == Track::testSynth)
    {
        const auto s = v["synth"];
        auto& sp = tr.synthParams;
        sp.cutoffHz.store (getF (s, "cutoff", 1200.0f));   sp.resonance.store (getF (s, "resonance", 0.35f));
        sp.envAmount.store (getF (s, "envAmount", 0.6f));  sp.detuneCents.store (getF (s, "detune", 8.0f));
        sp.attack.store (getF (s, "attack", 0.003f));      sp.decay.store (getF (s, "decay", 0.25f));
        sp.sustain.store (getF (s, "sustain", 0.4f));      sp.release.store (getF (s, "release", 0.2f));
    }
    else if (type == Track::drums)
    {
        const auto k = v["kit"];
        tr.kit.choke.store (getB (k, "choke", true));
        tr.kit.velocity.store (getF (k, "velocity", 0.5f));
        if (const auto* drums = k["drums"].getArray())
        {
            for (int d = 0; d < juce::jmin (DrumKit::numDrums, drums->size()); ++d)
            {
                const auto* a = (*drums)[d].getArray();
                if (a == nullptr || a->size() < 7) continue;
                auto& dp = tr.kit.drums[(size_t) d];
                dp.pitch.store ((float) (double) (*a)[0]);  dp.length.store ((float) (double) (*a)[1]);
                dp.tone.store ((float) (double) (*a)[2]);   dp.snap.store ((float) (double) (*a)[3]);
                dp.drive.store ((float) (double) (*a)[4]);  dp.pan.store ((float) (double) (*a)[5]);
                dp.volume.store ((float) (double) (*a)[6]);
                dp.model.store (a->size() > 7 ? juce::jlimit (0, 2, juce::roundToInt ((double) (*a)[7])) : 0);
                dp.colour.store (a->size() > 8 ? (float) (double) (*a)[8] : 0.5f);
            }
        }
    }
    else if (type == Track::poly)
    {
        applyPoly (tr.polyParams, v["poly"]);
    }
    else if (type == Track::wavetable)
    {
        applyWaveParams (tr.waveParams, v["wave"]);
    }
    else if (type == Track::fm)
    {
        applyFmParams (tr.fmParams, v["fm"]);
    }
    else
    {
        const auto m = v["sampler"];
        auto& sm = tr.samplerEngine.params;
        sm.start.store (getF (m, "start", 0.0f));         sm.length.store (getF (m, "length", 1.0f));
        sm.pitch.store (getF (m, "pitch", 0.0f));         sm.brightness.store (getF (m, "brightness", 1.0f));
        sm.attack.store (getF (m, "attack", 0.002f));     sm.release.store (getF (m, "release", 0.25f));
        sm.sustain.store (getF (m, "sustain", 1.0f));     sm.loop.store (getB (m, "loop", false));
        sm.reverse.store (getB (m, "reverse", false));    sm.slices.store (getI (m, "slices", 1));
        sm.loopStart.store (getF (m, "loopStart", 0.0f));
        const auto* marks = m["sliceMarks"].getArray();
        const int n = marks != nullptr ? juce::jmin (Sampler::maxSlices, marks->size()) : 0;
        for (int i = 0; i < n; ++i)
            sm.slicePoints[(size_t) i].store ((float) (double) (*marks)[i]);
        sm.sliceCount.store (n);
    }

    tr.type.store (type);
}

// ================================================================ files

bool Presets::save (const Track& tr, const juce::File& file, juce::String& error)
{
    auto sound = soundToVar (tr);
    juce::ZipFile::Builder zip;

    if (clampType (tr.type.load()) == Track::sampler && tr.sampleFileData.getSize() > 0)
    {
        auto fileName = juce::File::createLegalFileName (tr.sampleFileName);
        if (fileName.isEmpty())
            fileName = "sample.wav";
        const auto entry = "sample/" + fileName;
        if (auto* s = sound["sampler"].getDynamicObject())
            s->setProperty ("entry", entry);
        zip.addEntry (new juce::MemoryInputStream (tr.sampleFileData, false), 0, entry, juce::Time::getCurrentTime());
    }

    // A drum kit brings the recordings on its pads.
    if (clampType (tr.type.load()) == Track::drums)
    {
        juce::Array<juce::var> padEntries, padFiles;
        for (int d = 0; d < DrumKit::numDrums; ++d)
        {
            juce::String padEntry;
            if (tr.padFileData[(size_t) d].getSize() > 0)
            {
                auto padName = juce::File::createLegalFileName (tr.padFileNames[(size_t) d]);
                if (padName.isEmpty())
                    padName = "pad.wav";
                padEntry = "pads/" + juce::String (d + 1).paddedLeft ('0', 2) + "_" + padName;
                zip.addEntry (new juce::MemoryInputStream (tr.padFileData[(size_t) d], false), 0, padEntry, juce::Time::getCurrentTime());
            }
            padEntries.add (padEntry);
            padFiles.add (tr.padFileNames[(size_t) d]);
        }
        if (auto* k = sound["kit"].getDynamicObject())
        {
            k->setProperty ("padEntries", padEntries);
            k->setProperty ("padFiles", padFiles);
        }
    }

    const auto json = juce::JSON::toString (sound);
    juce::MemoryBlock jsonData (json.toRawUTF8(), json.getNumBytesAsUTF8());
    zip.addEntry (new juce::MemoryInputStream (jsonData, true), 6, "sound.json", juce::Time::getCurrentTime());

    // Write to a temporary file first, so a failed save never destroys the old sound.
    juce::TemporaryFile temp (file);
    {
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk() || ! zip.writeToStream (out, nullptr))
        {
            error = "Could not write " + file.getFullPathName();
            return false;
        }
    }
    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = "Could not replace " + file.getFullPathName();
        return false;
    }
    return true;
}

bool Presets::load (AudioEngine& e, int trackIndex, const juce::File& file,
                    juce::AudioFormatManager& formats, juce::String& error)
{
    juce::ZipFile zip (file);
    const int jsonIndex = zip.getIndexOfFileName ("sound.json");
    if (jsonIndex < 0)
    {
        error = file.getFileName() + " is not a BizGroove sound.";
        return false;
    }

    std::unique_ptr<juce::InputStream> jsonStream (zip.createStreamForEntry (jsonIndex));
    const auto sound = juce::JSON::parse (jsonStream != nullptr ? jsonStream->readEntireStreamAsString() : juce::String());
    if (sound["app"].toString() != "BizGroove" || sound["kind"].toString() != "sound")
    {
        error = file.getFileName() + " could not be read.";
        return false;
    }

    // A sampler sound brings its recording with it.
    if (clampType (getI (sound, "type", 0)) == Track::sampler)
    {
        const auto entry = sound["sampler"]["entry"].toString();
        if (entry.isNotEmpty())
        {
            const int index = zip.getIndexOfFileName (entry);
            std::unique_ptr<juce::InputStream> stream (index >= 0 ? zip.createStreamForEntry (index) : nullptr);
            if (stream == nullptr)
            {
                error = "The recording inside " + file.getFileName() + " is missing.";
                return false;
            }
            juce::MemoryBlock data;
            stream->readIntoMemoryBlock (data);

            auto name = sound["sampler"]["file"].toString();
            if (name.isEmpty())
                name = entry.fromLastOccurrenceOf ("/", false, false);
            if (! Project::loadSampleIntoTrack (e, trackIndex, name, data, formats, error))
                return false;
        }
    }

    // A drum kit brings the recordings on its pads; a kit without any leaves the pads as they are.
    if (clampType (getI (sound, "type", 0)) == Track::drums)
    {
        const auto* padEntries = sound["kit"]["padEntries"].getArray();
        const auto* padFiles = sound["kit"]["padFiles"].getArray();
        if (padEntries != nullptr)
        {
            for (int d = 0; d < DrumKit::numDrums; ++d)
            {
                Project::clearPad (e, trackIndex, d);
                const auto padEntry = d < padEntries->size() ? (*padEntries)[d].toString() : juce::String();
                if (padEntry.isEmpty())
                    continue;
                const int index = zip.getIndexOfFileName (padEntry);
                std::unique_ptr<juce::InputStream> stream (index >= 0 ? zip.createStreamForEntry (index) : nullptr);
                juce::MemoryBlock data;
                if (stream != nullptr)
                    stream->readIntoMemoryBlock (data);
                const auto name = padFiles != nullptr && d < padFiles->size() ? (*padFiles)[d].toString()
                                                                               : padEntry.fromLastOccurrenceOf ("/", false, false);
                juce::String padError;
                if (stream == nullptr || ! Project::loadSampleIntoPad (e, trackIndex, d, name, data, formats, padError))
                    error = "Some pad recordings in " + file.getFileName() + " could not be loaded.";
            }
        }
    }

    applySoundVar (e.track (trackIndex), sound);
    return true;
}

} // namespace bizgroove
