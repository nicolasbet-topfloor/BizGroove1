#pragma once

#include "LaunchkeyPanel.h"   // PanelKnob
#include "LookAndFeel.h"

namespace bizgroove
{

/** The selected track's two effect slots: choose an effect in each, then turn
    its four knobs. Opened from the panel's Effects button. */
class EffectsPanel final : public juce::Component, private juce::Timer
{
public:
    EffectsPanel (AudioEngine& e, const ControlTargets& t) : engine (e), targets (t)
    {
        for (int slot = 0; slot < 2; ++slot)
        {
            auto& row = rows[(size_t) slot];
            row.title.setText ("Effect " + juce::String (slot + 1), juce::dontSendNotification);
            row.title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            row.title.setColour (juce::Label::textColourId, Colours::text);
            addAndMakeVisible (row.title);

            for (int type = 0; type < InsertEffects::numTypes; ++type)
                row.menu.addItem (InsertEffects::name (type), type + 1);
            row.menu.setTooltip ("The effect in this slot. Slot 1 comes first, then slot 2, before the track's volume, pan and sends.");
            row.menu.onChange = [this, slot] {
                const int type = rows[(size_t) slot].menu.getSelectedId() - 1;
                if (type >= 0)
                    engine.selected().inserts[(size_t) slot].choose (type);
                relabel();
            };
            addAndMakeVisible (row.menu);

            for (auto& knob : row.knobs)
            {
                knob = std::make_unique<PanelKnob> (targets);
                addAndMakeVisible (*knob);
            }
        }
        // Performance effects on the whole mix: they play while held.
        perfTitle.setText ("Whole mix: hold to play", juce::dontSendNotification);
        perfTitle.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        perfTitle.setColour (juce::Label::textColourId, Colours::text);
        addAndMakeVisible (perfTitle);
        repeatButton.setTooltip ("Hold to loop the last slice of the music (its length is chosen beside it); let go to carry on.");
        stutterButton.setTooltip ("Hold to chop a tiny slice into a rattling stutter.");
        stopButton.setTooltip ("Hold to slow everything down to a halt, like a tape machine switched off; let go to come back.");
        repeatButton.onStateChange = [this] { holdChanged (repeatButton, engine.perform.repeat); };
        stutterButton.onStateChange = [this] { holdChanged (stutterButton, engine.perform.stutter); };
        stopButton.onStateChange = [this] { holdChanged (stopButton, engine.perform.tapeStop); };
        for (auto* b : { &repeatButton, &stutterButton, &stopButton })
        {
            b->setWantsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonOnColourId, Colours::accent);
            addAndMakeVisible (*b);
        }
        for (int d = 0; d < PerformSettings::numDivisions; ++d)
            division.addItem ("Repeat " + PerformSettings::divisionName (d), d + 1);
        division.setTooltip ("How long a slice Repeat loops, in step with the tempo.");
        division.onChange = [this] { engine.perform.repeatDivision.store (division.getSelectedId() - 1); };
        addAndMakeVisible (division);
        stopKnob = std::make_unique<PanelKnob> (targets);
        stopKnob->setKnob ({ "Stop time", "perform.stoptime", "How long Tape stop takes to slow to a halt." });
        addAndMakeVisible (*stopKnob);

        // The Vocoder effect: what it listens to (shown when a slot holds one).
        vocoderTitle.setText ("Vocoder listens to", juce::dontSendNotification);
        vocoderTitle.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        vocoderTitle.setColour (juce::Label::textColourId, Colours::text);
        addChildComponent (vocoderTitle);
        const std::array<SynthKnob, 5> listen { {
            { "Voice in",   "vocoder.voice",  "How much the vocoder listens to the Voice tracks (the speaking synth), all of them mixed." },
            { "Mic in",     "vocoder.mic",    "How much it listens to the sound card's input: sing or talk into a microphone." },
            { "Track in",   "vocoder.track",  "How much it listens to the track chosen with From track." },
            { "From track", "vocoder.source", "Which track Track in listens to (its sound before its effects and volume)." },
            { "Bands",      "vocoder.bands",  "How many bands it splits the voice into: few for a rough robot, many for clear words." }
        } };
        for (size_t i = 0; i < listen.size(); ++i)
        {
            vocoderKnobs[i] = std::make_unique<PanelKnob> (targets);
            vocoderKnobs[i]->setKnob (listen[i]);
            addChildComponent (*vocoderKnobs[i]);
        }

        relabel();
        startTimerHz (20);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        for (auto& row : rows)
        {
            auto line = r.removeFromTop (rowHeight);
            auto left = line.removeFromLeft (150).reduced (6, 12);
            row.title.setBounds (left.removeFromTop (22));
            left.removeFromTop (4);
            row.menu.setBounds (left.removeFromTop (28));
            for (auto& knob : row.knobs)
                knob->setBounds (line.removeFromLeft (knobWidth));
            r.removeFromTop (10);
        }

