#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"
#include "engine/SynthLibrary.h"
#include "LookAndFeel.h"

namespace bizgroove
{

/** The whole pattern of the selected track, with a bar that runs across it
    while the song plays, so you can watch what you record land.

    - Drums: one row per drum, sixteen steps across. Click a cell to switch it.
    - Synths: a piano roll. Each note is a block as long as it plays, brighter
      the harder it is played; a chord is several blocks on one step. Click an
      empty place to add a note there, click a note to take it out, drag a
      note sideways to make it longer or shorter. Scroll to see higher or
      lower notes.
*/
class PatternView final : public juce::Component, private juce::Timer
{
public:
    explicit PatternView (AudioEngine& e) : engine (e)
    {
        for (auto* b : { &prevTrack, &nextTrack, &playButton, &recordButton })
        {
            b->setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        }
        prevTrack.setTooltip ("The track before");
        nextTrack.setTooltip ("The next track");
        prevTrack.onClick = [this] { engine.selectedTrack.store ((engine.selectedTrack.load() + Song::numTracks - 1) % Song::numTracks); };
        nextTrack.onClick = [this] { engine.selectedTrack.store ((engine.selectedTrack.load() + 1) % Song::numTracks); };
        playButton.setTooltip ("Play / stop");
        playButton.onClick = [this] { engine.transport.playing.store (! engine.transport.playing.load()); };
        recordButton.setTooltip ("Record what you play into this pattern, onto the nearest 1/16. From a stop, the metronome counts in first.");
        recordButton.onClick = [this] { engine.recorder.toggleRequested.store (true); };

        title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, Colours::text);
        addAndMakeVisible (title);
        position.setFont (juce::FontOptions (26.0f, juce::Font::bold));
        position.setJustificationType (juce::Justification::centredRight);
        position.setColour (juce::Label::textColourId, Colours::playhead);
        addAndMakeVisible (position);
        help.setFont (juce::FontOptions (12.0f));
        help.setColour (juce::Label::textColourId, Colours::textDim);
        addAndMakeVisible (help);
        startTimerHz (30);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (40);
        prevTrack.setBounds (top.removeFromLeft (34).reduced (2, 4));
        nextTrack.setBounds (top.removeFromLeft (34).reduced (2, 4));
        top.removeFromLeft (8);
        recordButton.setBounds (top.removeFromRight (90).reduced (2, 4));
        playButton.setBounds (top.removeFromRight (80).reduced (2, 4));
        position.setBounds (top.removeFromRight (190));
        title.setBounds (top);
        help.setBounds (r.removeFromBottom (22));
        r.removeFromBottom (4);
        grid = r.reduced (0, 4);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (grid.toFloat(), 6.0f);
        const auto area = gridArea();
        const bool drums = engine.selected().playsDrums();
        const int rows = drums ? Song::numDrums : visibleNotes;
        const float rowH = (float) area.getHeight() / (float) rows;
        const float stepW = (float) area.getWidth() / (float) Song::numSteps;
        auto& pattern = currentPattern();

        // Rows and their names.
        g.setFont (juce::FontOptions (juce::jmin (13.0f, rowH * 0.8f)));
        for (int row = 0; row < rows; ++row)
        {
            const float y = (float) area.getY() + (float) row * rowH;
            juce::String name;
            bool shade = false, picked = false;
            if (drums)
            {
                const int drum = row;
                name = DrumKit::drumName (drum);
                picked = engine.selected().selectedDrum.load() == drum;
            }
            else
            {
                const int note = noteForRow (row);
                shade = juce::MidiMessage::isMidiNoteBlack (note);
                if (note % 12 == 0 || rowH >= 12.0f)
                    name = juce::MidiMessage::getMidiNoteName (note, true, true, 4);
            }
            if (shade || picked)
            {
                g.setColour (picked ? Colours::accentDim.withAlpha (0.6f) : juce::Colours::black.withAlpha (0.18f));
                g.fillRect (juce::Rectangle<float> ((float) grid.getX(), y, (float) grid.getWidth(), rowH));
            }
            g.setColour (picked ? Colours::text : Colours::textDim);
            g.drawText (name, juce::Rectangle<float> ((float) grid.getX() + 6.0f, y, (float) labelWidth - 10.0f, rowH),
                        juce::Justification::centredLeft, false);
        }

        // Step lines, stronger on each beat.
        for (int s = 0; s <= Song::numSteps; ++s)
        {
            const float x = (float) area.getX() + (float) s * stepW;
            g.setColour (s % 4 == 0 ? Colours::textDim.withAlpha (0.5f) : Colours::panelEdge);
            g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
        }

        // The step playing: a column that runs across.
        const int playhead = engine.song.playheadStep.load();
        if (playhead >= 0 && (engine.transport.playing.load()))
        {
            const auto col = juce::Rectangle<float> ((float) area.getX() + (float) playhead * stepW, (float) area.getY(), stepW, (float) area.getHeight());
            g.setColour (Colours::playhead.withAlpha (0.18f));
            g.fillRect (col);
            g.setColour (Colours::playhead);
            g.fillRect (col.withWidth (3.0f));
        }

