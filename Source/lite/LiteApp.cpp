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
