#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/AudioEngine.h"
#include "engine/Project.h"
#include "engine/SampleTools.h"
#include "LookAndFeel.h"
#include <algorithm>
#include <vector>

namespace bizgroove
{

/** A button that reports when it is pressed and released, to audition sounds. */
struct HoldButton final : public juce::TextButton
{
    using juce::TextButton::TextButton;
    std::function<void (bool down)> onHold;

    void buttonStateChanged() override
    {
        const bool down = isDown();
        if (down != held)
        {
            held = down;
            if (onHold)
                onHold (down);
        }
    }

    bool held = false;
};

/** The sample editor, in the manner of an MPC: the waveform with Start, End
    and Loop markers, Chop (equal regions, by the hits, or by hand) with one
    slice per pad, and processes on the chosen part. Everything can be undone.

    Markers and slices are the sampler's settings; processes change the
    recording itself, which is then reloaded (and saved with the project).
*/
class SampleEditor final : public juce::Component
{
public:
    SampleEditor (AudioEngine& e, int trackIndex, juce::AudioFormatManager& f)
        : engine (e), track (trackIndex), formats (f)
    {
        auto& tr = engine.track (track);
        juce::String error;
        Project::decodeAudio (tr.sampleFileData, formats, audio, rate, error);
        fileName = juce::File::createFileWithoutCheckingPath (tr.sampleFileName).getFileNameWithoutExtension();
        if (fileName.isEmpty())
            fileName = "Sample";
        readMarkers();

        auto setUp = [this] (juce::Button& b, const juce::String& tip) {
            b.setTooltip (tip);
            b.setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        };

        setUp (playButton, "Hold to hear the chosen part (with slices: the first slice).");
        playButton.onHold = [this] (bool down) { audition (Sampler::rootNote, down); };

        setUp (zoomIn, "Zoom in (or use the mouse wheel over the waveform)");
        setUp (zoomOut, "Zoom out");
        setUp (zoomFit, "Show the whole sample");
        zoomIn.onClick  = [this] { zoom (0.5, 0.5f); };
        zoomOut.onClick = [this] { zoom (2.0, 0.5f); };
        zoomFit.onClick = [this] { viewStart = 0.0; viewEnd = 1.0; repaint(); };

        for (auto* b : { &modeTrim, &modeLoop, &modeChop })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (77);
        }
        setUp (modeTrim, "Clicks move Start or End, whichever is nearer.");
        setUp (modeLoop, "Clicks set the Loop point: where a held note returns to when Loop is on.");
        setUp (modeChop, "Clicks add a slice; drag a slice line to move it; right-click (or Alt-click) a line to remove it.");
        modeTrim.setToggleState (true, juce::dontSendNotification);

        setUp (loopButton, "Loop on or off: a held note keeps repeating from the Loop point.");
        loopButton.setClickingTogglesState (true);
        loopButton.setToggleState (params().loop.load(), juce::dontSendNotification);
        loopButton.onClick = [this] { params().loop.store (loopButton.getToggleState()); repaint(); };

        for (int n : { 2, 4, 8, 16 })
            regionsMenu.addItem (juce::String (n) + " slices", n);
        regionsMenu.setSelectedId (8, juce::dontSendNotification);
        regionsMenu.setTooltip ("How many equal slices Cut equally makes");
        addAndMakeVisible (regionsMenu);
        setUp (cutEqually, "Chop the chosen part into equal slices, one per pad.");
        cutEqually.onClick = [this] { chopEqually (regionsMenu.getSelectedId()); };

        threshold.setRange (0.0, 1.0, 0.01);
        threshold.setValue (0.5, juce::dontSendNotification);
        threshold.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        threshold.setTooltip ("Threshold: to the right finds more hits, to the left only the loudest");
        addAndMakeVisible (threshold);
        setUp (findHits, "Chop at each hit, by the threshold on the left.");
        findHits.onClick = [this] { chopByHits(); };
        setUp (clearSlices, "Remove all slices.");
        clearSlices.onClick = [this] { snapshot (false); slices.clear(); writeMarkers(); };