        // The notes.
        if (drums)
        {
            for (int d = 0; d < Song::numDrums; ++d)
                for (int s = 0; s < Song::numSteps; ++s)
                    if ((pattern.drumLanes[(size_t) d].load() >> s) & 1u)
                    {
                        const auto cell = juce::Rectangle<float> ((float) area.getX() + (float) s * stepW, (float) area.getY() + (float) d * rowH, stepW, rowH).reduced (2.0f, 1.5f);
                        g.setColour (s == playhead && engine.transport.playing.load() ? Colours::playhead : Colours::accent);
                        g.fillRoundedRectangle (cell, 3.0f);
                    }
        }
        else
        {
            for (int s = 0; s < Song::numSteps; ++s)
            {
                const auto& st = pattern.steps[(size_t) s];
                if (! st.on.load())
                    continue;
                std::array<int, Song::maxChord> notes {};
                const int count = st.getNotes (notes);
                const float length = st.length.load() > 0.0f ? st.length.load() : 0.5f;
                const float alpha = 0.45f + 0.55f * (float) st.velocity.load() / 127.0f;
                for (int i = 0; i < count; ++i)
                {
                    const int row = rowForNote (notes[(size_t) i]);
                    if (row < 0)
                        continue;
                    const float x = (float) area.getX() + (float) s * stepW;
                    const float w = juce::jmin (length * stepW, (float) area.getRight() - x);
                    const auto block = juce::Rectangle<float> (x, (float) area.getY() + (float) row * rowH, w, rowH).reduced (1.5f, 1.0f);
                    g.setColour ((s == playhead && engine.transport.playing.load() ? Colours::playhead : Colours::accent).withAlpha (alpha));
                    g.fillRoundedRectangle (block, 3.0f);
                    // Notes past the end of the bar carry on into the next one.
                    const float rest = length * stepW - w;
                    if (rest > 1.0f)
                        g.fillRoundedRectangle (juce::Rectangle<float> ((float) area.getX(), block.getY(), juce::jmin (rest, (float) area.getWidth()), block.getHeight()), 3.0f);
                }
            }
        }

        if (recordingNotes())
        {
            g.setColour (juce::Colours::red.withAlpha (0.8f));
            g.drawRoundedRectangle (grid.toFloat().reduced (1.0f), 6.0f, 2.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragNote = -1;
        dragged = false;
        const auto area = gridArea();
        if (! area.contains (e.getPosition()))
        {
            // Clicking a drum's name picks that drum (for the knobs and the pads).
            if (engine.selected().playsDrums() && grid.contains (e.getPosition()) && e.x < area.getX())
            {
                const int row = rowAt (e.y);
                if (row >= 0)
                    engine.selected().selectedDrum.store (row);
            }
            return;
        }
        const int s = stepAt (e.x), row = rowAt (e.y);
        if (s < 0 || row < 0)
            return;
        auto& pattern = currentPattern();

        if (engine.selected().playsDrums())
        {
            auto& lane = pattern.drumLanes[(size_t) row];
            lane.store ((std::uint16_t) (lane.load() ^ (1u << s)));
            engine.selected().selectedDrum.store (row);
            repaint();
            return;
        }

        const int note = noteForRow (row);
        auto& st = pattern.steps[(size_t) s];
        if (st.on.load() && stepHasNote (st, note))
        {
            dragStep = s;
            dragNote = note;
            dragStartLength = st.length.load() > 0.0f ? st.length.load() : 0.5f;
            return;   // a click takes it out (on mouse up); a drag changes its length
        }
        if (! st.on.load())
            st.setSingle (note);
        else
            st.addNote (note);
        st.on.store (true);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragNote < 0)
            return;
        const float stepW = (float) gridArea().getWidth() / (float) Song::numSteps;
        if (std::abs (e.getDistanceFromDragStartX()) > 4)
            dragged = true;
        if (! dragged)
            return;
        const float length = juce::jlimit (0.25f, 16.0f, std::round ((dragStartLength + (float) e.getDistanceFromDragStartX() / stepW) * 4.0f) / 4.0f);
        currentPattern().steps[(size_t) dragStep].length.store (length);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragNote >= 0 && ! dragged)
            currentPattern().steps[(size_t) dragStep].removeNote (dragNote);
        dragNote = -1;
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (engine.selected().playsDrums() || std::abs (wheel.deltaY) < 1.0e-6f)
            return;
        lowestNote = juce::jlimit (0, 127 - visibleNotes + 1, lowestNote + (wheel.deltaY > 0 ? 2 : -2));
        followNotes = false;
        repaint();
    }

private:
    static constexpr int labelWidth = 92, visibleNotes = 36;

