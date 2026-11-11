#include "Project.h"

namespace bizgroove
{

namespace
{
    using Obj = juce::DynamicObject;

    juce::var arrayOf (std::initializer_list<juce::var> items)
    {
        juce::Array<juce::var> a;
        for (const auto& i : items) a.add (i);
        return a;
    }

    float  getF (const juce::var& v, const char* key, float fallback)  { return v.hasProperty (key) ? (float) (double) v[key] : fallback; }
    int    getI (const juce::var& v, const char* key, int fallback)    { return v.hasProperty (key) ? (int) v[key] : fallback; }
    bool   getB (const juce::var& v, const char* key, bool fallback)   { return v.hasProperty (key) ? (bool) v[key] : fallback; }

    juce::String safeName (const juce::String& s)
    {
        return juce::File::createLegalFileName (s).replaceCharacter (' ', '_');
    }
}

// ============================================================== settings <-> JSON

juce::var Project::toVar (AudioEngine& e)
{
    auto* root = new Obj();
    root->setProperty ("app", "BizGroove");
    root->setProperty ("formatVersion", formatVersion);
    root->setProperty ("tempo", e.transport.bpm.load());
    root->setProperty ("masterVolume", e.masterGain.load());
    root->setProperty ("arp", arpToVar (e.arp));

    auto* fx = new Obj();
    fx->setProperty ("reverbSize", e.effects.reverbSize.load());
    fx->setProperty ("delayDivision", e.effects.delayDivision.load());
    fx->setProperty ("delayFeedback", e.effects.delayFeedback.load());
    fx->setProperty ("reverbType", e.effects.reverbType.load());
    fx->setProperty ("reverbTone", e.effects.reverbTone.load());
    fx->setProperty ("springDrip", e.effects.springDrip.load());
    fx->setProperty ("delayType", e.effects.delayType.load());
    fx->setProperty ("tapeHeads", e.effects.tapeHeads.load());
    fx->setProperty ("tapeWow", e.effects.tapeWow.load());
    fx->setProperty ("tapeTone", e.effects.tapeTone.load());
    fx->setProperty ("tapeWear", e.effects.tapeWear.load());
    root->setProperty ("effects", juce::var (fx));

    // ---- tracks
    juce::Array<juce::var> tracks;
    for (int t = 0; t < AudioEngine::numTracks; ++t)
    {
        auto& tr = e.track (t);
        auto* o = new Obj();
        o->setProperty ("type", tr.type.load());
        o->setProperty ("volume", tr.volume.load());
        o->setProperty ("mute", tr.mute.load());
        o->setProperty ("solo", tr.mix.solo.load());
        o->setProperty ("pan", tr.mix.pan.load());
        o->setProperty ("eqLow", tr.mix.eqLow.load());
        o->setProperty ("eqMid", tr.mix.eqMid.load());
        o->setProperty ("eqHigh", tr.mix.eqHigh.load());
        o->setProperty ("reverb", tr.mix.reverb.load());
        o->setProperty ("delay", tr.mix.delay.load());
        o->setProperty ("swing", e.song.swing[(size_t) t].load());

        const auto& sp = tr.synthParams;
        auto* synth = new Obj();
        synth->setProperty ("cutoff", sp.cutoffHz.load());     synth->setProperty ("resonance", sp.resonance.load());
        synth->setProperty ("envAmount", sp.envAmount.load()); synth->setProperty ("detune", sp.detuneCents.load());
        synth->setProperty ("attack", sp.attack.load());       synth->setProperty ("decay", sp.decay.load());
        synth->setProperty ("sustain", sp.sustain.load());     synth->setProperty ("release", sp.release.load());
        synth->setProperty ("glide", sp.glide.load());         synth->setProperty ("glideMode", sp.glideMode.load());
        synth->setProperty ("osc", oscToVar (sp.osc));
        o->setProperty ("synth", juce::var (synth));

        const auto& pp = tr.polyParams;
        auto* poly = new Obj();
        poly->setProperty ("cutoff", pp.cutoffHz.load());    poly->setProperty ("resonance", pp.resonance.load());
        poly->setProperty ("shape", pp.shape.load());        poly->setProperty ("envAmount", pp.envAmount.load());
        poly->setProperty ("attack", pp.attack.load());      poly->setProperty ("decay", pp.decay.load());
        poly->setProperty ("sustain", pp.sustain.load());    poly->setProperty ("release", pp.release.load());
        poly->setProperty ("detune", pp.detuneCents.load()); poly->setProperty ("pulseWidth", pp.pulseWidth.load());
        poly->setProperty ("mix", pp.mix.load());            poly->setProperty ("octave", pp.octave.load());
        poly->setProperty ("unison", pp.unison.load());      poly->setProperty ("glide", pp.glide.load());
        poly->setProperty ("lfoRate", pp.lfoRate.load());    poly->setProperty ("lfoFilter", pp.lfoFilter.load());
        poly->setProperty ("lfoPulse", pp.lfoPulse.load());  poly->setProperty ("noise", pp.noise.load());
        poly->setProperty ("glideMode", pp.glideMode.load());
        poly->setProperty ("osc", oscToVar (pp.osc));
        o->setProperty ("poly", juce::var (poly));
        o->setProperty ("wave", waveParamsToVar (tr.waveParams));
        o->setProperty ("fm", fmParamsToVar (tr.fmParams));
        o->setProperty ("noise", noiseParamsToVar (tr.noiseParams));
        o->setProperty ("voice", voiceParamsToVar (tr.voiceParams));
        o->setProperty ("vocoder", vocoderParamsToVar (tr.vocoderParams));
        o->setProperty ("external", externalParamsToVar (tr.ext, tr.externalDeviceName));
        o->setProperty ("inserts", insertsToVar (tr.inserts));
        o->setProperty ("lfos", lfosToVar (tr.lfos));

        auto* kit = new Obj();
        kit->setProperty ("choke", tr.kit.choke.load());
        kit->setProperty ("velocity", tr.kit.velocity.load());
        juce::Array<juce::var> drums;
        for (const auto& d : tr.kit.drums)
            drums.add (arrayOf ({ d.pitch.load(), d.length.load(), d.tone.load(), d.snap.load(),
                                  d.drive.load(), d.pan.load(), d.volume.load(),
                                  (float) d.model.load(), d.colour.load() }));
        kit->setProperty ("drums", drums);
        juce::Array<juce::var> padFiles;
        for (const auto& name : tr.padFileNames)
            padFiles.add (name);
        kit->setProperty ("padFiles", padFiles);
        o->setProperty ("kit", juce::var (kit));

        const auto& sm = tr.samplerEngine.params;
        auto* smp = new Obj();
        smp->setProperty ("start", sm.start.load());           smp->setProperty ("length", sm.length.load());
        smp->setProperty ("pitch", sm.pitch.load());           smp->setProperty ("brightness", sm.brightness.load());
        smp->setProperty ("attack", sm.attack.load());         smp->setProperty ("release", sm.release.load());
        smp->setProperty ("sustain", sm.sustain.load());       smp->setProperty ("loop", sm.loop.load());
        smp->setProperty ("reverse", sm.reverse.load());       smp->setProperty ("slices", sm.slices.load());
        smp->setProperty ("file", tr.sampleFileName);
        smp->setProperty ("loopStart", sm.loopStart.load());
        juce::Array<juce::var> marks;
        for (int i = 0; i < juce::jlimit (0, Sampler::maxSlices, sm.sliceCount.load()); ++i)
            marks.add (sm.slicePoints[(size_t) i].load());
        smp->setProperty ("sliceMarks", marks);
        o->setProperty ("sampler", juce::var (smp));

        tracks.add (juce::var (o));
    }
    root->setProperty ("tracks", tracks);

    // ---- scenes: per track, a 16-bit on-mask, 16 notes and 16 drum lanes
    juce::Array<juce::var> scenes;
    for (int s = 0; s < Song::numScenes; ++s)
    {
        juce::Array<juce::var> perTrack;
        for (int t = 0; t < Song::numTracks; ++t)
        {
            const auto& p = e.song.scene (s).tracks[(size_t) t];
            int mask = 0;
            bool extras = false;
            juce::Array<juce::var> notes, lanes, chords, lengths, velocities;
            for (int i = 0; i < Song::numSteps; ++i)
            {
                const auto& st = p.steps[(size_t) i];
                if (st.on.load()) mask |= (1 << i);
                notes.add (st.note.load());
                juce::Array<juce::var> chord;
                for (const auto& c : st.chord)
                    if (const int v = c.load(); v > 0)
                        chord.add (v - 1);
                chords.add (chord);
                lengths.add (st.length.load());
                velocities.add (st.velocity.load());
                extras = extras || ! chord.isEmpty() || st.length.load() > 0.0f || st.velocity.load() != 100;
            }
            for (int d = 0; d < Song::numDrums; ++d)
                lanes.add ((int) p.drumLanes[(size_t) d].load());
            auto* o = new Obj();
            o->setProperty ("on", mask);
            o->setProperty ("notes", notes);
            o->setProperty ("drums", lanes);
            if (extras)   // chords, lengths and how hard: only when some step has them
            {
                o->setProperty ("chords", chords);
                o->setProperty ("lengths", lengths);
                o->setProperty ("velocities", velocities);
            }
            perTrack.add (juce::var (o));
        }
        scenes.add (perTrack);
    }
    root->setProperty ("scenes", scenes);
    root->setProperty ("currentScene", e.song.currentScene.load());
    root->setProperty ("selectedTrack", e.selectedTrack.load());

    // ---- song
    auto* song = new Obj();
    song->setProperty ("songMode", e.song.songMode.load());
    song->setProperty ("loop", e.song.loopSong.load());
    song->setProperty ("useTimeline", e.song.useTimeline.load());
    juce::Array<juce::var> chain;
    for (int i = 0; i < juce::jlimit (1, Song::maxChain, e.song.chainLength.load()); ++i)
        chain.add (arrayOf ({ e.song.chain[(size_t) i].scene.load(), e.song.chain[(size_t) i].repeats.load() }));
    song->setProperty ("chain", chain);
    const int tlLength = juce::jlimit (1, Song::maxBars, e.song.timelineLength.load());
    song->setProperty ("timelineLength", tlLength);
    juce::Array<juce::var> timeline;
    for (int t = 0; t < Song::numTracks; ++t)
    {
        juce::Array<juce::var> row;
        for (int b = 0; b < tlLength; ++b)
            row.add (e.song.timeline[(size_t) t][(size_t) b].load());
        timeline.add (row);
    }
    song->setProperty ("timeline", timeline);
    root->setProperty ("song", juce::var (song));

    // ---- stem deck
    juce::Array<juce::var> stems;
    for (int i = 0; i < StemDeck::numStems; ++i)
    {
        auto& st = e.stems.stem (i);
        auto* o = new Obj();
        o->setProperty ("volume", st.volume.load());
        o->setProperty ("filter", st.filter.load());
        o->setProperty ("mute", st.mute.load());
        o->setProperty ("solo", st.solo.load());
        o->setProperty ("file", e.stemFiles[(size_t) i].name);
        stems.add (juce::var (o));
    }
    root->setProperty ("stems", stems);

    return juce::var (root);
}

void Project::applyVar (AudioEngine& e, const juce::var& v)
{
    e.transport.playing.store (false);
    e.transport.bpm.store (juce::jlimit (40.0, 240.0, (double) getF (v, "tempo", 120.0f)));
    e.masterGain.store (getF (v, "masterVolume", 0.8f));
    applyArp (e.arp, v["arp"]);

    const auto fx = v["effects"];
    e.effects.reverbSize.store (getF (fx, "reverbSize", 0.5f));
    e.effects.delayDivision.store (getI (fx, "delayDivision", MasterEffects::dottedEighth));
    e.effects.delayFeedback.store (getF (fx, "delayFeedback", 0.35f));
    e.effects.reverbType.store (juce::jlimit (0, MasterEffects::numReverbTypes - 1, getI (fx, "reverbType", 0)));
    e.effects.reverbTone.store (getF (fx, "reverbTone", 0.5f));
    e.effects.springDrip.store (getF (fx, "springDrip", 0.5f));
    e.effects.delayType.store (juce::jlimit (0, MasterEffects::numDelayTypes - 1, getI (fx, "delayType", 0)));
    e.effects.tapeHeads.store (juce::jlimit (0, TapeEcho::numModes - 1, getI (fx, "tapeHeads", 6)));
    e.effects.tapeWow.store (getF (fx, "tapeWow", 0.4f));
    e.effects.tapeTone.store (getF (fx, "tapeTone", 0.5f));
    e.effects.tapeWear.store (getF (fx, "tapeWear", 0.3f));

    if (const auto* tracks = v["tracks"].getArray())
    {
        for (int t = 0; t < juce::jmin (AudioEngine::numTracks, tracks->size()); ++t)
        {
            const auto o = (*tracks)[t];
            auto& tr = e.track (t);
            tr.type.store (juce::jlimit (0, Track::numTypes - 1, getI (o, "type", 0)));
            tr.volume.store (getF (o, "volume", 0.8f));
            tr.mute.store (getB (o, "mute", false));
            tr.mix.solo.store (getB (o, "solo", false));
            tr.mix.pan.store (getF (o, "pan", 0.0f));
            tr.mix.eqLow.store (getF (o, "eqLow", 0.0f));
            tr.mix.eqMid.store (getF (o, "eqMid", 0.0f));
            tr.mix.eqHigh.store (getF (o, "eqHigh", 0.0f));
            tr.mix.reverb.store (getF (o, "reverb", 0.0f));
            tr.mix.delay.store (getF (o, "delay", 0.0f));
            e.song.swing[(size_t) t].store (getF (o, "swing", 0.0f));

            const auto s = o["synth"];
            auto& sp = tr.synthParams;
            sp.cutoffHz.store (getF (s, "cutoff", 1200.0f));   sp.resonance.store (getF (s, "resonance", 0.35f));
            sp.envAmount.store (getF (s, "envAmount", 0.6f));  sp.detuneCents.store (getF (s, "detune", 8.0f));
            sp.attack.store (getF (s, "attack", 0.003f));      sp.decay.store (getF (s, "decay", 0.25f));
            sp.sustain.store (getF (s, "sustain", 0.4f));      sp.release.store (getF (s, "release", 0.2f));
            sp.glide.store (getF (s, "glide", 0.0f));
            sp.glideMode.store (juce::jlimit (0, Glide::numModes - 1, getI (s, "glideMode", 0)));
            applyOsc (sp.osc, s["osc"]);

            const auto pv = o["poly"];
            auto& pp = tr.polyParams;
            pp.cutoffHz.store (getF (pv, "cutoff", 1800.0f));  pp.resonance.store (getF (pv, "resonance", 0.25f));
            pp.shape.store (getF (pv, "shape", 0.0f));         pp.envAmount.store (getF (pv, "envAmount", 0.45f));
            pp.attack.store (getF (pv, "attack", 0.005f));     pp.decay.store (getF (pv, "decay", 0.45f));
            pp.sustain.store (getF (pv, "sustain", 0.7f));     pp.release.store (getF (pv, "release", 0.35f));
            pp.detuneCents.store (getF (pv, "detune", 7.0f));  pp.pulseWidth.store (getF (pv, "pulseWidth", 0.5f));
            pp.mix.store (getF (pv, "mix", 0.5f));             pp.octave.store (juce::jlimit (-1, 1, getI (pv, "octave", 0)));
            pp.unison.store (juce::jlimit (1, 4, getI (pv, "unison", 1)));
            pp.glide.store (getF (pv, "glide", 0.0f));
            pp.glideMode.store (juce::jlimit (0, Glide::numModes - 1, getI (pv, "glideMode", 0)));
            applyOsc (pp.osc, pv["osc"]);
            pp.lfoRate.store (getF (pv, "lfoRate", 5.0f));     pp.lfoFilter.store (getF (pv, "lfoFilter", 0.0f));
            pp.lfoPulse.store (getF (pv, "lfoPulse", 0.0f));   pp.noise.store (getF (pv, "noise", 0.0f));

            applyWaveParams (tr.waveParams, o["wave"]);
            applyFmParams (tr.fmParams, o["fm"]);
            applyNoiseParams (tr.noiseParams, o["noise"]);
            applyVoiceParams (tr.voiceParams, o["voice"]);
            applyVocoderParams (tr.vocoderParams, o["vocoder"]);
            applyExternalParams (tr.ext, tr.externalDeviceName, o["external"]);
            tr.externalDeviceId = {};   // found again by name on this computer
            applyInserts (tr.inserts, o["inserts"]);
            applyLfos (tr.lfos, o["lfos"]);

            const auto k = o["kit"];
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

            const auto m = o["sampler"];
            auto& sm = tr.samplerEngine.params;
            sm.start.store (getF (m, "start", 0.0f));         sm.length.store (getF (m, "length", 1.0f));
            sm.pitch.store (getF (m, "pitch", 0.0f));         sm.brightness.store (getF (m, "brightness", 1.0f));
            sm.attack.store (getF (m, "attack", 0.002f));     sm.release.store (getF (m, "release", 0.25f));
            sm.sustain.store (getF (m, "sustain", 1.0f));     sm.loop.store (getB (m, "loop", false));
            sm.reverse.store (getB (m, "reverse", false));    sm.slices.store (getI (m, "slices", 1));
            sm.loopStart.store (getF (m, "loopStart", 0.0f));
            {
                const auto* marks = m["sliceMarks"].getArray();
                const int n = marks != nullptr ? juce::jmin (Sampler::maxSlices, marks->size()) : 0;
                for (int i = 0; i < n; ++i)
                    sm.slicePoints[(size_t) i].store ((float) (double) (*marks)[i]);
                sm.sliceCount.store (n);
            }
        }
    }

    if (const auto* scenes = v["scenes"].getArray())
    {
        for (int s = 0; s < juce::jmin (Song::numScenes, scenes->size()); ++s)
        {
            const auto* perTrack = (*scenes)[s].getArray();
            if (perTrack == nullptr) continue;
            for (int t = 0; t < juce::jmin (Song::numTracks, perTrack->size()); ++t)
            {
                const auto o = (*perTrack)[t];
                auto& p = e.song.scene (s).tracks[(size_t) t];
                const int mask = getI (o, "on", 0);
                const auto* notes = o["notes"].getArray();
                const auto* lanes = o["drums"].getArray();
                const auto* chords = o["chords"].getArray();
                const auto* lengths = o["lengths"].getArray();
                const auto* velocities = o["velocities"].getArray();
                for (int i = 0; i < Song::numSteps; ++i)
                {
                    auto& st = p.steps[(size_t) i];
                    st.setSingle (notes != nullptr && i < notes->size() ? (int) (*notes)[i] : 48,
                                  velocities != nullptr && i < velocities->size() ? (int) (*velocities)[i] : 100);
                    if (chords != nullptr && i < chords->size())
                        if (const auto* chord = (*chords)[i].getArray())
                            for (const auto& n : *chord)
                                st.addNote ((int) n);
                    if (lengths != nullptr && i < lengths->size())
                        st.length.store (juce::jlimit (0.0f, 16.0f, (float) (double) (*lengths)[i]));
                    st.on.store ((mask >> i) & 1);
                }
                for (int d = 0; d < Song::numDrums; ++d)
                    p.drumLanes[(size_t) d].store ((std::uint16_t) (lanes != nullptr && d < lanes->size() ? (int) (*lanes)[d] : 0));
            }
        }
    }
    e.song.currentScene.store (juce::jlimit (0, Song::numScenes - 1, getI (v, "currentScene", 0)));
    e.song.queuedScene.store (-1);
    e.selectedTrack.store (juce::jlimit (0, Song::numTracks - 1, getI (v, "selectedTrack", 0)));

    const auto song = v["song"];
    e.song.songMode.store (getB (song, "songMode", false));
    e.song.loopSong.store (getB (song, "loop", true));
    e.song.useTimeline.store (getB (song, "useTimeline", false));
    if (const auto* chain = song["chain"].getArray())
    {
        const int n = juce::jlimit (1, Song::maxChain, chain->size());
        for (int i = 0; i < n; ++i)
        {
            const auto* a = (*chain)[i].getArray();
            e.song.chain[(size_t) i].scene.store (a != nullptr && a->size() > 0 ? juce::jlimit (0, Song::numScenes - 1, (int) (*a)[0]) : 0);
            e.song.chain[(size_t) i].repeats.store (a != nullptr && a->size() > 1 ? juce::jlimit (1, 16, (int) (*a)[1]) : 1);
        }
        e.song.chainLength.store (n);
    }
    e.song.clearTimeline();
    e.song.timelineLength.store (juce::jlimit (1, Song::maxBars, getI (song, "timelineLength", 8)));
    if (const auto* timeline = song["timeline"].getArray())
        for (int t = 0; t < juce::jmin (Song::numTracks, timeline->size()); ++t)
            if (const auto* row = (*timeline)[t].getArray())
                for (int b = 0; b < juce::jmin (Song::maxBars, row->size()); ++b)
                    e.song.timeline[(size_t) t][(size_t) b].store (juce::jlimit (-1, Song::numScenes - 1, (int) (*row)[b]));

    if (const auto* stems = v["stems"].getArray())
    {
        for (int i = 0; i < juce::jmin (StemDeck::numStems, stems->size()); ++i)
        {
            const auto o = (*stems)[i];
            auto& st = e.stems.stem (i);
            st.volume.store (getF (o, "volume", 0.8f));
            st.filter.store (getF (o, "filter", 0.0f));
            st.mute.store (getB (o, "mute", false));
            st.solo.store (getB (o, "solo", false));
        }
    }
}

juce::String Project::fingerprint (AudioEngine& e)
{
    auto v = toVar (e);
    if (auto* o = v.getDynamicObject())
    {
        o->removeProperty ("currentScene");     // moving around is not a change worth saving
        o->removeProperty ("selectedTrack");
    }
    return juce::JSON::toString (v, true);
}

// ============================================================== audio files

bool Project::decodeAudio (const juce::MemoryBlock& data, juce::AudioFormatManager& formats,
                           juce::AudioBuffer<float>& out, double& sampleRate, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (
        formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, false)));
    if (reader == nullptr)
    {
        error = "This file is not an audio format BizGroove can read (use WAV, AIFF, FLAC or OGG).";
        return false;
    }
    if (reader->lengthInSamples < 64 || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 15 * 60))
    {
        error = "The recording must be between a moment and 15 minutes long.";
        return false;
    }
    out.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), (int) reader->lengthInSamples);
    reader->read (&out, 0, out.getNumSamples(), 0, true, out.getNumChannels() > 1);
    sampleRate = reader->sampleRate;
    return true;
}

