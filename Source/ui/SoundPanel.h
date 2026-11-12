#pragma once

#include "LaunchkeyPanel.h"   // PanelKnob
#include "LookAndFeel.h"
#include "engine/SynthLibrary.h"

namespace bizgroove
{

/** The envelope's shape, drawn from its Attack, Decay, Sustain and Release knobs. */
class EnvelopeView final : public juce::Component
{
public:
    EnvelopeView (const ControlTargets& t, const std::vector<SynthKnob>& knobs) : targets (t)
    {
        for (size_t i = 0; i < 4 && i < knobs.size(); ++i)
            ids[i] = knobs[i].target;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::background);
        g.fillRoundedRectangle (r, 4.0f);
        r = r.reduced (8.0f, 8.0f);

        auto value = [this] (int i) {
            const auto* t = targets.find (ids[(size_t) i]);
            return t != nullptr && t->get ? juce::jlimit (0.0f, 1.0f, t->get()) : 0.5f;
        };
        // Widths follow the knobs (which already turn on a time scale); the held part has a fixed width.
        const float a = 0.04f + value (0), d = 0.04f + value (1), s = value (2), rel = 0.04f + value (3), hold = 0.5f;
        const float unit = r.getWidth() / (a + d + hold + rel);
        const float x0 = r.getX(), x1 = x0 + a * unit, x2 = x1 + d * unit, x3 = x2 + hold * unit, x4 = r.getRight();
        const float top = r.getY(), bottom = r.getBottom(), sy = bottom - s * r.getHeight();

        juce::Path p;
        p.startNewSubPath (x0, bottom);
        p.lineTo (x1, top);
        p.quadraticTo (x1 + (x2 - x1) * 0.15f, sy, x2, sy);
        p.lineTo (x3, sy);
        p.quadraticTo (x3 + (x4 - x3) * 0.15f, bottom, x4, bottom);

        juce::Path fill (p);
        fill.lineTo (x0, bottom);
        fill.closeSubPath();
        g.setColour (Colours::accent.withAlpha (0.15f));
        g.fillPath (fill);
        g.setColour (Colours::accent);
        g.strokePath (p, juce::PathStrokeType (2.0f));

        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (10.0f));
        const float ly = bottom - 12.0f;
        g.drawText ("A", juce::Rectangle<float> (x0, ly, x1 - x0, 12.0f), juce::Justification::centred);
        g.drawText ("D", juce::Rectangle<float> (x1, ly, x2 - x1, 12.0f), juce::Justification::centred);
        g.drawText ("S", juce::Rectangle<float> (x2, ly, x3 - x2, 12.0f), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (x3, ly, x4 - x3, 12.0f), juce::Justification::centred);
    }

private:
    const ControlTargets& targets;
    std::array<juce::String, 4> ids;
};

/** Every setting of the loaded instrument in one window, in columns by section:
    source, filter, envelope (with its shape), modulation and output. While it is
    open the controller's eight knobs turn the highlighted sections, and the
    Track arrows move to the next ones. */
class SoundPanel final : public juce::Component, private juce::Timer
{
public:
    SoundPanel (AudioEngine& e, const ControlTargets& t)
        : engine (e), targets (t), track (e.selectedTrack.load()), type (e.selected().type.load()),
          sections (soundSections (type))
    {
        const auto& library = synthLibrary();
        title.setText (library[(size_t) juce::jlimit (0, (int) library.size() - 1, type)].name + juce::String::fromUTF8 ("  \xc2\xb7  Sound settings"),
                       juce::dontSendNotification);
        title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, Colours::text);
        addAndMakeVisible (title);

        for (int s = 0; s < (int) sections.size(); ++s)
        {
            auto& section = sections[(size_t) s];
            auto* column = columns.add (new Column());
            column->header.setButtonText (section.title);
            column->header.setWantsKeyboardFocus (false);
            column->header.setTooltip ("Click to turn this section with the controller's eight knobs.");
            const int page = soundPageOfSection (sections, s);
            column->header.onClick = [this, page] { engine.knobPage.store (page); showPage(); };
            addAndMakeVisible (column->header);
            if (section.envelope)
            {
                column->envelope = std::make_unique<EnvelopeView> (targets, section.knobs);
                addAndMakeVisible (*column->envelope);
            }
            for (const auto& k : section.knobs)
            {
                auto* knob = column->knobs.add (new PanelKnob (targets));
                knob->setKnob (k);
                addAndMakeVisible (knob);
            }
        }

        hint.setColour (juce::Label::textColourId, Colours::textDim);
        hint.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (hint);

