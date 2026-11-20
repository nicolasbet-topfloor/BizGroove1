#include "StepGrid.h"
#include "LookAndFeel.h"

namespace bizgroove
{

StepGrid::StepGrid (AudioEngine& e, const ControlTargets& t) : engine (e), targets (t)
{
    startTimerHz (60);
}

StepGrid::~StepGrid()
{
    endLockEdit();
}

Song::TrackPattern& StepGrid::pattern()
{
    return engine.song.scene (engine.song.currentScene.load()).tracks[(size_t) engine.selectedTrack.load()];
}

int StepGrid::stepOf (int index) const
{
    return juce::jlimit (0, 3, engine.stepPage.load()) * Song::stepsPerBar + index;
}

juce::Rectangle<float> StepGrid::cellBounds (int index) const
{
    if (twoRows)
    {
        // Launchkey layout: steps 1-8 on the top row, 9-16 on the bottom row.
        const float gap = 10.0f;
        const float w = ((float) getWidth() - gap * 7.0f) / 8.0f;
        const float h = ((float) getHeight() - gap) / 2.0f;
        const int col = index % 8, row = index / 8;
        return { (float) col * (w + gap), (float) row * (h + gap), w, h };
    }

    const float gap = 6.0f;
    const float groupGap = 10.0f; // extra space between each group of four steps
    const float totalGaps = gap * (Song::stepsPerBar - 1) + groupGap * 3;
    const float w = ((float) getWidth() - totalGaps) / (float) Song::stepsPerBar;
    const float x = (float) index * (w + gap) + (float) (index / 4) * groupGap;
    return { x, 0.0f, w, (float) getHeight() };
}

int StepGrid::stepAt (juce::Point<float> position) const
{
    for (int i = 0; i < Song::stepsPerBar; ++i)
        if (cellBounds (i).contains (position))
            return i;
    return -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    const int playhead = engine.song.trackStep[(size_t) engine.selectedTrack.load()].load();
    const bool drums = engine.selected().playsDrums();
    const bool playing = engine.transport.playing.load() && playhead >= 0;
    auto& p = pattern();
    const int length = p.getLength();

    for (int i = 0; i < Song::stepsPerBar; ++i)
    {
        const int step = stepOf (i);
        const auto r = cellBounds (i);
        const float corner = twoRows ? 4.0f : 6.0f;

        // Steps past the pattern's end: greyed, they do not play.
        if (step >= length)
        {
            g.setColour (Colours::panel.withAlpha (0.5f));
            g.fillRoundedRectangle (r, corner);
            g.setColour (Colours::panelEdge.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (1.0f), corner, 1.0f);
            g.setColour (Colours::textDim.withAlpha (0.35f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (juce::String (step + 1), r.reduced (2.0f, 6.0f), juce::Justification::centredTop);
            continue;
        }

        const bool on = engine.isStepOn (step);
        const bool now = playing && step == playhead;

        g.setColour (on ? Colours::accent : (twoRows ? juce::Colour (0xff3a3c42) : Colours::panel));
        g.fillRoundedRectangle (r, corner);

        // The step playing lights up, so the running light is easy to follow.
        if (now)
        {
            g.setColour (Colours::playhead.withAlpha (on ? 0.55f : 0.35f));
            g.fillRoundedRectangle (r, corner);
        }
        g.setColour (now ? Colours::playhead : (i % 4 == 0 ? Colours::textDim.withAlpha (0.6f) : Colours::panelEdge));
        g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, now ? 3.0f : 1.0f);

        // Held for locking: a yellow frame.
        if (step == lockStep)
        {
            g.setColour (lockColour());
            g.drawRoundedRectangle (r.reduced (2.0f), corner, 4.0f);
        }
        // Steps with locks: a yellow dot, top right.
        if (const int locks = p.numLocks (step); locks > 0)
        {
            g.setColour (on ? juce::Colours::black.withAlpha (0.75f) : lockColour());
            g.fillEllipse (r.getRight() - 14.0f, r.getY() + 6.0f, 8.0f, 8.0f);
        }

        g.setColour (on ? juce::Colours::black : Colours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        if (! drums)
        {
            // The note, and "+2" when it is a chord of three.
            const auto& st = p.steps[(size_t) step];
            std::array<int, Song::maxChord> notes {};
            const int count = st.getNotes (notes);
            auto label = juce::MidiMessage::getMidiNoteName (notes[0], true, true, 4);
            if (count > 1)
                label << " +" << (count - 1);
            g.drawText (label, r.reduced (2.0f, 6.0f), juce::Justification::centredBottom);
        }
        else
        {
            // Every drum that plays on this step: a dot each, the selected drum's bigger.
            const float dot = juce::jmin (6.0f, (r.getWidth() - 8.0f) / 8.0f - 1.0f);
            const int picked = engine.selected().selectedDrum.load();
            for (int d = 0; d < Song::numDrums; ++d)
            {
                if (((p.drumLanes[(size_t) d].load() >> step) & 1u) == 0)
                    continue;
                const float x = r.getX() + 4.0f + (float) (d % 8) * (dot + 1.0f);
                const float y = r.getBottom() - 6.0f - (float) (2 - d / 8) * (dot + 1.0f);
                g.setColour (on ? juce::Colours::black.withAlpha (d == picked ? 0.9f : 0.5f)
                                : Colours::text.withAlpha (d == picked ? 0.9f : 0.5f));
                g.fillEllipse (x, y, dot, dot);
            }
        }

        g.setColour (on ? juce::Colours::black : Colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String (step + 1), r.reduced (2.0f, 6.0f), juce::Justification::centredTop);
    }
}

void StepGrid::nudgeNote (int index, int semitones)
{
    if (engine.selected().playsDrums())
        return;   // drum steps have no note
    const int step = stepOf (index);
    engine.setStepNote (step, engine.getStepNote (step) + semitones);
    repaint();
}

void StepGrid::beginLockEdit (int step)
{
    lockStep = step;
    targets.beginStepHold (step);
    repaint();
}

void StepGrid::endLockEdit()
{
    if (lockStep < 0)
        return;
    lockStep = -1;
    targets.endStepHold();
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    dragged = false;
    pressBecameLock = false;
    const int index = stepAt (e.position);
    dragStep = (index >= 0 && stepOf (index) < pattern().getLength()) ? index : -1;
    if (dragStep < 0)
        return;
    const int step = stepOf (dragStep);

    // Right-click: hold the step for locking (or let it go).
    if (e.mods.isPopupMenu())
    {
        if (lockStep == step) endLockEdit();
        else                  { engine.rememberPattern(); beginLockEdit (step); }
        pressBecameLock = true;
        return;
    }
    // While a step is held for locking, a click lets it go, or holds another one.
    if (lockStep >= 0)
    {
        if (lockStep == step) endLockEdit();
        else                  { engine.rememberPattern(); beginLockEdit (step); }
        pressBecameLock = true;
        return;
    }
    engine.rememberPattern();   // a click toggles the step, a drag changes its note, a hold locks it
    pressTime = juce::Time::getMillisecondCounter();
    dragStartNote = engine.getStepNote (step);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0 || pressBecameLock)
        return;

    const int pixelsPerSemitone = e.mods.isShiftDown() ? 1 : 6;
    const int delta = -e.getDistanceFromDragStartY() / pixelsPerSemitone;
    if (std::abs (e.getDistanceFromDragStartY()) > 3)
        dragged = true;

    if (engine.selected().playsDrums())
        return;
    engine.setStepNote (stepOf (dragStep), dragStartNote + (e.mods.isShiftDown() ? delta / 12 * 12 : delta));
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragStep >= 0 && ! dragged && ! pressBecameLock)
    {
        engine.toggleStep (stepOf (dragStep));
        repaint();
    }
    dragStep = -1;
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int index = stepAt (e.position);
    if (index < 0 || std::abs (wheel.deltaY) < 1.0e-6f)
        return;

    const int direction = wheel.deltaY > 0 ? 1 : -1;
    engine.rememberPattern (1);   // a run of scrolls is one undo step
    nudgeNote (index, direction * (e.mods.isShiftDown() ? 12 : 1));
}

void StepGrid::timerCallback()
{
    // Held down for a moment without moving: the step is held for locking.
    if (dragStep >= 0 && ! dragged && ! pressBecameLock && lockStep < 0
        && juce::Time::getMillisecondCounter() - (juce::uint32) pressTime > 450u
        && juce::ModifierKeys::currentModifiers.isLeftButtonDown())
    {
        beginLockEdit (stepOf (dragStep));
        pressBecameLock = true;
    }

    if (lockStep >= 0 && engine.heldStep.load() != lockStep)
    {
        lockStep = -1;   // let go elsewhere (a controller's pad)
        repaint();
    }

    // Another track or scene, or the step went past the end: let the held step go.
    const int track = engine.selectedTrack.load(), scene = engine.song.currentScene.load();
    if (track != shownTrack || scene != shownScene)
    {
        shownTrack = track;
        shownScene = scene;
        endLockEdit();
    }
    auto& p = pattern();
    const int bars = (p.getLength() + Song::stepsPerBar - 1) / Song::stepsPerBar;
    if (engine.stepPage.load() >= bars)
        engine.stepPage.store (bars - 1);
    if (lockStep >= p.getLength())
        endLockEdit();

    // Repaint when the playhead moves or any shown step changes (e.g. from a controller).
    std::uint32_t hash = (std::uint32_t) (engine.song.trackStep[(size_t) track].load() + 1) * 2654435761u;
    for (int i = 0; i < Song::stepsPerBar; ++i)
    {
        const int step = stepOf (i);
        const auto& st = p.steps[(size_t) step];
        hash = hash * 31u + (engine.isStepOn (step) ? 1u : 0u) + (std::uint32_t) engine.getStepNote (step) * 7u
                          + (std::uint32_t) p.numLocks (step) * 13u;
        for (const auto& c : st.chord)
            hash = hash * 31u + (std::uint32_t) c.load();
    }
    for (const auto& lane : p.drumLanes)
        hash = hash * 31u + (std::uint32_t) (lane.load() ^ (lane.load() >> 32));
    hash = hash * 31u + (engine.transport.playing.load() ? 1u : 0u) + (std::uint32_t) engine.selected().selectedDrum.load() * 17u;
    hash = hash * 31u + (std::uint32_t) track * 131u + (std::uint32_t) scene;
    hash = hash * 31u + (std::uint32_t) engine.stepPage.load() * 7u + (std::uint32_t) p.getLength() * 3u + (std::uint32_t) (lockStep + 1);
    if (hash != lastHash)
    {
        lastHash = hash;
        repaint();
    }
}

// ============================================================== LengthControl

LengthControl::LengthControl (AudioEngine& e) : engine (e)
{
    for (auto* b : { &minus, &value, &plus })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    minus.setTooltip ("One step shorter");
    plus.setTooltip ("One step longer");
    value.setTooltip ("How many steps this track's pattern plays before it starts again. Click for 1, 2, 3 or 4 bars.");
    minus.onClick = [this] { setLength (pattern().getLength() - 1); };
    plus.onClick = [this] { setLength (pattern().getLength() + 1); };
    value.onClick = [this] {
        juce::PopupMenu menu;
        const int now = pattern().getLength();
        for (int bars = 1; bars <= 4; ++bars)
            menu.addItem (bars * Song::stepsPerBar, juce::String (bars) + (bars == 1 ? " bar" : " bars") + " (" + juce::String (bars * Song::stepsPerBar) + " steps)",
                          true, now == bars * Song::stepsPerBar);
        juce::PopupMenu steps;
        for (int n = 1; n <= Song::numSteps; ++n)
            steps.addItem (1000 + n, juce::String (n) + (n == 1 ? " step" : " steps"), true, now == n);
        menu.addSeparator();
        menu.addSubMenu ("Any number of steps", steps);
        menu.addSeparator();
        menu.addItem (2000, "Make it twice as long, repeating what is there", now * 2 <= Song::numSteps);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&value), [this] (int r) {
            if (r == 2000)
            {
                auto& p = pattern();
                const int n = p.getLength();
                for (int i = 0; i < n; ++i)
                {
                    p.steps[(size_t) (n + i)].copyFrom (p.steps[(size_t) i]);
                    for (size_t k = 0; k < (size_t) Song::maxLocks; ++k)
                    {
                        const auto& from = p.locks[(size_t) i][k];
                        auto& to = p.locks[(size_t) (n + i)][k];
                        to.value.store (from.value.load());
                        to.drum.store (from.drum.load());
                        to.target.store (from.target.load());
                    }
                }
                for (auto& lane : p.drumLanes)
                {
                    const auto bits = lane.load() & (n >= 64 ? ~(std::uint64_t) 0 : (((std::uint64_t) 1 << n) - 1));
                    lane.store (bits | (bits << n));
                }
                setLength (n * 2);
            }
            else if (r > 1000)
                setLength (r - 1000);
            else if (r > 0)
                setLength (r);
        });
    };
    startTimerHz (10);
}

