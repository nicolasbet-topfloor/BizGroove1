#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"
#include "TimelineView.h"

namespace bizgroove
{

/** The song, in two forms:
    - Chain: a numbered list of scenes, each played for a number of bars.
    - Timeline: a grid of tracks by bars, each cell naming a scene.
    Song mode plays whichever one is chosen under "Song mode plays". */
class SongView final : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit SongView (AudioEngine& engine);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Row;
    friend class Row;

    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;
    void timerCallback() override;

    void addEntry();
    void removeEntry (int index);
    void moveEntry (int index, int delta);

    AudioEngine& engine;
    juce::Label title, help;
    juce::ToggleButton songMode { "Song mode" }, loop { "Loop the song" };
    juce::ListBox list { "song", this };
    juce::TextButton addButton { "Add the current scene" }, clearButton { "Clear song" };
    juce::Label total;
    juce::TextButton chainTab { "Chain" }, timelineTab { "Timeline" };
    juce::Label playsLabel;
    juce::ComboBox plays;
    TimelineView timeline { engine };
    void showTab (bool chain);
    int lastPosition = -1, lastLength = -1;
};

} // namespace bizgroove
