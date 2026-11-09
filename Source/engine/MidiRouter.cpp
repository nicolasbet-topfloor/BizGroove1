#include "MidiRouter.h"
#include "AudioEngine.h"

namespace bizgroove
{

MidiRouter::MidiRouter (AudioEngine& e, const ControlTargets& t) : engine (e), targets (t)
{
    profiles = ControllerProfile::builtIns();
}

void MidiRouter::setProfiles (std::vector<ControllerProfile> newProfiles)
{
    const juce::ScopedLock sl (lock);
    profiles = std::move (newProfiles);
}

std::vector<ControllerProfile> MidiRouter::getProfiles() const
{
    const juce::ScopedLock sl (lock);
    return profiles;
}

void MidiRouter::updateProfile (const ControllerProfile& profile)
{
    const juce::ScopedLock sl (lock);
    for (auto& p : profiles)
    {
        if (p.name == profile.name)
        {
            p = profile;
            return;
        }
    }
    profiles.push_back (profile);
}

void MidiRouter::removeProfile (const juce::String& name)
{
    const juce::ScopedLock sl (lock);
    profiles.erase (std::remove_if (profiles.begin(), profiles.end(),
                                    [&] (const ControllerProfile& p) { return p.name == name; }),
                    profiles.end());
}

void MidiRouter::assignProfile (const juce::String& deviceIdentifier, const juce::String& profileName)
{
    const juce::ScopedLock sl (lock);
    if (profileName.isEmpty())
        assignments.erase (deviceIdentifier);
    else
        assignments[deviceIdentifier] = profileName;
}

juce::String MidiRouter::getAssignedProfile (const juce::String& deviceIdentifier) const
{
    const juce::ScopedLock sl (lock);
    const auto it = assignments.find (deviceIdentifier);
    return it != assignments.end() ? it->second : juce::String();
}

std::map<juce::String, juce::String> MidiRouter::getAssignments() const
{
    const juce::ScopedLock sl (lock);
    return assignments;
}

const ControllerProfile* MidiRouter::findProfile (const juce::String& name) const
{
    for (const auto& p : profiles)
        if (p.name == name)
            return &p;
    return nullptr;
}

juce::String MidiRouter::resolveProfileName (const juce::String& deviceIdentifier, const juce::String& deviceName) const
{
    const juce::ScopedLock sl (lock);

    const auto it = assignments.find (deviceIdentifier);
    if (it != assignments.end() && findProfile (it->second) != nullptr)
        return it->second;

    for (const auto& p : profiles)
        for (const auto& match : p.deviceMatches)
            if (match.isNotEmpty() && deviceName.containsIgnoreCase (match))
                return p.name;

    return ControllerProfile::defaultProfileName;
}

void MidiRouter::startLearn (const juce::String& profileName, int bindingIndex, std::function<void (MidiTrigger)> onLearned)
{
    const juce::ScopedLock sl (lock);
    learnProfile = profileName;
    learnIndex = bindingIndex;
    learnCallback = std::move (onLearned);
}

void MidiRouter::cancelLearn()
{
    const juce::ScopedLock sl (lock);
    learnIndex = -1;
    learnCallback = nullptr;
}

bool MidiRouter::isLearning() const
{
    const juce::ScopedLock sl (lock);
    return learnIndex >= 0;
}

juce::String MidiRouter::getLastMessageDescription() const
{
    const juce::ScopedLock sl (lock);
    return lastMessage;
}

void MidiRouter::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (source != nullptr)
        route (source->getIdentifier(), source->getName(), message, source);
    else
        route ({}, {}, message, nullptr);
}

void MidiRouter::route (const juce::String& deviceIdentifier, const juce::String& deviceName,
                        const juce::MidiMessage& message, juce::MidiInput* source)
{
    // Transport messages from any device (the Launchkey Play button sends these).
    if (message.isMidiStart() || message.isMidiContinue()) { engine.transport.playing.store (true);  return; }
    if (message.isMidiStop())                               { engine.transport.playing.store (false); return; }
    if (message.isActiveSense() || message.isMidiClock())   return;

    std::vector<std::pair<juce::String, MidiTrigger>> hits;  // target id + how it matched
    bool passThrough = true;
    std::function<void (MidiTrigger)> learnedCallback;
    MidiTrigger learned;

    {
        const juce::ScopedLock sl (lock);

        const auto describeDevice = deviceName.isNotEmpty() ? deviceName : juce::String ("on-screen");
        if (message.isNoteOnOrOff() || message.isController())
            lastMessage = describeDevice + ":  " + MidiTrigger::fromMessage (message).describe()
                          + "  value " + juce::String (message.isController() ? message.getControllerValue() : message.getVelocity());

        // Learning takes the next note-on or CC, from any device.
        if (learnIndex >= 0 && ((message.isNoteOn() && message.getVelocity() > 0) || message.isController()))
        {
            for (auto& p : profiles)
            {
                if (p.name == learnProfile && juce::isPositiveAndBelow (learnIndex, (int) p.bindings.size()))
                {
                    learned = MidiTrigger::fromMessage (message);
                    p.bindings[(size_t) learnIndex].midi = learned;
                    break;
                }
            }
            learnedCallback = std::move (learnCallback);
            learnCallback = nullptr;
            learnIndex = -1;
        }
        else
        {
            const auto name = assignments.count (deviceIdentifier) != 0 && findProfile (assignments[deviceIdentifier]) != nullptr
                                  ? assignments[deviceIdentifier]
                                  : juce::String();

            const ControllerProfile* profile = findProfile (name);
            if (profile == nullptr)
            {
                // Same rule as resolveProfileName, inlined because we hold the lock.
                for (const auto& p : profiles)
                    for (const auto& match : p.deviceMatches)
                        if (profile == nullptr && match.isNotEmpty() && deviceName.containsIgnoreCase (match))
                            profile = &p;
                if (profile == nullptr)
                    profile = findProfile (ControllerProfile::defaultProfileName);
            }

            if (profile != nullptr)
            {
                // In "play" mode the pads skip their step bindings, so their notes reach the drums.
                const bool padsPlay = engine.padsPlay.load();
                for (const auto& b : profile->bindings)
                    if (b.target.isNotEmpty() && b.midi.matches (message)
                        && ! (padsPlay && b.target.startsWith ("seq.step.")))
                        hits.emplace_back (b.target, b.midi);

                if (! profile->passUnmappedNotes && message.isNoteOnOrOff())
                    passThrough = false;
            }
        }
    }

    if (learnedCallback != nullptr)
    {
        juce::MessageManager::callAsync ([cb = std::move (learnedCallback), learned] { cb (learned); });
        return;
    }

    if (! hits.empty())
    {
        for (const auto& [target, trigger] : hits)
        {
            if (message.isController())
                targets.applyValue (target, message.getControllerValue() / 127.0f);
            else
                targets.applyButton (target, message.isNoteOn() && message.getVelocity() > 0);
        }
        return;
    }

    if (passThrough && ! message.isSysEx())
    {
        // On a drum track, a key picks the drum that the knobs and pads edit.
        auto& tr = engine.selected();
        if (message.isNoteOn() && message.getVelocity() > 0 && tr.type.load() == Track::drums)
        {
            const int drum = DrumKit::drumForNote (message.getNoteNumber());
            if (drum >= 0)
                tr.selectedDrum.store (drum);
        }
        engine.midiInput.handleIncomingMidiMessage (source, message);
    }
}

} // namespace bizgroove
