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

    // Header Title (Fills the empty space at the top)
    g.setColour(juce::Colour(0xFFFF9900));
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold")); // FIXED: Resolved Font deprecation warning
    g.drawText("HOME DISTO", 0, 15, 800, 30, juce::Justification::centred);

    // Panels (Shifted up to Y=60 for better balance)
    g.setColour(juce::Colour(0xFF2D2D2D));
    g.fillRoundedRectangle(20, 60, 500, 300, 10);  // Distortion Panel
    g.fillRoundedRectangle(540, 60, 240, 300, 10); // Color Panel
    g.fillRoundedRectangle(20, 380, 760, 100, 10); // Bottom Panel

    // Text Labels Configuration
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    
    // Panel Subtitles (Dimmed slightly for hierarchy)
    g.setColour(juce::Colour(0xFFAAAAAA)); 
    g.drawText("Distortion Engine", 20, 75, 500, 20, juce::Justification::centred);
    g.drawText("Domestic Color", 540, 75, 240, 20, juce::Justification::centred);

    // Reset colour for parameter labels
    g.setColour (juce::Colours::white);

    // Distortion Labels (Perfectly matched to bounding boxes)
    g.drawText("Drive", 50, 285, 160, 20, juce::Justification::centred);
    g.drawText("Shape", 240, 115, 230, 20, juce::Justification::centred);
    g.drawText("Low",   240, 285,  70, 20, juce::Justification::centred);
    g.drawText("Mid",   320, 285,  70, 20, juce::Justification::centred);
    g.drawText("High",  400, 285,  70, 20, juce::Justification::centred);
    
    // Color Labels
    g.drawText("Reverb",     565, 285, 80, 20, juce::Justification::centred);
    g.drawText("Saturation", 675, 245, 80, 20, juce::Justification::centred);

    // Bottom Panel Labels (Centered in the layout)
    g.drawText("Mix",    310, 460, 80, 20, juce::Justification::centred);
    g.drawText("Output", 410, 460, 80, 20, juce::Justification::centred);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Distortion Engine
    driveKnob.setBounds(50, 120, 160, 160);
    shapeSlider.setBounds(240, 140, 230, 30);
    lowKnob.setBounds(240, 210, 70, 70);
    midKnob.setBounds(320, 210, 70, 70);
    highKnob.setBounds(400, 210, 70, 70);

    // Domestic Color
    reverbSlider.setBounds(580, 120, 50, 160);
    satKnob.setBounds(675, 160, 80, 80);

    // Bottom Bar (Symmetrical inside the 760px panel)
    mixKnob.setBounds(310, 385, 80, 80);
    outputKnob.setBounds(410, 385, 80, 80);
}