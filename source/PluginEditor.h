#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Minimalist, Modern UI with sleek LED-style rings and Neon Glows
class MinimalistSynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MinimalistSynthLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white); 
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF000000).withAlpha(0.5f));
        
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF161618));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2A2A30));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFFFFFFF));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, 
                           float sliderPos, const float rotaryStartAngle, 
                           const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Fetch custom color for the neon glow
        auto neonColour = slider.findColour(juce::Slider::rotarySliderFillColourId);

        // 1. Base / Inner Shadow 
        g.setColour(juce::Colour(0xFF0A0A0C));
        g.fillEllipse(centreX - radius + 2.0f, centreY - radius + 2.0f, (radius - 2.0f) * 2.0f, (radius - 2.0f) * 2.0f);

        // 2. Background Track
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.4f)); 
        g.strokePath(bgArc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Active LED Track
        juce::Path fillArc;
        fillArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        
        // Outer Neon Halo
        g.setColour(neonColour.withAlpha(0.6f));
        g.strokePath(fillArc, juce::PathStrokeType(14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Solid White Core for 100% Legibility
        g.setColour(juce::Colours::white);
        g.strokePath(fillArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Minimalist Center Cap
        g.setColour(juce::Colours::white);
        g.fillEllipse(centreX - 3.5f, centreY - 3.5f, 7.0f, 7.0f);

        // 5. Minimalist Pointer Line
        juce::Path pointer;
        pointer.startNewSubPath(centreX, centreY);
        pointer.lineTo(centreX + (radius - 7.0f) * std::sin(angle), centreY - (radius - 7.0f) * std::cos(angle));
        
        // Pointer Glow & Core
        g.setColour(neonColour.withAlpha(0.5f));
        g.strokePath(pointer, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white);
        g.strokePath(pointer, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // 6. Subtle inner boundary ring
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawEllipse(centreX - (radius - 7.0f), centreY - (radius - 7.0f), (radius - 7.0f) * 2.0f, (radius - 7.0f) * 2.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, 
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto fontSize = 14.0f;
        auto tickWidth = 16.0f;
        
        juce::Rectangle<float> tickBounds (0.0f, ((float) button.getHeight() - tickWidth) * 0.5f, tickWidth, tickWidth);
        
        // Thick Checkbox Drop Shadow
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawRoundedRectangle(tickBounds.translated(0.0f, 1.5f), 3.0f, 2.5f);
        
        // Thick Checkbox Solid Outline
        g.setColour(juce::Colour(0xFF09090B)); 
        g.drawRoundedRectangle(tickBounds, 3.0f, 2.5f);
        
        if (button.getToggleState())
        {
            auto tickColour = button.findColour(juce::ToggleButton::tickColourId);
            juce::Path tickPath;
            tickPath.startNewSubPath(tickBounds.getX() + 3.0f, tickBounds.getCentreY());
            tickPath.lineTo(tickBounds.getCentreX() - 1.0f, tickBounds.getBottom() - 4.0f);
            tickPath.lineTo(tickBounds.getRight() - 2.0f, tickBounds.getY() + 2.0f);
            
            // Neon Glow for the Tick
            g.setColour(tickColour.withAlpha(0.6f));
            g.strokePath(tickPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Crisp White Core for the Tick
            g.setColour(juce::Colours::white);
            g.strokePath(tickPath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        
        // Matching Dark Text with Drop Shadow (Just like the cards)
        g.setFont(juce::FontOptions(fontSize).withName("Helvetica").withStyle("Bold"));
        auto textBounds = button.getLocalBounds().toFloat().withTrimmedLeft(tickWidth + 6.0f);
        
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawText(button.getButtonText(), textBounds.translated(0.0f, 1.5f), juce::Justification::centredLeft);
        
        g.setColour(juce::Colour(0xFF09090B));
        g.drawText(button.getButtonText(), textBounds, juce::Justification::centredLeft);
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

        // Standard Toggle Buttons (Modes)
        if (button.getToggleState())
        {
            g.setColour(juce::Colour(0xFF111114)); 
            g.fillRoundedRectangle(bounds, 4.0f);
            
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(0, 0, 4.0f, bounds.getHeight(), 2.0f);
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0xFF222228).withAlpha(0.6f));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF121215).withAlpha(0.7f));
            g.fillRoundedRectangle(bounds, 4.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        if (button.getName() == "SAVE" || button.getName() == "SETTINGS") return;
        
        g.setFont(juce::FontOptions(11.0f).withName("Helvetica").withStyle(button.getToggleState() ? "Bold" : "Plain"));
        g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colours::white.withAlpha(0.6f));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        // Dark Track line
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.5f));
        g.fillRoundedRectangle((float)x, (float)y + (float)height * 0.5f - 2.0f, (float)width, 4.0f, 2.0f);

        // Thumb Glow & Core
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 4.0f, 12.0f, 12.0f); // Shadow

        g.setColour(juce::Colours::white);
        g.fillEllipse(sliderPos - 5.0f, (float)y + (float)height * 0.5f - 5.0f, 10.0f, 10.0f);
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
    MinimalistSynthLookAndFeel synthLaf;

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