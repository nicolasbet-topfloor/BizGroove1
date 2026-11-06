#include "SongView.h"
#include "LookAndFeel.h"

namespace bizgroove
{

// ============================================================== Row

class SongView::Row final : public juce::Component
{
public:
    explicit Row (SongView& o) : owner (o)
    {
        for (int s = 1; s <= Song::numScenes; ++s)
            scene.addItem ("Scene " + juce::String (s), s);
        for (int r = 1; r <= 16; ++r)
            repeats.addItem (r == 1 ? "1 bar" : juce::String (r) + " bars", r);

        scene.onChange = [this] { owner.engine.song.chain[(size_t) index].scene.store (scene.getSelectedId() - 1); };
        repeats.onChange = [this] { owner.engine.song.chain[(size_t) index].repeats.store (repeats.getSelectedId()); };
        up.onClick = [this] { owner.moveEntry (index, -1); };
        down.onClick = [this] { owner.moveEntry (index, 1); };
        remove.onClick = [this] { owner.removeEntry (index); };
        up.setTooltip ("Move earlier");
        down.setTooltip ("Move later");
        remove.setTooltip ("Remove from the song");

        for (auto* c : { (juce::Component*) &scene, (juce::Component*) &repeats, (juce::Component*) &up,
                         (juce::Component*) &down, (juce::Component*) &remove })
        {
            c->setWantsKeyboardFocus (false);
            addAndMakeVisible (c);
        }
    }

