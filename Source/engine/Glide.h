#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace bizgroove
{

/** Portamento: each new note slides in pitch from the last one, at an even
    speed in semitones, as on analogue synths. Two ways to set how long:
    - Fixed time: every slide takes the glide time, short or wide.
    - By distance: the glide time is per octave, so wider slides take longer.
    Audio thread, one per voice. */
class Glide
{
public:
    enum Mode { fixedTime = 0, byDistance = 1, numModes };

    static juce::String modeName (int mode) { return mode == byDistance ? "By distance" : "Fixed time"; }

    /** At a note's start: the slide from fromHz (0: no slide) to toHz. */
    void start (double fromHz, double toHz, float seconds, int mode, double sampleRate)
    {
        target = noteOf (toHz);
        if (seconds <= 0.001f || fromHz <= 0.0 || sampleRate <= 0.0)
        {
            current = target;
            step = 0.0;
            return;
        }
        current = noteOf (fromHz);
        const double distance = std::abs (target - current);
        const double time = mode == byDistance ? (double) seconds * distance / 12.0 : (double) seconds;
        step = time > 1.0e-6 ? distance / (time * sampleRate) : distance;
    }

    /** Moves on by n samples and gives the pitch in Hz. */
    double advance (int n)
    {
        if (current < target)
            current = juce::jmin (target, current + step * (double) n);
        else if (current > target)
            current = juce::jmax (target, current - step * (double) n);
        return hz();
    }

    double hz() const { return 440.0 * std::pow (2.0, (current - 69.0) / 12.0); }
    bool sliding() const { return current != target; }

private:
    static double noteOf (double hz) { return 69.0 + 12.0 * std::log2 (juce::jmax (1.0, hz) / 440.0); }
    double current = 60.0, target = 60.0, step = 0.0;
};

} // namespace bizgroove
