#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include <map>

namespace bizgroove
{

/** The BIZARINTERNATIONAL(TM) wordmark, drawn from its outline so it stays sharp at
    any size, in any colour (BizGroove's orange by default). */
namespace BizarLogo
{
    /** Width over height of the wordmark. */
    constexpr float aspect = 1630.0f / 220.0f;

    inline std::unique_ptr<juce::Drawable> make (juce::Colour colour = Colours::accent)
    {
        static const char* svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1630 220"><g transform="translate(0.000000,220.000000) scale(0.100000,-0.100000)"
fill="#FFFFFF" stroke="none">
<path d="M349 1543 c-1 -37 -1 -920 0 -955 l1 -28 255 0 255 0 0 70 0 70 29 0
c17 0 33 5 36 10 6 9 10 176 6 253 l-1 27 -35 0 -35 0 0 70 0 70 29 0 c17 0
33 5 37 11 9 15 10 261 1 277 -5 6 -21 12 -38 12 l-29 0 0 70 0 70 -255 0
-255 0 -1 -27z m431 -263 l0 -140 -140 0 -140 0 0 140 0 140 140 0 140 0 0
-140z m0 -430 l0 -140 -140 0 -140 0 0 140 0 140 140 0 140 0 0 -140z M1287
1563 c-9 -8 -9 -106 -1 -127 4 -12 20 -16 55 -16 l49 0 0 -355 0 -355 -50 0
-50 0 -5 -23 c-4 -13 -5 -46 -3 -73 l3 -49 181 -3 182 -2 4 36 c2 21 2 53 -1
73 l-6 36 -50 3 -50 3 0 354 0 354 50 3 50 3 5 35 c3 19 3 52 1 73 l-3 37
-177 0 c-98 0 -181 -3 -184 -7z M2042 1498 l3 -73 178 -3 177 -2 0 -70 0 -70
-25 0 c-30 0 -40 -30 -32 -96 l5 -44 -53 0 -53 0 -7 -39 c-3 -21 -3 -55 0 -75
l7 -36 -56 0 -56 0 0 -70 0 -70 -45 0 -45 0 0 -145 0 -145 255 0 256 0 -3 73
-3 72 -177 3 -178 2 0 70 0 70 45 0 45 0 0 69 0 70 53 3 52 3 3 67 3 67 52 3
52 3 0 73 0 72 28 0 27 0 0 145 0 145 -255 0 -256 0 3 -72z M3017 1564 c-4 -4
-7 -36 -7 -71 0 -48 -3 -63 -14 -63 -18 0 -31 -74 -21 -118 l6 -31 -28 -3 -28
-3 -3 -67 c-2 -49 -7 -68 -16 -68 -19 0 -24 -45 -22 -177 l2 -112 -26 -3 -25
-3 0 -140 0 -140 72 -3 72 -3 6 23 c3 13 4 78 2 146 l-2 122 140 0 140 0 -2
-122 c-2 -68 -1 -133 2 -146 l6 -23 72 3 72 3 0 140 0 140 -25 3 -26 3 2 112
c2 132 -3 177 -22 177 -9 0 -14 19 -16 68 l-3 67 -28 3 -28 3 6 31 c10 43 -3
118 -20 118 -10 0 -15 18 -17 68 l-3 67 -106 3 c-58 1 -108 -1 -112 -4z m156
-323 c-7 -54 4 -111 22 -111 12 0 15 -15 15 -70 l0 -70 -85 0 -85 0 0 70 c0
55 3 70 15 70 18 0 29 57 22 111 l-5 39 53 0 53 0 -5 -39z M3660 1065 l0 -505
74 0 74 0 5 33 c2 17 4 114 2 215 l-2 182 86 0 86 0 0 -67 0 -68 27 -3 27 -3
-1 -63 c-1 -71 5 -86 33 -86 l19 0 0 -70 0 -70 74 0 c82 0 86 3 86 68 0 61
-10 82 -36 82 l-24 0 0 70 0 70 -27 0 -28 0 3 66 4 65 -21 6 -21 5 0 69 0 68
53 3 52 3 3 73 c2 52 7 72 16 72 18 0 26 22 26 68 0 68 -9 82 -47 82 l-35 0 6
31 c3 17 3 49 0 70 l-7 39 -253 0 -254 0 0 -505z m395 215 l0 -140 -121 0
-121 0 1 140 1 140 120 0 120 0 0 -140z M4596 1548 c-7 -29 -7 -77 0 -105 l6
-23 52 0 51 0 0 -355 0 -355 -51 0 -52 0 -6 -22 c-8 -30 -8 -92 0 -112 l6 -16
178 0 178 0 6 16 c8 20 8 82 0 112 l-6 22 -52 0 -51 0 0 355 0 355 51 0 52 0
6 23 c7 28 7 76 0 105 l-6 22 -178 0 -178 0 -6 -22z M5317 1553 c-3 -10 -5
-229 -5 -488 0 -259 2 -478 5 -487 l4 -18 75 0 74 0 -1 218 c0 119 0 247 0
285 l1 67 30 0 29 0 3 -67 3 -68 35 -5 35 -5 0 -65 0 -65 29 -3 28 -4 -6 -40
c-4 -22 -4 -55 0 -74 l6 -34 45 0 45 0 -7 -30 c-4 -17 -4 -48 0 -70 l7 -40 74
0 74 0 0 505 0 505 -74 0 -73 0 -5 -37 c-3 -21 -5 -152 -4 -290 l2 -253 -33 0
-33 0 6 40 c4 21 3 55 0 75 l-7 35 -35 0 -36 0 3 38 c1 20 1 52 0 70 l-1 32
-25 0 -24 0 -3 73 -3 72 -28 3 -28 3 7 35 c3 19 3 50 0 69 l-7 35 -89 0 -89 0
-4 -17z M6144 1557 c-3 -8 -4 -41 -2 -73 l3 -59 108 -3 107 -3 0 -429 0 -430
75 0 75 0 0 430 0 429 108 3 107 3 0 70 0 70 -288 3 -288 2 -5 -13z M7046
1553 c-6 -23 -6 -953 0 -975 l5 -18 214 0 213 0 6 16 c3 9 6 35 6 59 0 24 -3
50 -6 59 l-6 16 -139 0 -139 0 -1 68 c0 37 -1 100 -2 139 l-2 73 141 0 142 0
6 23 c8 29 8 91 0 111 l-6 16 -138 0 -138 0 -4 70 c-2 39 -2 102 0 140 l4 70
138 0 138 0 6 23 c8 29 8 91 0 111 l-6 16 -213 0 -214 0 -5 -17z M7800 1065
l0 -505 74 0 74 0 5 33 c2 17 4 114 2 215 l-2 182 86 0 86 0 0 -67 0 -68 27
-3 27 -3 -1 -63 c-1 -71 5 -86 33 -86 l19 0 0 -70 0 -70 74 0 c82 0 86 3 86
68 0 61 -10 82 -36 82 l-24 0 0 70 0 70 -27 0 -28 0 3 66 4 65 -21 6 -21 5 0
69 0 68 53 3 52 3 3 73 c2 52 7 72 16 72 18 0 26 22 26 68 0 68 -9 82 -47 82
l-35 0 6 31 c3 17 3 49 0 70 l-7 39 -253 0 -254 0 0 -505z m395 215 l0 -140
-121 0 -121 0 1 140 1 140 120 0 120 0 0 -140z M8629 1543 c-1 -37 -1 -920 0
-955 l1 -28 75 0 75 0 0 285 0 285 33 0 32 0 -3 -45 c-2 -25 -1 -57 2 -70 l6
-25 34 0 34 0 -5 -44 c-8 -66 2 -96 32 -96 l24 0 3 -72 3 -73 43 -3 42 -3 0
-70 0 -69 75 0 75 0 1 28 c1 34 1 917 0 955 l-1 27 -75 0 -75 0 0 -291 0 -290
-32 3 -32 3 3 52 c4 77 -4 93 -44 93 l-34 0 6 34 c3 19 2 51 -1 70 l-7 36 -24
0 -24 0 -3 73 -3 72 -25 3 -25 3 3 45 c2 24 1 56 -2 69 l-6 25 -90 0 -90 0 -1
-27z M9634 1544 c-4 -14 -4 -45 -2 -69 l5 -43 -18 -5 -19 -5 0 -71 0 -71 -25
0 c-30 0 -40 -30 -33 -95 l5 -43 -18 -5 -19 -5 0 -141 0 -141 -24 0 c-32 0
-35 -12 -35 -145 0 -60 3 -118 5 -127 l5 -18 75 0 75 0 -1 73 c-1 39 -1 105
-1 145 l1 72 139 0 139 0 -3 -112 c-2 -62 -1 -128 2 -145 l5 -33 74 0 74 0 0
145 0 145 -27 0 -28 0 2 122 c3 138 -1 168 -22 168 -12 0 -15 15 -15 70 l0 70
-29 0 -28 0 5 49 c6 61 -4 101 -23 101 -12 0 -15 15 -15 70 l0 70 -110 0 -110
0 -6 -26z m162 -298 c-10 -51 3 -116 23 -116 l16 0 -3 -70 -4 -70 -83 0 -84 0
1 69 0 69 19 5 19 5 0 71 0 71 51 0 51 0 -6 -34z M10284 1557 c-3 -8 -4 -41
-2 -73 l3 -59 108 -3 107 -3 0 -429 0 -430 75 0 75 0 0 430 0 429 108 3 107 3
0 70 0 70 -288 3 -288 2 -5 -13z M11224 1557 c-3 -8 -4 -41 -2 -73 l3 -59 53
-3 52 -3 0 -354 0 -354 -52 -3 -53 -3 -3 -72 -3 -73 186 0 186 0 -3 73 -3 72
-52 3 -53 3 0 354 0 354 53 3 52 3 0 70 0 70 -178 3 -177 2 -6 -13z M12084
1545 c-3 -13 -4 -45 -2 -70 l3 -45 -42 0 -43 0 -6 -25 c-3 -13 -4 -47 -2 -75
l3 -50 -27 0 -28 0 0 -215 0 -215 29 0 28 0 -5 -49 c-2 -28 -2 -61 2 -75 l6
-26 43 0 42 0 0 -67 0 -68 148 -3 147 -3 0 70 0 70 43 3 42 3 3 73 3 72 24 0
c14 0 26 7 28 18 3 9 5 98 5 197 0 99 -2 188 -5 198 -2 10 -14 17 -28 17 l-24
0 -3 73 -3 72 -42 3 -43 3 0 70 0 69 -145 0 -145 0 -6 -25z m235 -162 c-1 -21
-2 -53 -1 -70 l2 -33 25 0 25 0 1 -70 c0 -38 0 -104 0 -145 0 -41 0 -106 0
-145 l-1 -70 -25 0 -25 0 -1 -37 c-1 -21 -1 -53 0 -70 l3 -33 -89 0 -88 0 0
67 0 68 -25 3 -26 3 0 214 0 214 25 3 26 3 0 68 0 67 88 0 88 0 -2 -37z
M12769 1543 c-1 -37 -1 -920 0 -955 l1 -28 75 0 75 0 0 285 0 285 33 0 32 0
-3 -45 c-2 -25 -1 -57 2 -70 l6 -25 34 0 34 0 -5 -44 c-8 -66 2 -96 32 -96
l24 0 3 -72 3 -73 43 -3 42 -3 0 -70 0 -69 75 0 75 0 1 28 c1 34 1 917 0 955
l-1 27 -75 0 -75 0 0 -291 0 -290 -32 3 -32 3 3 52 c4 77 -4 93 -44 93 l-34 0
6 34 c3 19 2 51 -1 70 l-7 36 -24 0 -24 0 -3 73 -3 72 -25 3 -25 3 3 45 c2 24
1 56 -2 69 l-6 25 -90 0 -90 0 -1 -27z M13774 1544 c-4 -14 -4 -45 -2 -69 l5
-43 -18 -5 -19 -5 0 -71 0 -71 -25 0 c-30 0 -40 -30 -33 -95 l5 -43 -18 -5
-19 -5 0 -141 0 -141 -24 0 c-32 0 -35 -12 -35 -145 0 -60 3 -118 5 -127 l5
-18 75 0 75 0 -1 73 c-1 39 -1 105 -1 145 l1 72 139 0 139 0 -3 -112 c-2 -62
-1 -128 2 -145 l5 -33 74 0 74 0 0 145 0 145 -27 0 -28 0 2 122 c3 138 -1 168
-22 168 -12 0 -15 15 -15 70 l0 70 -29 0 -28 0 5 49 c6 61 -4 101 -23 101 -12
0 -15 15 -15 70 l0 70 -110 0 -110 0 -6 -26z m162 -298 c-10 -51 3 -116 23
-116 l16 0 -3 -70 -4 -70 -83 0 -84 0 1 69 0 69 19 5 19 5 0 71 0 71 51 0 51
0 -6 -34z M14497 1553 c-8 -28 -5 -970 2 -982 5 -8 81 -11 257 -9 l249 3 0 70
0 70 -177 3 -178 2 -1 68 c0 37 -1 229 -2 427 l-2 360 -72 3 -71 3 -5 -18z
M15254 1557 c-3 -8 -4 -41 -2 -73 l3 -59 30 -3 30 -3 -2 -187 c-1 -103 1 -199
3 -214 l6 -28 43 0 43 0 6 28 c2 15 4 111 3 214 l-2 187 30 3 30 3 0 70 0 70
-108 3 c-85 2 -109 0 -113 -11z M15507 1553 c-7 -23 -7 -522 -1 -545 4 -14 16
-18 49 -18 l43 0 5 33 c8 55 3 247 -8 255 -5 4 -10 37 -10 75 l0 67 23 0 22 0
0 -70 0 -70 40 0 40 0 0 70 0 70 23 0 22 0 0 -70 c0 -38 -3 -70 -7 -70 -5 0
-8 -65 -8 -146 l0 -145 48 3 47 3 0 285 0 285 -65 0 -65 0 -3 -67 -3 -68 -29
0 -29 0 -3 68 -3 67 -62 3 -61 3 -5 -18z"/>
</g></svg>)SVG";
        auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (svg));
        if (xml == nullptr)
            return {};
        auto d = juce::Drawable::createFromSVG (*xml);
        if (d != nullptr)
            d->replaceColour (juce::Colours::white, colour);
        return d;
    }

    /** Draws the wordmark as large as fits in `area`, placed as `placement` says. */
    inline void draw (juce::Graphics& g, juce::Rectangle<float> area, juce::RectanglePlacement placement,
                      juce::Colour colour = Colours::accent, float opacity = 1.0f)
    {
        static std::map<juce::uint32, std::unique_ptr<juce::Drawable>> cache;
        auto& d = cache[colour.getARGB()];
        if (d == nullptr)
            d = make (colour);
        if (d != nullptr)
            d->drawWithin (g, area, placement, opacity);
    }
}

