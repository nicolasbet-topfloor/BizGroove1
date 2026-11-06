#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "ControllerProfile.h"
#include "ControlTargets.h"
#include <functional>
#include <map>

namespace bizgroove
{

class AudioEngine;

/** Sits between the MIDI inputs and the engine.

    Every incoming message is checked against the profile assigned to its
    device. A match drives a control target; anything unmatched (normally the
    keys) passes through to the engine to be played. MIDI Start/Stop always
    drive the transport. A learn request captures the next note or CC.

    MIDI arrives on its own thread; the profile library is guarded by a lock
    that the message thread only holds briefly.
*/
class MidiRouter final : public juce::MidiInputCallback
{
public:
    MidiRouter (AudioEngine& engine, const ControlTargets& targets);

    // ---------------------------------------------------------- message thread
    void setProfiles (std::vector<ControllerProfile> profiles);
    std::vector<ControllerProfile> getProfiles() const;
    void updateProfile (const ControllerProfile& profile);   // replaces by name, or adds
    void removeProfile (const juce::String& name);

    /** Explicit choice for one device; an empty name means automatic. */
    void assignProfile (const juce::String& deviceIdentifier, const juce::String& profileName);
    juce::String getAssignedProfile (const juce::String& deviceIdentifier) const;
    std::map<juce::String, juce::String> getAssignments() const;

    /** The profile a device actually uses: its assignment, else a name match,
        else the default (Launchkey Mini). */
    juce::String resolveProfileName (const juce::String& deviceIdentifier, const juce::String& deviceName) const;

    /** Arms learning for one binding of one profile. The callback runs on the
        message thread after the binding has been stored. */
    void startLearn (const juce::String& profileName, int bindingIndex, std::function<void (MidiTrigger)> onLearned);
    void cancelLearn();
    bool isLearning() const;

    /** Last message seen, for the MIDI monitor in the Controllers window. */
    juce::String getLastMessageDescription() const;

    // ---------------------------------------------------------- MIDI thread
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    /** Same as above without a device: used by tests and the on-screen UI. */
    void route (const juce::String& deviceIdentifier, const juce::String& deviceName,
                const juce::MidiMessage& message, juce::MidiInput* source = nullptr);

private:
    const ControllerProfile* findProfile (const juce::String& name) const;

    AudioEngine& engine;
    const ControlTargets& targets;

    mutable juce::CriticalSection lock;
    std::vector<ControllerProfile> profiles;
    std::map<juce::String, juce::String> assignments;

    juce::String learnProfile;
    int learnIndex = -1;
    std::function<void (MidiTrigger)> learnCallback;

    juce::String lastMessage;
};

} // namespace bizgroove
