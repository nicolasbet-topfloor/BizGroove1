#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "engine/AudioEngine.h"
#include "LookAndFeel.h"

namespace bizgroove
{

/** The selected track's sound as it plays: its wave (an oscilloscope, held
    still on each cycle) and its spectrum (from deep lows on the left to highs
    on the right). For the Oscillators and Effects windows. */
class ScopeView final : public juce::Component, private juce::Timer
{
public:
    explicit ScopeView (AudioEngine& e) : engine (e)
    {
        bands.fill (-100.0f);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::background);
        g.fillRoundedRectangle (r, 6.0f);

        auto scopeArea = r.removeFromLeft (r.getWidth() * 0.55f).reduced (8.0f, 6.0f);
        r.removeFromLeft (6.0f);
        auto spectrumArea = r.reduced (8.0f, 6.0f);

        // ---- The wave
        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Wave (now)", scopeArea.removeFromTop (14.0f), juce::Justification::topLeft);
        g.setColour (Colours::panelEdge);
        g.drawHorizontalLine ((int) scopeArea.getCentreY(), scopeArea.getX(), scopeArea.getRight());
        if (quiet)
        {
            g.setColour (Colours::textDim);
            g.drawText ("Play a note to see the sound", scopeArea, juce::Justification::centred);
        }
        else
        {
            juce::Path p;
            const float gain = 0.45f * scopeArea.getHeight() / juce::jmax (0.05f, shownPeak);
            for (int i = 0; i < numScopePoints; ++i)
            {
                const float x = scopeArea.getX() + scopeArea.getWidth() * (float) i / (float) (numScopePoints - 1);
                const float y = scopeArea.getCentreY() - wave[(size_t) i] * gain;
                if (i == 0) p.startNewSubPath (x, y);
                else        p.lineTo (x, y);
            }
            g.setColour (Colours::playhead);
            g.strokePath (p, juce::PathStrokeType (1.6f));
        }

        // ---- The spectrum
        g.setColour (Colours::textDim);
        auto labels = spectrumArea.removeFromBottom (12.0f);
        g.drawText ("Spectrum", spectrumArea.removeFromTop (14.0f), juce::Justification::topLeft);
        for (const auto& [hz, text] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
        {
            const float x = spectrumArea.getX() + spectrumArea.getWidth() * position (hz);
            g.setColour (Colours::panelEdge);
            g.drawVerticalLine ((int) x, spectrumArea.getY(), spectrumArea.getBottom());
            g.setColour (Colours::textDim);
            g.drawText (text, juce::Rectangle<float> (x - 15.0f, labels.getY(), 30.0f, 12.0f), juce::Justification::centred);
        }
        const float barW = spectrumArea.getWidth() / (float) numBands;
        for (int b = 0; b < numBands; ++b)
        {
            const float level = juce::jlimit (0.0f, 1.0f, (bands[(size_t) b] + 80.0f) / 80.0f);   // -80 .. 0 dB
            if (level <= 0.0f) continue;
            const float h = level * spectrumArea.getHeight();
            g.setColour (Colours::accent.withAlpha (0.35f + 0.65f * level));
            g.fillRect (spectrumArea.getX() + (float) b * barW + 1.0f, spectrumArea.getBottom() - h, juce::jmax (1.0f, barW - 2.0f), h);
        }
    }

private:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, numBands = 40, numScopePoints = 512;
    static constexpr float lowHz = 30.0f, highHz = 16000.0f;

    static float position (float hz) { return std::log (hz / lowHz) / std::log (highHz / lowHz); }

