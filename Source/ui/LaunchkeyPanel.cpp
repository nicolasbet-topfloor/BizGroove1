#include "LaunchkeyPanel.h"
#include "LookAndFeel.h"
#include "engine/Project.h"
#include "engine/Presets.h"
#include "EffectsPanel.h"
#include "ArpPanel.h"

namespace bizgroove
{

namespace
{
    const juce::Colour body      { 0xff1b1c1f };
    const juce::Colour bodyEdge  { 0xff2e3036 };
    const juce::Colour printed   { 0xffa9abb2 };   // the small labels printed on the hardware
    const juce::Colour buttonOff { 0xff2a2c31 };

    void styleHardwareButton (juce::TextButton& b, const juce::String& tooltip, bool active)
    {
        b.setWantsKeyboardFocus (false);
        b.setColour (juce::TextButton::buttonColourId, buttonOff);
        b.setColour (juce::TextButton::textColourOffId, active ? Colours::text : printed.withAlpha (0.55f));
        b.setTooltip (tooltip);
        b.setEnabled (active);
    }

    void drawPrinted (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                      juce::Justification j = juce::Justification::centred, float size = 11.0f)
    {
        g.setColour (printed);
        g.setFont (juce::FontOptions (size));
        g.drawFittedText (text, area, j, 2);
    }
}

// ============================================================== PanelKnob

PanelKnob::PanelKnob (const ControlTargets& t) : targets (t)
{
    slider.setRange (0.0, 1.0);
    slider.setWantsKeyboardFocus (false);
    slider.onValueChange = [this] {
        targets.applyValue (targetId, (float) slider.getValue());
        value.setText (targets.describe (targetId), juce::dontSendNotification);
    };
    addAndMakeVisible (slider);

    name.setJustificationType (juce::Justification::centred);
    name.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    name.setColour (juce::Label::textColourId, Colours::text);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    // As printed on the hardware: small and grey, above the BizGroove name.
    printedName.setJustificationType (juce::Justification::centred);
    printedName.setFont (juce::FontOptions (11.0f));
    printedName.setColour (juce::Label::textColourId, juce::Colour (0xffa9abb2));
    printedName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (printedName);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (juce::FontOptions (12.0f));
    value.setColour (juce::Label::textColourId, Colours::textDim);
    value.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (value);
}

void PanelKnob::setKnob (const SynthKnob& knob)
{
    targetId = knob.target;
    name.setText (knob.label, juce::dontSendNotification);
    setTooltip (knob.help);
    slider.setTooltip (knob.help);
    slider.setDoubleClickReturnValue (true, targets.getValue (targetId));
    refresh();
}

void PanelKnob::refresh()
{
    const float v = targets.getValue (targetId);
    if (! slider.isMouseButtonDown() && std::abs (slider.getValue() - v) > 1.0e-4)
        slider.setValue (v, juce::dontSendNotification);
    const auto text = targets.describe (targetId);
    if (value.getText() != text)
        value.setText (text, juce::dontSendNotification);
}

void PanelKnob::resized()
{
    auto r = getLocalBounds();
    printedName.setBounds (r.removeFromTop (14));
    name.setBounds (r.removeFromTop (18));
    value.setBounds (r.removeFromBottom (16));
    slider.setBounds (r);
}

// ============================================================== TouchStrip

TouchStrip::TouchStrip (const juce::String& l, std::atomic<float>& v, bool springBack)
    : label (l), value (v), bipolar (springBack)
{
}

void TouchStrip::setFromY (float y)
{
    const float h = (float) getHeight() - 28.0f;
    const float pos = juce::jlimit (0.0f, 1.0f, 1.0f - (y - 4.0f) / h);   // 0 at the bottom, 1 at the top
    value.store (bipolar ? pos * 2.0f - 1.0f : pos);
    refresh();
}

void TouchStrip::mouseDown (const juce::MouseEvent& e) { setFromY (e.position.y); }
void TouchStrip::mouseDrag (const juce::MouseEvent& e) { setFromY (e.position.y); }

void TouchStrip::mouseUp (const juce::MouseEvent&)
{
    if (bipolar)
    {
        value.store (0.0f);   // the pitch strip springs back to the middle
        refresh();
    }
}

void TouchStrip::refresh()
{
    const float v = value.load();
    if (std::abs (v - shown) > 0.002f)
    {
        shown = v;
        repaint();
    }
}

void TouchStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto strip = r.withTrimmedBottom (24.0f).reduced (2.0f, 4.0f);

    g.setColour (juce::Colour (0xff121315));
    g.fillRoundedRectangle (strip, 5.0f);
    g.setColour (bodyEdge);
    g.drawRoundedRectangle (strip, 5.0f, 1.0f);

    const float pos = bipolar ? (shown + 1.0f) * 0.5f : shown;    // 0..1, bottom to top
    const float y = strip.getBottom() - pos * strip.getHeight();

