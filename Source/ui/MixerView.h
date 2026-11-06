#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace bizgroove
{

/** A small knob with a label above and its value below, bound to an atomic
    through get/set functions. */
class MixKnob final : public juce::Component, public juce::SettableTooltipClient
{
public:
    MixKnob (const juce::String& label, double min, double max, double reset,
             std::function<float()> get, std::function<void (float)> set,
             std::function<juce::String (float)> show);

    void refresh();
    void resized() override;

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label name, value;
    std::function<float()> get;
    std::function<void (float)> set;
    std::function<juce::String (float)> show;
};

/** One mixer channel: sound name, level meter, volume fader, mute and solo,
    pan, a three-band EQ, and the reverb and delay sends. */
class ChannelView final : public juce::Component
{
public:
    ChannelView (AudioEngine& engine, int track);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    AudioEngine& engine;
    const int track;
    juce::Label title;
    juce::OwnedArray<MixKnob> knobs;
    juce::Slider fader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::TextButton mute { "M" }, solo { "S" };
    juce::Rectangle<int> meter;
    float shownLevel = 0.0f;
};

/** The Mixer view: 16 channels and the master section. */
class MixerView final : public juce::Component, private juce::Timer
{
public:
    explicit MixerView (AudioEngine& engine);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AudioEngine& engine;
    juce::OwnedArray<ChannelView> channels;

    // Master section
    juce::Label masterTitle;
    juce::OwnedArray<MixKnob> masterKnobs;
    juce::ComboBox delayTime;
    juce::Label delayTimeLabel;
    juce::Slider masterFader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Rectangle<int> masterArea;
};

} // namespace bizgroove
