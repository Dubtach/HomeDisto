#pragma once

#include "PluginProcessor.h"

// Pure 2D Flat styling for the plugin
class FlatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId, juce::Colour(0xffff5500));
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff5500));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff222222));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour(0xff222222));
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, 
                           float sliderPos, const float rotaryStartAngle, 
                           const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Solid flat track
        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.fillEllipse (rx, ry, rw, rw);

        // Solid flat active arc
        juce::Path filledArc;
        filledArc.addPieSegment (rx, ry, rw, rw, rotaryStartAngle, angle, 0.0);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.fillPath (filledArc);
    }
};

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processorRef;
    FlatLookAndFeel flatLookAndFeel;

    juce::Slider driveSlider;
    juce::Slider outputSlider;
    juce::ComboBox modeBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};