    g.setColour (Colours::accent.withAlpha (0.35f));
    if (bipolar)
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (strip.getX() + 3, juce::jmin (y, strip.getCentreY()),
                                                                strip.getRight() - 3, juce::jmax (y, strip.getCentreY())));
    else
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (strip.getX() + 3, y, strip.getRight() - 3, strip.getBottom() - 3));

    if (bipolar)
    {
        g.setColour (printed.withAlpha (0.6f));
        g.drawHorizontalLine ((int) strip.getCentreY(), strip.getX() + 8, strip.getRight() - 8);
    }

    g.setColour (Colours::accent);
    g.fillRoundedRectangle (strip.getX() + 3, y - 2.0f, strip.getWidth() - 6, 4.0f, 2.0f);

    g.setColour (printed);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawFittedText (label, r.removeFromBottom (20.0f).toNearestInt().expanded (6, 0), juce::Justification::centred, 1, 0.9f);
}

// ============================================================== SymbolButton

void SymbolButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto fill = getToggleState() ? Colours::playhead.withAlpha (0.35f) : buttonOff;
    if (down) fill = fill.brighter (0.2f);
    else if (highlighted && isEnabled()) fill = fill.brighter (0.08f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (bodyEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    const auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.16f;
    if (symbol == Symbol::play)
    {
        juce::Path tri;
        tri.addTriangle (c.x - s * 0.8f, c.y - s, c.x - s * 0.8f, c.y + s, c.x + s, c.y);
        g.setColour (isEnabled() ? Colours::text : printed.withAlpha (0.5f));
        g.fillPath (tri);
    }
    else
    {
        g.setColour (juce::Colour (0xffe05252).withAlpha (isEnabled() ? 1.0f : 0.55f));
        g.fillEllipse (c.x - s, c.y - s, s * 2.0f, s * 2.0f);
    }
}

// ============================================================== LaunchkeyPanel

LaunchkeyPanel::LaunchkeyPanel (AudioEngine& e, const ControlTargets& t)
    : engine (e), targets (t),
      pitch ("Pitch", e.performance.pitchBend, true),
      modulation ("Modulation", e.performance.modWheel, false)
{
    pitch.setTooltip ("Pitch bend: drag up or down to bend the note by up to 2 semitones. It springs back when you let go.");
    modulation.setTooltip ("Modulation: drag up to add vibrato (a gentle wobble in pitch). It stays where you leave it.");
    addAndMakeVisible (pitch);
    addAndMakeVisible (modulation);

    const juce::String onDevice = "This button works on the Launchkey itself, not in BizGroove.";

    styleHardwareButton (shiftButton, onDevice, false);
    styleHardwareButton (transposeButton, "On the Launchkey, Transpose shifts its keys by semitones. " + onDevice, false);
    styleHardwareButton (octaveDown, "Moves the on-screen keyboard down an octave. (The Launchkey's own Octave buttons move its keys.)", true);
    styleHardwareButton (octaveUp, "Moves the on-screen keyboard up an octave. (The Launchkey's own Octave buttons move its keys.)", true);
    octaveDown.onClick = [this] { shiftOctave (-1); };
    octaveUp.onClick = [this] { shiftOctave (1); };
    for (auto* b : { &shiftButton, &transposeButton, &octaveDown, &octaveUp })
        addAndMakeVisible (b);

    // Printed beside the knobs on the Launchkey Mini MK3 (knobs 6-8 have none).
    const char* printedNames[8] = { "Tempo", "Swing", "Gate", "Mutate", "Deviate", "", "", "" };
    for (int i = 0; i < 8; ++i)
    {
        auto* k = knobs.add (new PanelKnob (targets));
        k->setPrinted (printedNames[i]);
        addAndMakeVisible (k);
    }

    styleHardwareButton (moreButton, "More settings for this synth", true);
    moreButton.setButtonText ("More sound settings");
    moreButton.onClick = [this] { showMore(); };
    addAndMakeVisible (moreButton);

    styleHardwareButton (fxButton, "Effects for this track: two slots, from chorus and drive to the moving filter, compressor and duck.", true);
    fxButton.onClick = [this] {
        juce::CallOutBox::launchAsynchronously (std::make_unique<EffectsPanel> (engine, targets), fxButton.getBounds(), this);
    };
    addAndMakeVisible (fxButton);

    pads.setTwoRows (true);
    addAndMakeVisible (pads);

    styleHardwareButton (launchButton, "Go to the next scene. While playing, it starts at the next bar.", true);
    styleHardwareButton (stopSoloMuteButton, "Mute or unmute the selected track.", true);
    launchButton.onClick = [this] { targets.applyButton ("scene.next", true); };
    stopSoloMuteButton.onClick = [this] { targets.applyButton ("track.mute", true); };

    trackPrev.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
    trackNext.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    styleHardwareButton (trackPrev, "Previous track", true);
    styleHardwareButton (trackNext, "Next track", true);
    trackPrev.onClick = [this] { targets.applyButton ("track.prev", true); };
    trackNext.onClick = [this] { targets.applyButton ("track.next", true); };
    addAndMakeVisible (trackPrev);
    addAndMakeVisible (trackNext);

    trackInfo.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    trackInfo.setColour (juce::Label::textColourId, Colours::accent);
    trackInfo.setJustificationType (juce::Justification::centredRight);
    trackInfo.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (trackInfo);
    addAndMakeVisible (launchButton);
    addAndMakeVisible (stopSoloMuteButton);

    styleHardwareButton (arpButton, "BizGroove's arpeggiator: plays the notes you hold one after another, in time with the song. "
                                    "Click to switch it on and choose how it plays. (The Launchkey has its own arp too.)", true);
    arpButton.onClick = [this] {
        juce::CallOutBox::launchAsynchronously (std::make_unique<ArpPanel> (engine, targets), arpButton.getBounds(), this);
    };
    styleHardwareButton (fixedChordButton, "On the Launchkey, Fixed Chord plays a stored chord from one key. " + onDevice, false);
    addAndMakeVisible (arpButton);
    addAndMakeVisible (fixedChordButton);

    playButton.setWantsKeyboardFocus (false);
    playButton.setTooltip ("Play / stop (space bar)");
    playButton.onClick = [this] { engine.transport.playing.store (! engine.transport.playing.load()); };
    addAndMakeVisible (playButton);

    recordButton.setWantsKeyboardFocus (false);
    recordButton.setTooltip ("Record. On a synth or Drums track: what you play on the keys or pads goes into the steps, "
                             "quantised to the nearest 1/16, with chords, how long and how hard each note is played. From a stop the "
                             "metronome counts in first. Press again, or Stop, to finish. On a Sampler: records a sound "
                             "(onto a drum pad: the Pad menu). The Launchkey's Record button does the same once assigned.");
    recordButton.onClick = [this] { engine.recorder.toggleRequested.store (true); };
    addAndMakeVisible (recordButton);

    styleHardwareButton (recSourceButton, "What Record captures: the sound card's input (a microphone or line in), "
                                          "or BizGroove's own sound (resampling). Click to switch.", true);
    recSourceButton.onClick = [this] {
        if (engine.selected().type.load() == Track::external)
        {
            auto& ext = engine.selected().ext;
            ext.monitor.store (! ext.monitor.load());   // hear the gear through this track, or not
            return;
        }
        if (! engine.recorder.isRecording())
            engine.recorder.source.store (engine.recorder.source.load() == SampleRecorder::input ? (int) SampleRecorder::output
                                                                                                : (int) SampleRecorder::input);
    };
    addChildComponent (recSourceButton);

    styleHardwareButton (padsModeButton, "What the Launchkey pads do: play the drums (or a Sampler's slices), "
                                         "or switch the sequencer's steps on and off. Click to switch.", true);
    padsModeButton.onClick = [this] { targets.applyButton ("pads.mode", true); };
    addAndMakeVisible (padsModeButton);

    styleHardwareButton (holdButton, "Hold: notes keep sounding after you let go of the keys, for long, evolving sounds. "
                                     "Play again once every key is up for a new held chord; switch Hold off to let them go. "
                                     "A controller button can do this too (Hold notes on / off).", true);
    holdButton.onClick = [this] { targets.applyButton ("perform.hold", true); };
    addAndMakeVisible (holdButton);

    // 25 keys, like the Launchkey Mini.
    keyboard.setAvailableRange (lowestKey, lowestKey + 24);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff2f2f0));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff15161a));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9a9a9a));
    keyboard.setBlackNoteLengthProportion (0.62f);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Colours::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colours::accent.withAlpha (0.25f));
    addAndMakeVisible (keyboard);

    formats.registerBasicFormats();
    styleHardwareButton (loadSampleButton, "Choose a recording for this Sampler track (WAV, AIFF, FLAC or OGG). You can also drop a file onto the panel.", true);
    loadSampleButton.onClick = [this] { chooseSample(); };
    loadSampleButton.setButtonText ("Load...");
    addChildComponent (loadSampleButton);

    styleHardwareButton (editSampleButton, "Open the sample editor: trim, chop into slices for the pads, and process the recording.", true);
    editSampleButton.onClick = [this] { openSampleEditor(); };
    addChildComponent (editSampleButton);

    engine.keyboardState.addListener (this);
    showTrack();
    startTimerHz (30);
}

