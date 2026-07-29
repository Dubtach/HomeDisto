#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 500);

    // Style Configuration
    getLookAndFeel().setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF9900)); // Orange Accents
    getLookAndFeel().setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF6600));
    getLookAndFeel().setColour(juce::Slider::trackColourId, juce::Colours::darkgrey);

    // Initialize Knobs
    auto setupKnob = [this](juce::Slider& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attach, bool isVertical = false) {
        slider.setSliderStyle(isVertical ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, slider);
    };

    setupKnob(driveKnob, "DRIVE", driveAttach);
    setupKnob(lowKnob, "LOW", lowAttach);
    setupKnob(midKnob, "MID", midAttach);
    setupKnob(highKnob, "HIGH", highAttach);
    setupKnob(satKnob, "SAT", satAttach);
    setupKnob(mixKnob, "MIX", mixAttach);
    setupKnob(outputKnob, "OUT", outAttach);

    // Setup Shape Slider (Horizontal)
    shapeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    shapeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(shapeSlider);
    shapeAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "SHAPE", shapeSlider);

    // Setup Reverb Slider (Vertical)
    setupKnob(reverbSlider, "REVERB", revAttach, true);
}

HomeDistoAudioProcessorEditor::~HomeDistoAudioProcessorEditor()
{
    // Reset LookAndFeel to prevent memory leaks in JUCE
    setLookAndFeel(nullptr); 
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (juce::Colour(0xFF1E1E1E)); 

    // Panels
    g.setColour(juce::Colour(0xFF2D2D2D));
    g.fillRoundedRectangle(20, 100, 500, 250, 10); // Distortion Panel
    g.fillRoundedRectangle(540, 100, 240, 250, 10); // Color Panel
    g.fillRoundedRectangle(20, 370, 760, 110, 10); // Bottom Panel

    // Text Labels
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    
    // Panel Titles
    g.drawText("Distortion Engine", 20, 110, 500, 20, juce::Justification::centred);
    g.drawText("Domestic Color", 540, 110, 240, 20, juce::Justification::centred);

    // Knob Labels
    g.drawText("Drive", 50, 290, 150, 20, juce::Justification::centred);
    g.drawText("Shape", 250, 130, 200, 20, juce::Justification::centred);
    g.drawText("Low", 220, 290, 80, 20, juce::Justification::centred);
    g.drawText("Mid", 320, 290, 80, 20, juce::Justification::centred);
    g.drawText("High", 420, 290, 80, 20, juce::Justification::centred);
    
    g.drawText("Reverb", 550, 290, 80, 20, juce::Justification::centred);
    g.drawText("Saturation", 670, 290, 80, 20, juce::Justification::centred);

    g.drawText("Mix", 410, 450, 80, 20, juce::Justification::centred);
    g.drawText("Output", 520, 450, 80, 20, juce::Justification::centred);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Distortion Engine
    driveKnob.setBounds(50, 140, 150, 150); // Large Left Knob
    shapeSlider.setBounds(250, 160, 200, 30);
    lowKnob.setBounds(220, 210, 80, 80);
    midKnob.setBounds(320, 210, 80, 80);
    highKnob.setBounds(420, 210, 80, 80);

    // Domestic Color
    reverbSlider.setBounds(570, 140, 40, 150);
    satKnob.setBounds(670, 200, 80, 80);

    // Bottom Bar
    mixKnob.setBounds(410, 380, 80, 80);
    outputKnob.setBounds(520, 380, 80, 80);
}