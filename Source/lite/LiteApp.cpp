// The light apps (BizHop, BizWave, BizTechno, BizNoise, BizSampler), made for phones and tablets:
// one track at a time, played by dragging a finger on a big pad, in BizGroove's colours.
// Which app this is comes from BIZ_LITE_STYLE, set by CMake.

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "engine/Project.h"
#include "ui/LookAndFeel.h"
#include "Generator.h"
#include "ui/GrainEditor.h"

#ifndef BIZ_LITE_STYLE
 #define BIZ_LITE_STYLE 0
#endif

namespace bizgroove::lite
{

namespace
{
    const juce::Colour glass { 0xff1f2127 };   // the panels (BizGroove's)

    juce::String noteName (int midi)
    {
        return juce::MidiMessage::getMidiNoteName (midi, true, true, 3);
    }

    /** The lanes a beats row shows: each lane is a few drums, the first is the one a click adds. */
    struct Lane { const char* name; std::vector<int> drums; };
    const std::array<Lane, 4>& lanes()
    {
        static const std::array<Lane, 4> l { {
            { "Kick",  { kick } },
            { "Snare", { snare, clap, sideStick, snare2 } },
            { "Hats",  { closedHat, openHat, ride, 8 } },
            { "Perc",  { lowTom, highTom, crash, 5, 7, 11, 12 } } } };
        return l;
    }
}

// ============================================================================ the steps of one track, one bar
class StepRow final : public juce::Component
{
public:
    StepRow (AudioEngine& e, const Style& s, int t) : engine (e), style (s), track (t), role (s.tracks[(size_t) t].role) {}

    std::function<void()> onEdit;
    int bar = 0;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (glass);
        g.fillRoundedRectangle (r, 6.0f);
        const auto& p = pattern();
        const float w = r.getWidth() / 16.0f;
        const int playStep = playingStep();

        for (int s = 0; s < 16; ++s)
        {
            auto cell = juce::Rectangle<float> (r.getX() + (float) s * w, r.getY(), w, r.getHeight()).reduced (2.0f, 4.0f);
            g.setColour (s % 4 == 0 ? juce::Colour (0x30ffffff) : juce::Colour (0x18ffffff));
            g.fillRoundedRectangle (cell, 3.0f);
            if (s == playStep)
            {
                g.setColour (Colours::playhead.withAlpha (0.35f));
                g.fillRoundedRectangle (cell, 3.0f);
            }

            if (role == Role::beats)
            {
                const float lh = cell.getHeight() / 4.0f;
                for (int l = 0; l < 4; ++l)
                {
                    bool on = false;
                    for (int d : lanes()[(size_t) l].drums)
                        on = on || (p.drumLanes[(size_t) d].load() >> s & 1u) != 0;
                    if (on)
                    {
                        g.setColour (laneColour (l));
                        g.fillRoundedRectangle (cell.getX() + 2.0f, cell.getY() + lh * (float) l + 1.0f, cell.getWidth() - 4.0f, lh - 2.0f, 2.0f);
                    }
                }
                continue;
            }

            const auto& st = p.steps[(size_t) s];
            if (! st.on.load())
                continue;
            const float len = juce::jmax (1.0f, st.length.load());
            auto lit = cell.withWidth (juce::jmin (w * len - 4.0f, r.getRight() - cell.getX() - 2.0f));
            g.setColour (Colours::accent.withAlpha (role == Role::rap ? 0.55f : 0.85f));
            g.fillRoundedRectangle (lit, 3.0f);
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (role == Role::rap ? wordAt (s) : label (st.note.load()), lit.reduced (3.0f, 2.0f),
                        juce::Justification::topLeft, false);
        }
        if (role == Role::beats)
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.setFont (juce::FontOptions (10.0f));
            const float lh = (r.getHeight() - 8.0f) / 4.0f;
            for (int l = 0; l < 4; ++l)
                g.drawText (lanes()[(size_t) l].name, juce::Rectangle<float> (r.getX() + 4.0f, r.getY() + 4.0f + lh * (float) l, 40.0f, lh),
                            juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int s = stepAt (e.position.x);
        dragStartY = e.position.y;
        if (role == Role::beats)
        {
            const int l = juce::jlimit (0, 3, (int) ((e.position.y - 4.0f) / ((float) getHeight() - 8.0f) * 4.0f));
            auto& p = pattern();
            bool on = false;
            for (int d : lanes()[(size_t) l].drums)
                on = on || (p.drumLanes[(size_t) d].load() >> s & 1u) != 0;
            for (int d : lanes()[(size_t) l].drums)   // a click clears the lane's step, or adds its first drum
            {
                auto& lane = p.drumLanes[(size_t) d];
                lane.store (on ? (lane.load() & ~((std::uint64_t) 1 << s)) : lane.load());
            }
            if (! on)
            {
                const int d = usedDrum (l);
                auto& lane = p.drumLanes[(size_t) d];
                lane.store (lane.load() | ((std::uint64_t) 1 << s));
            }
        }
        else if (role != Role::rap)
        {
            auto& st = pattern().steps[(size_t) s];
            if (st.on.load())
                dragging = s;   // drag up or down to change the note; a click without moving takes it off
            else
            {
                st.setSingle (lastNote > 0 ? lastNote : (chops() ? Sampler::rootNote : style.root + (role == Role::bass ? 0 : 24)));
                st.on.store (true);
                dragging = -1;
            }
            dragNote = st.note.load();
            moved = false;
        }
        changed();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0)
            return;
        const int semis = juce::roundToInt ((dragStartY - e.position.y) / 6.0f);
        if (semis == 0 && ! moved)
            return;
        moved = true;
        auto& st = pattern().steps[(size_t) dragging];
        st.setSingle (chops() ? juce::jlimit (Sampler::rootNote, Sampler::rootNote + 15, dragNote + semis) : snapToScale (dragNote + semis),
                      st.velocity.load());
        lastNote = st.note.load();
        changed();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging >= 0 && ! moved)
            pattern().steps[(size_t) dragging].on.store (false);
        dragging = -1;
        changed();
    }

