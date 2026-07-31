#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (640, 430); // More compact canvas
    setLookAndFeel(&flatLaf);

    // Top Bar
    presetCombo.addItemList({"Huge Punch", "Tape Saturation", "Digital Crush", "Warm Tube"}, 1);
    presetCombo.setSelectedId(1, juce::dontSendNotification);
    presetCombo.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetCombo);

    saveButton.setName("SAVE");
    addAndMakeVisible(saveButton);
    
    settingsButton.setName("SETTINGS");
    addAndMakeVisible(settingsButton);

    auto setupKnob = [this](juce::Slider& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attach) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, slider);
    };

    setupKnob(driveKnob, "DRIVE", driveAttach);
    setupKnob(outputKnob, "OUT", outAttach); 
    setupKnob(toneKnob, "TONE", toneAttach);
    setupKnob(punchKnob, "PUNCH", punchAttach);
    setupKnob(mixKnob, "MIX", mixAttach);

    autoToggle.setButtonText("AUTO");
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFA855F7));
    addAndMakeVisible(autoToggle);
    autoAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "AUTO", autoToggle);

    // Setup List Modes
    for (int i = 0; i < 4; ++i)
    {
        modeButtons[i].setButtonText(modeNames[i]);
        modeButtons[i].setRadioGroupId(100);
        modeButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(modeButtons[i]);
        
        modeButtons[i].onClick = [this, i] {
            audioProcessor.apvts.getParameter("MODE")->setValueNotifyingHost(i / 3.0f);
        };
    }
    
    audioProcessor.apvts.addParameterListener("MODE", this);
    int initialMode = (int)audioProcessor.apvts.getRawParameterValue("MODE")->load();
    modeButtons[initialMode].setToggleState(true, juce::dontSendNotification);

    // Interactive Filter Setup
    lowCutSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    lowCutSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lowCutSlider);
    lowAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "LOW_CUT", lowCutSlider);
    lowCutSlider.onValueChange = [this] { repaint(); };

    highCutSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    highCutSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(highCutSlider);
    highAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "HIGH_CUT", highCutSlider);
    highCutSlider.onValueChange = [this] { repaint(); };
}

HomeDistoAudioProcessorEditor::~HomeDistoAudioProcessorEditor()
{
    audioProcessor.apvts.removeParameterListener("MODE", this);
    setLookAndFeel(nullptr); 
}

void HomeDistoAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "MODE")
    {
        juce::MessageManager::callAsync([this, newValue]() {
            modeButtons[(int)newValue].setToggleState(true, juce::dontSendNotification);
        });
    }
}

juce::String HomeDistoAudioProcessorEditor::getFrequencyString(float hz)
{
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, 1) + " kHz";
    return juce::String(juce::roundToInt(hz)) + " Hz";
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Flat Dark Base
    g.fillAll (juce::Colour(0xFF101012)); 

    // Inner Plugin Bezel
    g.setColour(juce::Colour(0xFF141416));
    g.fillRoundedRectangle(10, 10, 620, 410, 10);
    g.setColour(juce::Colour(0xFF1C1C20));
    g.drawRoundedRectangle(10, 10, 620, 410, 10, 1.5f);

    // --- Header Section ---
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("Home:", 25, 20, 80, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("Disto", 90, 20, 80, 30, juce::Justification::centredLeft);

    // --- Line Separator ---
    g.setColour(juce::Colour(0xFF202024));
    g.drawLine(25, 65, 615, 65, 2.0f);

    // --- Main Labels ---
    g.setColour(juce::Colour(0xFF777777));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("MODE", 25, 80, 100, 20, juce::Justification::centred);
    g.drawText("DRIVE", 230, 80, 150, 20, juce::Justification::centred);
    g.drawText("OUTPUT", 475, 80, 80, 20, juce::Justification::centred);

    // --- Filter Box Panel ---
    g.setColour(juce::Colour(0xFF101012));
    g.fillRoundedRectangle(25, 260, 270, 140, 8); 
    
    // Static Filter Texts inside Box
    g.setColour(juce::Colour(0xFF777777));
    g.drawText("FILTER", 25, 265, 270, 20, juce::Justification::centred);
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("LOW CUT", 40, 290, 80, 20, juce::Justification::left);
    g.drawText("HIGH CUT", 200, 290, 80, 20, juce::Justification::right);
    g.setColour(juce::Colours::white);
    g.drawText(getFrequencyString(lowCutSlider.getValue()), 40, 310, 80, 20, juce::Justification::left);
    g.drawText(getFrequencyString(highCutSlider.getValue()), 200, 310, 80, 20, juce::Justification::right);

    // Filter Graph Math
    // We use the slider's exact proportion layout to map perfectly to the visual line
    float lowProp = lowCutSlider.valueToProportionOfLength(lowCutSlider.getValue());
    float lowX = lowCutSlider.getX() + (lowProp * lowCutSlider.getWidth());

    float highProp = highCutSlider.valueToProportionOfLength(highCutSlider.getValue());
    float highX = highCutSlider.getX() + (highProp * highCutSlider.getWidth());
    
    float graphY = 350.0f; // Slider Y center

    // Draw Graph Background Line
    g.setColour(juce::Colour(0xFF2A2A30));
    g.drawLine(40, graphY, 280, graphY, 2.0f);

    // Draw Dynamic Purple Curve connecting exactly to the mathematical thumbs
    juce::Path filterCurve;
    filterCurve.startNewSubPath(40, 380);
    filterCurve.cubicTo(lowX - 10, 380, lowX - 10, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 10, graphY, highX + 10, 380, 280, 380);
    
    g.setColour(juce::Colour(0xFFA855F7));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f));

    // --- Bottom Right Labels ---
    g.setColour(juce::Colour(0xFF777777));
    g.drawText("TONE",  330, 265, 70, 20, juce::Justification::centred);
    g.drawText("PUNCH", 420, 265, 70, 20, juce::Justification::centred);
    g.drawText("MIX",   510, 265, 70, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f));
    g.drawText("DARK     BRIGHT", 330, 355, 70, 20, juce::Justification::centred);
    g.drawText("SOFT     HARD",   420, 355, 70, 20, juce::Justification::centred);
    g.drawText("DRY     WET",     510, 355, 70, 20, juce::Justification::centred);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Top Bar 
    presetCombo.setBounds(200, 20, 180, 30);
    saveButton.setBounds(395, 20, 30, 30);
    settingsButton.setBounds(435, 20, 30, 30);

    // Mode List
    for (int i = 0; i < 4; ++i)
        modeButtons[i].setBounds(25, 110 + (i * 35), 100, 30);

    // Main Knobs
    driveKnob.setBounds(230, 100, 150, 150);
    outputKnob.setBounds(475, 110, 80, 80);
    autoToggle.setBounds(480, 200, 70, 25);

    // Filter Sliders overlaying the graph directly
    lowCutSlider.setBounds(40, 330, 120, 40);  
    highCutSlider.setBounds(160, 330, 120, 40); 

    // Bottom Right Knobs
    toneKnob.setBounds(330, 290, 70, 70);
    punchKnob.setBounds(420, 290, 70, 70);
    mixKnob.setBounds(510, 290, 70, 70);
}