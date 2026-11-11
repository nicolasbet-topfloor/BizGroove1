#pragma once

#include "LaunchkeyPanel.h"   // PanelKnob
#include "LookAndFeel.h"
#include "engine/Presets.h"
#include <map>

namespace bizgroove
{

/** An LFO's wave, with a dot where it is now. */
class LfoWaveView final : public juce::Component
{
public:
    void setLfo (const LfoSettings* l) { lfo = l; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::background);
        g.fillRoundedRectangle (r, 4.0f);
        if (lfo == nullptr)
            return;
        r = r.reduced (6.0f, 8.0f);
        g.setColour (Colours::panelEdge);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        const bool on = lfo->active();
        juce::Path p;
        constexpr int points = 160;
        for (int i = 0; i < points; ++i)
        {
            const double ph = (double) i / (double) (points - 1);
            const float x = r.getX() + r.getWidth() * (float) ph;
            const float y = r.getCentreY() - 0.45f * r.getHeight() * LfoRunner::shapeAt (lfo->shape.load(), ph);
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        g.setColour (on ? Colours::accent : Colours::textDim.withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (1.5f));

        // Where it is now.
        const float x = r.getX() + r.getWidth() * lfo->shownPhase.load();
        const float y = r.getCentreY() - 0.45f * r.getHeight() * lfo->shownValue.load();
        g.setColour (on ? Colours::playhead : Colours::textDim);
        g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        if (! on)
        {
            g.setColour (Colours::textDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (lfo->destIndex.load() < 0 ? "choose what it moves" : "turn up Amount", getLocalBounds().reduced (4), juce::Justification::bottomRight);
        }
    }

private:
    const LfoSettings* lfo = nullptr;
};

/** The selected track's two LFOs: what each one moves, its wave, speed,
    amount and fade in. While open, the controller's eight knobs turn them. */
class LfoPanel final : public juce::Component, private juce::Timer
{
public:
    LfoPanel (AudioEngine& e, const ControlTargets& t) : engine (e), targets (t), track (e.selectedTrack.load())
    {
        static const char* labels[4] = { "Shape", "Speed", "Amount", "Fade in" };
        static const char* ids[4] = { "shape", "rate", "depth", "fade" };
        static const char* help[4] = {
            "The wave: sine, triangle, saw up, saw down, square, random steps (sample and hold) or smooth random.",
            "How fast it moves. Left half: free, from very slow to 20 Hz. Right half: in time with the tempo, from 4 bars to 1/32.",
            "How far it moves the knob, either way: to the right it moves it up first, to the left down first. In the middle it is off.",
            "How long it takes to grow in after each note, for a vibrato that comes in late." };
        for (int n = 0; n < 2; ++n)
        {
            auto& row = rows[(size_t) n];
            row.title.setText ("LFO " + juce::String (n + 1), juce::dontSendNotification);
            row.title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            row.title.setColour (juce::Label::textColourId, Colours::text);
            addAndMakeVisible (row.title);
            addAndMakeVisible (row.wave);

            row.dest.setWantsKeyboardFocus (false);
            row.dest.setTooltip ("What this LFO moves: any knob of this track, its sound, oscillators, effects, volume or pan.");
            row.dest.onClick = [this, n] { chooseDestination (n); };
            addAndMakeVisible (row.dest);

            row.retrigger.setWantsKeyboardFocus (false);
            row.retrigger.setTooltip ("Restart: the wave starts again from the top on every note. Free: it keeps running (and plays in time with the song).");
            row.retrigger.onClick = [this, n] { auto& l = lfo (n); l.retrigger.store (! l.retrigger.load()); refreshButtons(); };
            addAndMakeVisible (row.retrigger);

            for (size_t k = 0; k < 4; ++k)
            {
                row.knobs[k] = std::make_unique<PanelKnob> (t);
                row.knobs[k]->setKnob ({ labels[k], "lfo" + juce::String (n + 1) + "." + ids[k], help[k] });
                addAndMakeVisible (*row.knobs[k]);
            }
        }
        hint.setText ("Controller: knobs 1-4 turn LFO 1, knobs 5-8 LFO 2", juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, Colours::textDim);
        hint.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (hint);

        engine.knobOverlay.store (AudioEngine::overlayLfo);
        engine.knobPages.store (1);
        engine.knobPage.store (1);

        setSize (labelWidth + waveWidth + 4 * knobWidth + 8, 2 * rowHeight + 10 + 26);
        refreshButtons();
        startTimerHz (30);
    }