private:
    /** A Sampler playing its slices: the notes are slices, not pitches. */
    bool chops() const { return role == Role::sampler && ! engine.track (track).grainMode.load(); }

    juce::String label (int note) const
    {
        if (chops())
            return "S" + juce::String (note - Sampler::rootNote + 1);
        if (role == Role::sampler && note < GranularParams::numPads)
            return "Spot " + juce::String (note + 1);
        return noteName (note);
    }

    Song::TrackPattern& pattern() { return engine.song.scene (bar).tracks[(size_t) track]; }

    int stepAt (float x) const { return juce::jlimit (0, 15, (int) (x / ((float) getWidth() / 16.0f))); }

    int playingStep() const
    {
        if (! engine.transport.playing.load() || engine.song.chainPosition.load() != bar)
            return -1;
        return engine.song.playheadStep.load();
    }

    static juce::Colour laneColour (int l)
    {
        static const juce::Colour c[4] = { juce::Colour (0xffff8a3d), juce::Colour (0xfff4f0e6), juce::Colour (0xff4fd1c5), juce::Colour (0xffe0457b) };
        return c[l];
    }

    /** The drum a click adds in a lane: one the part already uses, or the lane's first. */
    int usedDrum (int l)
    {
        for (int b = 0; b < numBars; ++b)
            for (int d : lanes()[(size_t) l].drums)
                if (engine.song.scene (b).tracks[(size_t) track].drumLanes[(size_t) d].load() != 0)
                    return d;
        return lanes()[(size_t) l].drums.front();
    }

    int snapToScale (int midi) const
    {
        for (int up = 0; up < 12; ++up)
            for (int sign : { 1, -1 })
            {
                const int n = midi + sign * up;
                const int pc = ((n - style.root) % 12 + 12) % 12;
                if (std::find (style.scale.begin(), style.scale.end(), pc) != style.scale.end())
                    return juce::jlimit (0, 127, n);
            }
        return midi;
    }

    /** The word the rap says on a step: count the notes before it. */
    juce::String wordAt (int step)
    {
        int index = 0;
        for (int b = 0; b <= bar; ++b)
            for (int s = 0; s < 16; ++s)
            {
                if (b == bar && s == step)
                {
                    const auto& vp = engine.track (track).voiceParams;
                    juce::StringArray words;
                    if (vp.mode.load() == VoiceParams::syllableBySyllable)
                    {
                        // By syllable: show each syllable as the Voice will say it.
                        auto sc = std::make_unique<speech::Script>();
                        speech::Pieces from {}, to {};
                        speech::compile (vp.getText(), vp.language.load(), *sc);
                        const int n = speech::syllables (*sc, from, to);
                        for (int i = 0; i < n; ++i)
                        {
                            juce::String syl;
                            for (int k = from[(size_t) i]; k < to[(size_t) i]; ++k)
                                if (speech::phone (sc->segs[(size_t) k].phone).kind != speech::closure)
                                    syl << speech::phone (sc->segs[(size_t) k].phone).name;
                            words.add (syl);
                        }
                    }
                    else
                        words.addTokens (vp.getText(), " ", "");
                    words.removeEmptyStrings();
                    return juce::isPositiveAndBelow (index, words.size()) ? words[index] : juce::String();
                }
                if (engine.song.scene (b).tracks[(size_t) track].steps[(size_t) s].on.load())
                    ++index;
            }
        return {};
    }

    void changed()
    {
        repaint();
        if (onEdit) onEdit();
    }

    AudioEngine& engine;
    const Style& style;
    const int track;
    const Role role;
    int dragging = -1, dragNote = 0, lastNote = 0;
    float dragStartY = 0.0f;
    bool moved = false;
};

// ============================================================================ the touch pad
/** The big pad of a track: drag a finger to play it, as on Figure.

    - Notes (bass, keys, pads, rap): columns of the key's notes, low on the left;
      sliding across changes the note, up and down opens the filter.
    - Drums: a column per lane (kick, snare, hats, percussion); holding one rolls it
      on every 1/16 while the loop plays.
    - Grains (a Sampler in Grain mode): the recording fills the pad; left to right
      moves the cloud through it, up and down sets the grain size. A second finger
      plays the nearest pad spot.
    - Chops (a Sampler in slices): 16 pads, one per slice.

    With Rec on, everything played goes into the loop (notes on the steps, grain moves as locks). */
class TouchPad final : public juce::Component, private juce::Timer
{
public:
    TouchPad (AudioEngine& e, const ControlTargets& t, const Style& s, int trackIndex)
        : engine (e), targets (t), style (s), track (trackIndex), role (s.tracks[(size_t) trackIndex].role)
    {
        setRepaintsOnMouseActivity (false);
        startTimerHz (60);
    }

    ~TouchPad() override { releaseAll(); }

    /** Up and down on a notes or drums pad: the track's tone, 0 (dark) .. 1 (bright). */
    std::function<void (float)> onTone;

    enum class Mode { notes, drums, grains, chops };
    Mode mode() const
    {
        if (role == Role::beats) return Mode::drums;
        if (role == Role::sampler) return engine.track (track).grainMode.load() ? Mode::grains : Mode::chops;
        return Mode::notes;
    }

    void releaseAll()
    {
        for (auto& [index, touch] : touches)
            stop (touch);
        touches.clear();
        engine.track (track).grainHold.store (false);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        const auto m = mode();
        if (m == Mode::grains)
            paintGrains (g, r);
        else
        {
            const int n = cellCount();
            const auto sounding = soundingCells();
            for (int c = 0; c < n; ++c)
            {
                const auto cell = cellBounds (c);
                bool held = false;
                for (const auto& [index, touch] : touches)
                    held = held || touch.cell == c;
                g.setColour (held ? Colours::accentDim : juce::Colour (0xff26282f));
                g.fillRoundedRectangle (cell, 8.0f);
                if (sounding[(size_t) c])
                {
                    g.setColour (Colours::playhead.withAlpha (0.28f));
                    g.fillRoundedRectangle (cell, 8.0f);
                }
                if (held)
                {
                    g.setColour (Colours::accent);
                    g.drawRoundedRectangle (cell.reduced (1.0f), 8.0f, 2.0f);
                }
                g.setColour (held ? Colours::text : Colours::textDim);
                g.setFont (juce::FontOptions (m == Mode::drums ? 15.0f : 13.0f, juce::Font::bold));
                g.drawText (cellName (c), cell.withHeight (30.0f), juce::Justification::centred);
            }
        }

        // The fingers and where they have been.
        for (const auto& [index, touch] : touches)
        {
            for (size_t i = 1; i < touch.trail.size(); ++i)
            {
                g.setColour (Colours::playhead.withAlpha (0.12f + 0.5f * (float) i / (float) touch.trail.size()));
                g.drawLine (juce::Line<float> (touch.trail[i - 1], touch.trail[i]), 3.0f);
            }
            g.setColour (Colours::accent.withAlpha (0.9f));
            g.fillEllipse (juce::Rectangle<float> (44.0f, 44.0f).withCentre (touch.at));
            g.setColour (Colours::accent.withAlpha (0.4f));
            g.drawEllipse (juce::Rectangle<float> (70.0f, 70.0f).withCentre (touch.at), 2.0f);
        }

        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (12.0f));
        const auto foot = r.reduced (12.0f, 8.0f).removeFromBottom (16.0f);
        const juce::String across = m == Mode::grains ? "position" : m == Mode::drums ? "drum" : m == Mode::chops ? "slice" : "note";
        g.drawText (across + juce::String::fromUTF8 (" \xe2\x86\x92"), foot, juce::Justification::centredLeft);
        if (m != Mode::chops)
            g.drawText (juce::String::fromUTF8 ("\xe2\x86\x91 ") + (m == Mode::grains ? "grain size" : "tone"), foot, juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& touch = touches[e.source.getIndex()];
        touch.at = e.position;
        touch.trail.assign (1, e.position);
        touch.cell = -1;
        touch.first = touches.size() == 1;
        move (touch, e.position);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto it = touches.find (e.source.getIndex());
        if (it == touches.end())
            return;
        auto& touch = it->second;
        touch.at = e.position;
        touch.trail.push_back (e.position);
        if (touch.trail.size() > 28)
            touch.trail.erase (touch.trail.begin());
        move (touch, e.position);
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto it = touches.find (e.source.getIndex());
        if (it == touches.end())
            return;
        stop (it->second);
        touches.erase (it);
        if (touches.empty())
            engine.track (track).grainHold.store (false);
        repaint();
    }

private:
    struct Touch
    {
        juce::Point<float> at;
        std::vector<juce::Point<float>> trail;
        int cell = -1, note = -1, channel = 1;
        bool first = true;
    };

