#include "StepGrid.h"
#include "LookAndFeel.h"

namespace bizgroove
{

StepGrid::StepGrid (AudioEngine& e) : engine (e)
{
    startTimerHz (60);
}

juce::Rectangle<float> StepGrid::cellBounds (int index) const
{
    if (twoRows)
    {
        // Launchkey layout: steps 1-8 on the top row, 9-16 on the bottom row.
        const float gap = 10.0f;
        const float w = ((float) getWidth() - gap * 7.0f) / 8.0f;
        const float h = ((float) getHeight() - gap) / 2.0f;
        const int col = index % 8, row = index / 8;
        return { (float) col * (w + gap), (float) row * (h + gap), w, h };
    }

    const float gap = 6.0f;
    const float groupGap = 10.0f; // extra space between each group of four steps
    const float totalGaps = gap * (Song::numSteps - 1) + groupGap * 3;
    const float w = ((float) getWidth() - totalGaps) / (float) Song::numSteps;
    const float x = (float) index * (w + gap) + (float) (index / 4) * groupGap;
    return { x, 0.0f, w, (float) getHeight() };
}

int StepGrid::stepAt (juce::Point<float> position) const
{
    for (int i = 0; i < Song::numSteps; ++i)
        if (cellBounds (i).contains (position))
            return i;
    return -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    const int playhead = engine.song.playheadStep.load();
    const bool drums = engine.selected().playsDrums();

    const bool playing = engine.transport.playing.load() && playhead >= 0;
    auto& pattern = engine.song.scene (engine.song.currentScene.load()).tracks[(size_t) engine.selectedTrack.load()];

    for (int i = 0; i < Song::numSteps; ++i)
    {
        const auto r = cellBounds (i);
        const bool on = engine.isStepOn (i);
        const bool now = playing && i == playhead;

        g.setColour (on ? Colours::accent : (twoRows ? juce::Colour (0xff3a3c42) : Colours::panel));
        g.fillRoundedRectangle (r, twoRows ? 4.0f : 6.0f);

        // The step playing lights up, so the running light is easy to follow.
        if (now)
        {
            g.setColour (Colours::playhead.withAlpha (on ? 0.55f : 0.35f));
            g.fillRoundedRectangle (r, twoRows ? 4.0f : 6.0f);
        }
        g.setColour (now ? Colours::playhead : (i % 4 == 0 ? Colours::textDim.withAlpha (0.6f) : Colours::panelEdge));
        g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, now ? 3.0f : 1.0f);

        g.setColour (on ? juce::Colours::black : Colours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        if (! drums)
        {
            // The note, and "+2" when it is a chord of three.
            const auto& st = pattern.steps[(size_t) i];
            std::array<int, Song::maxChord> notes {};
            const int count = st.getNotes (notes);
            auto label = juce::MidiMessage::getMidiNoteName (notes[0], true, true, 4);
            if (count > 1)
                label << " +" << (count - 1);
            g.drawText (label, r.reduced (2.0f, 6.0f), juce::Justification::centredBottom);
        }
        else
        {
            // Every drum that plays on this step: a dot each, the selected drum's bigger.
            const float dot = juce::jmin (6.0f, (r.getWidth() - 8.0f) / 8.0f - 1.0f);
            const int picked = engine.selected().selectedDrum.load();
            for (int d = 0; d < Song::numDrums; ++d)
            {
                if (((pattern.drumLanes[(size_t) d].load() >> i) & 1u) == 0)
                    continue;
                const float x = r.getX() + 4.0f + (float) (d % 8) * (dot + 1.0f);
                const float y = r.getBottom() - 6.0f - (float) (2 - d / 8) * (dot + 1.0f);
                g.setColour (on ? juce::Colours::black.withAlpha (d == picked ? 0.9f : 0.5f)
                                : Colours::text.withAlpha (d == picked ? 0.9f : 0.5f));
                g.fillEllipse (x, y, dot, dot);
            }
        }

        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String (i + 1), r.reduced (2.0f, 6.0f), juce::Justification::centredTop);
    }
}

void StepGrid::nudgeNote (int index, int semitones)
{
    if (engine.selected().playsDrums())
        return;   // drum steps have no note
    engine.setStepNote (index, engine.getStepNote (index) + semitones);
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    dragStep = stepAt (e.position);
    dragged = false;
    if (dragStep >= 0)
        dragStartNote = engine.getStepNote (dragStep);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0)
        return;

    const int pixelsPerSemitone = e.mods.isShiftDown() ? 1 : 6;
    const int delta = -e.getDistanceFromDragStartY() / pixelsPerSemitone;
    if (std::abs (e.getDistanceFromDragStartY()) > 3)
        dragged = true;

    if (engine.selected().playsDrums())
        return;
    engine.setStepNote (dragStep, dragStartNote + (e.mods.isShiftDown() ? delta / 12 * 12 : delta));
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragStep >= 0 && ! dragged)
    {
        engine.toggleStep (dragStep);
        repaint();
    }
    dragStep = -1;
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int index = stepAt (e.position);
    if (index < 0 || std::abs (wheel.deltaY) < 1.0e-6f)
        return;

    const int direction = wheel.deltaY > 0 ? 1 : -1;
    nudgeNote (index, direction * (e.mods.isShiftDown() ? 12 : 1));
}

void StepGrid::timerCallback()
{
    // Repaint when the playhead moves or any shown step changes (e.g. from a controller).
    std::uint32_t hash = (std::uint32_t) engine.song.playheadStep.load() * 2654435761u;
    auto& pattern = engine.song.scene (engine.song.currentScene.load()).tracks[(size_t) engine.selectedTrack.load()];
    for (int i = 0; i < Song::numSteps; ++i)
    {
        const auto& st = pattern.steps[(size_t) i];
        hash = hash * 31u + (engine.isStepOn (i) ? 1u : 0u) + (std::uint32_t) engine.getStepNote (i) * 7u;
        for (const auto& c : st.chord)
            hash = hash * 31u + (std::uint32_t) c.load();
    }
    for (const auto& lane : pattern.drumLanes)
        hash = hash * 31u + lane.load();
    hash = hash * 31u + (engine.transport.playing.load() ? 1u : 0u) + (std::uint32_t) engine.selected().selectedDrum.load() * 17u;
    hash = hash * 31u + (std::uint32_t) engine.selectedTrack.load() * 131u + (std::uint32_t) engine.song.currentScene.load();
    if (hash != lastHash)
    {
        lastHash = hash;
        repaint();
    }
}

} // namespace bizgroove