        if (showVocoder)
        {
            auto line = r.removeFromTop (rowHeight);
            vocoderTitle.setBounds (line.removeFromLeft (100).reduced (6, 12).removeFromTop (40));
            for (auto& knob : vocoderKnobs)
                knob->setBounds (line.removeFromLeft (knobWidth));
            r.removeFromTop (10);
        }

        perfTitle.setBounds (r.removeFromTop (20).reduced (6, 0));
        auto line = r.removeFromTop (84);
        for (auto* b : { &repeatButton, &stutterButton, &stopButton })
            b->setBounds (line.removeFromLeft (100).reduced (6, 22));
        division.setBounds (line.removeFromLeft (110).reduced (4, 27));
        stopKnob->setBounds (line.removeFromLeft (knobWidth));
    }

private:
    static constexpr int knobWidth = 100, rowHeight = 124;

    void relabel()
    {
        for (int slot = 0; slot < 2; ++slot)
        {
            auto& row = rows[(size_t) slot];
            const int type = engine.selected().inserts[(size_t) slot].type.load();
            row.menu.setSelectedId (type + 1, juce::dontSendNotification);
            const auto names = InsertEffects::knobNames (type);
            const auto prefix = "fx" + juce::String (slot + 1) + ".k";
            for (int k = 0; k < 4; ++k)
            {
                auto& knob = *row.knobs[(size_t) k];
                knob.setKnob ({ names[(size_t) k], prefix + juce::String (k + 1),
                                InsertEffects::name (type) + ": " + names[(size_t) k] });
                knob.setVisible (type != InsertEffects::off);
            }
            row.shownType = type;
        }
        shownTrack = engine.selectedTrack.load();

        bool vocoder = false;
        for (const auto& row : rows)
            vocoder = vocoder || row.shownType == InsertEffects::vocoder;
        showVocoder = vocoder;
        vocoderTitle.setVisible (vocoder);
        for (auto& knob : vocoderKnobs)
            knob->setVisible (vocoder);
        const int width = juce::jmax (150 + 4 * knobWidth, 100 + 5 * knobWidth);
        const int height = 2 * rowHeight + 10 + (vocoder ? rowHeight + 10 : 0) + 104;
        if (getWidth() != width || getHeight() != height)
            setSize (width, height);
        else
            resized();
    }

    void timerCallback() override
    {
        bool changed = engine.selectedTrack.load() != shownTrack;
        for (int slot = 0; slot < 2; ++slot)
            changed = changed || engine.selected().inserts[(size_t) slot].type.load() != rows[(size_t) slot].shownType;
        if (changed)
            relabel();
        for (auto& row : rows)
            for (auto& knob : row.knobs)
                knob->refresh();
        if (showVocoder)
            for (auto& knob : vocoderKnobs)
                knob->refresh();

        // Light the buttons, also when a controller switched them on.
        repeatButton.setToggleState (engine.perform.repeat.load(), juce::dontSendNotification);
        stutterButton.setToggleState (engine.perform.stutter.load(), juce::dontSendNotification);
        stopButton.setToggleState (engine.perform.tapeStop.load(), juce::dontSendNotification);
        division.setSelectedId (engine.perform.repeatDivision.load() + 1, juce::dontSendNotification);
        stopKnob->refresh();
    }

    /** A hold button: on while the mouse is down on it, off when let go. */
    void holdChanged (juce::Button& b, std::atomic<bool>& flag)
    {
        const bool down = b.isDown();
        if (down != heldDown[&b == &repeatButton ? 0 : (&b == &stutterButton ? 1 : 2)])
        {
            heldDown[&b == &repeatButton ? 0 : (&b == &stutterButton ? 1 : 2)] = down;
            flag.store (down);
        }
    }

    struct Row
    {
        juce::Label title;
        juce::ComboBox menu;
        std::array<std::unique_ptr<PanelKnob>, 4> knobs;
        int shownType = -1;
    };

    AudioEngine& engine;
    const ControlTargets& targets;
    std::array<Row, 2> rows;
    juce::Label perfTitle;
    juce::TextButton repeatButton { "Repeat" }, stutterButton { "Stutter" }, stopButton { "Tape stop" };
    juce::ComboBox division;
    std::unique_ptr<PanelKnob> stopKnob;
    juce::Label vocoderTitle;
    std::array<std::unique_ptr<PanelKnob>, 5> vocoderKnobs;
    bool showVocoder = false;
    bool heldDown[3] { false, false, false };
    int shownTrack = -1;
};

} // namespace bizgroove