    ~LfoPanel() override
    {
        if (engine.knobOverlay.load() == AudioEngine::overlayLfo)
        {
            engine.knobOverlay.store (AudioEngine::overlayNone);
            engine.knobPage.store (0);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        hint.setBounds (r.removeFromBottom (26).reduced (10, 2));
        for (auto& row : rows)
        {
            auto line = r.removeFromTop (rowHeight);
            auto left = line.removeFromLeft (labelWidth).reduced (6, 8);
            row.title.setBounds (left.removeFromTop (22));
            left.removeFromTop (6);
            row.dest.setBounds (left.removeFromTop (30));
            left.removeFromTop (8);
            row.retrigger.setBounds (left.removeFromTop (28));
            row.wave.setBounds (line.removeFromLeft (waveWidth).reduced (4, 14));
            for (auto& k : row.knobs)
                k->setBounds (line.removeFromLeft (knobWidth));
            r.removeFromTop (10);
        }
    }

private:
    static constexpr int labelWidth = 170, waveWidth = 170, knobWidth = 100, rowHeight = 124;

    LfoSettings& lfo (int n) { return engine.track (track).lfos[(size_t) n]; }

    void chooseDestination (int n)
    {
        // Every knob an LFO can move, by section; the sections of other instruments are left out.
        juce::PopupMenu menu;
        menu.addItem (1, "Nothing (LFO off)", true, lfo (n).destId.isEmpty());
        const auto& all = targets.all();
        const auto instrument = Presets::instrumentName (engine.track (track).type.load());
        std::map<juce::String, juce::PopupMenu> sections;
        std::vector<juce::String> order;
        for (size_t i = 0; i < all.size(); ++i)
        {
            const auto& tg = all[i];
            if (! ControlTargets::lfoCanMove (tg) || ! belongs (tg.group, instrument))
                continue;
            if (sections.find (tg.group) == sections.end())
                order.push_back (tg.group);
            sections[tg.group].addItem ((int) i + 100, tg.name, true, tg.id == lfo (n).destId);
        }
        for (const auto& g : order)
            menu.addSubMenu (g, sections[g]);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rows[(size_t) n].dest),
                            [this, n] (int result) {
                                if (result == 1)
                                    lfo (n).setDestination ({});
                                else if (result >= 100)
                                {
                                    auto& l = lfo (n);
                                    l.setDestination (targets.all()[(size_t) (result - 100)].id);
                                    if (std::abs (l.depth.load()) < 0.0005f)
                                        l.depth.store (0.5f);   // so it is heard straight away
                                }
                                targets.resolveLfos (engine);
                                refreshButtons();
                            });
    }

    /** The menu shows the sections of this track's instrument, and those every track has. */
    static bool belongs (const juce::String& group, const juce::String& instrument)
    {
        static const juce::StringArray everyTrack { "Tracks", "Effects", "Oscillators", "Vocoder" };
        if (everyTrack.contains (group))
            return true;
        static const juce::StringArray instruments { "Synth", "Test synth", "Drums", "Sampler", "Poly", "Wavetable", "FM", "External MIDI", "Noise", "Voice" };
        if (! instruments.contains (group))
            return true;
        return group == instrument || (instrument == "Test synth" && group == "Synth");
    }

    void refreshButtons()
    {
        for (int n = 0; n < 2; ++n)
        {
            auto& row = rows[(size_t) n];
            auto& l = lfo (n);
            const auto* target = targets.find (l.destId);
            row.dest.setButtonText (target != nullptr ? target->name : juce::String ("Moves: nothing"));
            row.retrigger.setButtonText (l.retrigger.load() ? "Restarts on each note" : "Free running");
            row.retrigger.setColour (juce::TextButton::buttonColourId, l.retrigger.load() ? Colours::accent : Colours::panelEdge);
            row.retrigger.setColour (juce::TextButton::textColourOffId, l.retrigger.load() ? juce::Colours::black : Colours::text);
            row.wave.setLfo (&l);
            row.shownDest = l.destId;
            row.shownRetrigger = l.retrigger.load();
        }
    }

    void timerCallback() override
    {
        if (engine.selectedTrack.load() != track)
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
            return;
        }
        bool changed = false;
        for (int n = 0; n < 2; ++n)
            changed = changed || lfo (n).destId != rows[(size_t) n].shownDest || lfo (n).retrigger.load() != rows[(size_t) n].shownRetrigger;
        if (changed)
            refreshButtons();
        for (auto& row : rows)
        {
            row.wave.repaint();
            for (auto& k : row.knobs)
                k->refresh();
        }
    }

    struct Row
    {
        juce::Label title;
        LfoWaveView wave;
        juce::TextButton dest, retrigger;
        std::array<std::unique_ptr<PanelKnob>, 4> knobs;
        juce::String shownDest;
        bool shownRetrigger = false;
    };

    AudioEngine& engine;
    const ControlTargets& targets;
    const int track;
    std::array<Row, 2> rows;
    juce::Label hint;
};

} // namespace bizgroove
