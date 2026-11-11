#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <map>
#include <vector>

namespace bizgroove
{

class AudioEngine;

/** How a target reacts to a control. */
enum class TargetKind
{
    continuous,   // knob or fader: takes a 0..1 value
    toggle,       // button: each press flips it
    momentary,    // button: on while held
    trigger       // button: acts once per press
};

/** Everything in BizGroove a hardware control can drive, by stable id.

    Ids are saved in controller profiles, so never rename one; add new ids
    instead. Setters only write atomics, so they are safe from the MIDI thread.
*/
class ControlTargets
{
public:
    struct Target
    {
        juce::String id;
        juce::String name;
        juce::String group;
        TargetKind kind;
        std::function<void (float)> set;            // value 0..1, or 1/0 for press/release
        std::function<float()> get;                 // continuous only: current value as 0..1
        std::function<juce::String (float)> show;   // continuous only: 0..1 as words people read
    };

    explicit ControlTargets (AudioEngine& engine);

    const std::vector<Target>& all() const { return targets; }
    const Target* find (const juce::String& id) const;
    int indexOf (const juce::String& id) const;

    /** Message thread: finds the knobs the LFOs move (after a project or sound is loaded). */
    void resolveLfos (AudioEngine& engine) const;

    /** Whether an LFO may move this target: a knob of the track, not a transport, stem or page control. */
    static bool lfoCanMove (const Target& t);

    /** Applies a knob/fader value (0..1) or a button press/release. */
    void applyValue (const juce::String& id, float value01) const;
    void applyButton (const juce::String& id, bool pressed) const;

    /** Current position of a continuous target (0..1), and its readable value. */
    float getValue (const juce::String& id) const;
    juce::String describe (const juce::String& id) const;

private:
    void add (const juce::String& id, const juce::String& name, const juce::String& group,
              TargetKind kind, std::function<void (float)> set,
              std::function<float()> get = {}, std::function<juce::String (float)> show = {});

    std::vector<Target> targets;
    std::map<juce::String, size_t> index;
};

} // namespace bizgroove