Song::TrackPattern& LengthControl::pattern()
{
    return engine.song.scene (engine.song.currentScene.load()).tracks[(size_t) engine.selectedTrack.load()];
}

void LengthControl::setLength (int steps)
{
    engine.rememberPattern (2);
    pattern().length.store (juce::jlimit (1, Song::numSteps, steps));
    timerCallback();
    if (onChange)
        onChange();
}

void LengthControl::resized()
{
    auto r = getLocalBounds();
    const int side = juce::jmin (26, r.getWidth() / 4);
    minus.setBounds (r.removeFromLeft (side));
    plus.setBounds (r.removeFromRight (side));
    value.setBounds (r.reduced (2, 0));
}

void LengthControl::paint (juce::Graphics&) {}

void LengthControl::timerCallback()
{
    const int n = pattern().getLength();
    const auto text = getWidth() < 120 ? juce::String (n)
                                       : juce::String (n) + " steps" + (n % Song::stepsPerBar == 0 ? " (" + juce::String (n / Song::stepsPerBar) + (n == Song::stepsPerBar ? " bar)" : " bars)") : juce::String());
    if (value.getButtonText() != text)
        value.setButtonText (text);
    minus.setEnabled (n > 1);
    plus.setEnabled (n < Song::numSteps);
}

// ============================================================== StepPager

