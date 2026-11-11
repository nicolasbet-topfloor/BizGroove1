#pragma once

#include "LaunchkeyPanel.h"   // PanelKnob
#include "LookAndFeel.h"

namespace bizgroove
{

/** The selected synth's Oscillators section: two oscillators of its own,
    mixed with the synth's sound, and how loud that sound itself is. Opened
    from the panel's Oscillators button. */
class OscPanel final : public juce::Component, private juce::Timer
{
public:
    OscPanel (AudioEngine& e, const ControlTargets& t) : engine (e), targets (t)
    {
        mainKnob = std::make_unique<PanelKnob> (t);
        mainKnob->setKnob ({ "Synth's own", "osc.main", "How loud the synth's own sound is. Turn it down to hear only the two oscillators." });
        addAndMakeVisible (*mainKnob);

        static const char* labels[6] = { "Wave", "Octave", "Semitone", "Fine", "Width", "Level" };
        static const char* ids[6] = { "wave", "octave", "semi", "fine", "width", "level" };
        static const char* help[6] = {
            "The wave: sine (pure), triangle (soft), saw (bright, buzzy), square (hollow), pulse (thin, nasal, set by Width) or noise.",
            "Plays the oscillator up to two octaves lower or higher.",
            "Tunes the oscillator up to an octave in semitones: +7 for a fifth, +12 for an octave.",
            "Fine tuning in cents: a little out of tune makes the sound beat and thicken.",
            "The width of the pulse wave, from thin to square.",
            "How loud this oscillator is. At 0 it is off." };
        for (int n = 0; n < 2; ++n)
        {
            auto& row = rows[(size_t) n];
            row.title.setText ("Oscillator " + juce::String (n + 1), juce::dontSendNotification);
            row.title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            row.title.setColour (juce::Label::textColourId, Colours::text);
            addAndMakeVisible (row.title);
            for (size_t k = 0; k < 6; ++k)
            {
                row.knobs[k] = std::make_unique<PanelKnob> (t);
                row.knobs[k]->setKnob ({ labels[k], "osc" + juce::String (n + 1) + "." + ids[k], help[k] });
                addAndMakeVisible (*row.knobs[k]);
            }
        }

        syncButton.setTooltip ("Sync: oscillator 2 restarts every time oscillator 1 does. Tune oscillator 2 up for a hard, tearing sound.");
        ringButton.setTooltip ("Ring: oscillator 2 is multiplied by oscillator 1, for bell-like, metallic tones.");
        syncButton.onClick = [this] { targets.applyButton ("osc.sync", true); };
        ringButton.onClick = [this] { targets.applyButton ("osc.ring", true); };
        for (auto* b : { &syncButton, &ringButton })
        {
            b->setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        }
        setSize (labelWidth + 7 * knobWidth, 2 * rowHeight + 10);
        startTimerHz (15);
        refreshButtons();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        for (int n = 0; n < 2; ++n)
        {
            auto line = r.removeFromTop (rowHeight);
            auto left = line.removeFromLeft (labelWidth).reduced (6, 8);
            auto& row = rows[(size_t) n];
            row.title.setBounds (left.removeFromTop (22));
            for (auto& k : row.knobs)
                k->setBounds (line.removeFromLeft (knobWidth));
            // The last column: the synth's own sound on the first row, sync and ring on the second.
            if (n == 0)
                mainKnob->setBounds (line);
            else
            {
                auto b = line.reduced (6, 20);
                syncButton.setBounds (b.removeFromTop (34));
                b.removeFromTop (10);
                ringButton.setBounds (b.removeFromTop (34));
            }
            r.removeFromTop (10);
        }
    }

private:
    static constexpr int labelWidth = 120, knobWidth = 100, rowHeight = 124;

    OscSettings* settings()
    {
        auto& tr = engine.selected();
        switch (tr.type.load())
        {
            case Track::testSynth: return &tr.synthParams.osc;
            case Track::poly:      return &tr.polyParams.osc;
            case Track::wavetable: return &tr.waveParams.osc;
            case Track::fm:        return &tr.fmParams.osc;
            case Track::noise:     return &tr.noiseParams.osc;
            case Track::voice:     return &tr.voiceParams.osc;
            case Track::vocoder:   return &tr.vocoderParams.osc;
            default:               return nullptr;
        }
    }

    void refreshButtons()
    {
        const auto* s = settings();
        const bool sync = s != nullptr && s->sync.load(), ring = s != nullptr && s->ring.load();
        syncButton.setColour (juce::TextButton::buttonColourId, sync ? Colours::accent : Colours::panelEdge);
        syncButton.setColour (juce::TextButton::textColourOffId, sync ? juce::Colours::black : Colours::text);
        ringButton.setColour (juce::TextButton::buttonColourId, ring ? Colours::accent : Colours::panelEdge);
        ringButton.setColour (juce::TextButton::textColourOffId, ring ? juce::Colours::black : Colours::text);
        shownSync = sync;
        shownRing = ring;
    }

    void timerCallback() override
    {
        const auto* s = settings();
        if (s == nullptr)
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
            return;
        }
        if (s->sync.load() != shownSync || s->ring.load() != shownRing)
            refreshButtons();
        mainKnob->refresh();
        for (auto& row : rows)
            for (auto& k : row.knobs)
                k->refresh();
    }

    struct Row
    {
        juce::Label title;
        std::array<std::unique_ptr<PanelKnob>, 6> knobs;
    };

    AudioEngine& engine;
    const ControlTargets& targets;
    std::unique_ptr<PanelKnob> mainKnob;
    std::array<Row, 2> rows;
    juce::TextButton syncButton { "Sync 2 to 1" }, ringButton { "Ring 1 x 2" };
    bool shownSync = false, shownRing = false;
};

} // namespace bizgroove
