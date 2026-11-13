#pragma once

#include "LaunchkeyPanel.h"
#include "LookAndFeel.h"

namespace bizgroove
{

/** The Voice's text: what it says, in which language, and what each note says
    (the whole text or the next word). Opened from the panel's Text button. */
class VoicePanel final : public juce::Component, private juce::Timer
{
public:
    VoicePanel (AudioEngine& e) : engine (e), track (e.selectedTrack.load())
    {
        auto& vp = params();

        title.setText ("What the voice says", juce::dontSendNotification);
        title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, Colours::text);
        addAndMakeVisible (title);

        editor.setMultiLine (true, true);
        editor.setReturnKeyStartsNewLine (false);
        editor.setFont (juce::FontOptions (18.0f));
        editor.setText (vp.getText(), juce::dontSendNotification);
        editor.setColour (juce::TextEditor::backgroundColourId, Colours::background);
        editor.setColour (juce::TextEditor::textColourId, Colours::text);
        editor.setColour (juce::TextEditor::outlineColourId, Colours::panelEdge);
        editor.setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent);
        editor.setTooltip ("Type the words. Commas and full stops make pauses; a question mark makes the voice rise at the end. Numbers are read digit by digit.");
        editor.onTextChange = [this] { params().setText (editor.getText()); refresh(); };
        editor.onReturnKey = [this] { if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss(); };
        editor.onEscapeKey = editor.onReturnKey;
        addAndMakeVisible (editor);

        for (int l = 0; l < speech::numLanguages; ++l)
        {
            auto& b = languageButtons[(size_t) l];
            b.setButtonText (speech::languageName (l));
            b.setWantsKeyboardFocus (false);
            b.setTooltip (l == speech::english ? "Read the text as English." : "Read the text as Spanish.");
            b.onClick = [this, l] { params().language.store (l); refresh(); };
            addAndMakeVisible (b);
        }

        static const char* modeHelp[VoiceParams::numModes] = {
            "Each note says the whole text, to the end.",
            "The whole text, over and over, while you hold the key.",
            "Each note says the next word: sequence it and the words come in order.",
            "Each note sings the next word: its last vowel lasts as long as the note.",
            "Each note says the next syllable (\"ciu-dad\", \"to-ma-te\"): for chopped, stuttering raps.",
            "Each note says the next single sound of the text, a vowel or a consonant; vowels and hisses last while you hold the key.",
            "No text: each key is a sound, as on the Elektron voice machines. Vowels from C2 (a, e, i, o, u...), then m, n, l, r, w, y, s, sh, z, f, v, th, j, h, p, b, t, d, k, g. The Keys note knob sets the pitch."
        };
        for (int m = 0; m < VoiceParams::numModes; ++m)
        {
            auto& b = modeButtons[(size_t) m];
            b.setButtonText (VoiceParams::modeName (m));
            b.setWantsKeyboardFocus (false);
            b.setTooltip (modeHelp[m]);
            b.onClick = [this, m] { params().mode.store (m); refresh(); };
            addAndMakeVisible (b);
        }

        firstWordButton.setWantsKeyboardFocus (false);
        firstWordButton.setTooltip ("Word by word: the next note says the first word again. (Play also starts from the first word.)");
        firstWordButton.onClick = [this] { params().nextWord.store (0); };
        addAndMakeVisible (firstWordButton);

        info.setColour (juce::Label::textColourId, Colours::textDim);
        addAndMakeVisible (info);

        setSize (720, 284);
        refresh();
        startTimerHz (10);
    }

    void visibilityChanged() override
    {
        if (isShowing())
            editor.grabKeyboardFocus();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        title.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        editor.setBounds (r.removeFromTop (78));
        r.removeFromTop (10);

        auto langRow = r.removeFromTop (30);
        for (auto& b : languageButtons)
        {
            b.setBounds (langRow.removeFromLeft (110));
            langRow.removeFromLeft (8);
        }
        firstWordButton.setBounds (langRow.removeFromRight (150));
        r.removeFromTop (8);

        // The modes in two rows: four that read the text, then the pieces and the keys.
        for (int row = 0; row < 2; ++row)
        {
            auto modeRow = r.removeFromTop (30);
            const int first = row == 0 ? 0 : 4, count = row == 0 ? 4 : VoiceParams::numModes - 4;
            const int w = (modeRow.getWidth() - 3 * 8) / 4;
            for (int m = first; m < first + count; ++m)
            {
                modeButtons[(size_t) m].setBounds (modeRow.removeFromLeft (w));
                modeRow.removeFromLeft (8);
            }
            r.removeFromTop (6);
        }
        r.removeFromTop (6);
        info.setBounds (r.removeFromTop (22));
    }

private:
    VoiceParams& params() { return engine.track (track).voiceParams; }

    void light (juce::TextButton& b, bool on)
    {
        b.setColour (juce::TextButton::buttonColourId, on ? Colours::accent : Colours::panelEdge);
        b.setColour (juce::TextButton::textColourOffId, on ? juce::Colours::black : Colours::text);
    }

    void refresh()
    {
        auto& vp = params();
        shownLanguage = vp.language.load();
        shownMode = vp.mode.load();
        for (int l = 0; l < speech::numLanguages; ++l)
            light (languageButtons[(size_t) l], l == shownLanguage);
        for (int m = 0; m < VoiceParams::numModes; ++m)
            light (modeButtons[(size_t) m], m == shownMode);
        const bool byWord = VoiceParams::byPiece (shownMode);
        const bool keys = shownMode == VoiceParams::soundsOnKeys;
        firstWordButton.setEnabled (byWord);
        editor.setEnabled (! keys);
        editor.setAlpha (keys ? 0.4f : 1.0f);
        const juce::String unit = shownMode == VoiceParams::syllableBySyllable ? "syllable" : shownMode == VoiceParams::soundBySound ? "sound" : "word";
        const int pieces = vp.pieceCount (shownMode);
        if (keys)
            info.setText (juce::String::fromUTF8 ("Keys from C2: a e i o u, then the consonants  \xc2\xb7  the text is not used in this mode"), juce::dontSendNotification);
        else
            info.setText (juce::String (pieces) + " " + unit + (pieces == 1 ? "" : "s")
                              + (byWord && pieces > 0 ? juce::String::fromUTF8 ("  \xc2\xb7  next note says ") + unit + " " + juce::String (vp.nextWord.load() % pieces + 1) : juce::String())
                              + juce::String::fromUTF8 ("  \xc2\xb7  play notes on the keys or pads to hear it"),
                          juce::dontSendNotification);
    }

    void timerCallback() override
    {
        if (engine.selectedTrack.load() != track || engine.track (track).type.load() != Track::voice)
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
            return;
        }
        auto& vp = params();
        if (vp.language.load() != shownLanguage || vp.mode.load() != shownMode || vp.nextWord.load() != shownNext)
        {
            shownNext = vp.nextWord.load();
            refresh();
        }
    }

    AudioEngine& engine;
    const int track;
    juce::Label title, info;
    juce::TextEditor editor;
    std::array<juce::TextButton, speech::numLanguages> languageButtons;
    std::array<juce::TextButton, VoiceParams::numModes> modeButtons;
    juce::TextButton firstWordButton { "Back to the start" };
    int shownLanguage = -1, shownMode = -1, shownNext = -1;
};

} // namespace bizgroove
