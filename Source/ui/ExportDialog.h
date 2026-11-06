#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace bizgroove
{

/** What to export, as chosen in the Export window. */
struct ExportOptions
{
    bool wholeSong = true;     // false = the current scene, looped
    int sceneBars = 4;
    bool mixdown = true;
    bool perTrack = false;
    bool midi = false;
    double sampleRate = 48000.0;
    int bitDepth = 24;
    double tailSeconds = 2.0;
};

/** The Export window's contents. Calls `onExport` with the choices. */
class ExportComponent final : public juce::Component
{
public:
    ExportComponent (AudioEngine& engine, std::function<void (const ExportOptions&)> onExport);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateSummary();

    AudioEngine& engine;
    std::function<void (const ExportOptions&)> onExport;

    juce::Label whatLabel, filesLabel, qualityLabel, summary;
    juce::ToggleButton song { "The whole song (Song mode)" }, scene { "The current scene, repeated" };
    juce::ComboBox sceneBars;
    juce::ToggleButton mixdown { "Mixdown: one stereo WAV of everything" };
    juce::ToggleButton perTrack { "One WAV per track (for mixing or mastering elsewhere)" };
    juce::ToggleButton midi { "MIDI file of the notes (to open in other music software)" };
    juce::ComboBox rate, depth, tail;
    juce::TextButton exportButton { "Export..." }, cancel { "Cancel" };
};

} // namespace bizgroove
