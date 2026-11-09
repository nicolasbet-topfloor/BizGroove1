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

    int clampType (int type) { return juce::jlimit ((int) Track::testSynth, (int) Track::sampler, type); }

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
        })
    };

    static const std::vector<FactorySound> sampler {
        //              name             fade in  fade out hold  loop   reverse slices
        samplerPreset ("One-shot",       0.002f,  0.25f,   1.0f, false, false,  1),
        samplerPreset ("Loop",           0.01f,   0.30f,   1.0f, true,  false,  1),
        samplerPreset ("Reversed",       0.05f,   0.40f,   1.0f, false, true,   1),
        samplerPreset ("Chop into 8",    0.002f,  0.05f,   1.0f, false, false,  8)
    };

    switch (clampType (type))
    {
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
            }
        }
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

    applySoundVar (e.track (trackIndex), sound);
    return true;
}

} // namespace bizgroove
