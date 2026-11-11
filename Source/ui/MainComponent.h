#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "engine/AudioEngine.h"
#include "engine/ControlTargets.h"
#include "engine/MidiRouter.h"
#include "engine/SynthLibrary.h"
#include "LookAndFeel.h"
#include "LaunchkeyPanel.h"
#include "StemPanel.h"
#include "SessionBar.h"
#include "SongView.h"
#include "PatternView.h"
#include "ControllersWindow.h"
#include "MixerView.h"
#include "ExportDialog.h"
#include "engine/Project.h"
#include "engine/Presets.h"
#include "engine/MidiClock.h"
#include "engine/SampleTools.h"

namespace bizgroove
{

/** The BizGroove window: a top bar (sound menu, transport, settings) above
    either the Launchkey-style panel or the stem deck. */
class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    /** Asks to save unsaved changes, then calls `proceed` unless cancelled. */
    void confirmClose (std::function<void()> proceed);

    /** Opens a project file directly (used at start-up). */
    void openFile (const juce::File& file);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void togglePlay();
    void showAudioSettings();
    void showControllers();
    enum class View { panel, pattern, song, mixer, stems };
    void showView (View view);
    void loadSynth (int index);
    void enableNewMidiInputs();
    void enableAllMidiInputs();
    void saveDeviceState();
    void updateClockOutputs();
    void updateExternalOutputs();
    int externalTicks = 0;
    bool externalInputAsked = false, vocoderInputAsked = false;

    // Recording into the Sampler
    void handleRecorder();
    bool ensureAudioInput();
    int recordingTrack = -1, recordingPad = 0;

    // Program bank
    void showSoundsMenu();
    void askForSoundName (const juce::String& heading, const juce::String& initial, std::function<void (juce::String)> onOk);
    void setCurrentSound (int track, const juce::String& name, const juce::File& file);
    void updateSoundsButton();

    // Projects and export
    void showFileMenu();
    void newProject();
    void openProject();
    void saveProject (bool saveAs, std::function<void (bool saved)> then = {});
    void showExport();
    void runExport (const ExportOptions& options, const juce::File& folder);
    void updateTitle();
    juce::String projectName() const;

    juce::AudioFormatManager formats;
    juce::File currentFile;
    juce::String savedFingerprint;
    juce::var defaultState;
    std::unique_ptr<juce::FileChooser> chooser;
    bool lastModified = false;

    BizLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 500 };

    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    AudioEngine engine;
    LiveOutput liveOutput { engine };   // plays the engine and sends MIDI clock
    ControlTargets controlTargets { engine };
    MidiRouter midiRouter { engine, controlTargets };
    juce::ApplicationProperties appProperties;
    juce::StringArray knownMidiInputs;
    int timerTicks = 0;

    // Top bar
    juce::Label title;
    juce::Label soundLabel;
    juce::ComboBox soundMenu;
    juce::TextButton soundsButton { "Sounds" };
    juce::TextButton playButton { "Play" };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label tempoLabel;
    juce::Slider masterVolume { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label volumeLabel;
    void showMasterVolume();
    juce::TextButton fileButton { "File" };
    juce::TextButton panelViewButton { "Keys & pads" }, patternViewButton { "Pattern" }, songViewButton { "Song" },
                     mixerViewButton { "Mixer" }, stemViewButton { "Stems" };
    juce::TextButton settingsButton { "Audio / MIDI" };
    juce::TextButton controllersButton { "Controllers" };
    juce::TextButton setupButton { "Setup" };          // Audio / MIDI and Controllers, in one menu
    juce::TextButton metronomeButton { "Click" };      // the metronome and its count-in
    void showMetronomeMenu();
    void showSetupMenu();
    int shownMetronome = -1;
    juce::Label midiStatus;

    // Views
    LaunchkeyPanel panel { engine, controlTargets };
    StemPanel stemPanel { engine };
    SongView songView { engine };
    PatternView patternView { engine };
    MixerView mixerView { engine };
    SessionBar sessionBar { engine };
    int shownSoundTrack = -1, shownSoundType = -1;

    /** The sound last loaded or saved on each track, for the Sounds button. */
    struct CurrentSound { juce::String name; juce::File file; int type = -1; };
    std::array<CurrentSound, AudioEngine::numTracks> currentSounds;

    juce::Rectangle<int> meterArea;
    float meterLevels[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace bizgroove