    void timerCallback() override
    {
        // The latest samples, oldest first.
        const int end = engine.scopeWrite.load (std::memory_order_acquire);
        std::array<float, fftSize> latest {};
        for (int i = 0; i < fftSize; ++i)
            latest[(size_t) i] = engine.scope[(size_t) ((end - fftSize + i) & (AudioEngine::scopeSize - 1))].load (std::memory_order_relaxed);

        float peak = 0.0f;
        for (float v : latest) peak = juce::jmax (peak, std::abs (v));
        quiet = peak < 1.0e-4f;
        shownPeak = juce::jmax (peak, shownPeak * 0.92f);

        // The wave, from a rising zero crossing so that it stands still.
        const int span = fftSize / 2;
        int start = 0;
        for (int i = 1; i < fftSize - span; ++i)
            if (latest[(size_t) i - 1] < 0.0f && latest[(size_t) i] >= 0.0f) { start = i; }
        start = juce::jmin (start, fftSize - span);
        for (int i = 0; i < numScopePoints; ++i)
            wave[(size_t) i] = latest[(size_t) (start + i * span / numScopePoints)];

        // The spectrum: a windowed FFT, gathered into bands spaced by octaves, that fall back slowly.
        std::array<float, 2 * fftSize> data {};
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
            data[(size_t) i] = latest[(size_t) i] * w;
        }
        fft.performFrequencyOnlyForwardTransform (data.data());
        const double rate = juce::jmax (8000.0, engine.getSampleRate());
        for (int b = 0; b < numBands; ++b)
        {
            const float f0 = lowHz * std::pow (highHz / lowHz, (float) b / (float) numBands);
            const float f1 = lowHz * std::pow (highHz / lowHz, (float) (b + 1) / (float) numBands);
            const int i0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 * fftSize / rate));
            const int i1 = juce::jlimit (i0, fftSize / 2 - 1, (int) (f1 * fftSize / rate));
            float m = 0.0f;
            for (int i = i0; i <= i1; ++i) m = juce::jmax (m, data[(size_t) i]);
            const float db = juce::Decibels::gainToDecibels (m * 4.0f / (float) fftSize, -100.0f);
            bands[(size_t) b] = db > bands[(size_t) b] ? db : bands[(size_t) b] - 1.5f;
        }
        repaint();
    }

    AudioEngine& engine;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, numScopePoints> wave {};
    std::array<float, numBands> bands {};
    float shownPeak = 0.1f;
    bool quiet = true;
};

/** One oscillator's wave, drawn from its settings (with sync and ring on
    oscillator 2), so you can see the shape before you play. */
class OscWaveView final : public juce::Component
{
public:
    OscWaveView (int oscillator) : n (oscillator) {}

    void setSettings (const OscSettings* s)
    {
        settings = s;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::background);
        g.fillRoundedRectangle (r, 4.0f);
        if (settings == nullptr)
            return;
        r = r.reduced (4.0f);

        // Two cycles of the oscillator alone, at its own octave and tuning against a note.
        OscSettings one;
        for (size_t k = 0; k < 2; ++k)
        {
            const auto& from = settings->osc[k];
            auto& to = one.osc[k];
            to.wave.store (from.wave.load()); to.octave.store (from.octave.load()); to.semi.store (from.semi.load());
            to.fine.store (from.fine.load()); to.width.store (from.width.load());
            to.level.store ((int) k == n ? 1.0f : 0.0f);
        }
        one.sync.store (settings->sync.load());
        one.ring.store (settings->ring.load());
        OscLayer layer;
        layer.reset();
        constexpr int points = 240;
        const double rate = 48000.0, note = rate / 120.0;   // the note's cycle is 120 samples: two cycles on screen
        layer.setPitch (note, rate, one);
        juce::Path p;
        for (int i = 0; i < points; ++i)
        {
            const float v = juce::jlimit (-1.2f, 1.2f, 2.0f * layer.next (one));
            const float x = r.getX() + r.getWidth() * (float) i / (float) (points - 1);
            const float y = r.getCentreY() - v * 0.42f * r.getHeight();
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        const bool on = settings->osc[(size_t) n].level.load() > 0.0005f;
        g.setColour (on ? Colours::accent : Colours::textDim.withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (1.5f));
        if (! on)
        {
            g.setColour (Colours::textDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText ("off (turn up Level)", getLocalBounds().reduced (3), juce::Justification::bottomRight);
        }
    }

private:
    const int n;
    const OscSettings* settings = nullptr;
};

} // namespace bizgroove