bool Project::loadSampleIntoTrack (AudioEngine& e, int track, const juce::String& name, const juce::MemoryBlock& data,
                                   juce::AudioFormatManager& formats, juce::String& error)
{
    juce::AudioBuffer<float> audio;
    double sr = 44100.0;
    if (! decodeAudio (data, formats, audio, sr, error))
        return false;
    auto& tr = e.track (track);
    tr.samplerEngine.setSample (audio, sr, juce::File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension());
    tr.sampleFileName = name;
    tr.sampleFileData = data;
    return true;
}

bool Project::loadSampleIntoPad (AudioEngine& e, int track, int drum, const juce::String& name, const juce::MemoryBlock& data,
                                 juce::AudioFormatManager& formats, juce::String& error)
{
    if (! juce::isPositiveAndBelow (drum, DrumKit::numDrums))
    {
        error = "There is no pad " + juce::String (drum + 1) + ".";
        return false;
    }
    juce::AudioBuffer<float> audio;
    double sr = 44100.0;
    if (! decodeAudio (data, formats, audio, sr, error))
        return false;
    auto& tr = e.track (track);
    tr.kit.setPadSample (drum, audio, sr, juce::File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension());
    tr.padFileNames[(size_t) drum] = name;
    tr.padFileData[(size_t) drum] = data;
    return true;
}

