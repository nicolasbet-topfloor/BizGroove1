#include "StemPanel.h"
#include "LookAndFeel.h"
#include "engine/Project.h"

namespace bizgroove
{

namespace
{
    void styleToggle (juce::TextButton& b, juce::Colour onColour)
    {
        b.setClickingTogglesState (true);
        b.setWantsKeyboardFocus (false);
        b.setColour (juce::TextButton::buttonOnColourId, onColour);
    }
}

// ============================================================== StemStrip

StemStrip::StemStrip (AudioEngine& e, int i, juce::AudioFormatManager& f)
    : engine (e), index (i), formats (f), stem (e.stems.stem (i))
{
    title.setText ("STEM " + juce::String (i + 1), juce::dontSendNotification);
    title.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (title);

    fileLabel.setText ("Drop an audio file", juce::dontSendNotification);
    fileLabel.setFont (juce::FontOptions (12.0f));
    fileLabel.setColour (juce::Label::textColourId, Colours::textDim);
    fileLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (fileLabel);

    loadButton.setWantsKeyboardFocus (false);
    loadButton.onClick = [this] { chooseFile(); };
    addAndMakeVisible (loadButton);

    clearButton.setWantsKeyboardFocus (false);
    clearButton.setTooltip ("Remove this stem");
    clearButton.onClick = [this] {
        engine.stems.clearStem (index);
        engine.stemFiles[(size_t) index] = {};
        repaint();
    };
    addAndMakeVisible (clearButton);

    styleToggle (muteButton, juce::Colour (0xffe05252));
    muteButton.setTooltip ("Mute");
    muteButton.onClick = [this] { stem.mute.store (muteButton.getToggleState()); };
    addAndMakeVisible (muteButton);

    styleToggle (soloButton, juce::Colour (0xfff2c94c));
    soloButton.setTooltip ("Solo");
    soloButton.onClick = [this] { stem.solo.store (soloButton.getToggleState()); };
    addAndMakeVisible (soloButton);

    volume.setRange (0.0, 1.0);
    volume.setValue (stem.volume.load(), juce::dontSendNotification);
    volume.setWantsKeyboardFocus (false);
    volume.setDoubleClickReturnValue (true, 0.8);
    volume.onValueChange = [this] { stem.volume.store ((float) volume.getValue()); };
    addAndMakeVisible (volume);

    filter.setRange (-1.0, 1.0);
    filter.setValue (0.0, juce::dontSendNotification);
    filter.setWantsKeyboardFocus (false);
    filter.setDoubleClickReturnValue (true, 0.0);
    filter.setTooltip ("Turn left for low-pass, right for high-pass. Double-click to reset.");
    filter.onValueChange = [this] {
        const float v = (float) filter.getValue();
        stem.filter.store (std::abs (v) < 0.04f ? 0.0f : v);
    };
    addAndMakeVisible (filter);

    filterLabel.setText ("FILTER", juce::dontSendNotification);
    filterLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    filterLabel.setColour (juce::Label::textColourId, Colours::textDim);
    filterLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (filterLabel);
}

bool StemStrip::loadFile (const juce::File& file, juce::String& error)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
    {
        error = "Could not read " + file.getFileName() + ".";
        return false;
    }
    if (! Project::loadStem (engine, index, file.getFileName(), data, formats, error))
    {
        error = file.getFileName() + ": " + error;
        return false;
    }
    return true;
}

void StemStrip::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load stem " + juce::String (index + 1),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   formats.getWildcardForAllFormats());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<StemStrip> (this)] (const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              juce::String error;
                              if (! safe->loadFile (fc.getResult(), error))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Couldn't load stem", error);
                          });
}

bool StemStrip::isInterestedInFileDrag (const juce::StringArray& files)
{
    dragHover = files.size() == 1;
    repaint();
    return dragHover;
}

void StemStrip::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    repaint();
    juce::String error;
    if (! loadFile (juce::File (files[0]), error))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load stem", error);
}