    // ---------------------------------------------------------------- cells
    int cellCount() const
    {
        switch (mode())
        {
            case Mode::drums: return 4;
            case Mode::chops: return 16;
            default:          return role == Role::bass ? 8 : 10;
        }
    }

    juce::Rectangle<float> cellBounds (int c) const
    {
        auto area = getLocalBounds().toFloat().reduced (8.0f).withTrimmedBottom (20.0f);
        if (mode() == Mode::chops)
        {
            const float w = area.getWidth() / 4.0f, h = area.getHeight() / 4.0f;
            return { area.getX() + (float) (c % 4) * w, area.getY() + (float) (c / 4) * h, w, h };
        }
        const int n = cellCount();
        const float w = area.getWidth() / (float) n;
        return juce::Rectangle<float> (area.getX() + (float) c * w, area.getY(), w, area.getHeight()).reduced (2.0f, 0.0f);
    }

    int cellAt (juce::Point<float> p) const
    {
        for (int c = 0; c < cellCount(); ++c)
            if (cellBounds (c).expanded (2.0f, 0.0f).contains (p))
                return c;
        const auto area = getLocalBounds().toFloat();
        if (mode() != Mode::chops)   // past the edges: the first or last column
            return p.x < area.getCentreX() ? 0 : cellCount() - 1;
        return -1;
    }

    int scaleNote (int degree, int octave) const
    {
        const int n = (int) style.scale.size();
        const int oct = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
        return style.root + style.scale[(size_t) (degree - oct * n)] + 12 * (oct + octave);
    }

    int baseOctave() const
    {
        switch (role)
        {
            case Role::bass: return 0;
            case Role::rap:  return 1;
            default:         return 2;
        }
    }

    /** The drum a lane plays: one the part already uses, or the lane's first. */
    int laneDrum (int l) const
    {
        for (int b = 0; b < numBars; ++b)
            for (int d : lanes()[(size_t) l].drums)
                if (engine.song.scene (b).tracks[(size_t) track].drumLanes[(size_t) d].load() != 0)
                    return d;
        return lanes()[(size_t) l].drums.front();
    }

    int noteFor (int c) const
    {
        switch (mode())
        {
            case Mode::drums: return Song::drumBaseNote + laneDrum (c);
            case Mode::chops: return Sampler::rootNote + c;
            default:          return scaleNote (c, baseOctave());
        }
    }

    juce::String cellName (int c) const
    {
        switch (mode())
        {
            case Mode::drums: return lanes()[(size_t) c].name;
            case Mode::chops: return juce::String (c + 1);
            default:          return juce::MidiMessage::getMidiNoteName (noteFor (c), true, false, 3);
        }
    }

    /** The cells the loop is playing right now, lit in teal. */
    std::vector<bool> soundingCells() const
    {
        std::vector<bool> on ((size_t) cellCount(), false);
        const int step = engine.song.trackStep[(size_t) track].load();
        if (! engine.transport.playing.load() || step < 0)
            return on;
        const auto& p = engine.song.scene (engine.song.chainPosition.load()).tracks[(size_t) track];
        if (mode() == Mode::drums)
        {
            for (int l = 0; l < 4; ++l)
                for (int d : lanes()[(size_t) l].drums)
                    if (((p.drumLanes[(size_t) d].load() >> step) & 1u) != 0)
                        on[(size_t) l] = true;
            return on;
        }
        // A held note keeps its column lit.
        for (int s = 0; s <= step; ++s)
        {
            const auto& st = p.steps[(size_t) s];
            if (! st.on.load()) continue;
            const float len = st.length.load() > 0.0f ? st.length.load() : 0.5f;
            if ((float) s + len <= (float) step) continue;
            for (int c = 0; c < cellCount(); ++c)
                if (noteFor (c) == st.note.load())
                    on[(size_t) c] = true;
        }
        return on;
    }

