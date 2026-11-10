#include "MainComponent.h"

namespace bizgroove
{

MainComponent::MainComponent()
{
    setLookAndFeel (&lookAndFeel);
    formats.registerBasicFormats();

    // The state of a fresh project, for File > New.
    {
        AudioEngine fresh;
        defaultState = Project::toVar (fresh);
    }

    juce::PropertiesFile::Options options;
    options.applicationName = "BizGroove";
    options.filenameSuffix = ".settings";
    options.folderName = "BizGroove";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    // ---- Audio device: restore last settings, otherwise default stereo out.
    std::unique_ptr<juce::XmlElement> savedState;
    if (auto* settings = appProperties.getUserSettings())
        savedState = settings->getXmlValue ("audioDeviceState");

    const auto error = deviceManager.initialise (0, 2, savedState.get(), true);
    if (error.isNotEmpty())
    {
        DBG ("Audio device error: " << error);
    }

    if (savedState == nullptr)
        enableAllMidiInputs();
    for (const auto& input : juce::MidiInput::getAvailableDevices())
        knownMidiInputs.add (input.identifier);

    // Controller profiles: Launchkey Mini MK3 by default, Kontrol F1 for stems.
    ControllerStore::load (midiRouter);
    deviceManager.addMidiInputDeviceCallback ({}, &midiRouter);
    sourcePlayer.setSource (&liveOutput);   // the engine, plus MIDI clock for the Launchkey arp
    updateClockOutputs();
    deviceManager.addAudioCallback (&sourcePlayer);

    // ---- Top bar
    title.setText ("BizGroove", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Colours::accent);
    addAndMakeVisible (title);

    soundLabel.setText ("Sound", juce::dontSendNotification);
    soundLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (soundLabel);

    const auto& library = synthLibrary();
    for (size_t i = 0; i < library.size(); ++i)
        soundMenu.addItem (library[i].name, (int) i + 1);
    soundMenu.setSelectedId (engine.selected().type.load() + 1, juce::dontSendNotification);
    soundMenu.setTooltip ("Choose the sound of the selected track.");
    soundMenu.setWantsKeyboardFocus (false);
    soundMenu.onChange = [this] { loadSynth (soundMenu.getSelectedId() - 1); };
    addAndMakeVisible (soundMenu);

    soundsButton.setTooltip ("Sounds for the selected track: the built-in factory sounds and your own saved sounds.");
    soundsButton.onClick = [this] { showSoundsMenu(); };
    addAndMakeVisible (soundsButton);

    // Only the on-screen keyboard takes key focus, so A-K always play notes.
    for (auto* b : { &fileButton, &playButton, &panelViewButton, &songViewButton, &mixerViewButton, &stemViewButton,
                     &settingsButton, &controllersButton, &soundsButton })
        b->setWantsKeyboardFocus (false);
    tempoSlider.setWantsKeyboardFocus (false);

    playButton.setClickingTogglesState (true);
    playButton.setTooltip ("Play / stop (space bar)");
    playButton.onClick = [this] {
        engine.transport.playing.store (playButton.getToggleState());
        playButton.setButtonText (playButton.getToggleState() ? "Stop" : "Play");
    };
    addAndMakeVisible (playButton);

    tempoLabel.setText ("Tempo", juce::dontSendNotification);
    tempoLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (tempoLabel);

    tempoSlider.setRange (40.0, 240.0, 1.0);
    tempoSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    tempoSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setDoubleClickReturnValue (true, 120.0);
    tempoSlider.setTooltip ("Speed in beats per minute. Double-click to reset to 120.");
    tempoSlider.onValueChange = [this] { engine.transport.bpm.store (tempoSlider.getValue()); };
    addAndMakeVisible (tempoSlider);

    fileButton.setTooltip ("New, open, save and export");
    fileButton.onClick = [this] { showFileMenu(); };
    addAndMakeVisible (fileButton);

    for (auto* b : { &panelViewButton, &songViewButton, &mixerViewButton, &stemViewButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        addAndMakeVisible (b);
    }
    panelViewButton.setTooltip ("The keys, knobs and pads, laid out like the Launchkey Mini");
    stemViewButton.setTooltip ("Four looping audio stems, for playing live with the Kontrol F1");
    songViewButton.setTooltip ("Arrange scenes into a song");
    panelViewButton.onClick = [this] { showView (View::panel); };
    songViewButton.onClick = [this] { showView (View::song); };
    mixerViewButton.setTooltip ("Volume, pan, tone and effects for every track");
    mixerViewButton.onClick = [this] { showView (View::mixer); };
    stemViewButton.onClick = [this] { showView (View::stems); };

    settingsButton.setTooltip ("Sound card, sample rate and which MIDI devices are on");
    settingsButton.onClick = [this] { showAudioSettings(); };
    addAndMakeVisible (settingsButton);

    controllersButton.setTooltip ("Choose and customise what each knob, pad and button on your controller does");
    controllersButton.onClick = [this] { showControllers(); };
    addAndMakeVisible (controllersButton);

    midiStatus.setColour (juce::Label::textColourId, Colours::textDim);
    midiStatus.setFont (juce::FontOptions (12.0f));
    midiStatus.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (midiStatus);

    addAndMakeVisible (panel);
    addChildComponent (stemPanel);
    addChildComponent (songView);
    addChildComponent (mixerView);
    addAndMakeVisible (sessionBar);
    panelViewButton.setToggleState (true, juce::dontSendNotification);

    savedFingerprint = Project::fingerprint (engine);
    setSize (1320, 820);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    saveDeviceState();
    deviceManager.removeAudioCallback (&sourcePlayer);
    deviceManager.removeMidiInputDeviceCallback ({}, &midiRouter);
    sourcePlayer.setSource (nullptr);
    setLookAndFeel (nullptr);
}

void MainComponent::loadSynth (int index)
{
    const auto& library = synthLibrary();
    if (! juce::isPositiveAndBelow (index, (int) library.size()))
        return;
    engine.selected().type.store (index);   // the panel follows on its next tick
    currentSounds[(size_t) juce::jlimit (0, AudioEngine::numTracks - 1, engine.selectedTrack.load())] = {};
    updateSoundsButton();
}

// ============================================================== recording into the Sampler

bool MainComponent::ensureAudioInput()
{
    // Switch on the sound card's input the first time something is recorded.
    auto setup = deviceManager.getAudioDeviceSetup();
    bool changed = false;
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        if (setup.inputDeviceName.isEmpty())
        {
            const auto names = type->getDeviceNames (true);
            if (! names.isEmpty())
            {
                setup.inputDeviceName = names[juce::jlimit (0, names.size() - 1, type->getDefaultDeviceIndex (true))];
                changed = true;
            }
        }
    }
    if (setup.inputDeviceName.isNotEmpty() && setup.inputChannels.countNumberOfSetBits() == 0)
    {
        setup.useDefaultInputChannels = true;
        changed = true;
    }
    if (changed)
    {
        const auto error = deviceManager.setAudioDeviceSetup (setup, true);
        if (error.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Can't record", error);
            return false;
        }
        saveDeviceState();
    }

    int active = 0;
    if (auto* device = deviceManager.getCurrentAudioDevice())
        active = device->getActiveInputChannels().countNumberOfSetBits();
    if (active == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Can't record",
                                                "BizGroove has no audio input. Choose one under Audio / MIDI "
                                                "(a microphone, or the input of your sound card), then press Record again.");
        return false;
    }
    engine.recorder.monoInput.store (active == 1);
    return true;
}

