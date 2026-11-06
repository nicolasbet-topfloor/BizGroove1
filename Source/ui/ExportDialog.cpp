#include "ExportDialog.h"
#include "LookAndFeel.h"

namespace bizgroove
{

ExportComponent::ExportComponent (AudioEngine& e, std::function<void (const ExportOptions&)> cb)
    : engine (e), onExport (std::move (cb))
{
    auto heading = [this] (juce::Label& l, const juce::String& t) {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, Colours::text);
        addAndMakeVisible (l);
    };
    heading (whatLabel, "What to export");
    heading (filesLabel, "Files");
    heading (qualityLabel, "Quality");

    for (auto* b : { &song, &scene })
    {
        b->setRadioGroupId (3);
        b->onClick = [this] { updateSummary(); };
    }
    song.setToggleState (true, juce::dontSendNotification);
    for (int b : { 1, 2, 4, 8, 16, 32 })
        sceneBars.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    sceneBars.setSelectedId (4, juce::dontSendNotification);
    sceneBars.onChange = [this] { updateSummary(); };

    mixdown.setToggleState (true, juce::dontSendNotification);
    for (auto* b : { &mixdown, &perTrack, &midi })
        b->onClick = [this] { updateSummary(); };

    rate.addItem ("44.1 kHz (CD)", 1);
    rate.addItem ("48 kHz (video, most mastering)", 2);
    rate.setSelectedId (2, juce::dontSendNotification);
    depth.addItem ("24-bit", 1);
    depth.addItem ("32-bit float", 2);
    depth.setSelectedId (1, juce::dontSendNotification);
    tail.addItem ("No tail", 1);
    tail.addItem ("2 seconds of tail for reverb and echoes", 2);
    tail.addItem ("5 seconds of tail", 3);
    tail.setSelectedId (2, juce::dontSendNotification);

    for (auto* c : { (juce::Component*) &song, (juce::Component*) &scene, (juce::Component*) &sceneBars,
                     (juce::Component*) &mixdown, (juce::Component*) &perTrack, (juce::Component*) &midi,
                     (juce::Component*) &rate, (juce::Component*) &depth, (juce::Component*) &tail })
    {
        if (auto* tb = dynamic_cast<juce::ToggleButton*> (c))
            tb->setColour (juce::ToggleButton::textColourId, Colours::text);
        addAndMakeVisible (c);
    }

    summary.setColour (juce::Label::textColourId, Colours::textDim);
    summary.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (summary);

    exportButton.onClick = [this] {
        ExportOptions o;
        o.wholeSong = song.getToggleState();
        o.sceneBars = sceneBars.getSelectedId();
        o.mixdown = mixdown.getToggleState();
        o.perTrack = perTrack.getToggleState();
        o.midi = midi.getToggleState();
        o.sampleRate = rate.getSelectedId() == 1 ? 44100.0 : 48000.0;
        o.bitDepth = depth.getSelectedId() == 1 ? 24 : 32;
        o.tailSeconds = tail.getSelectedId() == 1 ? 0.0 : tail.getSelectedId() == 2 ? 2.0 : 5.0;
        if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (0);
        if (onExport)
            onExport (o);
    };
    cancel.onClick = [this] { if (auto* w = findParentComponentOfClass<juce::DialogWindow>()) w->exitModalState (0); };
    addAndMakeVisible (exportButton);
    addAndMakeVisible (cancel);

    updateSummary();
    setSize (560, 470);
}

void ExportComponent::updateSummary()
{
    const int bars = song.getToggleState() ? engine.song.songLengthBars() : sceneBars.getSelectedId();
    const double seconds = bars * 4.0 * 60.0 / engine.transport.bpm.load();
    juce::String text = juce::String (bars) + (bars == 1 ? " bar" : " bars") + ", "
                        + juce::String ((int) seconds / 60) + ":" + juce::String ((int) seconds % 60).paddedLeft ('0', 2)
                        + " at " + juce::String (juce::roundToInt (engine.transport.bpm.load())) + " BPM.";
    if (song.getToggleState())
        text << " The song plays " << (engine.song.useTimeline.load() ? "the timeline" : "the chain") << " (see the Song view).";
    text << " Tracks that are empty are skipped when exporting one file per track."
            " You will choose the folder next.";
    summary.setText (text, juce::dontSendNotification);
    sceneBars.setEnabled (scene.getToggleState());
    exportButton.setEnabled (mixdown.getToggleState() || perTrack.getToggleState() || midi.getToggleState());
}

void ExportComponent::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
}

void ExportComponent::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    whatLabel.setBounds (r.removeFromTop (24));
    song.setBounds (r.removeFromTop (26));
    auto row = r.removeFromTop (28);
    scene.setBounds (row.removeFromLeft (240));
    sceneBars.setBounds (row.removeFromLeft (110).reduced (0, 2));
    r.removeFromTop (10);

    filesLabel.setBounds (r.removeFromTop (24));
    mixdown.setBounds (r.removeFromTop (26));
    perTrack.setBounds (r.removeFromTop (26));
    midi.setBounds (r.removeFromTop (26));
    r.removeFromTop (10);

    qualityLabel.setBounds (r.removeFromTop (24));
    auto q = r.removeFromTop (28);
    rate.setBounds (q.removeFromLeft (250).reduced (0, 2));
    q.removeFromLeft (10);
    depth.setBounds (q.removeFromLeft (130).reduced (0, 2));
    tail.setBounds (r.removeFromTop (30).withWidth (390).reduced (0, 2));
    r.removeFromTop (8);

    auto buttons = r.removeFromBottom (34);
    exportButton.setBounds (buttons.removeFromRight (120).reduced (0, 2));
    buttons.removeFromRight (8);
    cancel.setBounds (buttons.removeFromRight (100).reduced (0, 2));
    summary.setBounds (r);
}

} // namespace bizgroove
