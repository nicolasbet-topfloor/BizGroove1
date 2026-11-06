#include "TimelineView.h"
#include "LookAndFeel.h"
#include "engine/SynthLibrary.h"

namespace bizgroove
{

// ============================================================== TimelineGrid

void TimelineGrid::updateSize()
{
    const int bars = juce::jlimit (1, Song::maxBars, engine.song.timelineLength.load());
    setSize (labelWidth + bars * barWidth + 2, headerHeight + Song::numTracks * rowHeight + 2);
}

void TimelineGrid::paint (juce::Graphics& g)
{
    auto& song = engine.song;
    const int bars = juce::jlimit (1, Song::maxBars, song.timelineLength.load());
    const bool playingTimeline = engine.transport.playing.load() && song.songMode.load() && song.useTimeline.load();
    const int playBar = playingTimeline ? song.timelineBar.load() : -1;
    const auto& library = synthLibrary();

    g.fillAll (Colours::background);

    // Bar numbers, every bar; a stronger line every 4 bars.
    g.setFont (juce::FontOptions (11.0f));
    for (int b = 0; b < bars; ++b)
    {
        const int x = labelWidth + b * barWidth;
        if (b == playBar)
        {
            g.setColour (Colours::playhead.withAlpha (0.18f));
            g.fillRect (x, 0, barWidth, getHeight());
        }
        g.setColour (b % 4 == 0 ? Colours::text : Colours::textDim);
        g.drawText (juce::String (b + 1), x, 0, barWidth, headerHeight, juce::Justification::centred);
        g.setColour (b % 4 == 0 ? Colours::panelEdge.brighter (0.3f) : Colours::panelEdge);
        g.drawVerticalLine (x, (float) headerHeight, (float) getHeight());
    }

    for (int t = 0; t < Song::numTracks; ++t)
    {
        const int y = headerHeight + t * rowHeight;
        const int type = juce::jlimit (0, (int) library.size() - 1, engine.track (t).type.load());
        g.setColour (engine.selectedTrack.load() == t ? Colours::accent : Colours::text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (juce::String (t + 1) + "  " + library[(size_t) type].name, 6, y, labelWidth - 8, rowHeight,
                    juce::Justification::centredLeft);
        g.setColour (Colours::panelEdge);
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

        for (int b = 0; b < bars; ++b)
        {
            const int scene = song.timeline[(size_t) t][(size_t) b].load();
            if (scene < 0)
                continue;
            const auto cell = juce::Rectangle<int> (labelWidth + b * barWidth, y, barWidth, rowHeight).reduced (2).toFloat();
            const float hue = (float) scene / (float) Song::numScenes;
            g.setColour (juce::Colour::fromHSV (hue, 0.55f, 0.75f, 1.0f));
            g.fillRoundedRectangle (cell, 3.0f);
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (juce::String (scene + 1), cell, juce::Justification::centred);
        }
    }
}

void TimelineGrid::paintAt (juce::Point<int> p, bool clearCell)
{
    const int b = (p.x - labelWidth) / barWidth;
    const int t = (p.y - headerHeight) / rowHeight;
    if (p.x < labelWidth || p.y < headerHeight || ! juce::isPositiveAndBelow (t, Song::numTracks)
        || ! juce::isPositiveAndBelow (b, juce::jlimit (1, Song::maxBars, engine.song.timelineLength.load())))
        return;
    engine.song.timeline[(size_t) t][(size_t) b].store (clearCell ? -1 : brush);
    repaint();
}

void TimelineGrid::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < labelWidth && e.y >= headerHeight)
    {
        const int t = (e.y - headerHeight) / rowHeight;
        if (juce::isPositiveAndBelow (t, Song::numTracks))
            engine.selectedTrack.store (t);
        return;
    }
    paintAt (e.getPosition(), e.mods.isPopupMenu() || e.mods.isAltDown());
}

void TimelineGrid::mouseDrag (const juce::MouseEvent& e)
{
    paintAt (e.getPosition(), e.mods.isPopupMenu() || e.mods.isAltDown());
}

// ============================================================== TimelineView

TimelineView::TimelineView (AudioEngine& e) : engine (e), grid (e)
{
    brushLabel.setText ("Paint with", juce::dontSendNotification);
    lengthLabel.setText ("Length (bars)", juce::dontSendNotification);
    for (auto* l : { &brushLabel, &lengthLabel })
    {
        l->setColour (juce::Label::textColourId, Colours::textDim);
        addAndMakeVisible (l);
    }

    for (int s = 0; s < Song::numScenes; ++s)
        brush.addItem ("Scene " + juce::String (s + 1), s + 1);
    brush.addItem ("Silence", 100);
    brush.setSelectedId (1, juce::dontSendNotification);
    brush.setTooltip ("What clicking a cell puts there. Right-click (or Alt-click) a cell to clear it.");
    brush.setWantsKeyboardFocus (false);
    brush.onChange = [this] { grid.brush = brush.getSelectedId() == 100 ? -1 : brush.getSelectedId() - 1; };
    addAndMakeVisible (brush);

    length.setRange (1, Song::maxBars, 1);
    length.setValue (engine.song.timelineLength.load(), juce::dontSendNotification);
    length.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 24);
    length.setWantsKeyboardFocus (false);
    length.onValueChange = [this] { engine.song.timelineLength.store ((int) length.getValue()); grid.updateSize(); grid.repaint(); };
    addAndMakeVisible (length);

    fromChain.setTooltip ("Replace the timeline with the song chain: every track plays each chain scene for its bars.");
    fromChain.onClick = [this] { engine.song.copyChainToTimeline(); length.setValue (engine.song.timelineLength.load()); grid.updateSize(); grid.repaint(); };
    clear.onClick = [this] { engine.song.clearTimeline(); grid.repaint(); };
    for (auto* b : { &fromChain, &clear })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    grid.updateSize();
    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, true);
    addAndMakeVisible (viewport);

    startTimerHz (10);
}

void TimelineView::timerCallback()
{
    if (! isShowing())
        return;
    const int bar = engine.song.timelineBar.load();
    const int len = engine.song.timelineLength.load();
    if (len != lastLength)
    {
        length.setValue (len, juce::dontSendNotification);
        grid.updateSize();
        lastLength = len;
    }
    if (bar != lastBar || engine.transport.playing.load())
    {
        lastBar = bar;
        grid.repaint();
    }
}

void TimelineView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (30);
    brushLabel.setBounds (top.removeFromLeft (80));
    brush.setBounds (top.removeFromLeft (130).reduced (0, 2));
    top.removeFromLeft (20);
    lengthLabel.setBounds (top.removeFromLeft (96));
    length.setBounds (top.removeFromLeft (110));
    clear.setBounds (top.removeFromRight (120).reduced (0, 2));
    top.removeFromRight (8);
    fromChain.setBounds (top.removeFromRight (200).reduced (0, 2));
    r.removeFromTop (8);
    viewport.setBounds (r);
}

} // namespace bizgroove