void StemStrip::refresh()
{
    // Show what is loaded (it may have been loaded by opening a project).
    const auto name = engine.stems.getStemName (index);
    juce::String text = "Drop an audio file";
    if (name.isNotEmpty())
    {
        const int bars = engine.stems.getStemBars (index);
        text = name + juce::String::fromUTF8 ("  \xc2\xb7  ") + juce::String (bars) + (bars == 1 ? " bar" : " bars");
    }
    if (fileLabel.getText() != text)
    {
        fileLabel.setText (text, juce::dontSendNotification);
        fileLabel.setColour (juce::Label::textColourId, name.isNotEmpty() ? Colours::text : Colours::textDim);
    }

    // Reflect controller moves in the widgets.
    if (! volume.isMouseButtonDown())
        volume.setValue (stem.volume.load(), juce::dontSendNotification);
    if (! filter.isMouseButtonDown())
        filter.setValue (stem.filter.load(), juce::dontSendNotification);
    muteButton.setToggleState (stem.mute.load(), juce::dontSendNotification);
    soloButton.setToggleState (stem.solo.load(), juce::dontSendNotification);

    const float fraction = stem.playFraction.load();
    const float level = stem.level.load();
    if (std::abs (fraction - shownFraction) > 0.002f || std::abs (level - shownLevel) > 0.01f)
    {
        shownFraction = fraction;
        shownLevel = level;
        repaint (positionBar.expanded (2));
    }
}

void StemStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (dragHover ? Colours::accent : Colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, dragHover ? 2.0f : 1.0f);

    // Loop position, brighter while the stem is sounding.
    const auto bar = positionBar.toFloat();
    g.setColour (Colours::background);
    g.fillRoundedRectangle (bar, 3.0f);
    if (engine.stems.hasAudio (index))
    {
        const float alpha = juce::jlimit (0.35f, 1.0f, 0.35f + shownLevel * 3.0f);
        g.setColour (Colours::playhead.withAlpha (alpha));
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * shownFraction)), 3.0f);
    }
}

void StemStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (18);
    clearButton.setBounds (top.removeFromRight (22));
    title.setBounds (top);
    fileLabel.setBounds (r.removeFromTop (18));
    r.removeFromTop (4);
    positionBar = r.removeFromTop (6);
    r.removeFromTop (8);

    auto buttons = r.removeFromBottom (26);
    loadButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 2));
    buttons.removeFromLeft (4);
    muteButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 2));
    buttons.removeFromLeft (4);
    soloButton.setBounds (buttons);
    r.removeFromBottom (6);

    volume.setBounds (r.removeFromLeft (36));
    auto knob = r.reduced (4, 0);
    filterLabel.setBounds (knob.removeFromTop (14));
    const int size = juce::jmin (knob.getWidth(), knob.getHeight(), 130);
    filter.setBounds (knob.withSizeKeepingCentre (size, size));
}

// ============================================================== StemPanel

StemPanel::StemPanel (AudioEngine& engine)
{
    formats.registerBasicFormats();

    header.setText (juce::String::fromUTF8 ("Stem deck  \xc2\xb7  loops follow the tempo"), juce::dontSendNotification);
    header.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (header);

    for (int i = 0; i < StemDeck::numStems; ++i)
        addAndMakeVisible (strips.add (new StemStrip (engine, i, formats)));

    guide.setText (juce::String::fromUTF8 (
        "How it works\n"
        "Drop a loop (WAV, AIFF, FLAC or OGG) on a slot, or press Load. Its length is rounded to 1, 2, 4, 8 or 16 bars "
        "and it is sped up or slowed down to stay in time with the tempo. Press Play and every loaded stem starts with the song.\n\n"
        "With a Native Instruments Traktor Kontrol F1 (in MIDI mode; teach it once in Controllers with Learn all):\n"
        "Faders: stem volume  \xc2\xb7  Knobs: filter (left muffles, right thins)  \xc2\xb7  Stop buttons: mute\n"
        "Pad rows, one column per stem: 1 solo  \xc2\xb7  2 restart on the next beat  \xc2\xb7  3 repeat one beat while held  "
        "\xc2\xb7  4 repeat a 1/16 while held"), juce::dontSendNotification);
    guide.setColour (juce::Label::textColourId, Colours::textDim);
    guide.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (guide);

    startTimerHz (30);
}

void StemPanel::paint (juce::Graphics& g)
{
    g.setColour (Colours::panel.darker (0.2f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void StemPanel::resized()
{
    auto r = getLocalBounds().reduced (16);
    header.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    auto stripsArea = r.removeFromTop (juce::jmin (r.getHeight(), 330));
    r.removeFromTop (14);
    guide.setBounds (r);
    r = stripsArea;
    const int w = (r.getWidth() - 12 * (StemDeck::numStems - 1)) / StemDeck::numStems;
    for (auto* s : strips)
    {
        s->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (12);
    }
}

void StemPanel::timerCallback()
{
    for (auto* s : strips)
        s->refresh();
}

} // namespace bizgroove
