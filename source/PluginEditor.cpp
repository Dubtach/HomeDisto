#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (780, 450); // Canvas expanded perfectly to fit 3 uniform column groups
    setLookAndFeel(&flatLaf);

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
    // Base Canvas Background
    g.fillAll (juce::Colour(0xFF0C0C0E)); 

    // Inner Window Bezel
    g.setColour(juce::Colour(0xFF121214));
    g.fillRoundedRectangle(10, 10, 760, 430, 8);
    g.setColour(juce::Colour(0xFF1C1C20));
    g.drawRoundedRectangle(10, 10, 760, 430, 8, 1.5f);

    // --- Header Section ---
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("Home:", 25, 20, 80, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("Disto", 85, 20, 80, 30, juce::Justification::centredLeft);

    // Line Separator
    g.setColour(juce::Colour(0xFF202024));
    g.drawLine(25, 65, 755, 65, 2.0f);

    // ==========================================
    // 3 COLUMN SEMANTIC LAYOUT PANELS
    // ==========================================
    int colWidth = 230;
    int colHeight = 345;
    int colY = 80;

    g.setColour(juce::Colour(0xFF18181A)); // Panel Background
    g.fillRoundedRectangle(25,  colY, colWidth, colHeight, 8); // Col 1: Modes & Filter
    g.fillRoundedRectangle(275, colY, colWidth, colHeight, 8); // Col 2: Drive, Tone, Punch
    g.fillRoundedRectangle(525, colY, colWidth, colHeight, 8); // Col 3: Output, Auto, Mix

    // Shared Header Font for Sections
    g.setColour(juce::Colour(0xFF888888));
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));

    // --- COLUMN 1: MODES & FILTER ---
    g.drawText("MODE", 25, 95, colWidth, 20, juce::Justification::centred);
    g.drawText("FILTER", 25, 255, colWidth, 20, juce::Justification::centred);
    g.setColour(juce::Colour(0xFF202024));
    g.drawLine(40, 245, 235, 245, 1.5f); // Sub-divider

    // Dynamic Filter Readouts
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("LOW CUT", 45, 280, 80, 20, juce::Justification::left);
    g.drawText("HIGH CUT", 155, 280, 80, 20, juce::Justification::right);
    g.setColour(juce::Colours::white);
    g.drawText(getFrequencyString(lowCutSlider.getValue()), 45, 295, 80, 20, juce::Justification::left);
    g.drawText(getFrequencyString(highCutSlider.getValue()), 155, 295, 80, 20, juce::Justification::right);

    // Exact Graphic Math based on sliders true proportion
    float lowProp = lowCutSlider.valueToProportionOfLength(lowCutSlider.getValue());
    float lowX = lowCutSlider.getX() + (lowProp * lowCutSlider.getWidth());

    float highProp = highCutSlider.valueToProportionOfLength(highCutSlider.getValue());
    float highX = highCutSlider.getX() + (highProp * highCutSlider.getWidth());
    float graphY = 360.0f; // Align to horizontal center of sliders

    g.setColour(juce::Colour(0xFF2A2A30));
    g.drawLine(45, graphY, 235, graphY, 2.0f); // Track line

    juce::Path filterCurve;
    filterCurve.startNewSubPath(45, 390);
    filterCurve.cubicTo(lowX - 8, 390, lowX - 8, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 8, graphY, highX + 8, 390, 235, 390);
    g.setColour(juce::Colour(0xFFA855F7));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f));

    // --- COLUMN 2: DRIVE, TONE, PUNCH ---
    g.setColour(juce::Colour(0xFF888888));
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    g.drawText("DRIVE", 275, 95, colWidth, 20, juce::Justification::centred);
    g.drawText("TONE", 310, 335, 70, 20, juce::Justification::centred);
    g.drawText("PUNCH", 400, 335, 70, 20, juce::Justification::centred);

    // --- COLUMN 3: OUTPUT, AUTO, MIX ---
    g.drawText("OUTPUT", 525, 95, colWidth, 20, juce::Justification::centred);
    g.drawText("MIX", 605, 335, 70, 20, juce::Justification::centred);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // --- TOP BAR --- 
    presetCombo.setBounds(240, 20, 300, 30); // Longer preset bar in center
    saveButton.setBounds(680, 20, 30, 30); // Pushed to right cluster
    settingsButton.setBounds(720, 20, 30, 30); // Far corner

    // --- COLUMN 1 ---
    for (int i = 0; i < 4; ++i)
        modeButtons[i].setBounds(50, 125 + (i * 28), 180, 24);

    lowCutSlider.setBounds(45, 340, 95, 40);  
    highCutSlider.setBounds(140, 340, 95, 40); 

    // --- COLUMN 2 ---
    driveKnob.setBounds(290, 120, 200, 200); // Massive center focus
    toneKnob.setBounds(310, 355, 70, 70);
    punchKnob.setBounds(400, 355, 70, 70);

    // --- COLUMN 3 ---
    outputKnob.setBounds(565, 130, 150, 150);
    autoToggle.setBounds(605, 300, 70, 25);
    mixKnob.setBounds(605, 355, 70, 70);
}