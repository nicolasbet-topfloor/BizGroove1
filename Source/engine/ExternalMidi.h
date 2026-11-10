#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace bizgroove
{

/** Settings of an External MIDI track: it plays gear outside BizGroove (the
    FM-1, an app on an iPhone or iPad, another synth) over MIDI, and can bring
    that gear's sound back in through the sound card's input, so it goes
    through the mixer like any other track. */
struct ExternalParams
{
    static constexpr int numKnobs = 8;

    std::atomic<int> channel { 1 };   // 1..16

    /** The CC each panel knob sends. The default is the standard map of sound
        controllers, which SLOOP on the FM-1 follows: brightness 74, edge 71,
        fade in 73, punch 75, fade out 72, glide 5, reverb 91, delay 94. */
    std::array<std::atomic<int>, numKnobs> ccNumber { { 74, 71, 73, 75, 72, 5, 91, 94 } };
    std::array<std::atomic<float>, numKnobs> ccValue { { 0.5f, 0.0f, 0.0f, 0.5f, 0.3f, 0.0f, 0.0f, 0.0f } };
    std::array<std::atomic<bool>, numKnobs> ccChanged {};   // set by a knob move, cleared when sent

    std::atomic<int>  program { 0 };              // 0..127
    std::atomic<bool> programChanged { false };
    std::atomic<bool> drums { false };            // the steps play drum notes (36-51), like a Drums track
    std::atomic<bool> monitor { false };          // hear the sound card's input through this track
    std::atomic<int>  inputPair { 0 };            // 0 = inputs 1-2, 1 = inputs 3-4 ...
    std::atomic<bool> panic { false };            // stop every note on the device

    int sentChannel = 0;   // audio thread only: the channel the device last heard (0: none yet)
};

/** Audio thread: this block's MIDI for the device. The track's notes (and
    bend and controllers) on the chosen channel, plus knob moves and program
    changes. "active" is false once the track is no longer External: then only
    a pending stop for hanging notes goes out. */
inline void buildExternalMidi (ExternalParams& p, const juce::MidiBuffer& in, juce::MidiBuffer& out, bool active)
{
    out.clear();
    const int channel = juce::jlimit (1, 16, p.channel.load());

    const bool panic = p.panic.exchange (false);
    if (p.sentChannel > 0 && (panic || channel != p.sentChannel))
    {
        out.addEvent (juce::MidiMessage::controllerEvent (p.sentChannel, 64, 0), 0);   // sustain pedal up
        out.addEvent (juce::MidiMessage::allNotesOff (p.sentChannel), 0);
    }
    p.sentChannel = channel;

    if (! active)
        return;

    if (p.programChanged.exchange (false))
        out.addEvent (juce::MidiMessage::programChange (channel, juce::jlimit (0, 127, p.program.load())), 0);

    for (size_t k = 0; k < ExternalParams::numKnobs; ++k)
        if (p.ccChanged[k].exchange (false))
            out.addEvent (juce::MidiMessage::controllerEvent (channel, juce::jlimit (0, 127, p.ccNumber[k].load()),
                                                              juce::roundToInt (127.0f * juce::jlimit (0.0f, 1.0f, p.ccValue[k].load()))), 0);

    for (const auto meta : in)
    {
        auto m = meta.getMessage();
        if (m.getChannel() <= 0)
            continue;   // SysEx and timing messages are not the track's
        m.setChannel (channel);
        out.addEvent (m, meta.samplePosition);
    }
}

/** The External MIDI settings as JSON, for projects and saved sounds. */
inline juce::var externalParamsToVar (const ExternalParams& p, const juce::String& deviceName)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("device", deviceName);
    o->setProperty ("channel", p.channel.load());
    juce::Array<juce::var> numbers, values;
    for (size_t k = 0; k < ExternalParams::numKnobs; ++k)
    {
        numbers.add (p.ccNumber[k].load());
        values.add (p.ccValue[k].load());
    }
    o->setProperty ("ccNumbers", numbers);
    o->setProperty ("ccValues", values);
    o->setProperty ("program", p.program.load());
    o->setProperty ("drums", p.drums.load());
    o->setProperty ("monitor", p.monitor.load());
    o->setProperty ("inputPair", p.inputPair.load());
    return juce::var (o);
}

/** Reads settings written by externalParamsToVar(); anything missing gets its default. */
inline void applyExternalParams (ExternalParams& p, juce::String& deviceName, const juce::var& v)
{
    static constexpr int defaultCc[ExternalParams::numKnobs] = { 74, 71, 73, 75, 72, 5, 91, 94 };
    static constexpr float defaultValue[ExternalParams::numKnobs] = { 0.5f, 0.0f, 0.0f, 0.5f, 0.3f, 0.0f, 0.0f, 0.0f };

    deviceName = v.hasProperty ("device") ? v["device"].toString() : juce::String();
    p.channel.store (juce::jlimit (1, 16, v.hasProperty ("channel") ? (int) v["channel"] : 1));
    const auto* numbers = v["ccNumbers"].getArray();
    const auto* values = v["ccValues"].getArray();
    for (int k = 0; k < ExternalParams::numKnobs; ++k)
    {
        p.ccNumber[(size_t) k].store (juce::jlimit (0, 127, numbers != nullptr && k < numbers->size() ? (int) (*numbers)[k] : defaultCc[k]));
        p.ccValue[(size_t) k].store (values != nullptr && k < values->size() ? (float) (double) (*values)[k] : defaultValue[k]);
        p.ccChanged[(size_t) k].store (false);   // opening a project does not change the gear's sound
    }
    p.program.store (juce::jlimit (0, 127, v.hasProperty ("program") ? (int) v["program"] : 0));
    p.programChanged.store (false);
    p.drums.store (v.hasProperty ("drums") && (bool) v["drums"]);
    p.monitor.store (v.hasProperty ("monitor") && (bool) v["monitor"]);
    p.inputPair.store (juce::jlimit (0, 7, v.hasProperty ("inputPair") ? (int) v["inputPair"] : 0));
}

} // namespace bizgroove