void MainComponent::handleRecorder()
{
    auto& rec = engine.recorder;
    const bool pressed = rec.toggleRequested.exchange (false);
    const bool full = rec.hasStoppedItself();
    if (! pressed && ! full)
        return;

    const int t = engine.selectedTrack.load();

    // Recording notes: Record again stops it (so does Stop).
    if (pressed && engine.recordNotes.load())
    {
        engine.recordNotes.store (false);
        return;
    }

    // Start.
    if (! rec.isRecording() && ! full)
    {
        const int type = engine.track (t).type.load();
        const bool playing = engine.transport.playing.load();

        // What you play goes into the steps: on any track but a Sampler, and on a Drums
        // track while the song plays (stopped, a Drums track records a sound onto its pad).
        if (type != Track::sampler && ! (type == Track::drums && ! playing))
        {
            engine.recordNotes.store (true);
            if (! playing)
                engine.transport.playing.store (true);
            return;
        }
        if (rec.source.load() == SampleRecorder::input && ! ensureAudioInput())
            return;
        recordingTrack = t;
        recordingPad = engine.track (t).selectedDrum.load();
        rec.start();
        return;
    }

    // Stop: trim the silence and load the recording into the Sampler.
    juce::AudioBuffer<float> audio;
    double rate = 44100.0;
    rec.stop (audio, rate);
    const int track = recordingTrack >= 0 ? recordingTrack : t;
    recordingTrack = -1;

    if (! SampleTools::trimSilence (audio, rate))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Nothing was recorded",
                                                "BizGroove heard only silence. Check the input under Audio / MIDI. On a Mac, "
                                                "also check that BizGroove may use the microphone: System Settings > Privacy & Security > Microphone.");
        return;
    }

    const auto name = "Recording " + juce::Time::getCurrentTime().formatted ("%H-%M-%S") + ".wav";
    juce::String error;
    if (engine.track (track).type.load() == Track::drums)
    {
        if (! Project::loadSampleIntoPad (engine, track, recordingPad, name, SampleTools::encodeWav (audio, rate), formats, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the recording", error);
        return;
    }
    if (Project::loadSampleIntoTrack (engine, track, name, SampleTools::encodeWav (audio, rate), formats, error))
    {
        auto& sm = engine.track (track).samplerEngine.params;
        sm.start.store (0.0f);
        sm.length.store (1.0f);
        sm.loopStart.store (0.0f);
        sm.sliceCount.store (0);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the recording", error);
    }
}

// ============================================================== MIDI clock

void MainComponent::updateClockOutputs()
{
    // The Launchkey's MIDI port always gets the tempo; so does the MIDI
    // output chosen under Audio / MIDI, if any.
    juce::StringArray ids;
    for (const auto& out : juce::MidiOutput::getAvailableDevices())
        if (LiveOutput::isClockedByDefault (out))
            ids.add (out.identifier);

    const auto chosen = deviceManager.getDefaultMidiOutputIdentifier();
    if (chosen.isNotEmpty())
        ids.addIfNotAlreadyThere (chosen);

    liveOutput.setOutputs (ids);
}

void MainComponent::updateExternalOutputs()
{
    // Each External MIDI track plays the output chosen for it: found by its
    // identifier, or by name when the project comes from another computer.
    const auto devices = juce::MidiOutput::getAvailableDevices();
    std::array<juce::String, AudioEngine::numTracks> ids;
    bool anyHeard = false;
    for (int t = 0; t < AudioEngine::numTracks; ++t)
    {
        auto& tr = engine.track (t);
        if (tr.type.load() != Track::external)
            continue;
        anyHeard = anyHeard || tr.ext.monitor.load();

        juce::String id;
        for (const auto& d : devices)
            if (tr.externalDeviceId.isNotEmpty() && d.identifier == tr.externalDeviceId)
                id = d.identifier;
        if (id.isEmpty())
        {
            for (const auto& d : devices)
            {
                if (tr.externalDeviceName.isNotEmpty() && d.name == tr.externalDeviceName)
                {
                    id = d.identifier;
                    tr.externalDeviceId = id;
                    break;
                }
            }
        }
        ids[(size_t) t] = id;
    }
    liveOutput.setTrackOutputs (ids);

    // Hearing the gear needs the sound card's input: switch it on once.
    if (anyHeard && ! externalInputAsked)
    {
        externalInputAsked = true;
        ensureAudioInput();
    }
}

// ============================================================== program bank

void MainComponent::setCurrentSound (int t, const juce::String& name, const juce::File& file)
{
    auto& c = currentSounds[(size_t) juce::jlimit (0, AudioEngine::numTracks - 1, t)];
    c.name = name;
    c.file = file;
    c.type = engine.track (t).type.load();
    updateSoundsButton();
}

void MainComponent::updateSoundsButton()
{
    const int t = juce::jlimit (0, AudioEngine::numTracks - 1, engine.selectedTrack.load());
    auto& c = currentSounds[(size_t) t];
    if (c.type != engine.track (t).type.load())
        c = {};   // the instrument changed, so the name no longer applies
    soundsButton.setButtonText (c.name.isNotEmpty() ? c.name : juce::String ("Sounds"));
}

void MainComponent::askForSoundName (const juce::String& heading, const juce::String& initial,
                                     std::function<void (juce::String)> onOk)
{
    auto* w = new juce::AlertWindow (heading, "Give the sound a name.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", initial);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, onOk] (int result) {
        const auto name = w->getTextEditorContents ("name").trim();
        if (result == 1 && name.isNotEmpty())
            onOk (name);
    }), true);
}