    // ---------------------------------------------------------------- playing
    void move (Touch& touch, juce::Point<float> p)
    {
        const auto area = getLocalBounds().toFloat();
        const float x = juce::jlimit (0.0f, 1.0f, (p.x - area.getX()) / area.getWidth());
        const float y = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - area.getY()) / area.getHeight());
        const auto m = mode();

        if (m == Mode::grains)
        {
            if (touch.first)
            {
                // The cloud follows the finger, through the knobs' targets: with Rec on, the moves are recorded.
                if (auto* pos = targets.find ("gran.position")) pos->set (x);
                if (auto* size = targets.find ("gran.size")) size->set (0.15f + 0.8f * y);
                engine.track (track).grainHold.store (true);
            }
            else
            {
                // Another finger: the pad spot nearest it.
                const auto& pads = engine.track (track).granularParams.pads;
                int best = 0;
                for (int i = 1; i < GranularParams::numPads; ++i)
                    if (std::abs (pads[(size_t) i].position.load() - x) < std::abs (pads[(size_t) best].position.load() - x))
                        best = i;
                if (best != touch.cell)
                {
                    stop (touch);
                    touch.cell = best;
                    start (touch, Sampler::padBase + best, 10, 0.85f);
                }
            }
            return;
        }

        const int c = cellAt (p);
        if (c != touch.cell)
        {
            stop (touch);
            touch.cell = c;
            if (c >= 0)
                start (touch, noteFor (c), m == Mode::drums ? 10 : 1, 0.6f + 0.4f * y);
        }
        if (touch.first && m != Mode::chops && onTone)
            onTone (y);
    }

    void start (Touch& touch, int note, int channel, float velocity)
    {
        touch.note = note;
        touch.channel = channel;
        engine.keyboardState.noteOn (channel, note, velocity);
        lastRollStep = engine.song.trackStep[(size_t) track].load();
    }

    void stop (Touch& touch)
    {
        if (touch.note >= 0)
            engine.keyboardState.noteOff (touch.channel, touch.note, 0.0f);
        touch.note = -1;
    }

    void timerCallback() override
    {
        // Drums held while the loop plays: a roll, one hit per 1/16.
        if (mode() == Mode::drums && engine.transport.playing.load() && ! touches.empty())
        {
            const int step = engine.song.trackStep[(size_t) track].load();
            if (step != lastRollStep)
            {
                lastRollStep = step;
                for (auto& [index, touch] : touches)
                    if (touch.note >= 0)
                    {
                        engine.keyboardState.noteOff (touch.channel, touch.note, 0.0f);
                        engine.keyboardState.noteOn (touch.channel, touch.note, 0.85f);
                    }
            }
        }
        if (mode() == Mode::grains)
            updatePeaks();
        if (isShowing())
            repaint();
    }

    // ---------------------------------------------------------------- grains
    void updatePeaks()
    {
        constexpr int bins = 600;
        auto& tr = engine.track (track);
        if (tr.grainLive.load())
        {
            const auto& ring = engine.getLiveRing();
            const int len = ring.getNumSamples(), head = engine.getLiveRingHead();
            if (len < bins) return;
            peaks.assign ((size_t) bins, 0.0f);
            const float* l = ring.getReadPointer (0);
            for (int b = 0; b < bins; ++b)
                for (int i = b * len / bins; i < (b + 1) * len / bins; i += 16)
                    peaks[(size_t) b] = juce::jmax (peaks[(size_t) b], std::abs (l[(head + i) % len]));
            shownSample = "(live)";
            return;
        }
        const auto name = tr.samplerEngine.getSampleName() + juce::String (tr.samplerEngine.getSampleSeconds());
        if (name == shownSample) return;
        shownSample = name;
        peaks.clear();
        tr.samplerEngine.useSample ([this] (const juce::AudioBuffer<float>& audio, double) {
            const int len = audio.getNumSamples();
            if (len < bins) return;
            peaks.assign ((size_t) bins, 0.0f);
            const float* l = audio.getReadPointer (0);
            const int stride = juce::jmax (1, len / bins / 48);
            for (int b = 0; b < bins; ++b)
                for (int i = b * len / bins; i < (b + 1) * len / bins; i += stride)
                    peaks[(size_t) b] = juce::jmax (peaks[(size_t) b], std::abs (l[i]));
        });
    }

    void paintGrains (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto area = r.reduced (10.0f).withTrimmedBottom (20.0f);
        auto& tr = engine.track (track);
        const auto& gp = tr.granularParams;
        const float mid = area.getCentreY() + 8.0f, half = (area.getHeight() - 24.0f) * 0.48f;
        g.setColour (tr.grainLive.load() ? Colours::playhead.withAlpha (0.6f) : Colours::textDim.withAlpha (0.75f));
        const int bins = (int) peaks.size();
        for (int x = 0; x < (int) area.getWidth() && bins > 0; x += 2)
        {
            const float v = peaks[(size_t) juce::jlimit (0, bins - 1, x * bins / (int) area.getWidth())];
            g.fillRect (area.getX() + (float) x, mid - v * half, 1.5f, juce::jmax (1.0f, 2.0f * v * half));
        }
        // The pads' spots, numbered along the top.
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        for (int i = 0; i < GranularParams::numPads; ++i)
        {
            const float px = area.getX() + gp.pads[(size_t) i].position.load() * area.getWidth();
            const auto tag = juce::Rectangle<float> (px - 9.0f, area.getY(), 18.0f, 14.0f);
            g.setColour (Colours::panelEdge.brighter (0.2f));
            g.fillRoundedRectangle (tag, 3.0f);
            g.setColour (Colours::text);
            g.drawText (juce::String (i + 1), tag, juce::Justification::centred);
        }
        // The cloud: where it is, and how wide.
        const float pos = tr.grainHold.load() && gp.shownPosition.load() >= 0.0f ? gp.shownPosition.load() : gp.position.load();
        const float w = juce::jmax (4.0f, gp.spread.load() * area.getWidth());
        g.setColour (Colours::accent.withAlpha (0.16f));
        g.fillRect (juce::Rectangle<float> (area.getX() + pos * area.getWidth() - w * 0.5f, area.getY() + 18.0f, w, area.getHeight() - 18.0f));
        g.setColour (Colours::accent);
        g.fillRect (juce::Rectangle<float> (area.getX() + gp.position.load() * area.getWidth() - 1.5f, area.getY() + 18.0f, 3.0f, area.getHeight() - 18.0f));
        if (gp.freeze.load())
        {
            g.setColour (Colours::playhead);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText ("FROZEN", area.withTrimmedTop (20.0f).withHeight (18.0f), juce::Justification::centredRight);
        }
        if (bins == 0)
        {
            g.setColour (Colours::textDim);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText ("Load a recording, or switch on Live input", area, juce::Justification::centred);
        }
    }

    AudioEngine& engine;
    const ControlTargets& targets;
    const Style& style;
    const int track;
    const Role role;
    std::map<int, Touch> touches;
    int lastRollStep = -1;
    std::vector<float> peaks;
    juce::String shownSample;
};

// ============================================================================ a track's tile, sideways
/** Sideways, for playing live: one tile per track. A tap mutes it or brings it back;
    the bar along the bottom shows how loud it plays. */
class LiveTile final : public juce::Component
{
public:
    LiveTile (AudioEngine& e, const Style& s, int t) : engine (e), style (s), track (t) {}

    std::function<juce::String()> soundName;
    std::function<void()> onChange;

    void paint (juce::Graphics& g) override
    {
        const auto& tr = engine.track (track);
        const bool muted = tr.mute.load();
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (muted ? Colours::panel : juce::Colour (0xff3a3c42));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (muted ? Colours::panelEdge : Colours::accent);
        g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, muted ? 1.0f : 2.5f);

