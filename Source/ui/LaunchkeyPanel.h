#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/AudioEngine.h"
#include "engine/ControlTargets.h"
#include "engine/SynthLibrary.h"
#include "StepGrid.h"
#include "SampleEditor.h"

namespace bizgroove
{

/** A knob with a plain name above it and its value in words below. It drives
    a control target, so it moves when the hardware knob moves. */
class PanelKnob final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PanelKnob (const ControlTargets& targets);

    void setKnob (const SynthKnob& knob);

    /** The word printed beside this knob on the Launchkey (may be empty). */
    void setPrinted (const juce::String& text) { printedName.setText (text, juce::dontSendNotification); }
    void refresh();
    void resized() override;

private:
    const ControlTargets& targets;
    juce::String targetId;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label printedName, name, value;
};

/** A touch strip like the Launchkey's. The pitch strip springs back to the
    centre when released; the modulation strip stays where it is left. */
class TouchStrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TouchStrip (const juce::String& label, std::atomic<float>& value, bool bipolarSpringBack);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();

private:
    void setFromY (float y);

    juce::String label;
    std::atomic<float>& value;   // pitch: -1..1, modulation: 0..1
    const bool bipolar;
    float shown = 0.0f;
};

/** 25 keys with solid black keys, as on the Launchkey. */
class Keys final : public juce::MidiKeyboardComponent
{
public:
    explicit Keys (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, horizontalKeyboard) {}

    // Cmd/Ctrl shortcuts (save, open, export...) go to the window, not to notes.
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getModifiers().isCommandDown())
            return false;
        return juce::MidiKeyboardComponent::keyPressed (key);
    }

    bool keyStateChanged (bool isKeyDown) override
    {
        if (juce::ModifierKeys::currentModifiers.isCommandDown())
            return false;
        return juce::MidiKeyboardComponent::keyStateChanged (isKeyDown);
    }

    void drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour) override
    {
        g.setColour (juce::Colour (0xff15161a));
        g.fillRoundedRectangle (area.withTrimmedTop (-4.0f), 3.0f);
        if (isDown || isOver)
        {
            g.setColour (findColour (isDown ? keyDownOverlayColourId : mouseOverKeyOverlayColourId));
            g.fillRoundedRectangle (area.withTrimmedTop (-4.0f), 3.0f);
        }
    }
};

/** Play and record buttons drawn with their symbols, like on the hardware. */
class SymbolButton final : public juce::Button
{
public:
    enum class Symbol { play, record };
    explicit SymbolButton (Symbol s) : juce::Button ({}), symbol (s) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Symbol symbol;
};

/** The main view: laid out like a Novation Launchkey Mini MK3, so every
    on-screen control sits where its twin is on the hardware. */
class LaunchkeyPanel final : public juce::Component, public juce::FileDragAndDropTarget,
                             private juce::Timer, private juce::MidiKeyboardState::Listener
{
public:
    LaunchkeyPanel (AudioEngine& engine, const ControlTargets& targets);
    ~LaunchkeyPanel() override { engine.keyboardState.removeListener (this); }

    /** Re-labels the knobs for the synth loaded on the track. */
    void loadSynth (const SynthDefinition& synth);

    juce::MidiKeyboardComponent& getKeyboard() { return keyboard; }

    void paint (juce::Graphics&) override;
    void resized() override;

    // Drop a recording onto the panel to load it into a Sampler track.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void chooseSample();
    void chooseRecording();
    void showPadMenu();
    void loadSample (const juce::File& file);
    void timerCallback() override;
    void handleNoteOn (juce::MidiKeyboardState*, int, int note, float) override;
    void handleNoteOff (juce::MidiKeyboardState*, int, int, float) override {}
    void showMore();
    void shiftOctave (int octaves);
    void openSampleEditor();
    void showMidiOutMenu();
    void showChannelMenu();
    juce::String externalState() const;
    juce::String shownExternal;

    AudioEngine& engine;
    const ControlTargets& targets;
    std::vector<SynthKnob> moreKnobs;

    TouchStrip pitch, modulation;

    // Left column
    juce::TextButton shiftButton { "Shift" }, transposeButton { "Transpose" };
    juce::TextButton octaveDown { "-" }, octaveUp { "+" };

    // Knobs and pads
    juce::OwnedArray<PanelKnob> knobs;
    juce::TextButton moreButton { "More" };
    juce::TextButton fxButton { "Effects" };
    juce::TextButton oscButton { "Oscillators" };
    juce::TextButton voiceTextButton { "Text..." };
    juce::String shownVoiceText;
    juce::TextButton loadSampleButton { "Load sample..." };
    juce::TextButton editSampleButton { "Edit..." }, recSourceButton { "Rec: mic / line in" }, padsModeButton { "Pads: play" };
    juce::TextButton holdButton { "Hold" };
    bool shownRecording = false;
    juce::AudioFormatManager formats;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String shownSample;
    StepGrid pads { engine };

    // Beside the pads
    juce::TextButton launchButton { ">" }, stopSoloMuteButton { "Stop\nSolo\nMute" };
    juce::TextButton trackPrev, trackNext;
    juce::Label trackInfo;
    int shownTrack = -1, shownType = -1, shownDrum = -1;
    void showTrack();

    // Right column
    juce::TextButton arpButton { "Arp" }, fixedChordButton { "Fixed\nChord" };
    SymbolButton playButton { SymbolButton::Symbol::play }, recordButton { SymbolButton::Symbol::record };

    Keys keyboard { engine.keyboardState };
    int lowestKey = 48;

    // Painted labels, set in resized()
    juce::Rectangle<int> bodyArea, octaveLabelArea, trackLabelArea, recordLabelArea,
                         padModeLabelArea, wordmarkArea, launchArrowArea, ssmArrowArea;
};

} // namespace bizgroove