void MainComponent::showSoundsMenu()
{
    const int t = juce::jlimit (0, AudioEngine::numTracks - 1, engine.selectedTrack.load());
    const int type = juce::jlimit (0, (int) synthLibrary().size() - 1, engine.track (t).type.load());
    const auto& factory = Presets::factorySounds (type);
    const auto mine = Presets::userSounds (type);
    const auto current = currentSounds[(size_t) t];

    static constexpr int saveId = 1, saveAsId = 2, folderId = 3, noneId = 9999;
    static constexpr int factoryBase = 1000, mineBase = 2000, renameBase = 3000, deleteBase = 4000;

    juce::PopupMenu m;
    m.addSectionHeader ("Factory sounds: " + Presets::instrumentName (type));
    for (int i = 0; i < (int) factory.size(); ++i)
        m.addItem (factoryBase + i, factory[(size_t) i].name, true,
                   current.file == juce::File() && current.name == factory[(size_t) i].name);

    m.addSectionHeader ("My sounds");
    if (mine.isEmpty())
        m.addItem (noneId, "None saved yet", false);

    juce::PopupMenu renameMenu, deleteMenu;
    for (int i = 0; i < mine.size(); ++i)
    {
        const auto name = mine[i].getFileNameWithoutExtension();
        m.addItem (mineBase + i, name, true, current.file == mine[i]);
        renameMenu.addItem (renameBase + i, name);
        deleteMenu.addItem (deleteBase + i, name);
    }

    m.addSeparator();
    const bool canSave = current.file.existsAsFile();
    m.addItem (saveId, canSave ? "Save changes to " + current.name.quoted() : juce::String ("Save changes"), canSave);
    m.addItem (saveAsId, "Save as a new sound...");
    m.addSubMenu ("Rename", renameMenu, ! mine.isEmpty());
    m.addSubMenu ("Delete (moves it to the Trash)", deleteMenu, ! mine.isEmpty());
    m.addItem (folderId, "Show my sounds folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&soundsButton),
                     [this, t, type, mine] (int result) {
        if (result == 0)
            return;

        auto showError = [] (const juce::String& error) {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Sounds", error);
        };
        const auto& sounds = Presets::factorySounds (type);

        if (result >= factoryBase && result < factoryBase + (int) sounds.size())
        {
            const auto& f = sounds[(size_t) (result - factoryBase)];
            Presets::applySoundVar (engine.track (t), f.sound);
            setCurrentSound (t, f.name, {});
        }
        else if (result >= mineBase && result < mineBase + mine.size())
        {
            const auto file = mine[result - mineBase];
            juce::String error;
            if (Presets::load (engine, t, file, formats, error))
                setCurrentSound (t, file.getFileNameWithoutExtension(), file);
            else
                showError (error);
        }
        else if (result == saveId)
        {
            const auto c = currentSounds[(size_t) t];
            juce::String error;
            if (! Presets::save (engine.track (t), c.file, error))
                showError (error);
        }
        else if (result == saveAsId)
        {
            const auto& c = currentSounds[(size_t) t];
            askForSoundName ("Save as a new sound", c.name.isNotEmpty() ? c.name : juce::String ("My sound"),
                             [this, t, type, showError] (juce::String name) {
                const auto file = Presets::fileForName (type, name, false);
                juce::String error;
                if (Presets::save (engine.track (t), file, error))
                    setCurrentSound (t, file.getFileNameWithoutExtension(), file);
                else
                    showError (error);
            });
        }
        else if (result >= renameBase && result < renameBase + mine.size())
        {
            const auto file = mine[result - renameBase];
            askForSoundName ("Rename sound", file.getFileNameWithoutExtension(),
                             [this, file, type, showError] (juce::String name) {
                if (name == file.getFileNameWithoutExtension())
                    return;
                const auto target = Presets::fileForName (type, name, false);
                if (! file.moveFileTo (target))
                {
                    showError ("Could not rename " + file.getFileName());
                    return;
                }
                for (auto& c : currentSounds)
                {
                    if (c.file == file)
                    {
                        c.file = target;
                        c.name = target.getFileNameWithoutExtension();
                    }
                }
                updateSoundsButton();
            });
        }
        else if (result >= deleteBase && result < deleteBase + mine.size())
        {
            const auto file = mine[result - deleteBase];
            if (! file.moveToTrash())
            {
                showError ("Could not move " + file.getFileName() + " to the Trash.");
                return;
            }
            for (auto& c : currentSounds)
                if (c.file == file)
                    c = {};
            updateSoundsButton();
        }
        else if (result == folderId)
        {
            Presets::userFolder (type).revealToUser();
        }
    });
}