void LaunchkeyPanel::loadSynth (const SynthDefinition& synth)
{
    for (int i = 0; i < 8; ++i)
        knobs[i]->setKnob (synth.knobs[(size_t) i]);
    moreKnobs = synth.more;
    moreButton.setVisible (! moreKnobs.empty());
}

void LaunchkeyPanel::showMore()
{
    // The loaded synth's other settings, in rows of eight. While the window is open the
    // controller's eight knobs turn the highlighted row; click a row's button to pick another.
    struct MoreWindow final : public juce::Component, private juce::Timer
    {
        MoreWindow (AudioEngine& en, const ControlTargets& controls, const std::vector<SynthKnob>& list)
            : engine (en), track (en.selectedTrack.load()), type (en.selected().type.load())
        {
            const int count = (int) list.size();
            rows = juce::jmax (1, (count + 7) / 8);
            for (int r = 0; r < rows; ++r)
            {
                auto* b = rowButtons.add (new juce::TextButton ("Knobs " + juce::String (r * 8 + 1) + "-" + juce::String (juce::jmin (count, r * 8 + 8))));
                b->setWantsKeyboardFocus (false);
                b->setTooltip ("The controller's eight knobs turn this row while this window is open. Click to choose another row.");
                b->onClick = [this, r] { engine.knobPage.store (r + 1); showPage(); };
                addAndMakeVisible (b);
            }
            for (const auto& k : list)
            {
                auto* knob = knobs.add (new PanelKnob (controls));
                knob->setKnob (k);
                addAndMakeVisible (knob);
            }
            setSize (labelWidth + juce::jmax (1, juce::jmin (8, count)) * knobWidth, rows * rowHeight);
            engine.knobPage.store (1);
            showPage();
            startTimerHz (15);
        }

        ~MoreWindow() override { engine.knobPage.store (0); }

        void resized() override
        {
            for (int r = 0; r < rowButtons.size(); ++r)
                rowButtons[r]->setBounds (6, r * rowHeight + rowHeight / 2 - 16, labelWidth - 12, 32);
            for (int i = 0; i < knobs.size(); ++i)
                knobs[i]->setBounds (labelWidth + (i % 8) * knobWidth, (i / 8) * rowHeight, knobWidth, rowHeight);
        }

        void paint (juce::Graphics& g) override
        {
            const int page = engine.knobPage.load();
            if (page > 0)
            {
                g.setColour (Colours::text.withAlpha (0.07f));
                g.fillRoundedRectangle (getLocalBounds().withY ((page - 1) * rowHeight).withHeight (rowHeight).reduced (2).toFloat(), 6.0f);
            }
        }

        void showPage()
        {
            shownPage = engine.knobPage.load();
            for (int r = 0; r < rowButtons.size(); ++r)
                rowButtons[r]->setToggleState (r + 1 == shownPage, juce::dontSendNotification);
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
            for (auto* knob : knobs)
                knob->refresh();
        }

        const int labelWidth = 96, knobWidth = 110, rowHeight = 120;   // a local class cannot hold static members
        AudioEngine& engine;
        const int track, type;
        int rows = 1, shownPage = -1;
        juce::OwnedArray<juce::TextButton> rowButtons;
        juce::OwnedArray<PanelKnob> knobs;
    };

    juce::CallOutBox::launchAsynchronously (std::make_unique<MoreWindow> (engine, targets, moreKnobs),
                                            moreButton.getBounds(), this);
}

