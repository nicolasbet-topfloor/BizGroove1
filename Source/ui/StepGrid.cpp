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

    for (int i = 0; i < Song::numSteps; ++i)
    {
        const auto r = cellBounds (i);
        const bool on = engine.isStepOn (i);

        g.setColour (on ? Colours::accent : (twoRows ? juce::Colour (0xff3a3c42) : Colours::panel));
        g.fillRoundedRectangle (r, twoRows ? 4.0f : 6.0f);

        g.setColour (i == playhead ? Colours::playhead : Colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, i == playhead ? 3.0f : 1.0f);

        g.setColour (on ? juce::Colours::black : Colours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        if (! drums)
            g.drawText (juce::MidiMessage::getMidiNoteName (engine.getStepNote (i), true, true, 4),
                        r.reduced (2.0f, 6.0f), juce::Justification::centredBottom);

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
    for (int i = 0; i < Song::numSteps; ++i)
        hash = hash * 31u + (engine.isStepOn (i) ? 1u : 0u) + (std::uint32_t) engine.getStepNote (i) * 7u;
    hash = hash * 31u + (std::uint32_t) engine.selectedTrack.load() * 131u + (std::uint32_t) engine.song.currentScene.load();
    if (hash != lastHash)
    {
        lastHash = hash;
        repaint();
    }
}

} // namespace bizgroove