/** The About window: what BizGroove is, who makes it, its version and licence. */
class AboutComponent final : public juce::Component
{
public:
    AboutComponent() { setSize (560, 352); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
        auto r = getLocalBounds().reduced (36, 30);
        g.setColour (Colours::accent);
        g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
        g.drawText ("BizGroove", r.removeFromTop (60), juce::Justification::centredLeft);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Version " + juce::String (BIZGROOVE_VERSION), r.removeFromTop (24), juce::Justification::centredLeft);
        r.removeFromTop (26);
        BizarLogo::draw (g, r.removeFromTop (44).toFloat(), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
        r.removeFromTop (14);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText ("BizGroove is a creation of Bizarinternational", r.removeFromTop (24), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (13.0f));
        g.setColour (Colours::textDim);
        g.drawText ("bizarinternational.com", r.removeFromTop (22), juce::Justification::centredLeft);
        g.drawText ("Open-source software under the GNU General Public License v3. Made with JUCE.", r.removeFromTop (22), juce::Justification::centredLeft);
        g.drawText ("The Macro Synth's sound engines are by Emilie Gillet, under the MIT licence.", r.removeFromTop (22), juce::Justification::centredLeft);
    }
};

/** Shown over the window for a moment when BizGroove starts: the name and the
    Bizarinternational wordmark. It fades out by itself; a click closes it at once. */
class StartScreen final : public juce::Component, private juce::Timer
{
public:
    StartScreen()
    {
        setInterceptsMouseClicks (true, false);
        startTimerHz (30);
    }