void LaunchkeyPanel::showTrack()
{
    const auto& library = synthLibrary();
    const int t = engine.selectedTrack.load();
    auto& tr = engine.track (t);
    const int type = juce::jlimit (0, (int) library.size() - 1, tr.type.load());
    const int drum = tr.selectedDrum.load();

    if (type != shownType || t != shownTrack)
    {
        loadSynth (library[(size_t) type]);

        // A Sampler track loads and edits its recording; a Drums track puts recordings on the selected pad.
        const bool sampler = type == Track::sampler, kit = type == Track::drums, ext = type == Track::external;
        loadSampleButton.setVisible (sampler || kit || ext);
        editSampleButton.setVisible (sampler || kit || ext);
        recSourceButton.setVisible (sampler || kit || ext);
        recSourceButton.setTooltip (ext ? "Hear the gear's sound through this track: connect its audio output to your sound card's input. Turn it off if you hear feedback."
                                        : "What Record captures: the sound card's input (a microphone or line in), or BizGroove's own sound (resampling). Click to switch.");
        recordButton.setEnabled (true);
        loadSampleButton.setButtonText (kit ? "Pad..." : "Load...");
        editSampleButton.setButtonText (kit ? "Clear pad" : "Edit...");
        loadSampleButton.setTooltip (kit ? "The selected pad: put a recording on it (or drop a file onto the panel), or take the same drum from another kit to build your own. Keep it with Sounds > Save as a new sound."
                                         : "Choose a recording for this Sampler track (WAV, AIFF, FLAC or OGG). You can also drop a file onto the panel.");
        editSampleButton.setTooltip (kit ? "Take the recording off the selected pad, so it plays its synthesised drum again."
                                         : "Open the sample editor: trim, chop into slices for the pads, and process the recording.");

        // The drum kit spans the keys from C2 (note 36), the kick on the lowest key.
        const bool drums = type == Track::drums;
        octaveDown.setEnabled (! drums);
        octaveUp.setEnabled (! drums);
        if (drums)
        {
            lowestKey = 36;
            keyboard.setAvailableRange (lowestKey, lowestKey + 24);
            keyboard.setLowestVisibleKey (lowestKey);
        }
        else if (shownType == Track::drums)
        {
            lowestKey = 48;
            keyboard.setAvailableRange (lowestKey, lowestKey + 24);
            keyboard.setLowestVisibleKey (lowestKey);
        }
    }

    // An External MIDI track: the output and channel it plays.
    if (type == Track::external)
    {
        loadSampleButton.setButtonText (tr.externalDeviceName.isNotEmpty() ? "Out: " + tr.externalDeviceName : juce::String ("Choose MIDI out..."));
        editSampleButton.setButtonText ("Channel " + juce::String (tr.ext.channel.load()));
        loadSampleButton.setTooltip ("The MIDI output this track plays: the FM-1, an app on an iPhone or iPad (over USB or network MIDI), or another synth.");
        editSampleButton.setTooltip ("The MIDI channel the gear listens on. With SLOOP on the FM-1, each of its tracks has its own channel (GLO > MIDI).");
        shownExternal = externalState();
    }

    shownTrack = t;
    shownType = type;
    shownDrum = drum;

    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    juce::String text = "Track " + juce::String (t + 1) + dot + library[(size_t) type].name;
    if (type == Track::drums)
    {
        text << dot << "editing " << DrumKit::drumName (drum);
        const auto padName = tr.kit.getPadSampleName (drum);
        if (padName.isNotEmpty())
            text << " (sample: " << padName << ")";
        shownSample = padName;
    }
    if (type == Track::sampler)
    {
        const auto sample = tr.samplerEngine.getSampleName();
        if (engine.recorder.isRecording())
            text << dot << "RECORDING " << juce::String (engine.recorder.getSeconds(), 1) << " s";
        else
            text << dot << (sample.isNotEmpty() ? sample : juce::String ("no recording loaded"));
        shownSample = sample;
    }
    if (engine.recordNotes.load())
        text << dot << (engine.countInBeatsLeft.load() > 0 ? "COUNT-IN " + juce::String (engine.countInBeatsLeft.load())
                                                           : juce::String ("RECORDING NOTES (onto the nearest 1/16)"));
    if (type == Track::external)
    {
        text << dot << (tr.externalDeviceName.isNotEmpty() ? tr.externalDeviceName + ", channel " + juce::String (tr.ext.channel.load())
                                                           : juce::String ("no MIDI out chosen"));
        if (tr.ext.monitor.load())
            text << dot << "hearing the input";
    }
    if (trackInfo.getText() != text)
        trackInfo.setText (text, juce::dontSendNotification);
}

