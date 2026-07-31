#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Flat 2D LookAndFeel matching the Mockup Colors
class FlatGodLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatGodLookAndFeel()
    {
        // Purple Accent: #A855F7, Dark Bg: #18181A
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFA855F7)); 
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF2A2A30));
        setColour(juce::Slider::thumbColourId, juce::Colours::transparentWhite);
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

        // Solid flat base circle
        g.setColour(juce::Colour(0xFF141416));
        g.fillEllipse(rx, ry, rw, rw);

        // Solid Track Arc
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(findColour(juce::Slider::trackColourId));
        g.strokePath(trackArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Solid Fill Arc (Purple LED Ring)
        juce::Path fillArc;
        fillArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(fillArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Indicator Line
        juce::Path p;
        auto pointerLength = radius * 0.6f;
        auto pointerThickness = 2.0f;
        p.addRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colours::white);
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
    FlatGodLookAndFeel flatLaf;

    // UI Elements
    juce::Slider driveKnob;
    juce::Slider topOutKnob; // Labeled "MIX" in mockup, controls output trim conceptually 
    juce::Slider toneKnob;
    juce::Slider punchKnob;
    juce::Slider mixKnob;

    juce::ToggleButton autoToggle;
    juce::ComboBox modeCombo;
    
    // Filter controls (invisible sliders mapped to the graph area conceptually)
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> driveAttach, outAttach, toneAttach, punchAttach, mixAttach;
    std::unique_ptr<SliderAttachment> lowAttach, highAttach;
    std::unique_ptr<ButtonAttachment> autoAttach;
    std::unique_ptr<ComboBoxAttachment> modeAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessorEditor)
};