StepPager::StepPager (AudioEngine& e) : engine (e)
{
    for (int i = 0; i < 4; ++i)
    {
        auto& b = pages[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.setWantsKeyboardFocus (false);
        b.setTooltip ("Show bar " + juce::String (i + 1) + " of the pattern on the pads");
        b.onClick = [this, i] { engine.stepPage.store (i); refresh(); };
        addAndMakeVisible (b);
    }
    length.onChange = [this] { refresh(); };
    addAndMakeVisible (length);
    startTimerHz (15);
}

void StepPager::resized()
{
    auto r = getLocalBounds();
    barsLabel = r.removeFromTop (16);
    const int w = (r.getWidth() - 6) / 2;
    auto row1 = r.removeFromTop (26);
    pages[0].setBounds (row1.removeFromLeft (w));
    pages[1].setBounds (row1.removeFromRight (w));
    r.removeFromTop (6);
    auto row2 = r.removeFromTop (26);
    pages[2].setBounds (row2.removeFromLeft (w));
    pages[3].setBounds (row2.removeFromRight (w));
    r.removeFromTop (10);
    lengthLabel = r.removeFromTop (16);
    length.setBounds (r.removeFromTop (28));
}

void StepPager::paint (juce::Graphics& g)
{
    g.setColour (Colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Bar shown", barsLabel, juce::Justification::centred);
    g.drawText ("Length", lengthLabel, juce::Justification::centred);
}

void StepPager::timerCallback() { refresh(); }

void StepPager::refresh()
{
    const auto& p = engine.song.scene (engine.song.currentScene.load()).tracks[(size_t) engine.selectedTrack.load()];
    const int bars = (p.getLength() + Song::stepsPerBar - 1) / Song::stepsPerBar;
    const int page = juce::jlimit (0, 3, engine.stepPage.load());
    const int playing = engine.transport.playing.load() ? engine.song.trackStep[(size_t) engine.selectedTrack.load()].load() : -1;
    for (int i = 0; i < 4; ++i)
    {
        auto& b = pages[(size_t) i];
        const bool used = i < bars;
        b.setEnabled (used);
        const bool here = playing >= 0 && playing / Song::stepsPerBar == i;   // the bar playing now
        const auto colour = i == page ? Colours::accent : (here ? Colours::playhead.withAlpha (0.35f) : Colours::panelEdge);
        b.setColour (juce::TextButton::buttonColourId, colour);
        b.setColour (juce::TextButton::textColourOffId, i == page ? juce::Colours::black : Colours::text);
    }
}

} // namespace bizgroove