        int width = 2 * margin;
        for (const auto& section : sections)
            width += columnsFor (section) * knobWidth + gap;
        setSize (width - gap, titleHeight + headerHeight + envelopeHeight + maxRows * knobHeight + hintHeight + margin);

        engine.knobOverlay.store (AudioEngine::overlayMore);
        engine.knobPages.store (soundPageCount (sections));
        engine.knobPage.store (1);
        showPage();
        startTimerHz (30);
    }

    ~SoundPanel() override
    {
        if (engine.knobOverlay.load() == AudioEngine::overlayMore)
        {
            engine.knobOverlay.store (AudioEngine::overlayNone);
            engine.knobPage.store (0);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (margin, 0);
        title.setBounds (r.removeFromTop (titleHeight).reduced (2, 4));
        hint.setBounds (getLocalBounds().removeFromBottom (hintHeight + margin / 2).reduced (margin, 2));
        for (int s = 0; s < columns.size(); ++s)
        {
            auto& section = sections[(size_t) s];
            auto* column = columns[s];
            const int cols = columnsFor (section);
            auto c = r.removeFromLeft (cols * knobWidth);
            r.removeFromLeft (gap);
            column->area = c.withBottom (titleHeight + headerHeight + envelopeHeight + maxRows * knobHeight + 4);
            column->header.setBounds (c.removeFromTop (headerHeight).reduced (4, 3));
            if (column->envelope != nullptr)
                column->envelope->setBounds (c.removeFromTop (envelopeHeight).reduced (4, 4));
            for (int i = 0; i < column->knobs.size(); ++i)
                column->knobs[i]->setBounds (c.getX() + (i % cols) * knobWidth, c.getY() + (i / cols) * knobHeight, knobWidth, knobHeight);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::panel);   // solid, so the panel underneath does not show through
        for (int s = 0; s < columns.size(); ++s)
        {
            const bool lit = soundPageOfSection (sections, s) == shownPage;
            g.setColour (lit ? Colours::accent.withAlpha (0.10f) : Colours::text.withAlpha (0.03f));
            g.fillRoundedRectangle (columns[s]->area.toFloat().expanded (2.0f, 0.0f), 6.0f);
        }
    }

private:
    static constexpr int knobWidth = 100, knobHeight = 108, margin = 12, gap = 10, titleHeight = 30,
                         headerHeight = 30, envelopeHeight = 86, maxRows = 3, hintHeight = 22;

    /** Knobs side by side in a section's column: enough for three rows at most. */
    static int columnsFor (const SoundSection& s) { return juce::jmax (1, ((int) s.knobs.size() + maxRows - 1) / maxRows); }

    void showPage()
    {
        shownPage = engine.knobPage.load();
        for (int s = 0; s < columns.size(); ++s)
        {
            const bool lit = soundPageOfSection (sections, s) == shownPage;
            auto& b = columns[s]->header;
            b.setColour (juce::TextButton::buttonColourId, lit ? Colours::accent : Colours::panelEdge);
            b.setColour (juce::TextButton::textColourOffId, lit ? juce::Colours::black : Colours::text);
        }
        hint.setText ("Controller: knobs 1-8 turn the highlighted section" + juce::String (soundPageCount (sections) > 1 ? "s" : "")
                          + juce::String::fromUTF8 ("  \xc2\xb7  Track \xe2\x97\x80 \xe2\x96\xb6 changes page (")
                          + juce::String (shownPage) + " of " + juce::String (soundPageCount (sections)) + ")",
                      juce::dontSendNotification);
        repaint();
    }

    void timerCallback() override
    {
        // Another track or instrument: these knobs no longer apply.
        if (engine.selectedTrack.load() != track || engine.selected().type.load() != type)
        {
            stopTimer();
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
            return;
        }
        if (engine.knobPage.load() != shownPage)
            showPage();
        for (auto* column : columns)
        {
            for (auto* knob : column->knobs)
                knob->refresh();
            if (column->envelope != nullptr)
                column->envelope->repaint();
        }
    }

    struct Column
    {
        juce::TextButton header;
        std::unique_ptr<EnvelopeView> envelope;
        juce::OwnedArray<PanelKnob> knobs;
        juce::Rectangle<int> area;
    };

    AudioEngine& engine;
    const ControlTargets& targets;
    const int track, type;
    const std::vector<SoundSection>& sections;
    juce::Label title, hint;
    juce::OwnedArray<Column> columns;
    int shownPage = -1;
};

} // namespace bizgroove
