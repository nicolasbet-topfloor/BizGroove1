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
        relabel();
        setSize (150 + 4 * knobWidth, 2 * rowHeight + 10);
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
    int shownTrack = -1;
};

} // namespace bizgroove