void MainComponent::showView (View view)
{
    panel.setVisible (view == View::panel);
    songView.setVisible (view == View::song);
    mixerView.setVisible (view == View::mixer);
    stemPanel.setVisible (view == View::stems);
    sessionBar.setVisible (view == View::panel || view == View::song);
    resized();
}

void MainComponent::enableAllMidiInputs()
{
    for (const auto& input : juce::MidiInput::getAvailableDevices())
        deviceManager.setMidiInputDeviceEnabled (input.identifier, true);
}

void MainComponent::enableNewMidiInputs()
{
    // A controller plugged in while BizGroove runs is switched on automatically.
    for (const auto& input : juce::MidiInput::getAvailableDevices())
    {
        if (! knownMidiInputs.contains (input.identifier))
        {
            knownMidiInputs.add (input.identifier);
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);
        }
    }
}

void MainComponent::showControllers()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new ControllersComponent (midiRouter, controlTargets, deviceManager));
    o.dialogTitle = "Controllers";
    o.dialogBackgroundColour = Colours::background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::saveDeviceState()
{
    if (auto* settings = appProperties.getUserSettings())
    {
        if (auto state = deviceManager.createStateXml())
            settings->setValue ("audioDeviceState", state.get());
        settings->saveIfNeeded();
    }
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager,
                                                             0, 2,   // audio inputs (for audio tracks later)
                                                             2, 2,   // stereo output
                                                             true,   // MIDI inputs
                                                             true,   // MIDI output: it also gets the tempo (MIDI clock)
                                                             false, false);
    selector->setSize (520, 500);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio and MIDI settings";
    o.dialogBackgroundColour = Colours::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::togglePlay()
{
    engine.transport.playing.store (! engine.transport.playing.load());
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlay();
        return true;
    }
    // Cmd (Mac) or Ctrl (Windows) shortcuts.
    if (key.getModifiers().isCommandDown())
    {
        // Use the key code: with Ctrl held, the text character is a control code.
        const auto c = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key.getKeyCode());
        if (c == 's') { saveProject (key.getModifiers().isShiftDown()); return true; }
        if (c == 'o') { openProject(); return true; }
        if (c == 'n') { newProject(); return true; }
        if (c == 'e') { showExport(); return true; }
    }
    return false;
}

