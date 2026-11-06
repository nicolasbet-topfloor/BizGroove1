#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace bizgroove
{

/** One MIDI message pattern: a note or a CC on a channel. */
struct MidiTrigger
{
    enum class Type { none, note, cc };

    Type type = Type::none;
    int channel = 0;   // 1..16, or 0 for any channel
    int number = 0;    // note or controller number

    bool isSet() const { return type != Type::none; }
    bool matches (const juce::MidiMessage& m) const;
    juce::String describe() const;

    static MidiTrigger fromMessage (const juce::MidiMessage& m);
    static MidiTrigger note (int channel, int number) { return { Type::note, channel, number }; }
    static MidiTrigger cc (int channel, int number)   { return { Type::cc, channel, number }; }

    bool operator== (const MidiTrigger& o) const { return type == o.type && channel == o.channel && number == o.number; }
};

/** A physical control on a controller, what it drives, and the MIDI it sends. */
struct ControlBinding
{
    juce::String control;   // label for the hardware, e.g. "Fader 1"
    juce::String target;    // a ControlTargets id, or empty for nothing
    MidiTrigger midi;
};

/** A named set of bindings for one kind of controller. Saved as JSON. */
struct ControllerProfile
{
    juce::String name;
    juce::String notes;               // shown in the Controllers window
    juce::StringArray deviceMatches;  // MIDI device names containing any of these use this profile
    bool passUnmappedNotes = true;    // unmapped notes play the selected track
    std::vector<ControlBinding> bindings;

    juce::var toVar() const;
    static ControllerProfile fromVar (const juce::var&);

    /** Built-in profiles shipped with BizGroove. */
    static std::vector<ControllerProfile> builtIns();
    static ControllerProfile launchkeyMiniMk3();
    static ControllerProfile launchkeyMini();   // original (MK1) Launchkey Mini
    static ControllerProfile kontrolF1();
    static ControllerProfile generic();

    static constexpr const char* defaultProfileName = "Novation Launchkey Mini MK3";
};

} // namespace bizgroove
