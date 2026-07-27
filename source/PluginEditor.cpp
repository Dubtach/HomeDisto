#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Apply the flat 2D styling
    setLookAndFeel(&flatLookAndFeel);

    // Setup Drive Slider
    driveSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(driveSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.treeState, "DRIVE", driveSlider);

    // Setup Output Slider
    outputSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(outputSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.treeState, "OUTPUT", outputSlider);

    // Setup Mode Box
    modeBox.addItemList({"70s (Soft)", "80s (Hard)", "90s (Asym)", "Modern (Fold)"}, 1);
    addAndMakeVisible(modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.treeState, "MODE", modeBox);

    setSize (400, 300);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel(nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Pure flat background
    g.fillAll (juce::Colour (0xff181818));

    // Flat typography
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText ("HOME DISTO", 0, 20, getWidth(), 40, juce::Justification::centred, false);
    
    g.setFont (juce::FontOptions(14.0f, juce::Font::plain));
    g.drawText ("DRIVE", 50, 230, 100, 20, juce::Justification::centred, false);
    g.drawText ("OUTPUT", 250, 230, 100, 20, juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    // Layout 2D components
    modeBox.setBounds (125, 80, 150, 30);
    driveSlider.setBounds (50, 130, 100, 100);
    outputSlider.setBounds (250, 130, 100, 100);
}