void LaunchkeyPanel::chooseSample()
{
    if (engine.selected().type.load() == Track::external)
    {
        showMidiOutMenu();   // on an External track this button picks the MIDI output
        return;
    }
    if (engine.selected().type.load() == Track::drums)
    {
        showPadMenu();   // on a Drums track: a recording, or this drum from another kit
        return;
    }
    chooseRecording();
}

void LaunchkeyPanel::chooseRecording()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a recording into track " + juce::String (engine.selectedTrack.load() + 1),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   formats.getWildcardForAllFormats());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<LaunchkeyPanel> (this)] (const juce::FileChooser& fc) {
                              if (safe != nullptr && fc.getResult() != juce::File())
                                  safe->loadSample (fc.getResult());
                          });
}

void LaunchkeyPanel::loadSample (const juce::File& file)
{
    juce::MemoryBlock data;
    juce::String error;
    if (! file.loadFileAsData (data)
        || ! (engine.selected().type.load() == Track::drums
                  ? Project::loadSampleIntoPad (engine, engine.selectedTrack.load(), engine.selected().selectedDrum.load(),
                                                file.getFileName(), data, formats, error)
                  : Project::loadSampleIntoTrack (engine, engine.selectedTrack.load(), file.getFileName(), data, formats, error)))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the recording",
                                                file.getFileName() + (error.isNotEmpty() ? ": " + error : juce::String()));
    showTrack();
}

