#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Rich Tactile LookAndFeel with Depth & Shading
class FlatGodLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatGodLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFA855F7)); 
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF1A1A20));
        
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF161618));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2A2A30));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFA855F7));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, 
                           float sliderPos, const float rotaryStartAngle, 
                           const float rotaryEndAngle, juce::Slider&) override
    {
        auto radius = (float) juce::jmin (width / 2, height / 2) - 6.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // 1. Knob Drop Shadow
        for (int i = 1; i <= 4; ++i)
        {
            g.setColour(juce::Colours::black.withAlpha(0.12f * (5 - i)));
            g.fillEllipse(centreX - radius - i, centreY - radius + (i * 0.8f), radius * 2.0f + (i * 2), radius * 2.0f + (i * 2));
        }

        // 2. Outer Bevel Ring (3D Gradient)
        juce::ColourGradient ringGrad(juce::Colour(0xFF383842), centreX, centreY - radius,
                                       juce::Colour(0xFF121216), centreX, centreY + radius, false);
        g.setGradientFill(ringGrad);
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);

        // 3. Shaded Knob Cap
        auto capRadius = radius * 0.82f;
        juce::ColourGradient capGrad(juce::Colour(0xFF282830), centreX, centreY - capRadius,
                                      juce::Colour(0xFF0F0F12), centreX, centreY + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2, capRadius * 2);

        // Highlight ring on cap edge
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2, capRadius * 2, 1.0f);

        // 4. Track Arc
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(findColour(juce::Slider::trackColourId));
        g.strokePath(trackArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 5. Active Fill Arc (Glowing Accent)
        juce::Path fillArc;
        fillArc.addCentredArc(centreX, centreY, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(fillArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 6. Indicator Line with Glow
        juce::Path p;
        p.addRoundedRectangle(-1.5f, -capRadius + 4.0f, 3.0f, capRadius * 0.55f, 1.0f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        
        g.setColour(juce::Colour(0xFFA855F7));
        g.fillPath(p);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        
        if (button.getName() == "SAVE" || button.getName() == "SETTINGS")
        {
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xFF2A2A30) : juce::Colour(0xFF161618));
            g.fillRoundedRectangle(bounds, 4.0f);
            
            g.setColour(juce::Colours::white);
            if (button.getName() == "SAVE")
            {
                g.drawRoundedRectangle(bounds.reduced(6.0f), 2.0f, 1.5f);
                g.fillRect(bounds.getX() + 10, bounds.getY() + 6, bounds.getWidth() - 20, 6.0f);
                g.fillRect(bounds.getX() + 8, bounds.getBottom() - 12, bounds.getWidth() - 16, 6.0f);
            }
            else if (button.getName() == "SETTINGS")
            {
                auto cX = bounds.getCentreX();
                auto cY = bounds.getCentreY();
                for (int i = 0; i < 6; ++i)
                {
                    juce::Path spoke;
                    spoke.addRectangle(-1.5f, -9.0f, 3.0f, 18.0f);
                    spoke.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi * i / 3.0f).translated(cX, cY));
                    g.fillPath(spoke);
                }
                g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xFF2A2A30) : juce::Colour(0xFF161618));
                g.fillEllipse(cX - 5, cY - 5, 10, 10);
                g.setColour(juce::Colours::white);
                g.drawEllipse(cX - 5, cY - 5, 10, 10, 1.5f);
            }
            return;
        }

        if (button.getToggleState())
        {
            juce::ColourGradient btnGrad(juce::Colour(0xFF2A2A32), 0, 0, juce::Colour(0xFF1A1A20), 0, bounds.getHeight(), false);
            g.setGradientFill(btnGrad);
            g.fillRoundedRectangle(bounds, 4.0f);
            
            g.setColour(juce::Colour(0xFFA855F7));
            g.fillRoundedRectangle(0, 0, 4.0f, bounds.getHeight(), 2.0f);
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0xFF222228));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF121215).withAlpha(0.6f));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        if (button.getName() == "SAVE" || button.getName() == "SETTINGS") return; 
        
        g.setFont(juce::FontOptions(11.0f).withName("Helvetica").withStyle(button.getToggleState() ? "Bold" : "Plain"));
        g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(0xFFB0B0B8));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        // Track line
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect((float)x, (float)y + (float)height * 0.5f - 1.0f, (float)width, 2.0f);

        // Thumb Shadow & Glow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 4.0f, 12.0f, 12.0f);

        g.setColour(juce::Colour(0xFFA855F7));
        g.fillEllipse(sliderPos - 5.0f, (float)y + (float)height * 0.5f - 5.0f, 10.0f, 10.0f);
        
        g.setColour(juce::Colours::white);
        g.drawEllipse(sliderPos - 5.0f, (float)y + (float)height * 0.5f - 5.0f, 10.0f, 10.0f, 1.2f);
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

    // Header UI
    juce::ComboBox presetCombo;
    juce::TextButton saveButton;
    juce::TextButton settingsButton;

    // Main Knobs
    juce::Slider driveKnob;
    juce::Slider outputKnob; 
    juce::Slider toneKnob;
    juce::Slider punchKnob;
    juce::Slider mixKnob;
    juce::ToggleButton autoToggle;
    
    // Mode List 
    juce::TextButton modeButtons[6];
    juce::StringArray modeNames = { "PUNCH", "TUBE", "TAPE", "DIGITAL", "CRUNCH", "FUZZ" };
    
    // Filter Sliders
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> driveAttach, outAttach, toneAttach, punchAttach, mixAttach;
    std::unique_ptr<SliderAttachment> lowAttach, highAttach;
    std::unique_ptr<ButtonAttachment> autoAttach;

    juce::String getFrequencyString(float hz);
    void drawShadedCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessorEditor)
};