#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace bizgroove
{

/** The grid inside the timeline: one row per track, one column per bar.
    Each cell shows the scene whose pattern that track plays in that bar.
    Click or drag to paint the chosen scene; right-click or Alt-click to clear. */
class TimelineGrid final : public juce::Component
{
public:
    explicit TimelineGrid (AudioEngine& e) : engine (e) {}

    static constexpr int labelWidth = 92, headerHeight = 22, rowHeight = 24, barWidth = 34;

    int brush = 0;   // scene to paint, or -1 for silence

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void updateSize();

private:
    void paintAt (juce::Point<int> p, bool clear);
    AudioEngine& engine;
};

/** The timeline editor: brush, length, copy-from-chain, and the grid. */
class TimelineView final : public juce::Component, private juce::Timer
{
public:
    explicit TimelineView (AudioEngine& engine);
    void resized() override;

private:
    void timerCallback() override;

    AudioEngine& engine;
    juce::Label brushLabel, lengthLabel;
    juce::ComboBox brush;
    juce::Slider length { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::TextButton fromChain { "Copy the song chain here" }, clear { "Clear timeline" };
    TimelineGrid grid;
    juce::Viewport viewport;
    int lastBar = -1, lastLength = -1;
};

} // namespace bizgroove