// ============================================================== projects

juce::String MainComponent::projectName() const
{
    return currentFile == juce::File() ? juce::String ("Untitled") : currentFile.getFileNameWithoutExtension();
}

void MainComponent::updateTitle()
{
    const bool modified = Project::fingerprint (engine) != savedFingerprint;
    lastModified = modified;
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setName ("BizGroove - " + projectName() + (modified ? " (not saved)" : ""));
}

void MainComponent::showFileMenu()
{
    juce::PopupMenu m;
    const auto cmd = juce::String (
#if JUCE_MAC
        "Cmd"
#else
        "Ctrl"
#endif
    );
    m.addItem (1, "New project\t" + cmd + "+N");
    m.addItem (2, "Open...\t" + cmd + "+O");
    m.addSeparator();
    m.addItem (3, "Save\t" + cmd + "+S");
    m.addItem (4, "Save as...\t" + cmd + "+Shift+S");
    m.addSeparator();
    m.addItem (5, "Export audio or MIDI...\t" + cmd + "+E");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileButton), [this] (int result) {
        switch (result)
        {
            case 1: newProject(); break;
            case 2: openProject(); break;
            case 3: saveProject (false); break;
            case 4: saveProject (true); break;
            case 5: showExport(); break;
            default: break;
        }
    });
}

