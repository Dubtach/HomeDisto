#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Custom LookAndFeel for strict 2D visual aesthetic 
class FlatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF6B35));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF6B35));
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF383840));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF18181C));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF383840));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, 
                           float sliderPos, const float rotaryStartAngle, 
                           const float rotaryEndAngle, juce::Slider&) override
    {
        auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Flat 2D rendering without gradients or drop shadows
        g.setColour(juce::Colour(0xFF18181C));
        g.fillEllipse(rx, ry, rw, rw);
        g.setColour(juce::Colour(0xFF383840));
        g.drawEllipse(rx, ry, rw, rw, 2.0f);

        juce::Path p;
        auto pointerLength = radius * 0.8f;
        auto pointerThickness = 3.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        
        g.setColour(juce::Colour(0xFFFF6B35));
        g.fillPath(p);
    }
};

class HomeDistoAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor&);
    ~HomeDistoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    HomeDistoAudioProcessor& audioProcessor;
    FlatLookAndFeel flatLookAndFeel;

    // UI Elements
    juce::Slider driveKnob;
    juce::ComboBox algoSelector;
    juce::Slider lowCutKnob;
    juce::Slider highCutKnob;
    
    juce::Slider toneKnob;
    juce::Slider punchKnob;
    juce::Slider textureKnob;
    
    juce::Slider mixKnob;
    juce::Slider outputKnob;
    juce::ToggleButton autoToggle;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> driveAttach, lowAttach, highAttach;
    std::unique_ptr<SliderAttachment> toneAttach, punchAttach, textureAttach;
    std::unique_ptr<SliderAttachment> mixAttach, outAttach;
    std::unique_ptr<ComboBoxAttachment> algoAttach;
    std::unique_ptr<ButtonAttachment> autoAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessorEditor)
};