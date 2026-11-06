#include "MixerView.h"
#include "LookAndFeel.h"
#include "engine/SynthLibrary.h"

namespace bizgroove
{

namespace
{
    juce::String percentText (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String dbText (float d)      { return std::abs (d) < 0.3f ? juce::String ("Flat") : (d > 0 ? "+" : "") + juce::String (d, 1) + " dB"; }
    juce::String panText (float p)
    {
        if (std::abs (p) < 0.04f) return "Centre";
        return (p < 0 ? "L " : "R ") + juce::String (juce::roundToInt (std::abs (p) * 100.0f));
    }
}

// ============================================================== MixKnob

MixKnob::MixKnob (const juce::String& label, double min, double max, double reset,
                  std::function<float()> g, std::function<void (float)> s, std::function<juce::String (float)> sh)
    : get (std::move (g)), set (std::move (s)), show (std::move (sh))
{
    slider.setRange (min, max);
    slider.setDoubleClickReturnValue (true, reset);
    slider.setWantsKeyboardFocus (false);
    slider.setValue (get(), juce::dontSendNotification);
    slider.onValueChange = [this] { set ((float) slider.getValue()); value.setText (show ((float) slider.getValue()), juce::dontSendNotification); };
    addAndMakeVisible (slider);

    name.setText (label, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    name.setColour (juce::Label::textColourId, Colours::textDim);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (juce::FontOptions (10.5f));
    value.setColour (juce::Label::textColourId, Colours::text);
    value.setInterceptsMouseClicks (false, false);
    value.setText (show (get()), juce::dontSendNotification);
    addAndMakeVisible (value);
}

void MixKnob::refresh()
{
    const float v = get();
    if (! slider.isMouseButtonDown() && std::abs (slider.getValue() - v) > 1.0e-4)
    {
        slider.setValue (v, juce::dontSendNotification);
        value.setText (show (v), juce::dontSendNotification);
    }
}

void MixKnob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (13));
    value.setBounds (r.removeFromBottom (13));
    slider.setBounds (r);
}

// ============================================================== ChannelView

ChannelView::ChannelView (AudioEngine& e, int t) : engine (e), track (t)
{
    auto& tr = e.track (t);
    auto& m = tr.mix;

    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    auto add = [this] (MixKnob* k, const juce::String& tip) { k->setTooltip (tip); addAndMakeVisible (knobs.add (k)); };
    add (new MixKnob ("PAN", -1.0, 1.0, 0.0, [&m] { return m.pan.load(); },
                      [&m] (float v) { m.pan.store (std::abs (v) < 0.04f ? 0.0f : v); }, panText),
         "Places the track left or right. Double-click to centre.");
    add (new MixKnob ("BASS", -12.0, 12.0, 0.0, [&m] { return m.eqLow.load(); }, [&m] (float v) { m.eqLow.store (v); }, dbText),
         "More or less low end (below about 120 Hz). Double-click to reset.");
    add (new MixKnob ("MIDDLE", -12.0, 12.0, 0.0, [&m] { return m.eqMid.load(); }, [&m] (float v) { m.eqMid.store (v); }, dbText),
         "More or less of the middle (around 1 kHz). Double-click to reset.");
    add (new MixKnob ("TREBLE", -12.0, 12.0, 0.0, [&m] { return m.eqHigh.load(); }, [&m] (float v) { m.eqHigh.store (v); }, dbText),
         "More or less top end (above about 8 kHz). Double-click to reset.");
    add (new MixKnob ("REVERB", 0.0, 1.0, 0.0, [&m] { return m.reverb.load(); }, [&m] (float v) { m.reverb.store (v); }, percentText),
         "How much of this track goes to the reverb (room sound).");
    add (new MixKnob ("DELAY", 0.0, 1.0, 0.0, [&m] { return m.delay.load(); }, [&m] (float v) { m.delay.store (v); }, percentText),
         "How much of this track goes to the delay (echoes in time).");

    fader.setRange (0.0, 1.0);
    fader.setValue (tr.volume.load(), juce::dontSendNotification);
    fader.setDoubleClickReturnValue (true, 0.8);
    fader.setWantsKeyboardFocus (false);
    fader.setTooltip ("Track volume. Double-click to reset.");
    fader.onValueChange = [this] { engine.track (track).volume.store ((float) fader.getValue()); };
    addAndMakeVisible (fader);

    for (auto* b : { &mute, &solo })
    {
        b->setClickingTogglesState (true);
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe05252));
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfff2c94c));
    mute.setTooltip ("Mute this track");
    solo.setTooltip ("Solo: hear only soloed tracks");
    mute.onClick = [this] { engine.track (track).mute.store (mute.getToggleState()); };
    solo.onClick = [this] { engine.track (track).mix.solo.store (solo.getToggleState()); };
}