        setUp (cropButton, "Keep only the chosen part (from Start to End) and throw the rest away.");
        setUp (normaliseButton, "Make the chosen part as loud as it can be without distorting.");
        setUp (reverseButton, "Turn the chosen part backwards.");
        setUp (fadeInButton, "Fade the chosen part in from silence.");
        setUp (fadeOutButton, "Fade the chosen part out to silence.");
        setUp (silenceButton, "Make the chosen part silent.");
        cropButton.onClick      = [this] { process ([] (juce::AudioBuffer<float>& a, int f, int t) { SampleTools::crop (a, f, t); }, true); };
        normaliseButton.onClick = [this] { process ([] (juce::AudioBuffer<float>& a, int f, int t) { SampleTools::normalise (a, f, t); }, false); };
        reverseButton.onClick   = [this] { process ([] (juce::AudioBuffer<float>& a, int f, int t) { SampleTools::reverse (a, f, t); }, false); };
        fadeInButton.onClick    = [this] { process ([] (juce::AudioBuffer<float>& a, int f, int t) { SampleTools::fadeIn (a, f, t); }, false); };
        fadeOutButton.onClick   = [this] { process ([] (juce::AudioBuffer<float>& a, int f, int t) { SampleTools::fadeOut (a, f, t); }, false); };
        silenceButton.onClick   = [this] { process ([] (juce::AudioBuffer<float>& a, int f, int t) { SampleTools::silence (a, f, t); }, false); };

        setUp (undoButton, "Undo the last change (up to 20).");
        undoButton.onClick = [this] { undo(); };

        info.setColour (juce::Label::textColourId, Colours::textDim);
        info.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (info);
        updateInfo();

        setSize (980, 440);
    }

    ~SampleEditor() override
    {
        if (auditioning >= 0)
            engine.keyboardState.noteOff (1, auditioning, 0.0f);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::background);

        // Slice numbers, above the waveform.
        g.setColour (Colours::panel);
        g.fillRect (numberStrip);
        g.fillRect (wave);

        if (audio.getNumSamples() < 2)
        {
            g.setColour (Colours::textDim);
            g.drawText ("This recording could not be read.", wave, juce::Justification::centred);
            return;
        }

        // The waveform: the loudest and quietest point under each pixel.
        const int n = audio.getNumSamples();
        const float mid = (float) wave.getCentreY(), half = wave.getHeight() * 0.48f;
        g.setColour (Colours::accent.withAlpha (0.85f));
        for (int x = 0; x < wave.getWidth(); ++x)
        {
            const int a = juce::jlimit (0, n - 1, (int) (xToPos (wave.getX() + x) * (n - 1)));
            const int b = juce::jlimit (a + 1, n, (int) (xToPos (wave.getX() + x + 1) * (n - 1)) + 1);
            float lo = 0.0f, hi = 0.0f;
            const int stride = juce::jmax (1, (b - a) / 256);   // long views: look at a sample of points
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            {
                const float* d = audio.getReadPointer (ch);
                for (int i = a; i < b; i += stride)
                {
                    lo = juce::jmin (lo, d[i]);
                    hi = juce::jmax (hi, d[i]);
                }
            }
            g.drawVerticalLine (wave.getX() + x, mid - hi * half, mid - lo * half + 1.0f);
        }

        // Outside Start..End is dimmed.
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        const int xs = posToX (startPos), xe = posToX (endPos);
        if (xs > wave.getX())     g.fillRect (wave.withRight (xs));
        if (xe < wave.getRight()) g.fillRect (wave.withLeft (xe));

        // Start and End.
        g.setColour (juce::Colours::white);
        g.drawVerticalLine (xs, (float) wave.getY(), (float) wave.getBottom());
        g.drawVerticalLine (xe, (float) wave.getY(), (float) wave.getBottom());
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("S", xs + 3, wave.getY() + 2, 14, 14, juce::Justification::left);
        g.drawText ("E", xe - 16, wave.getY() + 2, 14, 14, juce::Justification::right);

        // Loop point.
        if (slices.empty())
        {
            const int xl = posToX (juce::jlimit (startPos, endPos, loopPos));
            g.setColour (params().loop.load() ? juce::Colours::yellow : juce::Colours::yellow.withAlpha (0.4f));
            g.drawVerticalLine (xl, (float) wave.getY() + 18.0f, (float) wave.getBottom());
            g.drawText ("L", xl + 3, wave.getBottom() - 16, 14, 14, juce::Justification::left);
        }

