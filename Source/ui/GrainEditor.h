#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/AudioEngine.h"
#include "engine/ControlTargets.h"
#include "engine/Project.h"
#include "LookAndFeel.h"

namespace bizgroove
{

/** A Sampler track's Grain mode, for field recordings and anything long.

    - Grain mode on: the recording plays as a cloud of short grains instead of
      from start to end (the keys and steps play it at their pitch).
    - Drag on the waveform to move the cloud through the recording while it
      sounds; Freeze holds it where it is. Hold note keeps it sounding while you edit.
    - The 16 pads each play from a spot of their own: the numbered marks on the
      waveform. Drag a mark to move it; pick a pad to give it its own grain size and pitch.
    - Live input: the grains come from the microphone or line in, as it comes
      in (the last ten seconds); Keep turns those seconds into the track's recording.
    - The knobs are the Granular's, so an LFO or a step's lock can move them in time.
*/
class GrainEditor final : public juce::Component, private juce::Timer
{
public:
    GrainEditor (AudioEngine& e, const ControlTargets& t, int trackIndex, std::function<void()> askForRecording)
        : engine (e), targets (t), track (trackIndex), loadRecording (std::move (askForRecording))
    {
        auto setUp = [this] (juce::Button& b, const juce::String& tip, bool toggle) {
            b.setTooltip (tip);
            b.setWantsKeyboardFocus (false);
            if (toggle)
                b.setColour (juce::TextButton::buttonOnColourId, Colours::accent);
            addAndMakeVisible (b);
        };
        setUp (modeButton, "Grain mode on: the recording plays as grains. Off: the Sampler plays it as before.", true);
        modeButton.onClick = [this] {
            auto& tr = engine.track (track);
            tr.grainMode.store (! tr.grainMode.load());
            if (! tr.grainMode.load())
                tr.grainHold.store (false);
        };
        setUp (holdButton, "Keep a note sounding, so you hear the grains while you drag and turn the knobs.", true);
        holdButton.onClick = [this] { auto& tr = engine.track (track); tr.grainMode.store (true); tr.grainHold.store (! tr.grainHold.load()); };
        setUp (freezeButton, "Freeze: the grains stay where they are (Scan stops).", true);
        freezeButton.onClick = [this] { auto& f2 = params().freeze; f2.store (! f2.load()); };
        setUp (liveButton, "Live input: grains of the microphone or line in, as it comes in (the last ten seconds).", true);
        liveButton.onClick = [this] { auto& tr = engine.track (track); tr.grainMode.store (true); tr.grainLive.store (! tr.grainLive.load()); };
        setUp (keepButton, "Keep the last ten seconds of the live input as this track's recording.", false);
        keepButton.onClick = [this] { keepLive(); };
        setUp (loadButton, "Load a field recording (WAV, AIFF, FLAC or OGG, up to five minutes).", false);
        loadButton.onClick = [this] { if (loadRecording) loadRecording(); };
        setUp (spreadButton, "Put the 16 pads' spots back, evenly along the recording.", false);
        spreadButton.onClick = [this] { params().spreadPads(); repaint(); };

        for (int i = 0; i < GranularParams::numPads; ++i)
        {
            auto& b = padButtons[(size_t) i];
            b.setButtonText (juce::String (i + 1));
            b.setTooltip ("Pad " + juce::String (i + 1) + ": pick it to set its own size and pitch. Its mark on the waveform is where it plays.");
            b.setWantsKeyboardFocus (false);
            b.onClick = [this, i] { pickedPad = i; refreshPad(); repaint(); };
            addAndMakeVisible (b);
        }
        for (auto* s : { &padSize, &padPitch })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
            addAndMakeVisible (s);
        }
        padSize.setRange (0.0, 0.5, 0.005);
        padSize.textFromValueFunction = [] (double v) { return v < 0.005 ? juce::String ("Main") : juce::String (juce::roundToInt (v * 1000.0)) + " ms"; };
        padSize.setTooltip ("This pad's grain size (far left: the main Size)");
        padSize.onValueChange = [this] { params().pads[(size_t) pickedPad].size.store ((float) padSize.getValue()); };
        padPitch.setRange (-24.0, 24.0, 1.0);
        padPitch.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v) + " st"; };
        padPitch.setTooltip ("This pad's pitch, in semitones");
        padPitch.onValueChange = [this] { params().pads[(size_t) pickedPad].pitch.store ((float) padPitch.getValue()); };
        padSize.updateText();
        padPitch.updateText();
        for (auto* l : { &padSizeLabel, &padPitchLabel, &info })
        {
            l->setFont (juce::FontOptions (12.0f));
            l->setColour (juce::Label::textColourId, Colours::textDim);
            addAndMakeVisible (l);
        }
        padSizeLabel.setText ("Pad size", juce::dontSendNotification);
        padPitchLabel.setText ("Pad pitch", juce::dontSendNotification);

        // The first eight in the order of the controller's knobs 1-8 (as on the panel); two more below.
        static const char* ids[numKnobs] = { "gran.scan", "gran.position", "gran.size", "gran.density", "gran.spread", "gran.pitch", "gran.attack", "gran.release",
                                             "gran.reverse", "gran.spray" };
        static const char* names[numKnobs] = { "Scan", "Position", "Size", "Density", "Spread", "Pitch", "Attack", "Release", "Reverse", "Spray" };
        for (int k = 0; k < numKnobs; ++k)
        {
            auto& knob = knobs[(size_t) k];
            knob.target = targets.find (ids[k]);
            knob.name = names[k];
            knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            knob.slider.setRange (0.0, 1.0);
            knob.slider.setWantsKeyboardFocus (false);
            knob.slider.setColour (juce::Slider::rotarySliderFillColourId, Colours::accent);
            knob.slider.setColour (juce::Slider::rotarySliderOutlineColourId, Colours::panelEdge);
            knob.slider.setColour (juce::Slider::thumbColourId, Colours::text);
            knob.slider.onValueChange = [this, k] { turn (k, (float) knobs[(size_t) k].slider.getValue()); };
            addAndMakeVisible (knob.slider);
        }

        refreshPad();
        setSize (1000, 670);
        startTimerHz (30);
    }

    ~GrainEditor() override
    {
        engine.track (track).grainHold.store (false);   // closing the window lets the held note go
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::background);
        const auto w = wave.toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (w, 6.0f);

        // The recording (or the live input), as peaks.
        const int bins = (int) peaks.size() / 2;
        if (bins > 0)
        {
            g.setColour (live() ? Colours::playhead.withAlpha (0.55f) : Colours::textDim.withAlpha (0.8f));
            const float mid = w.getCentreY() + padBand * 0.5f, half = (w.getHeight() - padBand) * 0.45f;
            for (int x = 0; x < (int) w.getWidth(); ++x)
            {
                const int b = juce::jlimit (0, bins - 1, x * bins / juce::jmax (1, (int) w.getWidth()));
                const float lo = peaks[(size_t) (2 * b)], hi = peaks[(size_t) (2 * b + 1)];
                g.drawVerticalLine ((int) w.getX() + x, mid - hi * half, mid - lo * half + 1.0f);
            }
        }
        else
        {
            g.setColour (Colours::textDim);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText ("No recording yet: Load a field recording, or switch on Live input. Until then the grains come from a choir.",
                        wave, juce::Justification::centred, true);
        }

        auto& p = params();
        const float x0 = w.getX(), width = w.getWidth();

        // The cloud: where it is, how wide (Spread), and the last grains.
        const float pos = p.shownPosition.load() >= 0.0f && engine.track (track).grainMode.load() ? p.shownPosition.load() : p.position.load();
        const float spread = p.spread.load();
        g.setColour (Colours::accent.withAlpha (0.15f));
        g.fillRect (juce::Rectangle<float> (x0 + (pos - spread * 0.5f) * width, w.getY() + padBand, spread * width, w.getHeight() - padBand));
        g.setColour (Colours::accent);
        g.fillRect (juce::Rectangle<float> (x0 + p.position.load() * width - 1.5f, w.getY() + padBand, 3.0f, w.getHeight() - padBand));
        if (std::abs (pos - p.position.load()) > 0.002f)
        {
            g.setColour (Colours::accent.withAlpha (0.6f));
            g.drawVerticalLine ((int) (x0 + pos * width), w.getY() + padBand, w.getBottom());
        }
        g.setColour (Colours::text.withAlpha (0.8f));
        const int count = p.shownGrainCount.load();
        for (int i = 0; i < juce::jmin (count, GranularParams::numShownGrains); ++i)
        {
            const float gx = x0 + p.shownGrains[(size_t) i].load() * width;
            const float gy = w.getY() + padBand + 10.0f + (float) ((i * 37) % 23) / 23.0f * (w.getHeight() - padBand - 20.0f);
            g.fillEllipse (gx - 2.0f, gy - 2.0f, 4.0f, 4.0f);
        }

        // The pads' spots: numbered marks along the top.
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        for (int i = 0; i < GranularParams::numPads; ++i)
        {
            const float px = x0 + p.pads[(size_t) i].position.load() * width;
            const bool picked = i == pickedPad;
            g.setColour (picked ? Colours::playhead : Colours::text.withAlpha (0.35f));
            g.drawVerticalLine ((int) px, w.getY() + padBand, w.getBottom());
            const auto tag = juce::Rectangle<float> (px - 9.0f, w.getY() + 2.0f, 18.0f, padBand - 4.0f);
            g.setColour (picked ? Colours::playhead : Colours::panelEdge.brighter (0.3f));
            g.fillRoundedRectangle (tag, 3.0f);
            g.setColour (picked ? juce::Colours::black : Colours::text);
            g.drawText (juce::String (i + 1), tag, juce::Justification::centred);
        }

        // The knobs' names and values.
        g.setFont (juce::FontOptions (12.0f));
        for (const auto& k : knobs)
        {
            const auto b = k.slider.getBounds();
            g.setColour (Colours::text);
            g.drawText (k.name, b.getX(), b.getBottom(), b.getWidth(), 16, juce::Justification::centred);
            g.setColour (Colours::textDim);
            if (k.target != nullptr && k.target->get && k.target->show)
                g.drawText (k.target->show (k.target->get()), b.getX(), b.getBottom() + 15, b.getWidth(), 16, juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        auto top = r.removeFromTop (30);
        for (auto* b : { (juce::Button*) &modeButton, (juce::Button*) &holdButton, (juce::Button*) &freezeButton,
                         (juce::Button*) &liveButton, (juce::Button*) &keepButton, (juce::Button*) &loadButton })
        {
            b->setBounds (top.removeFromLeft (juce::jmax (110, top.getWidth() / 6)).reduced (3, 0));
        }
        r.removeFromTop (6);
        info.setBounds (r.removeFromBottom (20));
        auto moreRow = r.removeFromBottom (106);
        auto knobRow = r.removeFromBottom (110);
        const int kw = knobRow.getWidth() / 8;
        for (int k = 0; k < numKnobs; ++k)
        {
            const auto& row = k < 8 ? knobRow : moreRow;
            const int col = k < 8 ? k : k - 8;
            knobs[(size_t) k].slider.setBounds (row.getX() + col * kw + (kw - 70) / 2, row.getY() + 4, 70, 70);
        }
        r.removeFromBottom (6);
        auto padRow = r.removeFromBottom (58);
        auto padButtonsRow = padRow.removeFromTop (26);
        spreadButton.setBounds (padButtonsRow.removeFromRight (120).reduced (3, 0));
        const int pw = padButtonsRow.getWidth() / GranularParams::numPads;
        for (int i = 0; i < GranularParams::numPads; ++i)
            padButtons[(size_t) i].setBounds (padButtonsRow.getX() + i * pw, padButtonsRow.getY(), pw - 4, 26);
        padRow.removeFromTop (6);
        auto half = padRow.removeFromLeft (padRow.getWidth() / 2);
        padSizeLabel.setBounds (half.removeFromLeft (70));
        padSize.setBounds (half.reduced (4, 0));
        padPitchLabel.setBounds (padRow.removeFromLeft (70));
        padPitch.setBounds (padRow.reduced (4, 0));
        r.removeFromBottom (8);
        wave = r;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragPad = -1;
        if (! wave.contains (e.getPosition()))
            return;
        const float x = fractionAt (e.x);
        if (e.y < wave.getY() + (int) padBand)
        {
            // A pad's mark: the nearest one, to drag.
            float best = 1.0e9f;
            for (int i = 0; i < GranularParams::numPads; ++i)
                if (const float d = std::abs (params().pads[(size_t) i].position.load() - x); d < best) { best = d; dragPad = i; }
            if (best * (float) wave.getWidth() > 14.0f)
                dragPad = -1;
            if (dragPad >= 0) { pickedPad = dragPad; refreshPad(); }
        }
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! wave.contains (e.getMouseDownPosition()))
            return;
        const float x = fractionAt (e.x);
        if (dragPad >= 0)
            params().pads[(size_t) dragPad].position.store (x);
        else
            turn (0, x);   // the cloud's position, through its knob (so a held step or recording locks it)
        repaint();
    }