void MainComponent::confirmClose (std::function<void()> proceed)
{
    if (Project::fingerprint (engine) == savedFingerprint)
    {
        proceed();
        return;
    }
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle ("Save changes?")
                       .withMessage ("\"" + projectName() + "\" has changes that are not saved.")
                       .withButton ("Save")
                       .withButton ("Don't save")
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);
    juce::AlertWindow::showAsync (options, [this, proceed] (int button) {
        if (button == 1)      saveProject (false, [proceed] (bool saved) { if (saved) proceed(); });
        else if (button == 2) proceed();
    });
}

void MainComponent::newProject()
{
    confirmClose ([this] {
        engine.transport.playing.store (false);
        Project::applyVar (engine, defaultState);
        for (int t = 0; t < AudioEngine::numTracks; ++t)
        {
            engine.track (t).samplerEngine.clearSample();
            engine.track (t).sampleFileName = {};
            engine.track (t).sampleFileData.reset();
            for (int d = 0; d < DrumKit::numDrums; ++d)
                Project::clearPad (engine, t, d);
        }
        for (int i = 0; i < StemDeck::numStems; ++i)
        {
            engine.stems.clearStem (i);
            engine.stemFiles[(size_t) i] = {};
        }
        currentFile = juce::File();
        savedFingerprint = Project::fingerprint (engine);
        updateTitle();
    });
}

void MainComponent::openProject()
{
    confirmClose ([this] {
        chooser = std::make_unique<juce::FileChooser> ("Open a BizGroove project",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       juce::String ("*") + Project::fileExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) {
            if (fc.getResult() != juce::File())
                openFile (fc.getResult());
        });
    });
}