// The selected pad: put a recording on it, or take the same drum from another kit
// (a factory kit or one of yours), to build a kit of your own; then keep it with Sounds > Save as a new sound.
void LaunchkeyPanel::showPadMenu()
{
    const int t = engine.selectedTrack.load();
    const int drum = engine.selected().selectedDrum.load();
    const auto name = DrumKit::drumName (drum);

    juce::PopupMenu menu;
    menu.addItem (1, "Load a recording onto the " + name + "...");
    const bool padRecording = engine.recorder.isRecording();
    menu.addItem (2, padRecording ? juce::String ("Stop recording, and put it on the pad")
                                  : "Record onto the " + name + " (" + (engine.recorder.source.load() == SampleRecorder::input
                                                                         ? juce::String ("mic / line in") : juce::String ("BizGroove's sound")) + ")");
    menu.addSectionHeader ("Take the " + name + " from");
    const auto& kits = Presets::factorySounds (Track::drums);
    for (size_t i = 0; i < kits.size(); ++i)
        menu.addItem (100 + (int) i, kits[i].name);
    const auto mine = Presets::userSounds (Track::drums);
    if (! mine.isEmpty())
    {
        menu.addSectionHeader ("My kits");
        for (int i = 0; i < mine.size(); ++i)
            menu.addItem (1000 + i, mine[i].getFileNameWithoutExtension());
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadSampleButton),
                        [safe = juce::Component::SafePointer<LaunchkeyPanel> (this), t, drum, mine] (int id) {
                            if (safe == nullptr || id == 0)
                                return;
                            auto& self = *safe;
                            if (id == 1)
                            {
                                self.chooseRecording();
                                return;
                            }
                            if (id == 2)
                            {
                                self.engine.recordPadRequested.store (true);   // Record again, or this item, stops it
                                return;
                            }
                            if (self.engine.track (t).type.load() != Track::drums)
                                return;
                            if (id >= 1000)
                            {
                                juce::String error;
                                if (juce::isPositiveAndBelow (id - 1000, mine.size())
                                    && ! Presets::copyDrumFromFile (self.engine, t, drum, mine[id - 1000], self.formats, error))
                                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't take the drum", error);
                            }
                            else
                            {
                                const auto& factory = Presets::factorySounds (Track::drums);
                                if (juce::isPositiveAndBelow (id - 100, (int) factory.size()))
                                    Presets::copyDrum (self.engine, t, drum, factory[(size_t) (id - 100)].sound);
                            }
                            self.showTrack();
                        });
}

void LaunchkeyPanel::openSampleEditor()
{
    if (engine.selected().type.load() == Track::external)
    {
        showChannelMenu();   // on an External track this button picks the MIDI channel
        return;
    }

    // On a Drums track this button clears the selected pad.
    if (engine.selected().type.load() == Track::drums)
    {
        Project::clearPad (engine, engine.selectedTrack.load(), engine.selected().selectedDrum.load());
        showTrack();
        return;
    }

    const int t = engine.selectedTrack.load();
    if (engine.track (t).sampleFileData.getSize() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Sample editor",
                                                "Load or record a sample on this Sampler track first.");
        return;
    }

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new SampleEditor (engine, t, formats));
    o.dialogTitle = "Sample editor - track " + juce::String (t + 1);
    o.dialogBackgroundColour = Colours::background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

bool LaunchkeyPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    const int type = engine.selected().type.load();
    return files.size() == 1 && (type == Track::sampler || type == Track::drums);
}

void LaunchkeyPanel::filesDropped (const juce::StringArray& files, int, int)
{
    loadSample (juce::File (files[0]));
}

void LaunchkeyPanel::handleNoteOn (juce::MidiKeyboardState*, int, int note, float)
{
    // On a drum track, a key picks the drum that the knobs and pads edit.
    auto& tr = engine.selected();
    if (tr.type.load() == Track::drums)
    {
        const int drum = DrumKit::drumForNote (note);
        if (drum >= 0)
            tr.selectedDrum.store (drum);
    }
}

void LaunchkeyPanel::shiftOctave (int octaves)
{
    lowestKey = juce::jlimit (12, 96, lowestKey + 12 * octaves);
    keyboard.setAvailableRange (lowestKey, lowestKey + 24);
    keyboard.setLowestVisibleKey (lowestKey);
    resized();
}

juce::String LaunchkeyPanel::externalState() const
{
    auto& tr = engine.selected();
    return tr.externalDeviceName + "/" + juce::String (tr.ext.channel.load()) + "/" + (tr.ext.monitor.load() ? "on" : "off");
}

void LaunchkeyPanel::showMidiOutMenu()
{
    const auto devices = juce::MidiOutput::getAvailableDevices();
    const int t = engine.selectedTrack.load();
    auto& tr = engine.track (t);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Play this track on");
    menu.addItem (1, "None", true, tr.externalDeviceName.isEmpty());
    for (int i = 0; i < devices.size(); ++i)
        menu.addItem (i + 2, devices[i].name, true, devices[i].name == tr.externalDeviceName);
    if (devices.isEmpty())
        menu.addItem (-1, "No MIDI outputs found: connect the gear by USB, or turn on network MIDI", false, false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadSampleButton),
                        [this, t, devices] (int result) {
                            if (result <= 0)
                                return;
                            auto& track = engine.track (t);
                            track.ext.panic.store (true);   // the old device stops its notes
                            if (result == 1)
                            {
                                track.externalDeviceName = {};
                                track.externalDeviceId = {};
                            }
                            else if (juce::isPositiveAndBelow (result - 2, devices.size()))
                            {
                                track.externalDeviceName = devices[result - 2].name;
                                track.externalDeviceId = devices[result - 2].identifier;
                            }
                            showTrack();
                        });
}