        auto inner = r.reduced (10.0f, 8.0f);
        g.setColour (muted ? Colours::textDim : Colours::text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText (style.tracks[(size_t) track].label, inner.removeFromTop (24.0f), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (Colours::textDim);
        g.drawText (soundName ? soundName() : juce::String(), inner.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (muted ? Colours::textDim : Colours::accent);
        g.drawText (muted ? "MUTED" : "ON", inner.removeFromTop (18.0f), juce::Justification::centredLeft);

        // How loud it plays.
        const auto meter = r.reduced (10.0f, 0.0f).removeFromBottom (12.0f).withTrimmedBottom (6.0f);
        g.setColour (Colours::background);
        g.fillRoundedRectangle (meter, 2.0f);
        const float level = muted ? 0.0f : juce::jlimit (0.0f, 1.0f, tr.level.load() * 1.5f);
        g.setColour (Colours::playhead);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * level), 2.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        auto& m = engine.track (track).mute;
        m.store (! m.load());
        repaint();
        if (onChange) onChange();
    }

private:
    AudioEngine& engine;
    const Style& style;
    const int track;
};

// ============================================================================ sound to and from the sound card
/** Hands the engine the sound card's input (for live grains), then fills the output. */
struct LiteAudio final : public juce::AudioSource
{
    explicit LiteAudio (AudioEngine& e) : engine (e) {}
    void prepareToPlay (int block, double rate) override { engine.prepareToPlay (block, rate); }
    void releaseResources() override { engine.releaseResources(); }
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        engine.setLiveInput (*info.buffer, info.startSample, info.numSamples);
        engine.getNextAudioBlock (info);
    }
    AudioEngine& engine;
};

// ============================================================================ the window
class LiteComponent final : public juce::Component, private juce::Timer
{
public:
    LiteComponent() : style (lite::style (BIZ_LITE_STYLE))
    {
        setLookAndFeel (&lookAndFeel);
        formats.registerBasicFormats();
        engine.lfoTargets.store (&targets);
        deviceManager.initialiseWithDefaultDevices (0, 2);   // the input only when Live input is switched on
        sourcePlayer.setSource (&liteAudio);
        deviceManager.addAudioCallback (&sourcePlayer);

        Generator::setUp (engine, style, states);
        seeds.resize (style.tracks.size());
        for (auto& s : seeds) s = rng.nextInt (100000);

        title.setText (style.appName, juce::dontSendNotification);
        title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, Colours::accent);
        title.setMinimumHorizontalScale (0.7f);
        addAndMakeVisible (title);

        button (playButton, juce::String::fromUTF8 ("\xe2\x96\xb6"), [this] { engine.transport.playing.store (! engine.transport.playing.load()); });
        button (recButton, juce::String::fromUTF8 ("\xe2\x97\x8f Rec"), [this] { toggleRecord(); });
        recButton.setTooltip ("Record: what you play on the pad goes into the loop (and grain moves, as locks). Starts the loop if it is stopped.");
        button (generateButton, "Generate", [this] { generateAll(); });
        generateButton.setColour (juce::TextButton::buttonColourId, Colours::accent);
        generateButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        generateButton.setTooltip ("A new song in the style: new chords and a new part on every track.");
        button (menuButton, juce::String::fromUTF8 ("\xe2\x8b\xaf"), [this] { showMenu(); });
        menuButton.setTooltip ("Open, save, export and audio settings");

        for (int t = 0; t < (int) style.tracks.size(); ++t)
        {
            auto* tab = tabs.add (new juce::TextButton (style.tracks[(size_t) t].label));
            tab->setWantsKeyboardFocus (false);
            tab->onClick = [this, t] { showTrack (t); };
            addAndMakeVisible (tab);

            auto* pad = pads.add (new TouchPad (engine, targets, style, t));
            pad->onTone = [this, t] (float y) { setTone (t, y); };
            addChildComponent (pad);

            auto* row = stepRows.add (new StepRow (engine, style, t));
            row->onEdit = [this] { repaint(); };
            addChildComponent (row);

            auto* tile = tiles.add (new LiveTile (engine, style, t));
            tile->soundName = [this, t] {
                const int which = states[(size_t) t].sound;
                return juce::String (which == 0 ? "A  " : "B  ") + style.tracks[(size_t) t].sounds[(size_t) which].name;
            };
            addChildComponent (tile);
        }

        for (int b = 0; b < numBars; ++b)
        {
            auto* bb = barButtons.add (new juce::TextButton (juce::String (b + 1)));
            bb->setWantsKeyboardFocus (false);
            bb->setTooltip ("Show bar " + juce::String (b + 1) + " of the loop in the steps");
            bb->onClick = [this, b] { showBar (b); };
            addAndMakeVisible (bb);
        }
        tempo.setSliderStyle (juce::Slider::LinearBar);
        tempo.setRange (60.0, 180.0, 1.0);
        tempo.setValue (style.bpm, juce::dontSendNotification);
        tempo.setTextValueSuffix (" BPM");
        tempo.setColour (juce::Slider::trackColourId, Colours::accentDim);
        tempo.onValueChange = [this] { engine.transport.bpm.store (tempo.getValue()); };
        addAndMakeVisible (tempo);

        static const char* knobNames[4] = { "Rhythm", "Alter", "Tone", "Volume" };
        static const char* knobTips[4] = { "Fewer notes to the left, more to the right: the part is rewritten when you let go.",
                                           "Another version of the part, in the style and on the same chords: turn and let go.",
                                           "Darker or brighter (the pad's up and down does the same).",
                                           "This track's volume" };
        for (int k = 0; k < 4; ++k)
        {
            auto& s = knobs[(size_t) k];
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            s.setRange (0.0, 1.0);
            s.setTooltip (knobTips[k]);
            s.onValueChange = [this, k] { knobMoved (k, false); };
            s.onDragEnd = [this, k] { knobMoved (k, true); };
            addAndMakeVisible (s);
            auto& l = knobLabels[(size_t) k];
            l.setText (knobNames[k], juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centred);
            l.setFont (juce::FontOptions (12.0f));
            l.setColour (juce::Label::textColourId, Colours::textDim);
            addAndMakeVisible (l);
        }
        for (int i = 0; i < 2; ++i)
        {
            auto& b = soundButtons[(size_t) i];
            b.setWantsKeyboardFocus (false);
            b.onClick = [this, i] { chooseSound (current, i); };
            addAndMakeVisible (b);
        }