    Song::TrackPattern& currentPattern()
    {
        return engine.song.scene (engine.song.currentScene.load()).tracks[(size_t) engine.selectedTrack.load()];
    }

    juce::Rectangle<int> gridArea() const { return grid.withTrimmedLeft (labelWidth).reduced (0, 2); }

    int stepAt (int x) const
    {
        const auto area = gridArea();
        const int s = (int) ((float) (x - area.getX()) / ((float) area.getWidth() / (float) Song::numSteps));
        return juce::isPositiveAndBelow (s, Song::numSteps) ? s : -1;
    }

    int rowAt (int y) const
    {
        const auto area = gridArea();
        const int rows = engine.selected().playsDrums() ? Song::numDrums : visibleNotes;
        const int row = (int) ((float) (y - area.getY()) / ((float) area.getHeight() / (float) rows));
        return juce::isPositiveAndBelow (row, rows) ? row : -1;
    }

    int noteForRow (int row) const { return lowestNote + visibleNotes - 1 - row; }   // high notes at the top
    int rowForNote (int note) const
    {
        const int row = lowestNote + visibleNotes - 1 - note;
        return juce::isPositiveAndBelow (row, visibleNotes) ? row : -1;
    }

    static bool stepHasNote (const Song::Step& st, int note)
    {
        std::array<int, Song::maxChord> notes {};
        const int count = st.getNotes (notes);
        for (int i = 0; i < count; ++i)
            if (notes[(size_t) i] == note)
                return true;
        return false;
    }

    bool recordingNotes() const { return engine.recordNotes.load(); }

    /** Keeps the notes of the pattern in view, until you scroll yourself. */
    void followPattern()
    {
        if (! followNotes || engine.selected().playsDrums())
            return;
        int low = 128, high = -1;
        for (auto& st : currentPattern().steps)
        {
            if (! st.on.load())
                continue;
            std::array<int, Song::maxChord> notes {};
            const int count = st.getNotes (notes);
            for (int i = 0; i < count; ++i)
            {
                low = juce::jmin (low, notes[(size_t) i]);
                high = juce::jmax (high, notes[(size_t) i]);
            }
        }
        if (high < 0)
            return;
        if (low < lowestNote || high > lowestNote + visibleNotes - 1)
            lowestNote = juce::jlimit (0, 127 - visibleNotes + 1, (low + high) / 2 - visibleNotes / 2);
    }

    void timerCallback() override
    {
        const int t = engine.selectedTrack.load();
        const int type = engine.selected().type.load();
        if (t != shownTrack || type != shownType)
        {
            shownTrack = t;
            shownType = type;
            followNotes = true;
        }
        followPattern();

        const auto& library = synthLibrary();
        const auto instrument = library[(size_t) juce::jlimit (0, (int) library.size() - 1, type)].name;
        const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
        const auto newTitle = "Track " + juce::String (t + 1) + dot + instrument + dot + "Scene " + juce::String (engine.song.currentScene.load() + 1);
        if (title.getText() != newTitle)
            title.setText (newTitle, juce::dontSendNotification);

        // Where the song is: bar position as beat.sixteenth, or the count-in.
        const int step = engine.song.playheadStep.load();
        juce::String where;
        if (engine.countInBeatsLeft.load() > 0)
            where = "Count-in " + juce::String (engine.countInBeatsLeft.load());
        else if (engine.transport.playing.load() && step >= 0)
            where = "Beat " + juce::String (step / 4 + 1) + "." + juce::String (step % 4 + 1);
        else
            where = "Stopped";
        if (position.getText() != where)
            position.setText (where, juce::dontSendNotification);

        const bool drums = engine.selected().playsDrums();
        const juce::String helpText = drums ? "Click a cell to switch that drum on that step. Click a drum's name to pick it for the knobs and pads."
                                            : "Click to add a note, click a note to take it out, drag a note sideways to change how long it plays. Scroll for higher or lower notes.";
        if (help.getText() != helpText)
            help.setText (helpText, juce::dontSendNotification);

        playButton.setButtonText (engine.transport.playing.load() ? "Stop" : "Play");
        recordButton.setButtonText (recordingNotes() || engine.countInBeatsLeft.load() > 0 ? "Recording" : "Record");
        recordButton.setColour (juce::TextButton::buttonColourId, recordingNotes() ? juce::Colour (0xffb3261e) : Colours::panelEdge);

        if (isShowing())
            repaint();
    }

    AudioEngine& engine;
    juce::TextButton prevTrack { "<" }, nextTrack { ">" }, playButton { "Play" }, recordButton { "Record" };
    juce::Label title, position, help;
    juce::Rectangle<int> grid;
    int lowestNote = 36;
    bool followNotes = true;
    int shownTrack = -1, shownType = -1;
    int dragStep = -1, dragNote = -1;
    float dragStartLength = 0.5f;
    bool dragged = false;
};

} // namespace bizgroove