void Project::clearPad (AudioEngine& e, int track, int drum)
{
    if (! juce::isPositiveAndBelow (drum, DrumKit::numDrums))
        return;
    auto& tr = e.track (track);
    tr.kit.clearPadSample (drum);
    tr.padFileNames[(size_t) drum] = {};
    tr.padFileData[(size_t) drum].reset();
}

bool Project::loadStem (AudioEngine& e, int slot, const juce::String& name, const juce::MemoryBlock& data,
                        juce::AudioFormatManager& formats, juce::String& error)
{
    juce::AudioBuffer<float> audio;
    double sr = 44100.0;
    if (! decodeAudio (data, formats, audio, sr, error))
        return false;
    e.stems.setStemAudio (slot, audio, sr, juce::File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension(),
                          e.transport.bpm.load());
    e.stemFiles[(size_t) slot] = { name, data };
    return true;
}

// ============================================================== save / open

bool Project::save (AudioEngine& e, const juce::File& file, juce::String& error)
{
    auto state = toVar (e);
    juce::ZipFile::Builder zip;

    // Record the archive name of each sample in the JSON.
    if (auto* tracks = state["tracks"].getArray())
    {
        for (int t = 0; t < tracks->size(); ++t)
        {
            auto& tr = e.track (t);
            auto sampler = (*tracks)[t]["sampler"];
            if (tr.sampleFileData.getSize() > 0)
            {
                const auto entry = "samples/track" + juce::String (t + 1).paddedLeft ('0', 2) + "_" + safeName (tr.sampleFileName);
                sampler.getDynamicObject()->setProperty ("entry", entry);
                zip.addEntry (new juce::MemoryInputStream (tr.sampleFileData, false), 0, entry, juce::Time::getCurrentTime());
            }

            // The recordings on the drum pads.
            juce::Array<juce::var> padEntries;
            for (int d = 0; d < DrumKit::numDrums; ++d)
            {
                juce::String padEntry;
                if (tr.padFileData[(size_t) d].getSize() > 0)
                {
                    padEntry = "samples/track" + juce::String (t + 1).paddedLeft ('0', 2) + "_pad" + juce::String (d + 1).paddedLeft ('0', 2)
                               + "_" + safeName (tr.padFileNames[(size_t) d]);
                    zip.addEntry (new juce::MemoryInputStream (tr.padFileData[(size_t) d], false), 0, padEntry, juce::Time::getCurrentTime());
                }
                padEntries.add (padEntry);
            }
            if (auto* kitObject = (*tracks)[t]["kit"].getDynamicObject())
                kitObject->setProperty ("padEntries", padEntries);
        }
    }
    if (auto* stems = state["stems"].getArray())
    {
        for (int i = 0; i < stems->size(); ++i)
        {
            const auto& f = e.stemFiles[(size_t) i];
            if (f.data.getSize() > 0)
            {
                const auto entry = "samples/stem" + juce::String (i + 1) + "_" + safeName (f.name);
                (*stems)[i].getDynamicObject()->setProperty ("entry", entry);
                zip.addEntry (new juce::MemoryInputStream (f.data, false), 0, entry, juce::Time::getCurrentTime());
            }
        }
    }

    const auto json = juce::JSON::toString (state);
    juce::MemoryBlock jsonData (json.toRawUTF8(), json.getNumBytesAsUTF8());
    zip.addEntry (new juce::MemoryInputStream (jsonData, true), 6, "project.json", juce::Time::getCurrentTime());

    // Write to a temporary file first, so a failed save never destroys the old project.
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

bool Project::open (AudioEngine& e, const juce::File& file, juce::AudioFormatManager& formats, juce::String& error)
{
    juce::ZipFile zip (file);
    const int jsonIndex = zip.getIndexOfFileName ("project.json");
    if (jsonIndex < 0)
    {
        error = file.getFileName() + " is not a BizGroove project.";
        return false;
    }

    std::unique_ptr<juce::InputStream> jsonStream (zip.createStreamForEntry (jsonIndex));
    const auto state = juce::JSON::parse (jsonStream != nullptr ? jsonStream->readEntireStreamAsString() : juce::String());
    if (state["app"].toString() != "BizGroove")
    {
        error = file.getFileName() + " could not be read.";
        return false;
    }

    applyVar (e, state);

    auto readEntry = [&zip] (const juce::String& entry, juce::MemoryBlock& out) {
        const int idx = zip.getIndexOfFileName (entry);
        if (idx < 0) return false;
        std::unique_ptr<juce::InputStream> s (zip.createStreamForEntry (idx));
        if (s == nullptr) return false;
        s->readIntoMemoryBlock (out);
        return true;
    };

    juce::StringArray problems;
    if (const auto* tracks = state["tracks"].getArray())
    {
        for (int t = 0; t < juce::jmin (AudioEngine::numTracks, tracks->size()); ++t)
        {
            auto& tr = e.track (t);
            tr.samplerEngine.clearSample();
            tr.sampleFileName = {};
            tr.sampleFileData.reset();
            const auto sampler = (*tracks)[t]["sampler"];
            const auto entry = sampler["entry"].toString();
            juce::MemoryBlock data;
            juce::String err;
            if (entry.isNotEmpty() && (! readEntry (entry, data)
                                       || ! loadSampleIntoTrack (e, t, sampler["file"].toString(), data, formats, err)))
                problems.add ("track " + juce::String (t + 1) + ": " + sampler["file"].toString());

            const auto kit = (*tracks)[t]["kit"];
            const auto* padEntries = kit["padEntries"].getArray();
            const auto* padFiles = kit["padFiles"].getArray();
            for (int d = 0; d < DrumKit::numDrums; ++d)
            {
                clearPad (e, t, d);
                const auto padEntry = padEntries != nullptr && d < padEntries->size() ? (*padEntries)[d].toString() : juce::String();
                const auto padFile = padFiles != nullptr && d < padFiles->size() ? (*padFiles)[d].toString() : juce::String();
                juce::MemoryBlock padData;
                juce::String padError;
                if (padEntry.isNotEmpty() && (! readEntry (padEntry, padData)
                                              || ! loadSampleIntoPad (e, t, d, padFile, padData, formats, padError)))
                    problems.add ("track " + juce::String (t + 1) + " pad " + juce::String (d + 1) + ": " + padFile);
            }
        }
    }
    if (const auto* stems = state["stems"].getArray())
    {
        for (int i = 0; i < juce::jmin (StemDeck::numStems, stems->size()); ++i)
        {
            e.stems.clearStem (i);
            e.stemFiles[(size_t) i] = {};
            const auto o = (*stems)[i];
            const auto entry = o["entry"].toString();
            juce::MemoryBlock data;
            juce::String err;
            if (entry.isNotEmpty() && (! readEntry (entry, data)
                                       || ! loadStem (e, i, o["file"].toString(), data, formats, err)))
                problems.add ("stem " + juce::String (i + 1) + ": " + o["file"].toString());
        }
    }

    if (! problems.isEmpty())
        error = "Opened, but these recordings could not be loaded: " + problems.joinIntoString (", ");
    return true;
}

// ============================================================== export

bool Exporter::writeWav (const juce::AudioBuffer<float>& audio, double sampleRate, int bitDepth,
                         const juce::File& file, juce::String& error)
{
    file.deleteFile();
    auto stream = file.createOutputStream();
    if (stream == nullptr)
    {
        error = "Could not create " + file.getFullPathName();
        return false;
    }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, 2,
                                                                          bitDepth, {}, 0));
    if (writer == nullptr)
    {
        error = "Could not write a " + juce::String (bitDepth) + "-bit WAV file.";
        return false;
    }
    stream.release();   // the writer owns it now
    if (! writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
    {
        error = "Writing " + file.getFileName() + " failed.";
        return false;
    }
    return true;
}

