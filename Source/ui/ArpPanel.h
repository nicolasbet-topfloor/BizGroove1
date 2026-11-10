#pragma once

#include "LaunchkeyPanel.h"   // PanelKnob
#include "LookAndFeel.h"

namespace bizgroove
{

/** The arpeggiator: on or off, and how it plays the notes you hold. Opened
    from the panel's Arp button. Hold keeps the notes going without the keys. */
class ArpPanel final : public juce::Component, private juce::Timer
{
public:
    ArpPanel (AudioEngine& e, const ControlTargets& t) : engine (e)
    {
        onButton.setClickingTogglesState (false);
        onButton.setWantsKeyboardFocus (false);
        onButton.setTooltip ("Switch the arpeggiator on or off. With Hold on, the notes keep going after you let go of the keys.");
        onButton.onClick = [this] { engine.arp.on.store (! engine.arp.on.load()); };
        addAndMakeVisible (onButton);

        holdButton.setWantsKeyboardFocus (false);
        holdButton.setTooltip ("Hold: the arpeggio keeps playing after you let go of the keys. Play again for new notes.");
        holdButton.onClick = [this] { engine.hold.store (! engine.hold.load()); };
        addAndMakeVisible (holdButton);

        const std::array<SynthKnob, 5> list { {
            { "Mode",    "arp.mode",    "How the held notes are played: up, down, up and down, down and up, at random, in the order you played them, all together (chord), from the outside in (converge) or from the middle out (diverge)." },
            { "Rate",    "arp.rate",    "How fast the notes come, in time with the tempo: from quarter notes to 32nds, with triplets." },
            { "Octaves", "arp.octaves", "Over how many octaves the pattern climbs: 1 to 4." },
            { "Length",  "arp.gate",    "How long each note lasts: short and plucked, or long and joined." },
            { "Swing",   "arp.swing",   "Pushes every second note later, for a shuffle." }
        } };
        for (size_t i = 0; i < list.size(); ++i)
        {
            knobs[i] = std::make_unique<PanelKnob> (t);
            knobs[i]->setKnob (list[i]);
            addAndMakeVisible (*knobs[i]);
        }
        setSize (110 + 5 * 104, 128);
        refreshButtons();
        startTimerHz (15);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (110).reduced (8, 18);
        onButton.setBounds (left.removeFromTop (40));
        left.removeFromTop (10);
        holdButton.setBounds (left.removeFromTop (34));
        for (auto& k : knobs)
            k->setBounds (r.removeFromLeft (104));
    }

private:
    void refreshButtons()
    {
        const bool on = engine.arp.on.load(), held = engine.hold.load();
        onButton.setButtonText (on ? "Arp on" : "Arp off");
        onButton.setColour (juce::TextButton::buttonColourId, on ? Colours::accent : Colours::panelEdge);
        onButton.setColour (juce::TextButton::textColourOffId, on ? juce::Colours::black : Colours::text);
        holdButton.setColour (juce::TextButton::buttonColourId, held ? Colours::accent : Colours::panelEdge);
        holdButton.setColour (juce::TextButton::textColourOffId, held ? juce::Colours::black : Colours::text);
        shownOn = on;
        shownHold = held;
    }

    void timerCallback() override
    {
        if (engine.arp.on.load() != shownOn || engine.hold.load() != shownHold)
            refreshButtons();
        for (auto& k : knobs)
            k->refresh();
    }

    AudioEngine& engine;
    juce::TextButton onButton { "Arp off" }, holdButton { "Hold" };
    std::array<std::unique_ptr<PanelKnob>, 5> knobs;
    bool shownOn = false, shownHold = false;
};

} // namespace bizgroove