void LaunchkeyPanel::showChannelMenu()
{
    const int t = engine.selectedTrack.load();
    const int current = engine.track (t).ext.channel.load();

    juce::PopupMenu menu;
    menu.addSectionHeader ("MIDI channel");
    for (int c = 1; c <= 16; ++c)
        menu.addItem (c, "Channel " + juce::String (c) + (c == 10 ? "  (drums on most gear)" : ""), true, c == current);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&editSampleButton),
                        [this, t] (int result) {
                            if (result >= 1 && result <= 16)
                            {
                                engine.track (t).ext.channel.store (result);
                                showTrack();
                            }
                        });
}

void LaunchkeyPanel::timerCallback()
{
    {
        auto& tr = engine.selected();
        if (engine.selectedTrack.load() != shownTrack || tr.type.load() != shownType || tr.selectedDrum.load() != shownDrum
            || (tr.type.load() == Track::sampler && tr.samplerEngine.getSampleName() != shownSample)
            || (tr.type.load() == Track::drums && tr.kit.getPadSampleName (tr.selectedDrum.load()) != shownSample)
            || (tr.type.load() == Track::external && externalState() != shownExternal))
            showTrack();
    }

    // Recording: the time runs in the track line. And the pads and record-source buttons.
    const bool recording = engine.recorder.isRecording() || engine.recordNotes.load();
    if (recording || recording != shownRecording || engine.countInBeatsLeft.load() > 0)
    {
        shownRecording = recording;
        showTrack();
    }
    const auto padsText = engine.padsPlay.load() ? juce::String ("Pads: play") : juce::String ("Pads: steps");
    if (padsModeButton.getButtonText() != padsText)
        padsModeButton.setButtonText (padsText);
    if (arpButton.getToggleState() != engine.arp.on.load())
    {
        arpButton.setToggleState (engine.arp.on.load(), juce::dontSendNotification);
        arpButton.setColour (juce::TextButton::buttonColourId, engine.arp.on.load() ? Colours::accent : buttonOff);
        arpButton.setColour (juce::TextButton::textColourOffId, engine.arp.on.load() ? juce::Colours::black : Colours::text);
    }
    if (holdButton.getToggleState() != engine.hold.load())
    {
        holdButton.setToggleState (engine.hold.load(), juce::dontSendNotification);
        holdButton.setColour (juce::TextButton::buttonColourId, engine.hold.load() ? Colours::accent : buttonOff);
        holdButton.setColour (juce::TextButton::textColourOffId, engine.hold.load() ? juce::Colours::black : Colours::text);
    }
    const auto sourceText = engine.selected().type.load() == Track::external
                                ? juce::String (engine.selected().ext.monitor.load() ? "Hear input: on" : "Hear input: off")
                                : (engine.recorder.source.load() == SampleRecorder::input ? juce::String ("Rec: mic / line in")
                                                                                          : juce::String ("Rec: BizGroove"));
    if (recSourceButton.getButtonText() != sourceText)
        recSourceButton.setButtonText (sourceText);

    for (auto* k : knobs)
        k->refresh();
    pitch.refresh();
    modulation.refresh();

    const bool playing = engine.transport.playing.load();
    if (playButton.getToggleState() != playing)
        playButton.setToggleState (playing, juce::dontSendNotification);
}

void LaunchkeyPanel::paint (juce::Graphics& g)
{
    // The controller's body.
    g.setColour (body);
    g.fillRoundedRectangle (bodyArea.toFloat(), 14.0f);
    g.setColour (bodyEdge);
    g.drawRoundedRectangle (bodyArea.toFloat().reduced (0.5f), 14.0f, 1.0f);

    // Labels printed on the hardware, in the same places.
    drawPrinted (g, "Octave", octaveLabelArea, juce::Justification::centred, 11.0f);
    drawPrinted (g, juce::String::fromUTF8 ("\xe2\x96\xb2"), launchArrowArea);
    drawPrinted (g, juce::String::fromUTF8 ("\xe2\x96\xbc"), ssmArrowArea);
    drawPrinted (g, "Track", trackLabelArea);
    drawPrinted (g, "Record", recordLabelArea, juce::Justification::centredRight, 10.0f);

    const char* padModes[8] = { "Session", "Drum", "Custom", "Device", "Volume", "Pan", "Sends", "Custom" };
    const int w = padModeLabelArea.getWidth() / 8;
    for (int i = 0; i < 8; ++i)
        drawPrinted (g, padModes[i], padModeLabelArea.withX (padModeLabelArea.getX() + i * w).withWidth (w), juce::Justification::centred, 10.0f);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("BizGroove", wordmarkArea, juce::Justification::centredRight);
}