bool Exporter::writeMidi (AudioEngine& e, bool playSong, int bars, const juce::File& file, juce::String& error)
{
    constexpr int ppq = 960;
    constexpr int ticksPerStep = ppq / 4;
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ppq);

    juce::MidiMessageSequence tempoTrack;
    tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / e.transport.bpm.load())), 0);
    tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    tempoTrack.addEvent (juce::MidiMessage::textMetaEvent (3, "BizGroove"), 0);
    midi.addTrack (tempoTrack);

    for (int t = 0; t < Song::numTracks; ++t)
    {
        const bool isDrum = e.track (t).playsDrums();
        const int channel = isDrum ? 10 : 1;
        const double swing = e.song.swing[(size_t) t].load();
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Track " + juce::String (t + 1)), 0);

        for (int bar = 0; bar < bars; ++bar)
        {
            const int scene = e.song.sceneAt (t, bar, playSong);
            if (scene < 0) continue;
            const auto& p = e.song.scene (scene).tracks[(size_t) t];
            for (int st = 0; st < Song::numSteps; ++st)
            {
                const double push = (st % 2 == 1) ? swing * 0.5 * ticksPerStep : 0.0;
                const double tick = (bar * Song::numSteps + st) * ticksPerStep + push;
                if (isDrum)
                {
                    for (int d = 0; d < Song::numDrums; ++d)
                    {
                        if ((p.drumLanes[(size_t) d].load() >> st) & 1u)
                        {
                            seq.addEvent (juce::MidiMessage::noteOn (channel, Song::drumBaseNote + d, (juce::uint8) 100), tick);
                            seq.addEvent (juce::MidiMessage::noteOff (channel, Song::drumBaseNote + d), tick + ticksPerStep / 2);
                        }
                    }
                }
                else if (p.steps[(size_t) st].on.load())
                {
                    const auto& step = p.steps[(size_t) st];
                    std::array<int, Song::maxChord> stepNotes {};
                    const int count = step.getNotes (stepNotes);
                    const float length = step.length.load();
                    const double ticks = length > 0.0f ? length * ticksPerStep : ticksPerStep / 2.0;
                    for (int i = 0; i < count; ++i)
                    {
                        seq.addEvent (juce::MidiMessage::noteOn (channel, stepNotes[(size_t) i], (juce::uint8) juce::jlimit (1, 127, step.velocity.load())), tick);
                        seq.addEvent (juce::MidiMessage::noteOff (channel, stepNotes[(size_t) i]), tick + ticks);
                    }
                }
            }
        }

        if (seq.getNumEvents() > 1)
        {
            seq.updateMatchedPairs();
            midi.addTrack (seq);
        }
    }

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! midi.writeTo (out, 1))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    return true;
}

} // namespace bizgroove
