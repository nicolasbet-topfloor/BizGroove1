#include "SessionBar.h"
#include "LookAndFeel.h"
#include "engine/SynthLibrary.h"

namespace bizgroove
{

SessionBar::SessionBar (AudioEngine& e) : engine (e)
{
    for (auto* l : { &tracksLabel, &scenesLabel })
    {
        l->setColour (juce::Label::textColourId, Colours::textDim);
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    }
    tracksLabel.setText ("TRACKS", juce::dontSendNotification);
    scenesLabel.setText ("SCENES", juce::dontSendNotification);

    copyButton.setTooltip ("Copy the current scene");
    pasteButton.setTooltip ("Paste the copied scene over the current scene");
    clearButton.setTooltip ("Empty the current scene (all 16 tracks)");
    copyButton.onClick = [this] { copiedScene = engine.song.currentScene.load(); pasteButton.setEnabled (true); };
    pasteButton.onClick = [this] { if (copiedScene >= 0) engine.song.copyScene (copiedScene, engine.song.currentScene.load()); };
    clearButton.onClick = [this] { engine.song.clearScene (engine.song.currentScene.load()); };
    pasteButton.setEnabled (false);

    songModeButton.setTooltip ("On: play the song (the chain of scenes in the Song view). Off: loop the current scene.");
    songModeButton.setColour (juce::ToggleButton::textColourId, Colours::text);
    songModeButton.onClick = [this] { engine.song.songMode.store (songModeButton.getToggleState()); };

    for (auto* b : { (juce::Button*) &copyButton, (juce::Button*) &pasteButton, (juce::Button*) &clearButton,
                     (juce::Button*) &songModeButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    startTimerHz (15);
}

juce::Rectangle<int> SessionBar::trackCell (int t) const
{
    const int w = tracksRow.getWidth() / Song::numTracks;
    return { tracksRow.getX() + t * w, tracksRow.getY(), w - 4, tracksRow.getHeight() };
}

juce::Rectangle<int> SessionBar::muteArea (int t) const
{
    auto c = trackCell (t);
    return c.removeFromRight (20).removeFromBottom (18).reduced (2);
}

juce::Rectangle<int> SessionBar::sceneCell (int s) const
{
    const int w = scenesRow.getWidth() / Song::numScenes;
    return { scenesRow.getX() + s * w, scenesRow.getY(), w - 4, scenesRow.getHeight() };
}

void SessionBar::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    for (int t = 0; t < Song::numTracks; ++t)
    {
        if (muteArea (t).contains (p))
        {
            auto& m = engine.track (t).mute;
            m.store (! m.load());
            repaint();
            return;
        }
        if (trackCell (t).contains (p))
        {
            engine.selectedTrack.store (t);
            repaint();
            return;
        }
    }
    for (int s = 0; s < Song::numScenes; ++s)
    {
        if (sceneCell (s).contains (p))
        {
            engine.song.queueScene (s, engine.transport.playing.load());
            repaint();
            return;
        }
    }
}

void SessionBar::timerCallback()
{
    if (++ticks % 4 == 0)
        blink = ! blink;
    songModeButton.setToggleState (engine.song.songMode.load(), juce::dontSendNotification);
    repaint (tracksRow.expanded (2));
    repaint (scenesRow.expanded (2));
}

void SessionBar::paint (juce::Graphics& g)
{
    const auto& library = synthLibrary();
    const int sel = engine.selectedTrack.load();

    for (int t = 0; t < Song::numTracks; ++t)
    {
        const auto r = trackCell (t).toFloat();
        auto& tr = engine.track (t);
        const bool muted = tr.mute.load();
        const bool isSel = t == sel;

        g.setColour (isSel ? Colours::accent.withAlpha (0.18f) : Colours::panel);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (isSel ? Colours::accent : Colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, isSel ? 2.0f : 1.0f);

        // Level light.
        const float lvl = juce::jlimit (0.0f, 1.0f, tr.level.load() * 3.0f);
        if (lvl > 0.01f && ! muted)
        {
            g.setColour (Colours::playhead.withAlpha (0.3f + 0.7f * lvl));
            g.fillEllipse (r.getRight() - 12.0f, r.getY() + 6.0f, 6.0f, 6.0f);
        }

        const int type = juce::jlimit (0, (int) library.size() - 1, tr.type.load());
        g.setColour (muted ? Colours::textDim.withAlpha (0.5f) : Colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (t + 1), r.reduced (6.0f, 3.0f).removeFromTop (16.0f), juce::Justification::topLeft);
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (muted ? Colours::textDim.withAlpha (0.5f) : Colours::textDim);
        g.drawText (type == Track::drums ? "Drums" : type == Track::sampler ? "Sampler" : "Synth",
                    r.reduced (6.0f, 3.0f), juce::Justification::bottomLeft);

        const auto m = muteArea (t).toFloat();
        g.setColour (muted ? juce::Colour (0xffe05252) : Colours::background);
        g.fillRoundedRectangle (m, 3.0f);
        g.setColour (muted ? juce::Colours::black : Colours::textDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("M", m, juce::Justification::centred);
    }

    const int current = engine.song.currentScene.load();
    const int queued = engine.song.queuedScene.load();
    for (int s = 0; s < Song::numScenes; ++s)
    {
        const auto r = sceneCell (s).toFloat();
        const bool isCurrent = s == current;
        const bool empty = engine.song.sceneIsEmpty (s);

        g.setColour (isCurrent ? Colours::accent : Colours::panel);
        g.fillRoundedRectangle (r, 5.0f);
        if (s == queued)
        {
            g.setColour (blink ? Colours::accent : Colours::panelEdge);
            g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 2.0f);
        }
        else
        {
            g.setColour (Colours::panelEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        }
        g.setColour (isCurrent ? juce::Colours::black : (empty ? Colours::textDim.withAlpha (0.5f) : Colours::text));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (s + 1), r, juce::Justification::centred);
    }
}

void SessionBar::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (44);
    tracksLabel.setBounds (top.removeFromLeft (70));
    tracksRow = top;

    r.removeFromTop (6);
    auto bottom = r.removeFromTop (30);
    scenesLabel.setBounds (bottom.removeFromLeft (70));
    songModeButton.setBounds (bottom.removeFromRight (108));
    bottom.removeFromRight (10);
    clearButton.setBounds (bottom.removeFromRight (56).reduced (0, 2));
    bottom.removeFromRight (4);
    pasteButton.setBounds (bottom.removeFromRight (56).reduced (0, 2));
    bottom.removeFromRight (4);
    copyButton.setBounds (bottom.removeFromRight (56).reduced (0, 2));
    bottom.removeFromRight (12);
    scenesRow = bottom;
}

} // namespace bizgroove