void ChannelView::refresh()
{
    auto& tr = engine.track (track);
    for (auto* k : knobs)
        k->refresh();
    if (! fader.isMouseButtonDown() && std::abs (fader.getValue() - tr.volume.load()) > 1.0e-4)
        fader.setValue (tr.volume.load(), juce::dontSendNotification);
    mute.setToggleState (tr.mute.load(), juce::dontSendNotification);
    solo.setToggleState (tr.mix.solo.load(), juce::dontSendNotification);

    const auto& library = synthLibrary();
    const auto text = juce::String (track + 1) + " " + library[(size_t) juce::jlimit (0, (int) library.size() - 1, tr.type.load())].name;
    if (title.getText() != text)
        title.setText (text, juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, engine.selectedTrack.load() == track ? Colours::accent : Colours::text);

    const float lvl = tr.level.load();
    const float next = juce::jmax (lvl, shownLevel * 0.85f);
    if (std::abs (next - shownLevel) > 0.002f)
    {
        shownLevel = next;
        repaint (meter.expanded (1));
    }
}

void ChannelView::mouseDown (const juce::MouseEvent&)
{
    engine.selectedTrack.store (track);   // clicking a channel selects its track
}

void ChannelView::paint (juce::Graphics& g)
{
    const bool selected = engine.selectedTrack.load() == track;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? Colours::accent.withAlpha (0.10f) : Colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (selected ? Colours::accent : Colours::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, selected ? 1.5f : 1.0f);

    g.setColour (Colours::background);
    g.fillRoundedRectangle (meter.toFloat(), 2.0f);
    const float db = juce::Decibels::gainToDecibels (shownLevel, -60.0f);
    const float fill = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
    g.setColour (db > -1.0f ? juce::Colours::red : Colours::playhead);
    g.fillRoundedRectangle (meter.toFloat().withTrimmedTop (meter.getHeight() * (1.0f - fill)), 2.0f);
}

void ChannelView::resized()
{
    auto r = getLocalBounds().reduced (4, 6);
    title.setBounds (r.removeFromTop (18));
    r.removeFromTop (2);

    auto buttons = r.removeFromBottom (24);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 1));
    buttons.removeFromLeft (2);
    solo.setBounds (buttons);
    r.removeFromBottom (4);

    const int knobH = juce::jmin (76, (r.getHeight() - 140) / knobs.size());
    for (auto* k : knobs)
        k->setBounds (r.removeFromTop (knobH));
    r.removeFromTop (4);

    meter = r.removeFromRight (8);
    fader.setBounds (r);
}

// ============================================================== MixerView

