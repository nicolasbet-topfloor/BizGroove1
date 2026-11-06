#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace bizgroove
{

/** The 16 steps of the selected track in the current scene. Click toggles a
    step; on synth tracks, scroll or drag up/down on a step to change its note
    (hold Shift to move by octaves). On drum tracks the steps belong to the
    selected drum. The playing step is outlined in teal. */
class StepGrid final : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (AudioEngine& engine);

    /** Two rows of eight, like the Launchkey's pads, instead of one row of 16. */
    void setTwoRows (bool shouldUseTwoRows) { twoRows = shouldUseTwoRows; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    int stepAt (juce::Point<float> position) const;
    juce::Rectangle<float> cellBounds (int index) const;
    void nudgeNote (int index, int semitones);

    AudioEngine& engine;
    std::uint32_t lastHash = 0;
    int lastPlayhead = -1;
    int dragStep = -1;
    int dragStartNote = 0;
    bool dragged = false;
    bool twoRows = false;
};

} // namespace bizgroove