void MainComponent::openFile (const juce::File& file)
{
    juce::String error;
    engine.transport.playing.store (false);
    if (! Project::open (engine, file, formats, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the project", error);
        return;
    }
    if (error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Opened with a problem", error);
    currentFile = file;
    savedFingerprint = Project::fingerprint (engine);
    updateTitle();
}

void MainComponent::saveProject (bool saveAs, std::function<void (bool)> then)
{
    auto writeTo = [this, then] (juce::File file) {
        if (file == juce::File())
        {
            if (then) then (false);
            return;
        }
        file = file.withFileExtension (Project::fileExtension);
        juce::String error;
        if (! Project::save (engine, file, error))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", error);
            if (then) then (false);
            return;
        }
        currentFile = file;
        savedFingerprint = Project::fingerprint (engine);
        updateTitle();
        if (then) then (true);
    };

    if (! saveAs && currentFile != juce::File())
    {
        writeTo (currentFile);
        return;
    }

    const auto start = currentFile != juce::File()
                           ? currentFile
                           : juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Untitled" + juce::String (Project::fileExtension));
    chooser = std::make_unique<juce::FileChooser> ("Save the project", start, juce::String ("*") + Project::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [writeTo] (const juce::FileChooser& fc) { writeTo (fc.getResult()); });
}

// ============================================================== export

void MainComponent::showExport()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new ExportComponent (engine, [this] (const ExportOptions& options) {
        chooser = std::make_unique<juce::FileChooser> ("Choose the folder for the exported files",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this, options] (const juce::FileChooser& fc) {
                                  if (fc.getResult() != juce::File())
                                      runExport (options, fc.getResult());
                              });
    }));
    o.dialogTitle = "Export";
    o.dialogBackgroundColour = Colours::background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::runExport (const ExportOptions& options, const juce::File& folder)
{
    // Rendering runs faster than real time on a background thread, with the
    // sound card disconnected so the two never fight over the engine.
    struct Job final : public juce::ThreadWithProgressWindow
    {
        Job (AudioEngine& e, ExportOptions o, juce::File dir, juce::String base)
            : juce::ThreadWithProgressWindow ("Exporting...", true, true), engine (e), options (o), folder (dir), name (base) {}

        void run() override
        {
            AudioEngine::RenderJob job;
            job.sampleRate = options.sampleRate;
            job.tailSeconds = options.tailSeconds;
            job.playSong = options.wholeSong;
            job.bars = options.wholeSong ? engine.song.songLengthBars() : options.sceneBars;

            std::vector<int> passes;   // -1 = the mix, 0..15 = a track, -2 = the stem deck
            if (options.mixdown) passes.push_back (-1);
            if (options.perTrack)
            {
                for (int t = 0; t < AudioEngine::numTracks; ++t)
                    if (engine.trackHasContent (t)) passes.push_back (t);
                for (int i = 0; i < StemDeck::numStems; ++i)
                    if (engine.stems.hasAudio (i)) { passes.push_back (-2); break; }
            }

            const int steps = (int) passes.size() + (options.midi ? 1 : 0);
            int done = 0;
            for (int pass : passes)
            {
                juce::String label = pass == -1 ? "Mixdown" : pass == -2 ? "Stem deck" : "Track " + juce::String (pass + 1);
                setStatusMessage ("Rendering " + label + "...");
                job.onlyTrack = pass;
                juce::AudioBuffer<float> audio;
                const bool ok = engine.renderOffline (job, audio, [this, done, steps] (float p) {
                    setProgress ((done + p) / juce::jmax (1, steps));
                    return ! threadShouldExit();
                });
                if (! ok) { cancelled = true; return; }

                const auto fileName = pass == -1 ? name + " - mixdown.wav"
                                    : pass == -2 ? name + " - stem deck.wav"
                                                 : name + " - track " + juce::String (pass + 1).paddedLeft ('0', 2) + ".wav";
                juce::String error;
                const auto file = folder.getChildFile (juce::File::createLegalFileName (fileName));
                if (Exporter::writeWav (audio, options.sampleRate, options.bitDepth, file, error))
                    written.add (file.getFileName());
                else
                    errors.add (error);
                ++done;
            }

            if (options.midi)
            {
                setStatusMessage ("Writing the MIDI file...");
                juce::String error;
                const auto file = folder.getChildFile (juce::File::createLegalFileName (name + ".mid"));
                if (Exporter::writeMidi (engine, options.wholeSong, job.bars, file, error))
                    written.add (file.getFileName());
                else
                    errors.add (error);
            }
            setProgress (1.0);
        }

        AudioEngine& engine;
        ExportOptions options;
        juce::File folder;
        juce::String name;
        juce::StringArray written, errors;
        bool cancelled = false;
    };

    engine.transport.playing.store (false);
    deviceManager.removeAudioCallback (&sourcePlayer);

    auto job = std::make_shared<Job> (engine, options, folder, projectName());
    job->launchThread();
    // Wait without blocking the window, then reconnect the sound card and report.
    struct Waiter final : public juce::Timer
    {
        std::shared_ptr<Job> job;
        std::function<void()> done;
        void timerCallback() override
        {
            if (job->isThreadRunning())
                return;
            stopTimer();
            done();
        }
    };
    auto* waiter = new Waiter();
    waiter->job = job;
    waiter->done = [this, job, waiter, folder] {
        deviceManager.addAudioCallback (&sourcePlayer);   // prepares the engine for the sound card again
        juce::String message;
        if (job->cancelled)
            message = "Export cancelled.";
        else
            message = "Saved in " + folder.getFullPathName() + ":\n\n" + job->written.joinIntoString ("\n");
        if (! job->errors.isEmpty())
            message << "\n\nProblems:\n" << job->errors.joinIntoString ("\n");
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export", message);
        juce::MessageManager::callAsync ([waiter] { delete waiter; });
    };
    waiter->startTimer (100);
}