    std::function<void()> onFinished;

    void paint (juce::Graphics& g) override
    {
        const float a = alpha();
        g.fillAll (juce::Colours::black.withAlpha (a));
        auto r = getLocalBounds().toFloat();
        const float w = juce::jmin (r.getWidth() * 0.62f, 760.0f);
        auto block = juce::Rectangle<float> (w, 280.0f).withCentre (r.getCentre());
        g.setColour (Colours::accent.withAlpha (a));
        g.setFont (juce::FontOptions (84.0f, juce::Font::bold));
        g.drawText ("BizGroove", block.removeFromTop (110.0f), juce::Justification::centred);
        block.removeFromTop (24.0f);
        BizarLogo::draw (g, block.removeFromTop (w / BizarLogo::aspect * 0.8f), juce::RectanglePlacement::centred, Colours::accent, a);
        block.removeFromTop (16.0f);
        g.setColour (Colours::textDim.withAlpha (a));
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("BizGroove is a creation of Bizarinternational", block.removeFromTop (24.0f), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { finish(); }

private:
    float alpha() const
    {
        const auto t = (double) (juce::Time::getMillisecondCounter() - start) / 1000.0;
        return t < holdSeconds ? 1.0f : (float) juce::jlimit (0.0, 1.0, 1.0 - (t - holdSeconds) / fadeSeconds);
    }

    void timerCallback() override
    {
        if (alpha() <= 0.0f)
            finish();
        else
            repaint();
    }

    void finish()
    {
        stopTimer();
        if (auto f = onFinished)
            juce::MessageManager::callAsync (f);   // the owner removes and deletes this
    }

    static constexpr double holdSeconds = 1.8, fadeSeconds = 0.6;
    juce::uint32 start = juce::Time::getMillisecondCounter();
};

} // namespace bizgroove