void LaunchkeyPanel::resized()
{
    auto r = getLocalBounds();
    auto keysArea = r.removeFromBottom (juce::jlimit (120, 190, getHeight() / 3));
    bodyArea = getLocalBounds();

    auto controls = r.reduced (18, 16);

    // Far left: pitch and modulation strips.
    auto strips = controls.removeFromLeft (100);
    pitch.setBounds (strips.removeFromLeft (46));
    strips.removeFromLeft (8);
    modulation.setBounds (strips.removeFromLeft (46));
    controls.removeFromLeft (18);

    // Vertical bands shared by the middle columns: knobs above, two pad rows below.
    const int knobBandH = juce::jmin (146, controls.getHeight() / 2);
    const int labelBandH = 16;
    const int padTop = controls.getY() + knobBandH + labelBandH + 6;
    const int padH = controls.getBottom() - padTop;
    const int rowH = (padH - 10) / 2;

    // Left column: Shift (knob row), Transpose (top pad row), Octave -/+ (bottom pad row).
    auto left = controls.removeFromLeft (86);
    shiftButton.setBounds (left.getX(), controls.getY() + 30, left.getWidth(), 34);
    padsModeButton.setBounds (left.getX(), controls.getY() + 74, left.getWidth(), 30);
    holdButton.setBounds (left.getX(), controls.getY() + 112, left.getWidth(), 30);
    transposeButton.setBounds (left.getX(), padTop + rowH / 2 - 17, left.getWidth(), 34);
    octaveLabelArea = { left.getX(), padTop + rowH + 10, left.getWidth(), 16 };
    octaveDown.setBounds (left.getX(), padTop + rowH + 28, left.getWidth() / 2 - 3, rowH - 28);
    octaveUp.setBounds (left.getX() + left.getWidth() / 2 + 3, padTop + rowH + 28, left.getWidth() / 2 - 3, rowH - 28);
    controls.removeFromLeft (18);

    // Right column: wordmark, Arp/Fixed Chord, Track, Play/Record.
    auto right = controls.removeFromRight (176);
    wordmarkArea = { right.getX(), controls.getY() + 4, right.getWidth(), 24 };
    const int moreW = (right.getWidth() - 22) * 3 / 5;
    moreButton.setBounds (right.getX() + 16, controls.getY() + 70, moreW, 30);
    fxButton.setBounds (right.getX() + 22 + moreW, controls.getY() + 70, right.getWidth() - 22 - moreW, 30);
    const int halfW = (right.getWidth() - 22) / 2;
    loadSampleButton.setBounds (right.getX() + 16, controls.getY() + 106, halfW, 26);
    editSampleButton.setBounds (right.getX() + 22 + halfW, controls.getY() + 106, halfW, 26);
    recSourceButton.setBounds (right.getX() + 16, controls.getY() + 136, right.getWidth() - 16, 24);
    trackInfo.setBounds (right.getX() - 120, controls.getY() + 34, right.getWidth() + 120, 22);
    const int bw = (right.getWidth() - 10) / 2;
    arpButton.setBounds (right.getX(), padTop, bw, rowH);
    fixedChordButton.setBounds (right.getX() + bw + 10, padTop, bw, rowH);
    trackLabelArea = { right.getX(), padTop + rowH - 6, right.getWidth(), 22 };
    trackPrev.setBounds (right.getX() + 22, padTop + rowH - 5, 28, 20);
    trackNext.setBounds (right.getRight() - 50, padTop + rowH - 5, 28, 20);
    playButton.setBounds (right.getX(), padTop + rowH + 10, bw, rowH);
    recordButton.setBounds (right.getX() + bw + 10, padTop + rowH + 10, bw, rowH);
    recordLabelArea = { right.getX(), padTop + 2 * rowH + 10, right.getWidth(), 16 };
    controls.removeFromRight (18);

    // Beside the pads: > and Stop/Solo/Mute.
    auto side = controls.removeFromRight (78);
    launchArrowArea = { side.getX(), padTop - 18, side.getWidth(), 16 };
    launchButton.setBounds (side.getX(), padTop, side.getWidth(), rowH);
    stopSoloMuteButton.setBounds (side.getX(), padTop + rowH + 10, side.getWidth(), rowH);
    ssmArrowArea = { side.getX(), padTop + 2 * rowH + 10, side.getWidth(), 16 };
    controls.removeFromRight (16);

    // Middle: eight knobs, the printed pad-mode labels, then the 2 x 8 pads.
    auto knobRow = controls.withHeight (knobBandH);
    const int kw = knobRow.getWidth() / 8;
    for (int i = 0; i < 8; ++i)
        knobs[i]->setBounds (knobRow.getX() + i * kw, knobRow.getY(), kw, knobBandH);

    padModeLabelArea = { controls.getX(), controls.getY() + knobBandH, controls.getWidth(), labelBandH };
    pads.setBounds (controls.getX(), padTop, controls.getWidth(), padH);

    // 25 keys across the full width.
    auto keys = keysArea.reduced (18, 0).withTrimmedBottom (14);
    keyboard.setBounds (keys);
    keyboard.setKeyWidth ((float) keys.getWidth() / 15.0f);   // 15 white keys in 25
    keyboard.setLowestVisibleKey (lowestKey);
}

} // namespace bizgroove
