#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace bizgroove
{

/** Performance effects on the whole mix, played live while held: Repeat
    loops the last slice of music (1/4 to 1/32, in time), Stutter chops a tiny
    slice into a machine-gun rattle, Tape stop slows everything down to a halt
    like a turntable or tape machine switched off. Shared with the UI and
    controllers. */
struct PerformSettings
{
    static constexpr int numDivisions = 4;

    std::atomic<bool> repeat { false }, stutter { false }, tapeStop { false };
    std::atomic<int> repeatDivision { 1 };   // 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32
    std::atomic<float> stopTime { 0.4f };    // 0..1 = 0.15 .. 2 seconds to stop

    static juce::String divisionName (int d)
    {
        static const char* names[numDivisions] = { "1/4", "1/8", "1/16", "1/32" };
        return names[juce::jlimit (0, numDivisions - 1, d)];
    }

    static double divisionBeats (int d)
    {
        static constexpr double beats[numDivisions] = { 1.0, 0.5, 0.25, 0.125 };
        return beats[juce::jlimit (0, numDivisions - 1, d)];
    }

    static float stopSeconds (float v) { return 0.15f * std::pow (13.0f, juce::jlimit (0.0f, 1.0f, v)); }
};

/** Audio thread: runs the performance effects on the master output. */
class PerformProcessor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int size = juce::nextPowerOfTwo ((int) (sampleRate * 4.0));
        for (auto& h : history)
            h.assign ((size_t) size, 0.0f);
        mask = size - 1;
        write = 0;
        mode = previous = live;
        fade = fadeLength;
    }

    void process (juce::AudioBuffer<float>& buffer, int start, int n, const PerformSettings& s, double bpm)
    {
        const int wanted = s.tapeStop.load() ? stop : (s.repeat.load() ? repeat : (s.stutter.load() ? stutter : live));
        if (wanted != mode)
            begin (wanted, s, bpm);
        if (mode == live && fade >= fadeLength)
        {
            // Nothing playing: just remember the music, for when a Repeat starts.
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* x = buffer.getReadPointer (juce::jmin (ch, buffer.getNumChannels() - 1), start);
                for (int i = 0; i < n; ++i)
                    history[(size_t) ch][(size_t) ((write + i) & mask)] = x[i];
            }
            write = (write + n) & mask;
            return;
        }

        float* l = buffer.getWritePointer (0, start);
        float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : l;
        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i], inR = r[i];
            const bool frozen = mode == repeat || mode == stutter;   // a loop keeps its slice
            if (! frozen)
            {
                history[0][(size_t) write] = inL;
                history[1][(size_t) write] = inR;
                write = (write + 1) & mask;
            }

            float outL, outR;
            sample (mode, inL, inR, outL, outR);
            if (fade < fadeLength)
            {
                float oldL, oldR;
                sample (previous, inL, inR, oldL, oldR);
                const float t = (float) fade / (float) fadeLength;
                outL = oldL + t * (outL - oldL);
                outR = oldR + t * (outR - oldR);
                ++fade;
            }
            advance();
            l[i] = outL;
            if (r != l)
                r[i] = outR;
        }
    }

private:
    enum Mode { live = 0, repeat, stutter, stop };
    static constexpr int fadeLength = 256;

    void begin (int newMode, const PerformSettings& s, double bpm)
    {
        previous = mode;
        mode = newMode;
        fade = 0;
        const double beat = 60.0 / juce::jlimit (20.0, 300.0, bpm) * sr;
        if (mode == repeat || mode == stutter)
        {
            const double beats = mode == stutter ? 0.125 : PerformSettings::divisionBeats (s.repeatDivision.load());
            loopLength = juce::jlimit (64, mask / 2, (int) (beat * beats));
            loopStart = (write - loopLength) & mask;   // the slice just played
            loopPos = 0;
        }
        else if (mode == stop)
        {
            readPos = (double) write;
            speed = 1.0;
            slowDown = 1.0 / (PerformSettings::stopSeconds (s.stopTime.load()) * sr);
        }
    }

    void sample (int m, float inL, float inR, float& outL, float& outR) const
    {
        switch (m)
        {
            case repeat:
            case stutter:
            {
                const int idx = (loopStart + loopPos) & mask;
                // Short ramps at both ends of the slice, so the loop doesn't click.
                // A stutter sounds for only the first 3/5 of each slice: the gaps make it rattle.
                const int sounding = m == stutter ? (loopLength * 3) / 5 : loopLength;
                const int ramp = juce::jmax (1, juce::jmin (96, sounding / 4));
                const float g = juce::jlimit (0.0f, 1.0f, juce::jmin ((float) loopPos / (float) ramp, (float) (sounding - loopPos) / (float) ramp));
                outL = history[0][(size_t) idx] * g;
                outR = history[1][(size_t) idx] * g;
                return;
            }
            case stop:
            {
                const double pos = readPos - 1.0;
                const int i0 = (int) std::floor (pos) & mask;
                const int i1 = (i0 + 1) & mask;
                const float frac = (float) (pos - std::floor (pos));
                const float level = (float) std::sqrt (juce::jmax (0.0, speed));   // the sound sinks as it slows
                outL = (history[0][(size_t) i0] + frac * (history[0][(size_t) i1] - history[0][(size_t) i0])) * level;
                outR = (history[1][(size_t) i0] + frac * (history[1][(size_t) i1] - history[1][(size_t) i0])) * level;
                return;
            }
            default:
                outL = inL;
                outR = inR;
                return;
        }
    }

    void advance()
    {
        if (mode == repeat || mode == stutter || previous == repeat || previous == stutter)
            loopPos = (loopPos + 1) % juce::jmax (1, loopLength);
        if (mode == stop || previous == stop)
        {
            readPos += speed;
            speed = juce::jmax (0.0, speed - slowDown);
        }
    }

    double sr = 44100.0;
    std::array<std::vector<float>, 2> history;
    int write = 0, mask = 0;
    int mode = live, previous = live, fade = fadeLength;
    int loopStart = 0, loopLength = 1, loopPos = 0;
    double readPos = 0.0, speed = 1.0, slowDown = 0.0;
};

} // namespace bizgroove