        // Slices, numbered as the pads.
        g.setColour (juce::Colours::orange);
        for (size_t i = 0; i < slices.size(); ++i)
        {
            const int x = posToX (slices[i]);
            if (x < wave.getX() || x > wave.getRight())
                continue;
            g.drawVerticalLine (x, (float) numberStrip.getY(), (float) wave.getBottom());
            g.drawText (juce::String ((int) i + 1), x + 3, numberStrip.getY(), 24, numberStrip.getHeight(), juce::Justification::left);
        }

        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (slices.empty() ? juce::String ("Chop to cut the sample into slices for the pads.")
                                   : juce::String ("Click a number to hear that slice. Pad 1 plays slice 1, and so on."),
                    numberStrip.withTrimmedLeft (8), juce::Justification::centredRight);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto bottom = r.removeFromBottom (32);
        r.removeFromBottom (8);
        auto middle = r.removeFromBottom (32);
        r.removeFromBottom (8);
        auto top = r.removeFromTop (32);
        r.removeFromTop (8);
        numberStrip = r.removeFromTop (20);
        wave = r;

        auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w) {
            c.setBounds (row.removeFromLeft (w).reduced (2, 0));
        };

        place (top, playButton, 90);
        top.removeFromLeft (8);
        place (top, zoomOut, 40);
        place (top, zoomIn, 40);
        place (top, zoomFit, 90);
        top.removeFromLeft (16);
        place (top, modeTrim, 110);
        place (top, modeLoop, 110);
        place (top, modeChop, 110);
        top.removeFromLeft (16);
        place (top, loopButton, 80);
        place (top, undoButton, 70);

        place (middle, regionsMenu, 110);
        place (middle, cutEqually, 110);
        middle.removeFromLeft (16);
        place (middle, threshold, 160);
        place (middle, findHits, 110);
        middle.removeFromLeft (16);
        place (middle, clearSlices, 110);

        place (bottom, cropButton, 80);
        place (bottom, normaliseButton, 90);
        place (bottom, reverseButton, 80);
        place (bottom, fadeInButton, 80);
        place (bottom, fadeOutButton, 80);
        place (bottom, silenceButton, 80);
        bottom.removeFromLeft (12);
        info.setBounds (bottom);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = Drag::none;
        if (audio.getNumSamples() < 2)
            return;

        // The number strip plays a slice while held.
        if (numberStrip.contains (e.getPosition()) && ! slices.empty())
        {
            const int k = sliceAt (xToPos (e.x));
            audition (Sampler::rootNote + k, true);
            return;
        }

        if (! wave.contains (e.getPosition()))
            return;

        const double pos = xToPos (e.x);
        if (modeChop.getToggleState())
        {
            const int near = sliceNear (e.x);
            if (near >= 0 && (e.mods.isPopupMenu() || e.mods.isAltDown()))
            {
                snapshot (false);
                slices.erase (slices.begin() + near);
                writeMarkers();
                return;
            }
            snapshot (false);
            if (near >= 0)
            {
                dragging = Drag::slice;
                dragIndex = near;
            }
            else if ((int) slices.size() < Sampler::maxSlices)
            {
                if (slices.empty())
                    slices.push_back ((float) startPos);   // the first slice always starts at Start
                slices.push_back ((float) pos);
                sortSlices();
                dragging = Drag::slice;
                dragIndex = sliceNear (posToX (pos));
            }
            writeMarkers();
            return;
        }

        snapshot (false);
        if (modeLoop.getToggleState())
        {
            dragging = Drag::loop;
        }
        else
        {
            dragging = std::abs (pos - startPos) <= std::abs (pos - endPos) ? Drag::start : Drag::end;
        }
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const double pos = juce::jlimit (0.0, 1.0, xToPos (e.x));
        const double gap = 1.0 / juce::jmax (2, audio.getNumSamples());
        switch (dragging)
        {
            case Drag::start: startPos = juce::jlimit (0.0, endPos - gap, pos); break;
            case Drag::end:   endPos = juce::jlimit (startPos + gap, 1.0, pos); break;
            case Drag::loop:  loopPos = juce::jlimit (startPos, endPos, pos); break;
            case Drag::slice:
                if (juce::isPositiveAndBelow (dragIndex, (int) slices.size()))
                {
                    slices[(size_t) dragIndex] = (float) pos;
                    const float moved = slices[(size_t) dragIndex];
                    sortSlices();
                    dragIndex = (int) (std::find (slices.begin(), slices.end(), moved) - slices.begin());
                }
                break;
            case Drag::none: return;
        }
        writeMarkers();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = Drag::none;
        if (auditioning >= 0)
            audition (auditioning, false);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! wave.contains (e.getPosition()))
            return;
        const float anchor = (float) (e.x - wave.getX()) / (float) juce::jmax (1, wave.getWidth());
        if (std::abs (w.deltaX) > std::abs (w.deltaY))
        {
            const double width = viewEnd - viewStart;
            const double shift = -w.deltaX * width;
            viewStart = juce::jlimit (0.0, 1.0 - width, viewStart + shift);
            viewEnd = viewStart + width;
            repaint();
        }
        else
        {
            zoom (w.deltaY > 0 ? 0.8 : 1.25, anchor);
        }
    }