MixerView::MixerView (AudioEngine& e) : engine (e)
{
    for (int t = 0; t < AudioEngine::numTracks; ++t)
        addAndMakeVisible (channels.add (new ChannelView (e, t)));

    masterTitle.setText ("MASTER", juce::dontSendNotification);
    masterTitle.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    masterTitle.setJustificationType (juce::Justification::centred);
    masterTitle.setColour (juce::Label::textColourId, Colours::accent);
    addAndMakeVisible (masterTitle);

    auto& fx = e.effects;
    auto add = [this] (MixKnob* k, const juce::String& tip) { k->setTooltip (tip); addAndMakeVisible (masterKnobs.add (k)); };
    add (new MixKnob ("REVERB SIZE", 0.0, 1.0, 0.5, [&fx] { return fx.reverbSize.load(); },
                      [&fx] (float v) { fx.reverbSize.store (v); }, percentText),
         "Small room (left) to large hall (right), for every track's reverb.");
    add (new MixKnob ("DELAY REPEATS", 0.0, 0.9, 0.35, [&fx] { return fx.delayFeedback.load(); },
                      [&fx] (float v) { fx.delayFeedback.store (v); },
                      [] (float v) { return v < 0.05f ? juce::String ("One echo") : percentText (v / 0.9f); }),
         "How many echoes the delay makes before fading away.");

    for (int d = 0; d < 4; ++d)
        delayTime.addItem (MasterEffects::divisionName (d), d + 1);
    delayTime.setSelectedId (fx.delayDivision.load() + 1, juce::dontSendNotification);
    delayTime.setTooltip ("Time between echoes, in step with the tempo.");
    delayTime.setWantsKeyboardFocus (false);
    delayTime.onChange = [this] { engine.effects.delayDivision.store (delayTime.getSelectedId() - 1); };
    addAndMakeVisible (delayTime);

    delayTimeLabel.setText ("DELAY TIME", juce::dontSendNotification);
    delayTimeLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    delayTimeLabel.setJustificationType (juce::Justification::centred);
    delayTimeLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (delayTimeLabel);

    masterFader.setRange (0.0, 1.0);
    masterFader.setValue (e.masterGain.load(), juce::dontSendNotification);
    masterFader.setDoubleClickReturnValue (true, 0.8);
    masterFader.setWantsKeyboardFocus (false);
    masterFader.setTooltip ("Master volume. Double-click to reset.");
    masterFader.onValueChange = [this] { engine.masterGain.store ((float) masterFader.getValue()); };
    addAndMakeVisible (masterFader);

    startTimerHz (20);
}

void MixerView::timerCallback()
{
    if (! isShowing())
        return;
    for (auto* c : channels)
        c->refresh();
    for (auto* k : masterKnobs)
        k->refresh();
    delayTime.setSelectedId (engine.effects.delayDivision.load() + 1, juce::dontSendNotification);
    if (! masterFader.isMouseButtonDown() && std::abs (masterFader.getValue() - engine.masterGain.load()) > 1.0e-4)
        masterFader.setValue (engine.masterGain.load(), juce::dontSendNotification);
}

void MixerView::paint (juce::Graphics& g)
{
    g.setColour (Colours::panel.darker (0.2f));
    g.fillRoundedRectangle (masterArea.toFloat(), 8.0f);
    g.setColour (Colours::accent.withAlpha (0.6f));
    g.drawRoundedRectangle (masterArea.toFloat().reduced (0.5f), 8.0f, 1.0f);
}

void MixerView::resized()
{
    auto r = getLocalBounds();
    masterArea = r.removeFromRight (118);
    r.removeFromRight (8);

    const int w = r.getWidth() / AudioEngine::numTracks;
    for (auto* c : channels)
        c->setBounds (r.removeFromLeft (w).reduced (1, 0));

    auto m = masterArea.reduced (8, 8);
    masterTitle.setBounds (m.removeFromTop (20));
    for (auto* k : masterKnobs)
        k->setBounds (m.removeFromTop (70));
    delayTimeLabel.setBounds (m.removeFromTop (14));
    delayTime.setBounds (m.removeFromTop (26));
    m.removeFromTop (10);
    masterFader.setBounds (m.withSizeKeepingCentre (40, m.getHeight()));
}

} // namespace bizgroove