void MainComponent::timerCallback()
{
    handleRecorder();
    if (externalTicks++ % 15 == 0)
        updateExternalOutputs();

    // Keep computer-keyboard notes working: the on-screen keyboard takes key
    // focus, and keys it doesn't use (space) bubble up to keyPressed().
    if (panel.isVisible() && isShowing() && juce::Component::getCurrentlyFocusedComponent() == nullptr)
        panel.getKeyboard().grabKeyboardFocus();

    // The Sound menu shows the selected track's sound.
    {
        const int t = engine.selectedTrack.load();
        const int type = engine.track (t).type.load();
        if (t != shownSoundTrack || type != shownSoundType)
        {
            soundMenu.setSelectedId (type + 1, juce::dontSendNotification);
            soundLabel.setText ("Track " + juce::String (t + 1), juce::dontSendNotification);
            shownSoundTrack = t;
            shownSoundType = type;
            updateSoundsButton();
        }
    }

    // Controllers can change the transport: mirror it.
    const bool playing = engine.transport.playing.load();
    if (playButton.getToggleState() != playing)
    {
        playButton.setToggleState (playing, juce::dontSendNotification);
        playButton.setButtonText (playing ? "Stop" : "Play");
    }
    if (! tempoSlider.isMouseButtonDown() && std::abs (tempoSlider.getValue() - engine.transport.bpm.load()) > 0.05)
        tempoSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);

    if (++timerTicks % 60 == 0)
    {
        enableNewMidiInputs();
        updateClockOutputs();
        updateTitle();
    }

    // Meters fall back smoothly rather than flickering.
    bool changed = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float target = engine.getPeak (ch);
        const float next = juce::jmax (target, meterLevels[ch] * 0.85f);
        if (std::abs (next - meterLevels[ch]) > 0.001f)
        {
            meterLevels[ch] = next;
            changed = true;
        }
    }
    if (changed)
        repaint (meterArea);

    // Status line.
    juce::StringArray enabledNames;
    for (const auto& input : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
            enabledNames.add (input.name);

    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    auto* device = deviceManager.getCurrentAudioDevice();
    const juce::String text = (device != nullptr ? "Sound: " + device->getName() : juce::String ("No sound output found"))
                              + dot
                              + (enabledNames.isEmpty() ? juce::String ("No MIDI controller connected")
                                                        : "Controllers: " + enabledNames.joinIntoString (", "));
    if (midiStatus.getText() != text)
        midiStatus.setText (text, juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    // Output meter: two vertical bars, scaled in dB from -60 to 0.
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = meterArea.toFloat().withWidth ((float) meterArea.getWidth() / 2.0f - 2.0f)
                       .withX ((float) meterArea.getX() + (float) ch * (float) meterArea.getWidth() / 2.0f);
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (bar, 2.0f);

        const float db = juce::Decibels::gainToDecibels (meterLevels[ch], -60.0f);
        const float fill = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
        auto level = bar.withTrimmedTop (bar.getHeight() * (1.0f - fill));
        g.setColour (db > -1.0f ? juce::Colours::red : Colours::playhead);
        g.fillRoundedRectangle (level, 2.0f);
    }
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (16, 12);

    // Top bar
    auto top = r.removeFromTop (40);
    fileButton.setBounds (top.removeFromLeft (58).reduced (0, 4));
    top.removeFromLeft (10);
    title.setBounds (top.removeFromLeft (106));
    soundLabel.setBounds (top.removeFromLeft (62));
    soundMenu.setBounds (top.removeFromLeft (120).reduced (0, 6));
    top.removeFromLeft (6);
    soundsButton.setBounds (top.removeFromLeft (110).reduced (0, 4));
    top.removeFromLeft (12);
    playButton.setBounds (top.removeFromLeft (70).reduced (0, 4));
    top.removeFromLeft (10);
    tempoLabel.setBounds ({});   // room for the Sounds button; the slider shows BPM
    tempoSlider.setBounds (top.removeFromLeft (124));

    meterArea = top.removeFromRight (14).reduced (0, 4);
    top.removeFromRight (10);
    settingsButton.setBounds (top.removeFromRight (106).reduced (0, 4));
    top.removeFromRight (6);
    controllersButton.setBounds (top.removeFromRight (100).reduced (0, 4));
    top.removeFromRight (12);
    stemViewButton.setBounds (top.removeFromRight (92).reduced (0, 4));
    top.removeFromRight (4);
    mixerViewButton.setBounds (top.removeFromRight (64).reduced (0, 4));
    top.removeFromRight (4);
    songViewButton.setBounds (top.removeFromRight (60).reduced (0, 4));
    top.removeFromRight (4);
    panelViewButton.setBounds (top.removeFromRight (118).reduced (0, 4));

    // Status line at the bottom.
    midiStatus.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (6);
    r.removeFromTop (12);

    stemPanel.setBounds (r);
    mixerView.setBounds (r);
    if (sessionBar.isVisible())
    {
        sessionBar.setBounds (r.removeFromTop (80));
        r.removeFromTop (10);
    }
    panel.setBounds (r);
    songView.setBounds (r);
}

} // namespace bizgroove
