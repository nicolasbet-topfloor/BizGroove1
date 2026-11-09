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
        engine.getNextAudioBlock (info);
        engine.recorder.write (*info.buffer, info.startSample, info.numSamples, SampleRecorder::output);
        sendClock (info.numSamples);
    }

    /** Message thread: the MIDI outputs that receive the clock, by identifier. */
    void setOutputs (const juce::StringArray& identifiers)
    {
        if (identifiers == currentIds)
            return;

        juce::OwnedArray<juce::MidiOutput> opened;
        for (const auto& id : identifiers)
        {
            if (auto out = juce::MidiOutput::openDevice (id))
            {
                out->startBackgroundThread();
                opened.add (out.release());
            }
        }

        const juce::ScopedLock sl (lock);
        outputs.swapWith (opened);   // the old outputs close when `opened` goes, after the lock is released
        currentIds = identifiers;
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
            for (auto* out : outputs)
                out->sendBlockOfMessages (clockBuffer, juce::Time::getMillisecondCounterHiRes(), sampleRate);
    }

    AudioEngine& engine;
    juce::CriticalSection lock;
    juce::OwnedArray<juce::MidiOutput> outputs;
    juce::StringArray currentIds;   // message thread

    // audio thread
    juce::MidiBuffer clockBuffer;
    double sampleRate = 44100.0;
    double samplesToNextTick = 0.0;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE (LiveOutput)
};

} // namespace bizgroove
