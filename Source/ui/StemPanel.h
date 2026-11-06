#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace bizgroove
{

/** One stem slot: load button (or drop a file), volume fader, filter knob,
    mute and solo, and a loop position bar. */
class StemStrip final : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    StemStrip (AudioEngine& engine, int index, juce::AudioFormatManager& formats);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pulls state written by controllers into the widgets. Called by the panel's timer. */
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    bool loadFile (const juce::File& file, juce::String& error);

private:
    void chooseFile();

    AudioEngine& engine;
    const int index;
    juce::AudioFormatManager& formats;
    StemDeck::Stem& stem;

    juce::Label title, fileLabel;
    juce::TextButton loadButton { "Load" }, clearButton { "x" };
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    juce::Slider volume { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider filter { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label filterLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> positionBar;
    float shownFraction = 0.0f, shownLevel = 0.0f;
    bool dragHover = false;
};

/** The stem deck: four strips side by side. */
class StemPanel final : public juce::Component, private juce::Timer
{
public:
    explicit StemPanel (AudioEngine& engine);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::AudioFormatManager formats;
    juce::Label header, guide;
    juce::OwnedArray<StemStrip> strips;
};

} // namespace bizgroove
