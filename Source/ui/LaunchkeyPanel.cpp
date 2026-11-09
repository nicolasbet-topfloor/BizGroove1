#include "LaunchkeyPanel.h"
#include "LookAndFeel.h"
#include "engine/Project.h"

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

    styleHardwareButton (arpButton, "On the Launchkey, Arp plays held notes one after another. " + onDevice, false);
    styleHardwareButton (fixedChordButton, "On the Launchkey, Fixed Chord plays a stored chord from one key. " + onDevice, false);
    addAndMakeVisible (arpButton);
    addAndMakeVisible (fixedChordButton);

    playButton.setWantsKeyboardFocus (false);
    playButton.setTooltip ("Play / stop (space bar)");
    playButton.onClick = [this] { engine.transport.playing.store (! engine.transport.playing.load()); };
    addAndMakeVisible (playButton);

    recordButton.setWantsKeyboardFocus (false);
    recordButton.setTooltip ("Record into the Sampler of the selected track: press to start, press again to stop. "
                             "The Launchkey's Record button does the same.");
    recordButton.onClick = [this] { engine.recorder.toggleRequested.store (true); };
    addAndMakeVisible (recordButton);

    styleHardwareButton (recSourceButton, "What Record captures: the sound card's input (a microphone or line in), "
                                          "or BizGroove's own sound (resampling). Click to switch.", true);
    recSourceButton.onClick = [this] {
        if (! engine.recorder.isRecording())
            engine.recorder.source.store (engine.recorder.source.load() == SampleRecorder::input ? (int) SampleRecorder::output
                                                                                                : (int) SampleRecorder::input);
    };
    addChildComponent (recSourceButton);

    styleHardwareButton (padsModeButton, "What the Launchkey pads do: play the drums (or a Sampler's slices), "
                                         "or switch the sequencer's steps on and off. Click to switch.", true);
    padsModeButton.onClick = [this] { targets.applyButton ("pads.mode", true); };
    addAndMakeVisible (padsModeButton);

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
    auto content = std::make_unique<juce::Component>();
    int x = 0;
    for (const auto& k : moreKnobs)
    {
        auto* knob = new PanelKnob (targets);
        knob->setKnob (k);
        knob->setBounds (x, 0, 110, 120);
        content->addAndMakeVisible (knob);
        x += 110;
    }
    content->setSize (juce::jmax (110, x), 120);

    // The call-out owns its content; the knobs are deleted with it.
    struct Owner final : public juce::Component
    {
        explicit Owner (std::unique_ptr<juce::Component> c) : inner (std::move (c))
        {
            addAndMakeVisible (*inner);
            setSize (inner->getWidth(), inner->getHeight());
        }
        ~Owner() override { inner->deleteAllChildren(); }
        std::unique_ptr<juce::Component> inner;
    };

    juce::CallOutBox::launchAsynchronously (std::make_unique<Owner> (std::move (content)),
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

        loadSampleButton.setVisible (type == Track::sampler);
        editSampleButton.setVisible (type == Track::sampler);
        recSourceButton.setVisible (type == Track::sampler);
        recordButton.setEnabled (type == Track::sampler);

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

    shownTrack = t;
    shownType = type;
    shownDrum = drum;

    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    juce::String text = "Track " + juce::String (t + 1) + dot + library[(size_t) type].name;
    if (type == Track::drums)
        text << dot << "editing " << DrumKit::drumName (drum);
    if (type == Track::sampler)
    {
        const auto sample = tr.samplerEngine.getSampleName();
        if (engine.recorder.isRecording())
            text << dot << "RECORDING " << juce::String (engine.recorder.getSeconds(), 1) << " s";
        else
            text << dot << (sample.isNotEmpty() ? sample : juce::String ("no recording loaded"));
        shownSample = sample;
    }
    if (trackInfo.getText() != text)
        trackInfo.setText (text, juce::dontSendNotification);
}

void LaunchkeyPanel::chooseSample()
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
        || ! Project::loadSampleIntoTrack (engine, engine.selectedTrack.load(), file.getFileName(), data, formats, error))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the recording",
                                                file.getFileName() + (error.isNotEmpty() ? ": " + error : juce::String()));
    showTrack();
}

void LaunchkeyPanel::openSampleEditor()
{
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
    return files.size() == 1 && engine.selected().type.load() == Track::sampler;
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

void LaunchkeyPanel::timerCallback()
{
    {
        auto& tr = engine.selected();
        if (engine.selectedTrack.load() != shownTrack || tr.type.load() != shownType || tr.selectedDrum.load() != shownDrum
            || (tr.type.load() == Track::sampler && tr.samplerEngine.getSampleName() != shownSample))
            showTrack();
    }

    // Recording: the time runs in the track line. And the pads and record-source buttons.
    const bool recording = engine.recorder.isRecording();
    if (recording || recording != shownRecording)
    {
        shownRecording = recording;
        showTrack();
    }
    const auto padsText = engine.padsPlay.load() ? juce::String ("Pads: play") : juce::String ("Pads: steps");
    if (padsModeButton.getButtonText() != padsText)
        padsModeButton.setButtonText (padsText);
    const auto sourceText = engine.recorder.source.load() == SampleRecorder::input ? juce::String ("Rec: mic / line in")
                                                                                   : juce::String ("Rec: BizGroove");
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
    drawPrinted (g, "Record sample", recordLabelArea, juce::Justification::centredRight, 10.0f);

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
    transposeButton.setBounds (left.getX(), padTop + rowH / 2 - 17, left.getWidth(), 34);
    octaveLabelArea = { left.getX(), padTop + rowH + 10, left.getWidth(), 16 };
    octaveDown.setBounds (left.getX(), padTop + rowH + 28, left.getWidth() / 2 - 3, rowH - 28);
    octaveUp.setBounds (left.getX() + left.getWidth() / 2 + 3, padTop + rowH + 28, left.getWidth() / 2 - 3, rowH - 28);
    controls.removeFromLeft (18);

    // Right column: wordmark, Arp/Fixed Chord, Track, Play/Record.
    auto right = controls.removeFromRight (176);
    wordmarkArea = { right.getX(), controls.getY() + 4, right.getWidth(), 24 };
    moreButton.setBounds (right.getX() + 16, controls.getY() + 70, right.getWidth() - 16, 30);
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
