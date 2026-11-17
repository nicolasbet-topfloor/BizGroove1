#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"
#include "engine/ControlTargets.h"

namespace bizgroove
{

/** Sixteen steps of the selected track in the current scene: the bar chosen with
    the pager (a pattern can be up to four bars long). Click toggles a step; on
    synth tracks, scroll or drag up/down on a step to change its note (hold Shift
    to move by octaves). On drum tracks the steps belong to the selected drum.
    The playing step is outlined in teal.

    Parameter locks: right-click a step, or hold it down for a moment, and it is
    held for locking (outlined in yellow): every knob turned now locks its value on
    that step. Click the step again to let go. Steps with locks show a yellow dot. */
class StepGrid final : public juce::Component, private juce::Timer
{
public:
    StepGrid (AudioEngine& engine, const ControlTargets& targets);
    ~StepGrid() override;

    /** Two rows of eight, like the Launchkey's pads, instead of one row of 16. */
    void setTwoRows (bool shouldUseTwoRows) { twoRows = shouldUseTwoRows; repaint(); }

    /** The step held for locking (-1 = none). */
    int getLockStep() const { return lockStep; }
    void endLockEdit();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static juce::Colour lockColour() { return juce::Colour (0xffffd166); }

private:
    void timerCallback() override;
    int stepAt (juce::Point<float> position) const;   // index on the grid (0 .. 15)
    int stepOf (int index) const;                     // the pattern's step for a grid index
    juce::Rectangle<float> cellBounds (int index) const;
    void nudgeNote (int index, int semitones);
    void beginLockEdit (int step);
    Song::TrackPattern& pattern();

    AudioEngine& engine;
    const ControlTargets& targets;
    std::uint32_t lastHash = 0;
    int dragStep = -1;
    int dragStartNote = 0;
    bool dragged = false;
    bool twoRows = false;
    int lockStep = -1;
    juce::int64 pressTime = 0;
    bool pressBecameLock = false;
    int shownTrack = -1, shownScene = -1;
};

/** The length of the selected track's pattern: - and + by one step, and the number,
    which opens a menu of 1, 2, 3 and 4 bars (and of any number of steps). */
class LengthControl final : public juce::Component, private juce::Timer
{
public:
    explicit LengthControl (AudioEngine& engine);
    void resized() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onChange;

private:
    void timerCallback() override;
    void setLength (int steps);
    Song::TrackPattern& pattern();

    AudioEngine& engine;
    juce::TextButton minus { "-" }, value, plus { "+" };
    bool compact = false;
};

/** Beside the step pads: which bar of the pattern they show (1 to 4; bars past the
    pattern's length are greyed), and the pattern's length. */
class StepPager final : public juce::Component, private juce::Timer
{
public:
    explicit StepPager (AudioEngine& engine);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void refresh();

    AudioEngine& engine;
    std::array<juce::TextButton, 4> pages;
    LengthControl length { engine };
    juce::Rectangle<int> barsLabel, lengthLabel;
};

} // namespace bizgroove
