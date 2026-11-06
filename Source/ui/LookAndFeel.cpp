#include "LookAndFeel.h"

namespace bizgroove
{

BizLookAndFeel::BizLookAndFeel()
{
    setColourScheme ({ Colours::background, Colours::panel, Colours::panel, Colours::panelEdge,
                       Colours::text, Colours::accent, juce::Colours::black, Colours::accent, Colours::text });

    setColour (juce::ResizableWindow::backgroundColourId, Colours::background);
    setColour (juce::Slider::rotarySliderFillColourId, Colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, Colours::panelEdge);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, Colours::accent);
    setColour (juce::Slider::backgroundColourId, Colours::panelEdge);
    setColour (juce::Slider::thumbColourId, Colours::text);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, Colours::panel);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void BizLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float lineW = 3.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - lineW, radius - lineW, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::panelEdge);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - lineW, radius - lineW, 0.0f, startAngle, angle, true);
    g.setColour (Colours::accent);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - lineW * 3.0f;
    g.setColour (Colours::panel.brighter (0.15f));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

    const juce::Point<float> tip (centre.x + (knobR - 3.0f) * std::sin (angle),
                                  centre.y - (knobR - 3.0f) * std::cos (angle));
    g.setColour (Colours::text);
    g.drawLine ({ centre, tip }, 2.0f);
}

void BizLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                           bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour;
    if (isDown)             colour = colour.brighter (0.2f);
    else if (isHighlighted) colour = colour.brighter (0.08f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}

} // namespace bizgroove