private:
    enum class Drag { none, start, end, loop, slice };

    struct Snapshot
    {
        bool hasAudio = false;
        juce::AudioBuffer<float> audio;
        double startPos = 0, endPos = 1, loopPos = 0;
        std::vector<float> slices;
    };

    Sampler::Params& params() { return engine.track (track).samplerEngine.params; }

    // ---------------------------------------------------------- markers <-> sampler
    void readMarkers()
    {
        auto& p = params();
        startPos = juce::jlimit (0.0, 1.0, (double) p.start.load());
        endPos = juce::jlimit (startPos, 1.0, startPos + (double) p.length.load() * (1.0 - startPos));
        if (endPos <= startPos)
            endPos = 1.0;
        loopPos = juce::jlimit (0.0, 1.0, (double) p.loopStart.load());
        slices.clear();
        for (int i = 0; i < juce::jlimit (0, Sampler::maxSlices, p.sliceCount.load()); ++i)
            slices.push_back (p.slicePoints[(size_t) i].load());
        sortSlices();
    }

    void writeMarkers()
    {
        auto& p = params();
        p.start.store ((float) startPos);
        p.length.store (startPos < 1.0 ? (float) juce::jlimit (0.01, 1.0, (endPos - startPos) / (1.0 - startPos)) : 1.0f);
        p.loopStart.store ((float) loopPos);
        const int n = juce::jmin ((int) slices.size(), Sampler::maxSlices);
        for (int i = 0; i < n; ++i)
            p.slicePoints[(size_t) i].store (slices[(size_t) i]);
        p.sliceCount.store (n);
        updateInfo();
        repaint();
    }

    void sortSlices()
    {
        std::sort (slices.begin(), slices.end());
        slices.erase (std::unique (slices.begin(), slices.end()), slices.end());
        if ((int) slices.size() > Sampler::maxSlices)
            slices.resize ((size_t) Sampler::maxSlices);
    }

    // ---------------------------------------------------------- chop
    void chopEqually (int count)
    {
        snapshot (false);
        slices.clear();
        count = juce::jlimit (2, Sampler::maxSlices, count);
        for (int i = 0; i < count; ++i)
            slices.push_back ((float) (startPos + (endPos - startPos) * i / count));
        modeChop.setToggleState (true, juce::dontSendNotification);
        writeMarkers();
    }

    void chopByHits()
    {
        snapshot (false);
        slices.clear();
        slices.push_back ((float) startPos);
        for (float h : SampleTools::findHits (audio, rate, (float) threshold.getValue(), 64))
            if (h > startPos + 0.002 && h < endPos && (int) slices.size() < Sampler::maxSlices)
                slices.push_back (h);
        modeChop.setToggleState (true, juce::dontSendNotification);
        writeMarkers();
    }

    int sliceNear (int x) const
    {
        for (size_t i = 0; i < slices.size(); ++i)
            if (std::abs (posToX (slices[i]) - x) <= 5)
                return (int) i;
        return -1;
    }

    int sliceAt (double pos) const
    {
        int k = 0;
        for (size_t i = 0; i < slices.size(); ++i)
            if (pos >= slices[i])
                k = (int) i;
        return k;
    }

    // ---------------------------------------------------------- processes
    template <typename Fn>
    void process (Fn&& fn, bool isCrop)
    {
        const int n = audio.getNumSamples();
        if (n < 2)
            return;
        const int from = juce::jlimit (0, n - 1, (int) (startPos * (n - 1)));
        const int to = juce::jlimit (from + 1, n, (int) (endPos * (n - 1)) + 1);

        snapshot (true);
        fn (audio, from, to);

        if (isCrop)
        {
            // The markers now refer to the kept part.
            const double s = startPos, w = juce::jmax (1.0e-9, endPos - startPos);
            std::vector<float> kept;
            for (float p : slices)
                if (p >= s && p < endPos)
                    kept.push_back ((float) ((p - s) / w));
            slices = kept;
            loopPos = juce::jlimit (0.0, 1.0, (loopPos - s) / w);
            startPos = 0.0;
            endPos = 1.0;
            viewStart = 0.0;
            viewEnd = 1.0;
        }
        reloadAudio();
        writeMarkers();
    }

    void reloadAudio()
    {
        juce::String error;
        if (! Project::loadSampleIntoTrack (engine, track, fileName + ".wav", SampleTools::encodeWav (audio, rate), formats, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Sample editor", error);
    }

    // ---------------------------------------------------------- undo
    void snapshot (bool withAudio)
    {
        Snapshot s;
        s.hasAudio = withAudio;
        if (withAudio)
            s.audio.makeCopyOf (audio);
        s.startPos = startPos;
        s.endPos = endPos;
        s.loopPos = loopPos;
        s.slices = slices;
        history.push_back (std::move (s));
        if (history.size() > 20)
            history.erase (history.begin());
    }

    void undo()
    {
        if (history.empty())
            return;
        auto s = std::move (history.back());
        history.pop_back();
        startPos = s.startPos;
        endPos = s.endPos;
        loopPos = s.loopPos;
        slices = s.slices;
        if (s.hasAudio)
        {
            audio = std::move (s.audio);
            reloadAudio();
        }
        writeMarkers();
    }

    // ---------------------------------------------------------- view and sound
    double xToPos (int x) const
    {
        const double t = (double) (x - wave.getX()) / (double) juce::jmax (1, wave.getWidth());
        return viewStart + t * (viewEnd - viewStart);
    }

    int posToX (double pos) const
    {
        return wave.getX() + juce::roundToInt ((pos - viewStart) / juce::jmax (1.0e-9, viewEnd - viewStart) * wave.getWidth());
    }

    void zoom (double factor, float anchor)
    {
        const double width = juce::jlimit (0.0005, 1.0, (viewEnd - viewStart) * factor);
        const double centre = viewStart + (viewEnd - viewStart) * anchor;
        viewStart = juce::jlimit (0.0, 1.0 - width, centre - width * anchor);
        viewEnd = viewStart + width;
        repaint();
    }

    void audition (int note, bool down)
    {
        if (down)
        {
            if (auditioning >= 0)
                engine.keyboardState.noteOff (1, auditioning, 0.0f);
            engine.selectedTrack.store (track);
            engine.keyboardState.noteOn (1, note, 0.9f);
            auditioning = note;
        }
        else if (auditioning >= 0)
        {
            engine.keyboardState.noteOff (1, auditioning, 0.0f);
            auditioning = -1;
        }
    }

    void updateInfo()
    {
        const double seconds = rate > 0.0 ? audio.getNumSamples() / rate : 0.0;
        const double chosen = seconds * (endPos - startPos);
        juce::String text;
        text << fileName << "   " << juce::String (seconds, 2) << " s, chosen part " << juce::String (chosen, 2) << " s";
        if (! slices.empty())
            text << ", " << (int) slices.size() << " slices on the pads";
        info.setText (text, juce::dontSendNotification);
    }

    AudioEngine& engine;
    const int track;
    juce::AudioFormatManager& formats;

    juce::AudioBuffer<float> audio;
    double rate = 44100.0;
    juce::String fileName;

    double startPos = 0.0, endPos = 1.0, loopPos = 0.0;
    std::vector<float> slices;
    double viewStart = 0.0, viewEnd = 1.0;
    std::vector<Snapshot> history;

    Drag dragging = Drag::none;
    int dragIndex = -1;
    int auditioning = -1;

    juce::Rectangle<int> wave, numberStrip;

    HoldButton playButton { "Play" };
    juce::TextButton zoomIn { "+" }, zoomOut { "-" }, zoomFit { "Whole" };
    juce::TextButton modeTrim { "Trim" }, modeLoop { "Loop point" }, modeChop { "Chop by hand" };
    juce::TextButton loopButton { "Loop" }, undoButton { "Undo" };
    juce::ComboBox regionsMenu;
    juce::TextButton cutEqually { "Cut equally" };
    juce::Slider threshold { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton findHits { "Chop by hits" }, clearSlices { "Clear slices" };
    juce::TextButton cropButton { "Crop" }, normaliseButton { "Normalise" }, reverseButton { "Reverse" },
                     fadeInButton { "Fade in" }, fadeOutButton { "Fade out" }, silenceButton { "Silence" };
    juce::Label info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleEditor)
};

} // namespace bizgroove
