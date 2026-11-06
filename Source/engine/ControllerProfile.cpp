#include "ControllerProfile.h"

namespace bizgroove
{

// ============================================================== MidiTrigger

bool MidiTrigger::matches (const juce::MidiMessage& m) const
{
    if (type == Type::none)
        return false;
    if (channel != 0 && m.getChannel() != channel)
        return false;

    if (type == Type::note)
        return (m.isNoteOn() || m.isNoteOff()) && m.getNoteNumber() == number;

    return m.isController() && m.getControllerNumber() == number;
}

juce::String MidiTrigger::describe() const
{
    const auto ch = channel == 0 ? juce::String ("any ch") : "ch " + juce::String (channel);
    switch (type)
    {
        case Type::note: return "Note " + juce::String (number) + " ("
                                + juce::MidiMessage::getMidiNoteName (number, true, true, 4) + "), " + ch;
        case Type::cc:   return "CC " + juce::String (number) + ", " + ch;
        case Type::none: break;
    }
    return "not set";
}

MidiTrigger MidiTrigger::fromMessage (const juce::MidiMessage& m)
{
    if (m.isNoteOnOrOff()) return note (m.getChannel(), m.getNoteNumber());
    if (m.isController())  return cc (m.getChannel(), m.getControllerNumber());
    return {};
}

// ============================================================== JSON

juce::var ControllerProfile::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("name", name);
    root->setProperty ("notes", notes);
    root->setProperty ("passUnmappedNotes", passUnmappedNotes);

    juce::Array<juce::var> matches;
    for (const auto& s : deviceMatches)
        matches.add (s);
    root->setProperty ("deviceMatches", matches);

    juce::Array<juce::var> list;
    for (const auto& b : bindings)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("control", b.control);
        o->setProperty ("target", b.target);
        o->setProperty ("type", b.midi.type == MidiTrigger::Type::note ? "note"
                                : b.midi.type == MidiTrigger::Type::cc ? "cc" : "none");
        o->setProperty ("channel", b.midi.channel);
        o->setProperty ("number", b.midi.number);
        list.add (juce::var (o));
    }
    root->setProperty ("bindings", list);
    root->setProperty ("formatVersion", 1);
    return juce::var (root);
}

ControllerProfile ControllerProfile::fromVar (const juce::var& v)
{
    ControllerProfile p;
    p.name = v.getProperty ("name", "Untitled").toString();
    p.notes = v.getProperty ("notes", "").toString();
    p.passUnmappedNotes = (bool) v.getProperty ("passUnmappedNotes", true);

    if (const auto* matches = v.getProperty ("deviceMatches", {}).getArray())
        for (const auto& m : *matches)
            p.deviceMatches.add (m.toString());

    if (const auto* list = v.getProperty ("bindings", {}).getArray())
    {
        for (const auto& o : *list)
        {
            ControlBinding b;
            b.control = o.getProperty ("control", "").toString();
            b.target = o.getProperty ("target", "").toString();
            const auto type = o.getProperty ("type", "none").toString();
            b.midi.type = type == "note" ? MidiTrigger::Type::note
                        : type == "cc"   ? MidiTrigger::Type::cc : MidiTrigger::Type::none;
            b.midi.channel = juce::jlimit (0, 16, (int) o.getProperty ("channel", 0));
            b.midi.number = juce::jlimit (0, 127, (int) o.getProperty ("number", 0));
            p.bindings.push_back (b);
        }
    }
    return p;
}

// ============================================================== built-ins

ControllerProfile ControllerProfile::launchkeyMiniMk3()
{
    ControllerProfile p;
    p.name = defaultProfileName;
    p.notes = "Launchkey Mini MK3 in its normal mode. Keys, pitch strip and modulation strip work "
              "at once. Novation does not publish the MIDI numbers of the pads, knobs and buttons, "
              "so teach them once: press 'Learn all' and touch each control when it is named. "
              "First hold Shift on the Launchkey and press the Drum pad (2nd) and the last Custom pad (8th).";
    p.deviceMatches = { "Launchkey Mini MK3" };
    p.passUnmappedNotes = true;

    for (int i = 0; i < 8; ++i)
        p.bindings.push_back ({ "Knob " + juce::String (i + 1), "panel.knob." + juce::String (i + 1), {} });
    for (int i = 0; i < 8; ++i)
        p.bindings.push_back ({ "Pad top " + juce::String (i + 1), "seq.step." + juce::String (i + 1), {} });
    for (int i = 0; i < 8; ++i)
        p.bindings.push_back ({ "Pad bottom " + juce::String (i + 1), "seq.step." + juce::String (i + 9), {} });

    p.bindings.push_back ({ "Play button", "transport.playstop", {} });
    p.bindings.push_back ({ "Record button", "", {} });          // nothing to record into yet
    p.bindings.push_back ({ "Track previous", "track.prev", {} });
    p.bindings.push_back ({ "Track next", "track.next", {} });
    p.bindings.push_back ({ "> (launch) button", "scene.next", {} });
    p.bindings.push_back ({ "Stop/Solo/Mute button", "track.mute", {} });
    return p;
}

