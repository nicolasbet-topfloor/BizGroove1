#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace bizgroove
{

/** Two rows above the main views:
    - Tracks 1-16: click to select the track that the keys, knobs and pads
      play; the small M mutes it. Each shows its sound and a level light.
    - Scenes 1-16: click to switch scene (while playing, it starts at the next
      bar). Copy / Paste / Clear act on the current scene. Song mode plays the
      song chain instead of looping one scene. */
class SessionBar final : public juce::Component, private juce::Timer
{
public:
    explicit SessionBar (AudioEngine& engine);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> trackCell (int t) const;
    juce::Rectangle<int> muteArea (int t) const;
    juce::Rectangle<int> sceneCell (int s) const;

    AudioEngine& engine;
    juce::Rectangle<int> tracksRow, scenesRow;
    juce::Label tracksLabel, scenesLabel;
    juce::TextButton copyButton { "Copy" }, pasteButton { "Paste" }, clearButton { "Clear" };
    juce::ToggleButton songModeButton { "Song mode" };
    int copiedScene = -1;
    bool blink = false;
    int ticks = 0;
};

} // namespace bizgroove