        // The Sampler's buttons.
        button (freezeButton, "Freeze", [this] { auto& f = engine.track (current).granularParams.freeze; f.store (! f.load()); });
        freezeButton.setTooltip ("Hold the grains where they are");
        button (liveButton, "Live input", [this] { toggleLive(); });
        liveButton.setTooltip ("Grains of the microphone or line in, as it comes in (the last ten seconds)");
        button (spotsButton, "Spots", [this] { openSpots(); });
        spotsButton.setTooltip ("The 16 pads' spots on the recording, each with its own size and pitch; keep the live input as the recording");
        button (loadButton, "Load", [this] { loadRecording(); });
        loadButton.setTooltip ("Load a field recording (WAV, AIFF, FLAC or OGG, up to five minutes)");

        // The rap's lyrics and how they sit.
        if (rapTrack() >= 0)
        {
            lyrics.setMultiLine (true, true);
            lyrics.setReturnKeyStartsNewLine (true);
            lyrics.setFont (juce::FontOptions (17.0f));
            lyrics.setText (defaultLyrics(), juce::dontSendNotification);
            lyrics.setColour (juce::TextEditor::backgroundColourId, Colours::panel);
            lyrics.setColour (juce::TextEditor::outlineColourId, Colours::panelEdge);
            lyrics.setTooltip ("Your lyrics. Rap it lays them on the beat; what does not fit in the four bars is left out.");
            addChildComponent (lyrics);
            for (int f = 0; f < (int) Flow::numFlows; ++f)
                flowBox.addItem (flowName ((Flow) f), f + 1);
            flowBox.setSelectedId (1, juce::dontSendNotification);
            flowBox.setTooltip ("How the words sit on the beat: laid back, double time, or choppy.");
            addChildComponent (flowBox);
            bySyllable.setButtonText ("By syllable");
            bySyllable.setClickingTogglesState (true);
            bySyllable.setColour (juce::TextButton::buttonOnColourId, Colours::accent);
            bySyllable.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            bySyllable.setTooltip ("On: one note per syllable, so the rap can be chopped in the steps. Off: one note per word.");
            bySyllable.onClick = [this] { rapIt(); };
            addChildComponent (bySyllable);
            for (int l = 0; l < speech::numLanguages; ++l)
            {
                auto& b = languageButtons[(size_t) l];
                b.setButtonText (l == speech::spanish ? "ES" : "EN");
                b.setTooltip (l == speech::spanish ? "Rap in Spanish" : "Rap in English");
                b.onClick = [this, l] { engine.track (rapTrack()).voiceParams.language.store (l); refresh(); };
                addChildComponent (b);
            }
            button (rapButton, "Rap it", [this] { rapIt(); });
            rapButton.setVisible (false);
            rapInfo.setColour (juce::Label::textColourId, Colours::textDim);
            rapInfo.setFont (juce::FontOptions (12.0f));
            addChildComponent (rapInfo);
        }

        generateAll();
        showTrack (0);
        setSize (430, 900);
        startTimerHz (20);
    }

    ~LiteComponent() override
    {
        deviceManager.removeAudioCallback (&sourcePlayer);
        sourcePlayer.setSource (nullptr);
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::background);
        const int barBottom = safeArea().getY() + topHeight;   // the top bar, under the phone's notch and clock
        g.setColour (Colours::panel);
        g.fillRect (getLocalBounds().withHeight (barBottom));
        g.setColour (Colours::panelEdge);
        g.drawHorizontalLine (barBottom - 1, 0.0f, (float) getWidth());
    }

    /** The part of the screen nothing covers: on a phone, inside the notch, the clock and the home bar. */
    juce::Rectangle<int> safeArea() const
    {
        auto r = getLocalBounds();
       #if JUCE_IOS || JUCE_ANDROID
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
            r = d->safeAreaInsets.subtractedFrom (r);
       #endif
        return r;
    }

    void resized() override
    {
        auto r = safeArea();
        auto top = r.removeFromTop (topHeight).reduced (10, 8);
        menuButton.setBounds (top.removeFromRight (40).reduced (2, 2));
        generateButton.setBounds (top.removeFromRight (96).reduced (2, 2));
        recButton.setBounds (top.removeFromRight (66).reduced (2, 2));
        playButton.setBounds (top.removeFromRight (46).reduced (2, 2));
        title.setBounds (top);

        r.reduce (10, 8);
        auto tabRow = r.removeFromTop (40);
        const int tw = tabRow.getWidth() / juce::jmax (1, tabs.size());
        for (auto* t : tabs)
            t->setBounds (tabRow.removeFromLeft (tw).reduced (2, 2));
        r.removeFromTop (8);

        const bool wide = getWidth() > getHeight();   // sideways: the tiles and the pad, for playing live
        updateVisibility (wide);
        if (wide)
        {
            layoutWide (r);
            return;
        }

        auto padArea = wide ? r.removeFromLeft (r.getWidth() * 3 / 5) : r;
        auto side = wide ? r.withTrimmedLeft (12) : r;

        // From the bottom up: A / B, the dials, the bars and tempo, the steps, the Sampler's or the rap's buttons.
        soundArea = side.removeFromBottom (44);
        side.removeFromBottom (6);
        auto knobRow = side.removeFromBottom (92);
        const int kw = knobRow.getWidth() / 4;
        for (int k = 0; k < 4; ++k)
        {
            auto c = knobRow.removeFromLeft (kw);
            knobLabels[(size_t) k].setBounds (c.removeFromBottom (18));
            knobs[(size_t) k].setBounds (c.reduced (6, 0));
        }
        side.removeFromBottom (6);
        auto barRow = side.removeFromBottom (32);
        tempo.setBounds (barRow.removeFromRight (juce::jmin (130, barRow.getWidth() / 3)).reduced (2, 2));
        barRow.removeFromRight (8);
        const int bw = barRow.getWidth() / numBars;
        for (auto* b : barButtons)
            b->setBounds (barRow.removeFromLeft (bw).reduced (2, 2));
        side.removeFromBottom (6);
        stepsArea = side.removeFromBottom (wide ? juce::jmin (130, side.getHeight() / 2) : 74);
        side.removeFromBottom (6);
        extraArea = side.removeFromBottom (role (current) == Role::sampler || role (current) == Role::rap ? 40 : 0);
        if (! wide)
            padArea = side.withTrimmedBottom (6);
        else
            padArea = padArea.withTrimmedBottom (0);

        for (int t = 0; t < pads.size(); ++t)
        {
            pads[t]->setBounds (padArea);
            stepRows[t]->setBounds (stepsArea);
        }
        soundButtons[0].setBounds (soundArea.withWidth (soundArea.getWidth() / 2).reduced (2, 2));
        soundButtons[1].setBounds (soundArea.withTrimmedLeft (soundArea.getWidth() / 2).reduced (2, 2));

        // The Sampler's row.
        auto ex = extraArea;
        const int ew = ex.getWidth() / 4;
        for (auto* b : { &freezeButton, &liveButton, &spotsButton, &loadButton })
            b->setBounds (ex.removeFromLeft (ew).reduced (2, 2));

        // The rap: the lyrics take the pad's place, the controls the row under it.
        if (rapTrack() >= 0)
        {
            auto lyricArea = padArea;
            rapInfo.setBounds (lyricArea.removeFromBottom (20));
            lyrics.setBounds (lyricArea.withTrimmedBottom (4));
            auto line = extraArea;
            languageButtons[0].setBounds (line.removeFromLeft (46).reduced (2, 2));
            languageButtons[1].setBounds (line.removeFromLeft (46).reduced (2, 2));
            rapButton.setBounds (line.removeFromRight (80).reduced (2, 2));
            bySyllable.setBounds (line.removeFromRight (100).reduced (2, 2));
            flowBox.setBounds (line.reduced (2, 2));
        }
    }