ControllerProfile ControllerProfile::launchkeyMini()
{
    ControllerProfile p;
    p.name = "Novation Launchkey Mini (original)";
    p.notes = "Original Launchkey Mini, with InControl off (its light unlit). Keys play the track; "
              "pads toggle steps 1-16; knobs control the synth; the top round button plays/stops, "
              "the bottom one stops; the up/down arrows nudge the tempo. If a control does "
              "nothing, press Learn on its row and move it.";
    p.deviceMatches = { "Launchkey Mini" };   // checked after the MK3 profile
    p.passUnmappedNotes = true;

    // Values from Novation's MIDI messages table for the original Launchkey Mini.
    // Knobs 1-8: CC 21-28 (any channel).
    for (int i = 0; i < 8; ++i)
        p.bindings.push_back ({ "Knob " + juce::String (i + 1), "panel.knob." + juce::String (i + 1), MidiTrigger::cc (0, 21 + i) });

    // Pads on channel 10, so they never clash with the keys. Top row: 40-43, 48-51.
    // Bottom row: 36-39, 44-47.
    const int topRow[8]    = { 40, 41, 42, 43, 48, 49, 50, 51 };
    const int bottomRow[8] = { 36, 37, 38, 39, 44, 45, 46, 47 };
    for (int i = 0; i < 8; ++i)
        p.bindings.push_back ({ "Pad top " + juce::String (i + 1), "seq.step." + juce::String (i + 1),
                                MidiTrigger::note (10, topRow[i]) });
    for (int i = 0; i < 8; ++i)
        p.bindings.push_back ({ "Pad bottom " + juce::String (i + 1), "seq.step." + juce::String (i + 9),
                                MidiTrigger::note (10, bottomRow[i]) });

    // The original has no transport buttons: use the round buttons and arrows.
    p.bindings.push_back ({ "Round button top", "transport.playstop", MidiTrigger::cc (0, 108) });
    p.bindings.push_back ({ "Round button bottom", "transport.stop", MidiTrigger::cc (0, 109) });
    p.bindings.push_back ({ "Arrow up", "transport.tempoup", MidiTrigger::cc (0, 104) });
    p.bindings.push_back ({ "Arrow down", "transport.tempodown", MidiTrigger::cc (0, 105) });
    p.bindings.push_back ({ "Track left", "track.prev", MidiTrigger::cc (0, 106) });
    p.bindings.push_back ({ "Track right", "track.next", MidiTrigger::cc (0, 107) });
    return p;
}

ControllerProfile ControllerProfile::kontrolF1()
{
    ControllerProfile p;
    p.name = "Native Instruments Traktor Kontrol F1";
    p.notes = "Stem performance: faders = stem volume, knobs = filter, stop buttons = mute; "
              "pad rows: 1 solo, 2 restart on the beat, 3 roll 1 beat (hold), 4 roll 1/16 (hold). "
              "The F1 must be in MIDI mode. Its MIDI numbers depend on its template, so press "
              "'Learn all' once and move each control when asked.";
    p.deviceMatches = { "Kontrol F1", "Traktor Kontrol F1" };
    p.passUnmappedNotes = false;

    // Pads are numbered left to right, top to bottom. Column = stem.
    const char* rowTargets[4] = { "solo", "restart", "roll1", "roll4" };
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            p.bindings.push_back ({ "Pad row " + juce::String (row + 1) + " col " + juce::String (col + 1),
                                    "stem." + juce::String (col + 1) + "." + rowTargets[row], {} });

    for (int i = 0; i < 4; ++i)
        p.bindings.push_back ({ "Knob " + juce::String (i + 1), "stem." + juce::String (i + 1) + ".filter", {} });
    for (int i = 0; i < 4; ++i)
        p.bindings.push_back ({ "Fader " + juce::String (i + 1), "stem." + juce::String (i + 1) + ".volume", {} });
    for (int i = 0; i < 4; ++i)
        p.bindings.push_back ({ "Stop button " + juce::String (i + 1), "stem." + juce::String (i + 1) + ".mute", {} });

    p.bindings.push_back ({ "Browse encoder push", "transport.playstop", {} });
    p.bindings.push_back ({ "Sync button", "transport.play", {} });
    return p;
}

ControllerProfile ControllerProfile::generic()
{
    ControllerProfile p;
    p.name = "Generic MIDI controller";
    p.notes = "For any other controller. Add a row for each knob, fader or pad, choose what it "
              "drives, then press Learn and move the control.";
    p.passUnmappedNotes = true;
    for (int i = 0; i < 8; ++i)
        p.bindings.push_back ({ "Control " + juce::String (i + 1), "", {} });
    return p;
}

std::vector<ControllerProfile> ControllerProfile::builtIns()
{
    // Order matters: the first profile whose name matches the device wins.
    return { launchkeyMiniMk3(), launchkeyMini(), kontrolF1(), generic() };
}

} // namespace bizgroove
