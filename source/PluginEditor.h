#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Improved Flat 2D LookAndFeel
class FlatGodLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatGodLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFA855F7)); 
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF2A2A30));
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

        // 1. Base ring (Dark outline for 2D separation)
        g.setColour(juce::Colour(0xFF101012));
        g.fillEllipse(rx - 2, ry - 2, rw + 4, rw + 4);

        // 2. Main Body
        g.setColour(juce::Colour(0xFF161618));
        g.fillEllipse(rx, ry, rw, rw);

        // 3. Inner Cap (gives the knob flat depth)
        auto capRadius = radius * 0.85f;
        g.setColour(juce::Colour(0xFF1E1E22));
        g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2, capRadius * 2);
        
        g.setColour(juce::Colour(0xFF2A2A30));
        g.drawEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2, capRadius * 2, 1.0f);

        // 4. Solid Track Arc
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, radius + 2.0f, radius + 2.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(findColour(juce::Slider::trackColourId));
        g.strokePath(trackArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 5. Fill Arc (Purple)
        juce::Path fillArc;
        fillArc.addCentredArc(centreX, centreY, radius + 2.0f, radius + 2.0f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(fillArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 6. Indicator Line
        juce::Path p;
        auto pointerLength = capRadius * 0.6f;
        p.addRoundedRectangle(-1.5f, -capRadius + 4.0f, 3.0f, pointerLength, 1.5f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colour(0xFFA855F7));
        g.fillPath(p);
    }

    // Flat Styling for Mode List Buttons
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        if (button.getToggleState())
        {
            g.setColour(juce::Colour(0xFF1E1E22));
            g.fillRoundedRectangle(bounds, 4.0f);
            
            // Left purple accent
            g.setColour(juce::Colour(0xFFA855F7));
            g.fillRoundedRectangle(0, 0, 4.0f, bounds.getHeight(), 2.0f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0xFF161618));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        g.setFont(juce::FontOptions(13.0f).withStyle(button.getToggleState() ? "Bold" : "Plain"));
        g.setColour(button.getToggleState() ? juce::Colour(0xFFA855F7) : juce::Colour(0xFF888888));
        g.drawText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(10), juce::Justification::centredLeft);
    }

    // Invisible track for filter sliders, only draw the draggable node
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        g.setColour(juce::Colour(0xFFA855F7));
        g.fillEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 6.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white);
        g.drawEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 6.0f, 12.0f, 12.0f, 1.5f);
    }
};

class HomeDistoAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       public juce::AudioProcessorValueTreeState::Listener
{
public:
    HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor&);
    ~HomeDistoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    HomeDistoAudioProcessor& audioProcessor;
    FlatGodLookAndFeel flatLaf;

    // Main Knobs
    juce::Slider driveKnob;
    juce::Slider outputKnob; 
    juce::Slider toneKnob;
    juce::Slider punchKnob;
    juce::Slider mixKnob;

    juce::ToggleButton autoToggle;
    
    // Mode List
    juce::TextButton modeButtons[4];
    juce::StringArray modeNames = { "PUNCH", "TUBE", "TAPE", "DIGITAL" };
    
    // Interactive Filter Sliders
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> driveAttach, outAttach, toneAttach, punchAttach, mixAttach;
    std::unique_ptr<SliderAttachment> lowAttach, highAttach;
    std::unique_ptr<ButtonAttachment> autoAttach;

    juce::String getFrequencyString(float hz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessorEditor)
};