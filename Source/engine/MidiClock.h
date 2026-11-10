#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "AudioEngine.h"

namespace bizgroove
{

/** Live playback: runs the engine for the sound card and sends MIDI clock
    to external gear, so the Launchkey's arpeggiator (or a drum machine)
    follows BizGroove's tempo.

    The clock is the MIDI standard: 24 ticks per beat at the current tempo,
    sent all the time so the tempo is followed even when stopped, plus Start
    when Play is pressed and Stop when it is released. The ticks are counted
    in audio samples, in step with the sequencer. Offline export uses the
    engine directly, so it sends no clock.
*/
class LiveOutput final : public juce::AudioSource
{
public:
    explicit LiveOutput (AudioEngine& e) : engine (e) {}

    void prepareToPlay (int samplesPerBlockExpected, double sr) override
    {
        engine.prepareToPlay (samplesPerBlockExpected, sr);
        engine.recorder.deviceRate.store (sr);
        sampleRate = sr;
        samplesToNextTick = 0.0;
        wasPlaying = false;
        clockBuffer.ensureSize (512);
    }

    void releaseResources() override { engine.releaseResources(); }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        // The sound card's input arrives in the buffer: record it before the engine replaces it.
        engine.recorder.write (*info.buffer, info.startSample, info.numSamples, SampleRecorder::input);
        engine.setLiveInput (*info.buffer, info.startSample, info.numSamples);   // heard through External tracks
        engine.getNextAudioBlock (info);
        engine.recorder.write (*info.buffer, info.startSample, info.numSamples, SampleRecorder::output);
        engine.renderMetronome (*info.buffer, info.startSample, info.numSamples);   // heard, never recorded
        sendClock (info.numSamples);
        sendExternal();
    }

    /** Message thread: the MIDI outputs that receive the clock, by identifier. */
    void setOutputs (const juce::StringArray& identifiers)
    {
        if (identifiers == clockIds)
            return;
        clockIds = identifiers;
        reopen();
    }

    /** Message thread: the MIDI output each External track plays (empty: none). */
    void setTrackOutputs (const std::array<juce::String, AudioEngine::numTracks>& identifiers)
    {
        if (identifiers == trackIds)
            return;
        trackIds = identifiers;
        reopen();
    }

    /** The MIDI outputs that get the clock by default: the Launchkey's MIDI
        port (not its DAW port). */
    static bool isClockedByDefault (const juce::MidiDeviceInfo& device)
    {
        const auto& n = device.name;
        return n.containsIgnoreCase ("Launchkey") && ! n.containsIgnoreCase ("DAW") && ! n.startsWithIgnoreCase ("MIDIOUT2");
    }

private:
    void sendClock (int numSamples)
    {
        clockBuffer.clear();

        const bool playing = engine.transport.playing.load();
        if (playing != wasPlaying)
        {
            clockBuffer.addEvent (playing ? juce::MidiMessage::midiStart() : juce::MidiMessage::midiStop(), 0);
            if (playing)
                samplesToNextTick = 0.0;   // the first tick after Start is beat one
            wasPlaying = playing;
        }

        const double bpm = juce::jlimit (20.0, 300.0, engine.transport.bpm.load());
        const double interval = sampleRate * 60.0 / bpm / 24.0;
        while (samplesToNextTick < (double) numSamples)
        {
            clockBuffer.addEvent (juce::MidiMessage::midiClock(), juce::jmax (0, (int) samplesToNextTick));
            samplesToNextTick += interval;
        }
        samplesToNextTick -= (double) numSamples;

        const juce::ScopedTryLock sl (lock);
        if (sl.isLocked())
            for (int index : clockOutputs)
                outputs[index]->sendBlockOfMessages (clockBuffer, juce::Time::getMillisecondCounterHiRes(), sampleRate);
    }

    /** Passes each External track's MIDI for this block on to its device. */
    void sendExternal()
    {
        const juce::ScopedTryLock sl (lock);
        if (! sl.isLocked())
            return;
        for (int t = 0; t < AudioEngine::numTracks; ++t)
        {
            const auto& midi = engine.externalMidiFor (t);
            const int index = trackOutputs[(size_t) t];
            if (index >= 0 && ! midi.isEmpty())
                outputs[index]->sendBlockOfMessages (midi, juce::Time::getMillisecondCounterHiRes(), sampleRate);
        }
    }

    /** Opens the outputs the clock and the tracks need, each device once, and
        keeps the ones already open. */
    void reopen()
    {
        juce::StringArray wanted (clockIds);
        for (const auto& id : trackIds)
            if (id.isNotEmpty())
                wanted.addIfNotAlreadyThere (id);

        // Open the new devices first, outside the lock.
        juce::OwnedArray<juce::MidiOutput> fresh;
        juce::StringArray freshIds;
        for (const auto& id : wanted)
        {
            if (openIds.contains (id))
                continue;
            if (auto out = juce::MidiOutput::openDevice (id))
            {
                out->startBackgroundThread();
                fresh.add (out.release());
                freshIds.add (id);
            }
        }

        juce::OwnedArray<juce::MidiOutput> unused;   // closed after the lock is released
        {
            const juce::ScopedLock sl (lock);
            juce::OwnedArray<juce::MidiOutput> next;
            juce::StringArray nextIds;
            for (const auto& id : wanted)
            {
                const int old = openIds.indexOf (id);
                const int added = freshIds.indexOf (id);
                if (old >= 0)
                    next.add (outputs[old]);
                else if (added >= 0)
                    next.add (fresh[added]);
                else
                    continue;   // could not be opened
                nextIds.add (id);
            }
            for (int i = 0; i < outputs.size(); ++i)
                if (! next.contains (outputs[i]))
                    unused.add (outputs[i]);
            for (int i = 0; i < fresh.size(); ++i)
                if (! next.contains (fresh[i]))
                    unused.add (fresh[i]);
            outputs.clear (false);
            fresh.clear (false);
            outputs.swapWith (next);
            openIds = nextIds;

            clockOutputs.clearQuick();
            for (const auto& id : clockIds)
                if (openIds.contains (id))
                    clockOutputs.add (openIds.indexOf (id));
            for (size_t t = 0; t < trackOutputs.size(); ++t)
                trackOutputs[t] = trackIds[t].isNotEmpty() ? openIds.indexOf (trackIds[t]) : -1;
        }
    }

    AudioEngine& engine;
    juce::CriticalSection lock;
    juce::OwnedArray<juce::MidiOutput> outputs;   // each open device once; guarded by lock
    juce::StringArray openIds;                    // identifiers of outputs, in the same order
    juce::Array<int> clockOutputs;                // indexes into outputs
    std::array<int, AudioEngine::numTracks> trackOutputs { [] { std::array<int, AudioEngine::numTracks> a {}; a.fill (-1); return a; }() };
    juce::StringArray clockIds;                                // message thread
    std::array<juce::String, AudioEngine::numTracks> trackIds;  // message thread

    // audio thread
    juce::MidiBuffer clockBuffer;
    double sampleRate = 44100.0;
    double samplesToNextTick = 0.0;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE (LiveOutput)
};

} // namespace bizgroove