private:
    static constexpr int topHeight = 58;

    Role role (int t) const { return style.tracks[(size_t) juce::jlimit (0, (int) style.tracks.size() - 1, t)].role; }

    void button (juce::TextButton& b, const juce::String& text, std::function<void()> click)
    {
        b.setButtonText (text);
        b.setWantsKeyboardFocus (false);
        b.onClick = std::move (click);
        addAndMakeVisible (b);
    }

    // ------------------------------------------------------------------ pages
    /** Upright: the track's screen (pad, steps, dials, A / B). Sideways: the tiles of every
        track and the selected track's pad, for playing live. */
    void updateVisibility (bool wide)
    {
        const bool rap = role (current) == Role::rap, smp = role (current) == Role::sampler;
        for (int i = 0; i < pads.size(); ++i)
        {
            pads[i]->setVisible (i == current && ! rap);
            stepRows[i]->setVisible (i == current && ! wide);
            tiles[i]->setVisible (wide);
        }
        for (auto* b : { &freezeButton, &liveButton, &spotsButton, &loadButton })
            b->setVisible (smp && ! wide);
        for (int k = 0; k < 4; ++k)
        {
            knobs[(size_t) k].setVisible (! wide);
            knobLabels[(size_t) k].setVisible (! wide);
        }
        for (auto& b : soundButtons)
            b.setVisible (! wide);
        if (rapTrack() >= 0)
        {
            lyrics.setVisible (rap);
            rapInfo.setVisible (rap);
            for (juce::Component* c : { (juce::Component*) &flowBox, (juce::Component*) &bySyllable, (juce::Component*) &rapButton,
                                        (juce::Component*) &languageButtons[0], (juce::Component*) &languageButtons[1] })
                c->setVisible (rap && ! wide);
        }
    }

    /** Sideways: the tiles on the left with the bars and tempo under them; the pad (or the lyrics) on the right. */
    void layoutWide (juce::Rectangle<int> r)
    {
        auto left = r.removeFromLeft (r.getWidth() * 2 / 5);
        r.removeFromLeft (12);
        auto barRow = left.removeFromBottom (36);
        tempo.setBounds (barRow.removeFromRight (juce::jmin (120, barRow.getWidth() / 3)).reduced (2, 2));
        barRow.removeFromRight (6);
        const int bw = barRow.getWidth() / numBars;
        for (auto* b : barButtons)
            b->setBounds (barRow.removeFromLeft (bw).reduced (2, 2));
        left.removeFromBottom (6);
        const int cols = 2, rows = (tiles.size() + cols - 1) / cols;
        const int th = left.getHeight() / juce::jmax (1, rows), tw = left.getWidth() / cols;
        for (int i = 0; i < tiles.size(); ++i)
            tiles[i]->setBounds (left.getX() + (i % cols) * tw, left.getY() + (i / cols) * th, tw, th);
        for (auto* p : pads)
            p->setBounds (r);
        if (rapTrack() >= 0)
        {
            auto lyricArea = r;
            rapInfo.setBounds (lyricArea.removeFromBottom (20));
            lyrics.setBounds (lyricArea.withTrimmedBottom (4));
        }
    }

    void showTrack (int t)
    {
        if (juce::isPositiveAndBelow (current, pads.size()))
            pads[current]->releaseAll();
        current = juce::jlimit (0, (int) style.tracks.size() - 1, t);
        engine.selectedTrack.store (current);   // the pad's notes, the knobs' targets and recording go to this track
        updateVisibility (getWidth() > getHeight());
        const auto& def = style.tracks[(size_t) current];
        for (int i = 0; i < 2; ++i)
            soundButtons[(size_t) i].setButtonText (juce::String (i == 0 ? "A  " : "B  ") + def.sounds[(size_t) i].name);
        readKnobs();
        resized();
        refresh();
    }

    void showBar (int b)
    {
        bar = b;
        for (auto* row : stepRows)
        {
            row->bar = b;
            row->repaint();
        }
        refresh();
    }

    void readKnobs()
    {
        auto& tr = engine.track (current);
        knobs[0].setValue (states[(size_t) current].rhythm, juce::dontSendNotification);
        knobs[1].setValue ((double) (seeds[(size_t) current] % 1000) / 1000.0, juce::dontSendNotification);
        knobs[2].setValue (juce::jlimit (0.0, 1.0, (tr.mix.eqHigh.load() + 12.0) / 18.0), juce::dontSendNotification);
        knobs[3].setValue (tr.volume.load(), juce::dontSendNotification);
    }

    void knobMoved (int k, bool released)
    {
        auto& tr = engine.track (current);
        const float v = (float) knobs[(size_t) k].getValue();
        if (k == 0)
        {
            states[(size_t) current].rhythm = v;
            if (released) regenerate (current);
        }
        else if (k == 1 && released)
        {
            seeds[(size_t) current] = juce::roundToInt (v * 999.0f) + 1000 * (1 + rng.nextInt (50));
            regenerate (current);
        }
        else if (k == 2)
            setTone (current, v);
        else if (k == 3)
            tr.volume.store (v);
    }

    /** The track's tone: a tilt, from dark (0) to bright (1). */
    void setTone (int t, float v)
    {
        auto& tr = engine.track (t);
        tr.mix.eqHigh.store (-12.0f + 18.0f * v);
        tr.mix.eqLow.store (3.0f - 6.0f * v);
        if (t == current && ! knobs[2].isMouseButtonDown())
            knobs[2].setValue (v, juce::dontSendNotification);
    }

    // ------------------------------------------------------------------ actions
    int rapTrack() const
    {
        for (int t = 0; t < (int) style.tracks.size(); ++t)
            if (style.tracks[(size_t) t].role == Role::rap)
                return t;
        return -1;
    }

    Flow flow() const { return (Flow) juce::jlimit (0, (int) Flow::numFlows - 1, flowBox.getSelectedId() - 1); }

    juce::String defaultLyrics() const
    {
        return juce::String::fromUTF8 ("Bajo el puente suena el bajo, la ciudad no duerme y yo tampoco, "
                                       "BizHop en la pared, el beat sigue y no se va");
    }

    void regenerate (int t)
    {
        juce::Random seeded (seeds[(size_t) t]);
        Generator::generate (engine, style, states, t, seeded, lyrics.getText(), flow(), bySyllable.getToggleState());
        afterRap();
        refresh();
    }

    void generateAll()
    {
        for (auto& s : seeds) s = rng.nextInt (100000);
        Generator::generateAll (engine, style, states, rng, lyrics.getText(), flow(), bySyllable.getToggleState());
        afterRap();
        refresh();
    }

    void rapIt()
    {
        if (rapTrack() < 0) return;
        regenerate (rapTrack());
    }

    void afterRap()
    {
        const int t = rapTrack();
        if (t < 0) return;
        juce::StringArray all, used;
        all.addTokens (lyrics.getText().replaceCharacters (",.;:!?\"", "       "), " \n\t", "");
        all.removeEmptyStrings();
        used.addTokens (engine.track (t).voiceParams.getText(), " ", "");
        used.removeEmptyStrings();
        rapInfo.setText (juce::String (used.size()) + " of " + juce::String (all.size()) + " words fit in the four bars"
                             + (used.size() < all.size() ? ": try Double time, or fewer words." : "."),
                         juce::dontSendNotification);
    }

    void chooseSound (int t, int which)
    {
        pads[t]->releaseAll();
        Generator::chooseSound (engine, style, states, t, which);
        if (role (t) == Role::sampler)
            regenerate (t);   // grains hold long notes; chops play slices: a part that fits the sound
        refresh();
    }

    void toggleRecord()
    {
        const bool on = ! engine.recordNotes.load();
        engine.recordNotes.store (on);
        if (on && ! engine.transport.playing.load())
            engine.transport.playing.store (true);
        refresh();
    }

    void toggleLive()
    {
        auto& tr = engine.track (current);
        const bool on = ! tr.grainLive.load();
        if (on)
        {
            // The sound card's input, the first time it is needed.
            auto setup = deviceManager.getAudioDeviceSetup();
            if (setup.inputChannels.countNumberOfSetBits() == 0)
            {
                setup.useDefaultInputChannels = true;
                setup.inputChannels.setRange (0, 2, true);
                deviceManager.setAudioDeviceSetup (setup, true);
            }
            if (! tr.grainMode.load())
                chooseSound (current, 0);
        }
        tr.grainLive.store (on);
        refresh();
    }

    void openSpots()
    {
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new GrainEditor (engine, targets, current, [safe = juce::Component::SafePointer<LiteComponent> (this)] {
            if (safe != nullptr) safe->loadRecording();
        }));
        o.dialogTitle = "Grains - " + style.tracks[(size_t) current].label;
        o.dialogBackgroundColour = Colours::background;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;
        o.launchAsync();
    }

    void loadRecording()
    {
        const int t = current;
        chooser = std::make_unique<juce::FileChooser> ("Load a field recording", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       formats.getWildcardForAllFormats());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, t] (const juce::FileChooser& fc) {
                                  const auto file = chosenFile (fc);
                                  juce::MemoryBlock data;
                                  if (file == juce::File() || ! file.loadFileAsData (data))
                                      return;
                                  juce::String error;
                                  const bool wasGrain = engine.track (t).grainMode.load();
                                  if (! Project::loadSampleIntoTrack (engine, t, file.getFileName(), data, formats, error))
                                  {
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load", error);
                                      return;
                                  }
                                  engine.track (t).type.store (Track::sampler);
                                  engine.track (t).grainMode.store (wasGrain);
                                  engine.track (t).grainLive.store (false);
                              });
    }

    void showMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Open...");
        m.addItem (2, "Save...");
        m.addItem (3, "Export WAV...");
        m.addSeparator();
        m.addItem (4, "Audio settings...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [this] (int r) {
            if (r == 1) openProject();
            if (r == 2) saveProject();
            if (r == 3) exportWav();
            if (r == 4) showAudioSettings();
        });
    }

    void refresh()
    {
        auto colourIt = [] (juce::TextButton& b, bool on, juce::Colour onColour = Colours::accent) {
            b.setColour (juce::TextButton::buttonColourId, on ? onColour : Colours::panelEdge);
            b.setColour (juce::TextButton::textColourOffId, on ? juce::Colours::black : Colours::text);
        };
        for (int t = 0; t < tabs.size(); ++t)
        {
            tabs[t]->setColour (juce::TextButton::buttonColourId, t == current ? Colours::accentDim : Colours::panel);
            tabs[t]->setColour (juce::TextButton::textColourOffId, t == current ? Colours::text : Colours::textDim);
        }
        for (int b = 0; b < barButtons.size(); ++b)
            colourIt (*barButtons[b], b == bar);
        for (int i = 0; i < 2; ++i)
            colourIt (soundButtons[(size_t) i], states[(size_t) current].sound == i);
        auto& tr = engine.track (current);
        colourIt (freezeButton, tr.granularParams.freeze.load(), Colours::playhead);
        colourIt (liveButton, tr.grainLive.load(), Colours::playhead);
        colourIt (recButton, engine.recordNotes.load(), juce::Colour (0xffe0453b));
        if (! engine.recordNotes.load())
            recButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6b1d18));
        if (rapTrack() >= 0)
            for (int l = 0; l < speech::numLanguages; ++l)
                colourIt (languageButtons[(size_t) l], engine.track (rapTrack()).voiceParams.language.load() == l);
        for (auto* row : stepRows)
            row->repaint();
        for (auto* tile : tiles)
            tile->repaint();
    }

    void timerCallback() override
    {
        if (getWidth() > getHeight())
            for (auto* tile : tiles)
                tile->repaint();   // the level bars
        const bool playing = engine.transport.playing.load();
        const auto playText = juce::String::fromUTF8 (playing ? "\xe2\x96\xa0" : "\xe2\x96\xb6");
        if (playButton.getButtonText() != playText)
            playButton.setButtonText (playText);
        if (engine.recordNotes.load() != shownRecording)
        {
            shownRecording = engine.recordNotes.load();
            refresh();
        }
