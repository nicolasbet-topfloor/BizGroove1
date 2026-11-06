#include "Mixer.h"

namespace bizgroove
{

// ============================================================== ChannelStrip

void ChannelStrip::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto* bank : { &low, &mid, &high })
        for (auto& f : *bank)
            f.reset();
    lastLow = lastMid = lastHigh = 99.0f;
    for (auto* s : { &panL, &panR, &sendR, &sendD })
        s->reset (sampleRate, 0.02);
    panL.setCurrentAndTargetValue (std::sqrt (0.5f));
    panR.setCurrentAndTargetValue (std::sqrt (0.5f));
    sendR.setCurrentAndTargetValue (0.0f);
    sendD.setCurrentAndTargetValue (0.0f);
}

void ChannelStrip::process (juce::AudioBuffer<float>& buffer, int numSamples, const ChannelSettings& s,
                            juce::AudioBuffer<float>& reverbBus, juce::AudioBuffer<float>& delayBus)
{
    // EQ: recompute coefficients only when a knob moved.
    const float gLow = s.eqLow.load(), gMid = s.eqMid.load(), gHigh = s.eqHigh.load();
    if (! juce::exactlyEqual (gLow, lastLow))   { for (auto& f : low)  f.setLowShelf (sr, 120.0, gLow);       lastLow = gLow; }
    if (! juce::exactlyEqual (gMid, lastMid))   { for (auto& f : mid)  f.setPeak (sr, 1000.0, 0.8, gMid);     lastMid = gMid; }
    if (! juce::exactlyEqual (gHigh, lastHigh)) { for (auto& f : high) f.setHighShelf (sr, 8000.0, gHigh);    lastHigh = gHigh; }
    const bool eqOn = std::abs (gLow) > 0.05f || std::abs (gMid) > 0.05f || std::abs (gHigh) > 0.05f;

    // Equal-power pan; centre leaves both channels at -3 dB, as most mixers do.
    const float pan = juce::jlimit (-1.0f, 1.0f, s.pan.load());
    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    panL.setTargetValue (std::cos (angle) * std::sqrt (2.0f));
    panR.setTargetValue (std::sin (angle) * std::sqrt (2.0f));
    sendR.setTargetValue (juce::jlimit (0.0f, 1.0f, s.reverb.load()));
    sendD.setTargetValue (juce::jlimit (0.0f, 1.0f, s.delay.load()));

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    float* rbL = reverbBus.getWritePointer (0);
    float* rbR = reverbBus.getWritePointer (1);
    float* dbL = delayBus.getWritePointer (0);
    float* dbR = delayBus.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        float x0 = l[i], x1 = r[i];
        if (eqOn)
        {
            x0 = high[0].process (mid[0].process (low[0].process (x0)));
            x1 = high[1].process (mid[1].process (low[1].process (x1)));
        }
        x0 *= panL.getNextValue();
        x1 *= panR.getNextValue();
        l[i] = x0;
        r[i] = x1;

        const float sr_ = sendR.getNextValue(), sd = sendD.getNextValue();
        rbL[i] += x0 * sr_;  rbR[i] += x1 * sr_;
        dbL[i] += x0 * sd;   dbR[i] += x1 * sd;
    }
}

// ============================================================== MasterEffects

juce::String MasterEffects::divisionName (int division)
{
    switch (division)
    {
        case sixteenth:    return "1/16";
        case eighth:       return "1/8";
        case dottedEighth: return "Dotted 1/8";
        case quarter:      return "1/4";
        default:           return {};
    }
}

void MasterEffects::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::ignoreUnused (maxBlock);
    reverb.setSampleRate (sampleRate);
    delayLine.setSize (2, (int) (sampleRate * 2.5) + 1);
    delayTime.reset (sampleRate, 0.05);
    reset();
}

void MasterEffects::reset()
{
    reverb.reset();
    delayLine.clear();
    writePos = 0;
    lastSize = -1.0f;
}

void MasterEffects::process (juce::AudioBuffer<float>& reverbBus, juce::AudioBuffer<float>& delayBus,
                             juce::AudioBuffer<float>& out, int startSample, int numSamples, double bpm)
{
    // Reverb: fully wet on the bus, added to the mix.
    const float size = juce::jlimit (0.0f, 1.0f, reverbSize.load());
    if (! juce::exactlyEqual (size, lastSize))
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.3f + 0.65f * size;
        p.damping = 0.5f;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);
        lastSize = size;
    }
    reverb.processStereo (reverbBus.getWritePointer (0), reverbBus.getWritePointer (1), numSamples);

    // Delay: time from the tempo; each repeat swaps sides (ping-pong).
    const double beat = 60.0 / juce::jlimit (20.0, 300.0, bpm) * sr;
    double factor = 0.75;
    switch (delayDivision.load())
    {
        case sixteenth:    factor = 0.25; break;
        case eighth:       factor = 0.5;  break;
        case dottedEighth: factor = 0.75; break;
        case quarter:      factor = 1.0;  break;
        default: break;
    }
    const int lineLength = delayLine.getNumSamples();
    delayTime.setTargetValue ((float) juce::jlimit (1.0, (double) lineLength - 2.0, beat * factor));
    const float fb = juce::jlimit (0.0f, 0.9f, delayFeedback.load());

    float* dl = delayLine.getWritePointer (0);
    float* dr = delayLine.getWritePointer (1);
    const float* inL = delayBus.getReadPointer (0);
    const float* inR = delayBus.getReadPointer (1);
    const float* rvL = reverbBus.getReadPointer (0);
    const float* rvR = reverbBus.getReadPointer (1);
    float* oL = out.getWritePointer (0, startSample);
    float* oR = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : oL;

    for (int i = 0; i < numSamples; ++i)
    {
        const float d = delayTime.getNextValue();
        double readPos = writePos - d;
        while (readPos < 0) readPos += lineLength;
        const int i0 = (int) readPos, i1 = (i0 + 1) % lineLength;
        const float frac = (float) (readPos - i0);
        const float yL = dl[i0] + frac * (dl[i1] - dl[i0]);
        const float yR = dr[i0] + frac * (dr[i1] - dr[i0]);

        dl[writePos] = inL[i] + yR * fb;   // ping-pong
        dr[writePos] = inR[i] + yL * fb;
        writePos = (writePos + 1) % lineLength;

        oL[i] += rvL[i] + yL;
        oR[i] += rvR[i] + yR;
    }
}

} // namespace bizgroove
