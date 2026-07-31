#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (420, 500);
    setLookAndFeel(&flatLookAndFeel);

    auto setupKnob = [this](juce::Slider& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attach) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, slider);
    };

    setupKnob(driveKnob, "DRIVE", driveAttach);
    setupKnob(lowCutKnob, "LOW_CUT", lowAttach);
    setupKnob(highCutKnob, "HIGH_CUT", highAttach);
    setupKnob(toneKnob, "TONE", toneAttach);
    setupKnob(punchKnob, "PUNCH", punchAttach);
    setupKnob(textureKnob, "TEXTURE", textureAttach);
    setupKnob(mixKnob, "MIX", mixAttach);
    setupKnob(outputKnob, "OUT", outAttach);

    algoSelector.addItemList({"Warm", "Punch", "Tape", "Digital", "Fuzz"}, 1);
    addAndMakeVisible(algoSelector);
    algoAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "ALGO", algoSelector);

    autoToggle.setButtonText("AUTO");
    addAndMakeVisible(autoToggle);
    autoAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "AUTO", autoToggle);
}

HomeDistoAudioProcessorEditor::~HomeDistoAudioProcessorEditor()
{
    setLookAndFeel(nullptr); 
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Flat solid background
    g.fillAll (juce::Colour(0xFF1A1A1E)); 

    // Header Panel
    g.setColour(juce::Colour(0xFF242429));
    g.fillRoundedRectangle(10, 10, 400, 40, 5);
    
    g.setColour(juce::Colour(0xFFFF6B35));
    g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    g.drawText("Dubtach", 20, 10, 100, 40, juce::Justification::centredLeft);
    
    g.setColour(juce::Colour(0xFF888890));
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("Home:Disto", 120, 10, 100, 40, juce::Justification::centredLeft);

    // Text Labels 
    g.setColour (juce::Colour(0xFF888890));
    g.setFont (12.0f);
    
    g.drawText("DRIVE",     160, 150, 100, 20, juce::Justification::centred);
    g.drawText("Low Cut",   60,  240, 80, 20,  juce::Justification::centred);
    g.drawText("High Cut",  280, 240, 80, 20,  juce::Justification::centred);
    
    g.drawText("Tone",      60,  340, 80, 20,  juce::Justification::centred);
    g.drawText("Punch",     170, 340, 80, 20,  juce::Justification::centred);
    g.drawText("Texture",   280, 340, 80, 20,  juce::Justification::centred);
    
    g.drawText("Mix",       60,  440, 80, 20,  juce::Justification::centred);
    g.drawText("Output",    170, 440, 80, 20,  juce::Justification::centred);
    g.drawText("Auto",      280, 440, 80, 20,  juce::Justification::centred);

    // Simulated 2D curve between filters
    g.setColour(juce::Colour(0xFFFF6B35));
    juce::Path curve;
    curve.startNewSubPath(130, 280);
    curve.quadraticTo(210, 210, 290, 280);
    g.strokePath(curve, juce::PathStrokeType(2.0f));
}

void HomeDistoAudioProcessorEditor::resized()
{
    driveKnob.setBounds(160, 70, 100, 100);
    algoSelector.setBounds(40, 180, 340, 24);
    
    lowCutKnob.setBounds(60, 260, 80, 80);
    highCutKnob.setBounds(280, 260, 80, 80);
    
    toneKnob.setBounds(60, 360, 80, 80);
    punchKnob.setBounds(170, 360, 80, 80);
    textureKnob.setBounds(280, 360, 80, 80);
    
    mixKnob.setBounds(60, 460, 40, 40);
    outputKnob.setBounds(170, 460, 40, 40);
    autoToggle.setBounds(300, 460, 40, 40);
}