    void update (int newIndex)
    {
        index = newIndex;
        scene.setSelectedId (owner.engine.song.chain[(size_t) index].scene.load() + 1, juce::dontSendNotification);
        repeats.setSelectedId (juce::jlimit (1, 16, owner.engine.song.chain[(size_t) index].repeats.load()), juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& song = owner.engine.song;
        const bool playingHere = song.songMode.load() && owner.engine.transport.playing.load()
                                 && song.chainPosition.load() == index;
        auto r = getLocalBounds().toFloat().reduced (0, 2);
        g.setColour (playingHere ? Colours::accent.withAlpha (0.2f) : Colours::panel);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (playingHere ? Colours::accent : Colours::text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (juce::String (index + 1), getLocalBounds().removeFromLeft (44), juce::Justification::centred);
        if (playingHere)
        {
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("playing, bar " + juce::String (song.chainRepeat.load() + 1),
                        getLocalBounds().withTrimmedLeft (430).withTrimmedRight (110), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 5);
        r.removeFromLeft (48);
        scene.setBounds (r.removeFromLeft (170));
        r.removeFromLeft (10);
        repeats.setBounds (r.removeFromLeft (120));
        remove.setBounds (r.removeFromRight (30));
        r.removeFromRight (6);
        down.setBounds (r.removeFromRight (30));
        r.removeFromRight (4);
        up.setBounds (r.removeFromRight (30));
    }

private:
    SongView& owner;
    int index = 0;
    juce::ComboBox scene, repeats;
    juce::TextButton up { juce::String::fromUTF8 ("\xe2\x96\xb2") }, down { juce::String::fromUTF8 ("\xe2\x96\xbc") }, remove { "x" };
};

// ============================================================== SongView

SongView::SongView (AudioEngine& e) : engine (e)
{
    title.setText ("Song", juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Colours::text);
    addAndMakeVisible (title);

    help.setText ("Chain: the song plays these scenes from top to bottom, each for the number of bars shown. "
                  "Timeline: each track plays the scene painted in each bar. Turn on Song mode to play the song; "
                  "turn it off to loop one scene while you build it.",
                  juce::dontSendNotification);

    for (auto* b : { &chainTab, &timelineTab })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (7);
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    chainTab.onClick = [this] { showTab (true); };
    timelineTab.onClick = [this] { showTab (false); };
    chainTab.setToggleState (true, juce::dontSendNotification);

    playsLabel.setText ("Song mode plays", juce::dontSendNotification);
    playsLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (playsLabel);
    plays.addItem ("the chain", 1);
    plays.addItem ("the timeline", 2);
    plays.setSelectedId (engine.song.useTimeline.load() ? 2 : 1, juce::dontSendNotification);
    plays.setWantsKeyboardFocus (false);
    plays.onChange = [this] { engine.song.useTimeline.store (plays.getSelectedId() == 2); };
    addAndMakeVisible (plays);
    addChildComponent (timeline);
    help.setColour (juce::Label::textColourId, Colours::textDim);
    help.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (help);

    songMode.onClick = [this] { engine.song.songMode.store (songMode.getToggleState()); };
    loop.setToggleState (engine.song.loopSong.load(), juce::dontSendNotification);
    loop.onClick = [this] { engine.song.loopSong.store (loop.getToggleState()); };
    loop.setTooltip ("On: start again after the last scene. Off: stop at the end.");
    for (auto* b : { &songMode, &loop })
    {
        b->setColour (juce::ToggleButton::textColourId, Colours::text);
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    list.setRowHeight (42);
    list.setColour (juce::ListBox::backgroundColourId, Colours::background);
    addAndMakeVisible (list);

    addButton.onClick = [this] { addEntry(); };
    clearButton.onClick = [this] {
        engine.song.chainLength.store (1);
        engine.song.chain[0].scene.store (engine.song.currentScene.load());
        engine.song.chain[0].repeats.store (1);
        list.updateContent();
        list.repaint();
    };
    for (auto* b : { &addButton, &clearButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    total.setColour (juce::Label::textColourId, Colours::textDim);
    total.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (total);

    startTimerHz (10);
}

int SongView::getNumRows()
{
    return juce::jlimit (0, Song::maxChain, engine.song.chainLength.load());
}

juce::Component* SongView::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    if (row >= getNumRows())
    {
        delete existing;
        return nullptr;
    }
    auto* r = dynamic_cast<Row*> (existing);
    if (r == nullptr)
    {
        delete existing;
        r = new Row (*this);
    }
    r->update (row);
    return r;
}

void SongView::addEntry()
{
    auto& song = engine.song;
    const int n = song.chainLength.load();
    if (n >= Song::maxChain)
        return;
    song.chain[(size_t) n].scene.store (song.currentScene.load());
    song.chain[(size_t) n].repeats.store (1);
    song.chainLength.store (n + 1);
    list.updateContent();
    list.scrollToEnsureRowIsOnscreen (n);
}

void SongView::removeEntry (int index)
{
    auto& song = engine.song;
    const int n = song.chainLength.load();
    if (n <= 1)
        return;   // a song keeps at least one scene
    for (int i = index; i < n - 1; ++i)
    {
        song.chain[(size_t) i].scene.store (song.chain[(size_t) i + 1].scene.load());
        song.chain[(size_t) i].repeats.store (song.chain[(size_t) i + 1].repeats.load());
    }
    song.chainLength.store (n - 1);
    list.updateContent();
    list.repaint();
}

void SongView::moveEntry (int index, int delta)
{
    auto& song = engine.song;
    const int other = index + delta;
    if (! juce::isPositiveAndBelow (other, song.chainLength.load()))
        return;
    auto& a = song.chain[(size_t) index];
    auto& b = song.chain[(size_t) other];
    const int s = a.scene.load(), r = a.repeats.load();
    a.scene.store (b.scene.load());  a.repeats.store (b.repeats.load());
    b.scene.store (s);               b.repeats.store (r);
    list.updateContent();
    list.repaint();
}

void SongView::showTab (bool chain)
{
    list.setVisible (chain);
    addButton.setVisible (chain);
    clearButton.setVisible (chain);
    total.setVisible (chain);
    timeline.setVisible (! chain);
}

void SongView::timerCallback()
{
    plays.setSelectedId (engine.song.useTimeline.load() ? 2 : 1, juce::dontSendNotification);
    songMode.setToggleState (engine.song.songMode.load(), juce::dontSendNotification);

    const int pos = engine.song.chainPosition.load() * 100 + engine.song.chainRepeat.load();
    const int len = engine.song.chainLength.load();
    if (pos != lastPosition || len != lastLength)
    {
        lastPosition = pos;
        lastLength = len;
        list.updateContent();
        list.repaint();
    }

    int bars = 0;
    for (int i = 0; i < len; ++i)
        bars += juce::jmax (1, engine.song.chain[(size_t) i].repeats.load());
    const double seconds = bars * 4.0 * 60.0 / engine.transport.bpm.load();
    const auto text = juce::String (len) + (len == 1 ? " scene, " : " scenes, ") + juce::String (bars)
                      + (bars == 1 ? " bar" : " bars") + juce::String::fromUTF8 ("  \xc2\xb7  ")
                      + juce::String ((int) seconds / 60) + ":" + juce::String ((int) seconds % 60).paddedLeft ('0', 2)
                      + " at " + juce::String (juce::roundToInt (engine.transport.bpm.load())) + " BPM";
    if (total.getText() != text)
        total.setText (text, juce::dontSendNotification);
}

void SongView::paint (juce::Graphics& g)
{
    g.setColour (Colours::panel.darker (0.2f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void SongView::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    auto top = r.removeFromTop (30);
    title.setBounds (top.removeFromLeft (70));
    chainTab.setBounds (top.removeFromLeft (90).reduced (0, 1));
    top.removeFromLeft (4);
    timelineTab.setBounds (top.removeFromLeft (90).reduced (0, 1));
    loop.setBounds (top.removeFromRight (140));
    top.removeFromRight (12);
    songMode.setBounds (top.removeFromRight (120));
    top.removeFromRight (12);
    plays.setBounds (top.removeFromRight (130).reduced (0, 2));
    playsLabel.setBounds (top.removeFromRight (110));
    help.setBounds (r.removeFromTop (40));
    r.removeFromTop (6);
    timeline.setBounds (r);

    auto bottom = r.removeFromBottom (34);
    addButton.setBounds (bottom.removeFromLeft (190).reduced (0, 2));
    bottom.removeFromLeft (8);
    clearButton.setBounds (bottom.removeFromLeft (110).reduced (0, 2));
    total.setBounds (bottom);
    r.removeFromBottom (8);

    list.setBounds (r);
}

} // namespace bizgroove
