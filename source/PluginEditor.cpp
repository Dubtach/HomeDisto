#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Reverted to a more compact window size while mapping out the new columns
    setSize (720, 410);
    setLookAndFeel(&flatLaf);

    presetCombo.addItemList({"Huge Punch", "Tape Saturation", "Digital Crush", "Warm Tube", "Heavy Fuzz"}, 1);
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

    // Expanded to loop through 6 modes 
    for (int i = 0; i < 6; ++i)
    {
        modeButtons[i].setButtonText(modeNames[i]);
        modeButtons[i].setRadioGroupId(100);
        modeButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(modeButtons[i]);
        
        modeButtons[i].onClick = [this, i] {
            audioProcessor.apvts.getParameter("MODE")->setValueNotifyingHost(i / 5.0f);
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
    g.fillAll (juce::Colour(0xFF0C0C0E)); 

    g.setColour(juce::Colour(0xFF121214));
    g.fillRoundedRectangle(10, 10, 700, 390, 8);
    g.setColour(juce::Colour(0xFF1C1C20));
    g.drawRoundedRectangle(10, 10, 700, 390, 8, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("Home:", 25, 20, 80, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("Disto", 85, 20, 80, 30, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xFF202024));
    g.drawLine(25, 65, 695, 65, 2.0f);

    // ==========================================
    // ASYMMETRIC COLUMN PANELS
    // ==========================================
    int colY = 80;
    int colHeight = 310;
    
    // Panel Backgrounds
    g.setColour(juce::Colour(0xFF18181A)); 
    g.fillRoundedRectangle(20,  colY, 230, colHeight, 8); // Col 1: Modes (70%) & Filter (30%)
    g.fillRoundedRectangle(260, colY, 260, colHeight, 8); // Col 2: Drive, Tone, Punch (Widest)
    g.fillRoundedRectangle(530, colY, 170, colHeight, 8); // Col 3: Thinner Output/Mix

    g.setColour(juce::Colour(0xFF888888));
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));

    // --- COLUMN 1: MODES & FILTER (70/30 Split) ---
    g.drawText("MODE", 20, 90, 230, 20, juce::Justification::centred);
    
    // 70% Divider
    g.setColour(juce::Colour(0xFF202024));
    g.drawLine(35, 280, 235, 280, 1.5f); 
    
    g.setColour(juce::Colour(0xFF888888));
    g.drawText("FILTER", 20, 290, 230, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("LOW CUT", 35, 315, 80, 15, juce::Justification::left);
    g.drawText("HIGH CUT", 155, 315, 80, 15, juce::Justification::right);
    g.setColour(juce::Colours::white);
    g.drawText(getFrequencyString(lowCutSlider.getValue()), 35, 330, 80, 15, juce::Justification::left);
    g.drawText(getFrequencyString(highCutSlider.getValue()), 155, 330, 80, 15, juce::Justification::right);

    // Mini filter graph aligned with sliders
    float lowProp = lowCutSlider.valueToProportionOfLength(lowCutSlider.getValue());
    float lowX = lowCutSlider.getX() + (lowProp * lowCutSlider.getWidth());
    float highProp = highCutSlider.valueToProportionOfLength(highCutSlider.getValue());
    float highX = highCutSlider.getX() + (highProp * highCutSlider.getWidth());
    float graphY = 360.0f; 

    g.setColour(juce::Colour(0xFF2A2A30));
    g.drawLine(35, graphY, 235, graphY, 2.0f); 

    juce::Path filterCurve;
    filterCurve.startNewSubPath(35, 385);
    filterCurve.cubicTo(lowX - 6, 385, lowX - 6, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 6, graphY, highX + 6, 385, 235, 385);
    g.setColour(juce::Colour(0xFFA855F7));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f));

    // --- COLUMN 2: DRIVE, TONE, PUNCH ---
    g.setColour(juce::Colour(0xFF888888));
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    g.drawText("DRIVE", 260, 90, 260, 20, juce::Justification::centred);
    g.drawText("TONE", 285, 260, 80, 20, juce::Justification::centred);
    g.drawText("PUNCH", 415, 260, 80, 20, juce::Justification::centred);

    // Tone & Punch Descriptors (Dark/Bright, Soft/Hard)
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.setColour(juce::Colour(0xFF55555D));
    g.drawText("DARK",  275, 360, 40, 15, juce::Justification::left);
    g.drawText("BRIGHT", 335, 360, 40, 15, juce::Justification::right);
    g.drawText("SOFT",  405, 360, 40, 15, juce::Justification::left);
    g.drawText("HARD",   465, 360, 40, 15, juce::Justification::right);

    // --- COLUMN 3: THINNER OUTPUT, MIX ---
    g.setColour(juce::Colour(0xFF888888));
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    g.drawText("OUTPUT", 530, 90, 170, 20, juce::Justification::centred);
    g.drawText("MIX", 580, 275, 70, 20, juce::Justification::centred);

    // Mix Descriptors
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.setColour(juce::Colour(0xFF55555D));
    g.drawText("DRY", 565, 360, 30, 15, juce::Justification::left);
    g.drawText("WET", 635, 360, 30, 15, juce::Justification::right);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Header
    presetCombo.setBounds(210, 20, 300, 30);
    saveButton.setBounds(620, 20, 30, 30); 
    settingsButton.setBounds(660, 20, 30, 30); 

    // --- COLUMN 1: MODES & FILTER ---
    // 2 Columns, 3 Rows configuration for 6 Modes
    for (int i = 0; i < 6; ++i)
    {
        int col = i % 2; // 0 for left, 1 for right
        int row = i / 2; // 0, 1, or 2
        modeButtons[i].setBounds(35 + (col * 105), 120 + (row * 45), 95, 25);
    }

    lowCutSlider.setBounds(30, 345, 90, 30);  
    highCutSlider.setBounds(150, 345, 90, 30); 

    // --- COLUMN 2: DRIVE & TONE SHAPING ---
    driveKnob.setBounds(315, 115, 150, 150); 
    toneKnob.setBounds(290, 285, 70, 70);
    punchKnob.setBounds(420, 285, 70, 70);

    // --- COLUMN 3: THIN OUTPUT/MIX ---
    outputKnob.setBounds(555, 125, 120, 120);
    autoToggle.setBounds(580, 250, 70, 20); // Stacked tightly above Mix
    mixKnob.setBounds(580, 290, 70, 70);
}