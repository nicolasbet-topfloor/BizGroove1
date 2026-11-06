#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bizgroove
{

/** BizGroove colour palette. */
namespace Colours
{
    inline const juce::Colour background { 0xff15161a };
    inline const juce::Colour panel      { 0xff1f2127 };
    inline const juce::Colour panelEdge  { 0xff2c2f37 };
    inline const juce::Colour text       { 0xffe8e6e1 };
    inline const juce::Colour textDim    { 0xff8d8f97 };
    inline const juce::Colour accent     { 0xffff8a3d };   // orange
    inline const juce::Colour accentDim  { 0xff5a3420 };
    inline const juce::Colour playhead   { 0xff4fd1c5 };   // teal
}

class BizLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BizLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
};

} // namespace bizgroove
