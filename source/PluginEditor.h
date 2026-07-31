#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Rich Tactile LookAndFeel with Depth, Gloss, & Metallic Shading[cite: 6]
class FlatGodLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatGodLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFFFFFF)); 
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF1A1A20));
        
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF161618));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2A2A30));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFFFFFFF));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, 
                           float sliderPos, const float rotaryStartAngle, 
                           const float rotaryEndAngle, juce::Slider&) override
    {
        auto radius = (float) juce::jmin (width / 2, height / 2) - 6.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // 1. Deep 3D Drop Shadow for Knobs
        for (int i = 1; i <= 7; ++i)
        {
            g.setColour(juce::Colours::black.withAlpha(0.15f * (8 - i)));
            g.fillEllipse(centreX - radius - (i * 0.5f), centreY - radius + (i * 1.5f), radius * 2.0f + i, radius * 2.0f + i);
        }

        // 2. Premium Metallic Outer Bevel Ring
        juce::ColourGradient ringGrad(juce::Colour(0xFFE5E5E5), centreX, centreY - radius,
                                       juce::Colour(0xFF555555), centreX, centreY + radius, false);
        g.setGradientFill(ringGrad);
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);

        // 3. Inner Glossy Dark Cap
        auto capRadius = radius * 0.85f;
        juce::ColourGradient capGrad(juce::Colour(0xFF353540), centreX, centreY - capRadius,
                                      juce::Colour(0xFF0A0A0C), centreX, centreY + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2, capRadius * 2);

        // 4. Top Glass Reflection/Glare
        juce::Path glare;
        glare.addEllipse(centreX - capRadius * 0.65f, centreY - capRadius * 0.85f, capRadius * 1.3f, capRadius * 0.8f);
        juce::ColourGradient glareGrad(juce::Colours::white.withAlpha(0.18f), centreX, centreY - capRadius,
                                       juce::Colours::white.withAlpha(0.0f), centreX, centreY, false);
        g.setGradientFill(glareGrad);
        g.fillPath(glare);

        // Highlight ring on cap edge
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2, capRadius * 2, 1.0f);

        // 5. Track Arc (Floating above the panel)
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, radius + 3.5f, radius + 3.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xFF1A1A20).withAlpha(0.7f));
        g.strokePath(trackArc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 6. Active Fill Arc (Glowing Accent)
        juce::Path fillArc;
        fillArc.addCentredArc(centreX, centreY, radius + 3.5f, radius + 3.5f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.strokePath(fillArc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 7. Sharp Indicator Line
        juce::Path p;
        p.addRoundedRectangle(-2.0f, -capRadius + 4.0f, 4.0f, capRadius * 0.45f, 2.0f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        
        g.setColour(juce::Colours::white);
        g.fillPath(p);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        
        if (button.getName() == "SAVE" || button.getName() == "SETTINGS")[cite: 6]
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

        if (button.getToggleState())[cite: 6]
        {
            juce::ColourGradient btnGrad(juce::Colour(0xFF2A2A32).withAlpha(0.5f), 0, 0, juce::Colour(0xFF1A1A20).withAlpha(0.5f), 0, bounds.getHeight(), false);
            g.setGradientFill(btnGrad);
            g.fillRoundedRectangle(bounds, 4.0f);
            
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(0, 0, 4.0f, bounds.getHeight(), 2.0f);
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0xFF222228).withAlpha(0.3f));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF121215).withAlpha(0.4f));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        if (button.getName() == "SAVE" || button.getName() == "SETTINGS") return;[cite: 6]
        
        g.setFont(juce::FontOptions(11.0f).withName("Helvetica").withStyle(button.getToggleState() ? "Bold" : "Plain"));
        g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colours::white.withAlpha(0.6f));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        // Track line
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect((float)x, (float)y + (float)height * 0.5f - 1.0f, (float)width, 2.0f);[cite: 6]

        // Thumb Shadow & Glow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 4.0f, 12.0f, 12.0f);

        g.setColour(juce::Colours::white);
        g.fillEllipse(sliderPos - 5.0f, (float)y + (float)height * 0.5f - 5.0f, 10.0f, 10.0f);
        
        g.setColour(juce::Colours::white);
        g.drawEllipse(sliderPos - 5.0f, (float)y + (float)height * 0.5f - 5.0f, 10.0f, 10.0f, 1.2f);
    }
};

class HomeDistoAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       public juce::AudioProcessorValueTreeState::Listener
{
public:
    HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor&);[cite: 6]
    ~HomeDistoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    HomeDistoAudioProcessor& audioProcessor;
    FlatGodLookAndFeel flatLaf;

    // Header UI
    juce::ComboBox presetCombo;[cite: 6]
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

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;[cite: 6]
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> driveAttach, outAttach, toneAttach, punchAttach, mixAttach;[cite: 6]
    std::unique_ptr<SliderAttachment> lowAttach, highAttach;
    std::unique_ptr<ButtonAttachment> autoAttach;

    juce::String getFrequencyString(float hz);
    void drawShadedCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour);[cite: 6]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessorEditor)
};