private:
    struct Knob { const ControlTargets::Target* target = nullptr; juce::String name; juce::Slider slider; };

    GranularParams& params() { return engine.track (track).granularParams; }
    bool live() const { auto& tr = engine.track (track); return tr.grainLive.load(); }

    float fractionAt (int x) const { return juce::jlimit (0.0f, 1.0f, (float) (x - wave.getX()) / (float) juce::jmax (1, wave.getWidth())); }

    /** A knob turned (or the waveform dragged): on this track's knobs, through the knob's target. */
    void turn (int k, float v)
    {
        const auto* tg = knobs[(size_t) k].target;
        if (tg == nullptr || ! tg->set || updating)
            return;
        withTrack ([tg, v] { tg->set (v); });
    }

    /** The targets act on the selected track: this window's track is that one, or (if another track
        was selected since) it stands in for it on this thread while f runs. */
    template <typename F>
    void withTrack (F&& f)
    {
        if (engine.selectedTrack.load() == track)
        {
            f();   // knobs turned here can lock on a held step, or be recorded
            return;
        }
        AudioEngine::trackOverride = track;
        f();
        AudioEngine::trackOverride = -1;
    }

    void refreshPad()
    {
        auto& pad = params().pads[(size_t) pickedPad];
        padSize.setValue (pad.size.load(), juce::dontSendNotification);
        padPitch.setValue (pad.pitch.load(), juce::dontSendNotification);
        for (int i = 0; i < GranularParams::numPads; ++i)
        {
            padButtons[(size_t) i].setColour (juce::TextButton::buttonColourId, i == pickedPad ? Colours::playhead : Colours::panelEdge);
            padButtons[(size_t) i].setColour (juce::TextButton::textColourOffId, i == pickedPad ? juce::Colours::black : Colours::text);
        }
    }

    void keepLive()
    {
        juce::AudioBuffer<float> audio;
        double rate = 48000.0;
        if (! engine.copyLiveRing (audio, rate))
            return;
        auto& tr = engine.track (track);
        tr.samplerEngine.setSample (audio, rate, "Live grains");
        tr.sampleFileName = "Live grains.wav";
        juce::MemoryBlock data;
        {
            juce::WavAudioFormat wav;
            auto stream = std::make_unique<juce::MemoryOutputStream> (data, false);
            if (std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0)); w != nullptr)
            {
                stream.release();
                w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
            }
        }
        tr.sampleFileData = data;
        tr.grainLive.store (false);   // now the grains come from what was kept
        shownSample.clear();
    }

    /** The waveform's peaks: the recording when it changes, the live input all the time. */
    void updatePeaks()
    {
        constexpr int bins = 1200;
        auto& tr = engine.track (track);
        if (live())
        {
            const auto& ring = engine.getLiveRing();
            const int len = ring.getNumSamples();
            if (len < bins)
                return;
            const int head = engine.getLiveRingHead();
            peaks.assign ((size_t) bins * 2, 0.0f);
            const float* l = ring.getReadPointer (0);
            for (int b = 0; b < bins; ++b)
            {
                float lo = 0.0f, hi = 0.0f;
                const int a = b * len / bins, z = (b + 1) * len / bins;
                for (int i = a; i < z; i += 8)
                {
                    const float v = l[(head + i) % len];
                    lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
                }
                peaks[(size_t) (2 * b)] = lo; peaks[(size_t) (2 * b + 1)] = hi;
            }
            shownSample = "(live)";
            return;
        }
        const auto name = tr.samplerEngine.getSampleName() + juce::String (tr.samplerEngine.getSampleSeconds());
        if (name == shownSample)
            return;
        shownSample = name;
        peaks.clear();
        tr.samplerEngine.useSample ([this] (const juce::AudioBuffer<float>& audio, double) {
            const int len = audio.getNumSamples();
            if (len < bins)
                return;
            peaks.assign ((size_t) bins * 2, 0.0f);
            const float* l = audio.getReadPointer (0);
            const int stride = juce::jmax (1, len / bins / 64);   // long recordings: a sample in every few
            for (int b = 0; b < bins; ++b)
            {
                float lo = 0.0f, hi = 0.0f;
                for (int i = b * len / bins; i < (b + 1) * len / bins; i += stride)
                {
                    lo = juce::jmin (lo, l[i]); hi = juce::jmax (hi, l[i]);
                }
                peaks[(size_t) (2 * b)] = lo; peaks[(size_t) (2 * b + 1)] = hi;
            }
        });
    }

    void timerCallback() override
    {
        auto& tr = engine.track (track);
        const bool grain = tr.grainMode.load();
        modeButton.setToggleState (grain, juce::dontSendNotification);
        modeButton.setButtonText (grain ? "Grain mode: on" : "Grain mode: off");
        holdButton.setToggleState (tr.grainHold.load(), juce::dontSendNotification);
        freezeButton.setToggleState (params().freeze.load(), juce::dontSendNotification);
        liveButton.setToggleState (tr.grainLive.load(), juce::dontSendNotification);
        keepButton.setEnabled (tr.grainLive.load());

        if (++ticks % 2 == 0 || ! live())
            updatePeaks();

        updating = true;   // the knobs follow LFOs, locks and the controller
        for (auto& k : knobs)
            if (k.target != nullptr && k.target->get && ! k.slider.isMouseButtonDown())
            {
                withTrack ([&k] { k.slider.setValue (k.target->get(), juce::dontSendNotification); });
            }
        updating = false;

        const double secs = tr.samplerEngine.getSampleSeconds();
        const auto text = live() ? juce::String ("Live input: the last ten seconds of the microphone or line in. Keep turns them into this track's recording.")
                                 : secs > 0.0 ? tr.samplerEngine.getSampleName() + " - " + juce::String (secs, 1)
                                                    + " s - drag on the waveform to move the cloud; drag a numbered mark to move a pad's spot"
                                              : juce::String ("Load a field recording (up to five minutes) or switch on Live input.");
        if (info.getText() != text)
            info.setText (text, juce::dontSendNotification);
        repaint (wave);
        for (const auto& k : knobs)
            repaint (k.slider.getBounds().withHeight (k.slider.getHeight() + 32));
    }

    static constexpr float padBand = 18.0f;

    AudioEngine& engine;
    const ControlTargets& targets;
    int track;
    std::function<void()> loadRecording;

    juce::TextButton modeButton { "Grain mode" }, holdButton { "Hold note" }, freezeButton { "Freeze" },
                     liveButton { "Live input" }, keepButton { "Keep live" }, loadButton { "Load recording..." },
                     spreadButton { "Spread pads" };
    std::array<juce::TextButton, GranularParams::numPads> padButtons;
    juce::Slider padSize, padPitch;
    juce::Label padSizeLabel, padPitchLabel, info;
    static constexpr int numKnobs = 10;
    std::array<Knob, numKnobs> knobs;
    juce::Rectangle<int> wave;
    std::vector<float> peaks;
    juce::String shownSample;
    int pickedPad = 0, dragPad = -1, ticks = 0;
    bool updating = false;
};

} // namespace bizgroove
