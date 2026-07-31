#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Helvetica");

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
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF111114));
    autoToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFF111114)); // Changed to dark for contrast on bright background[cite: 3]
    addAndMakeVisible(autoToggle);
    autoAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "AUTO", autoToggle);

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

// Flat 2D renderer with optional premium texture[cite: 3]
void HomeDistoAudioProcessorEditor::drawFlatCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour, bool addTexture)
{
    // 1. Solid Flat Background
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 8.0f);

    // 2. Premium Texture Overlay (Subtle dot matrix)
    if (addTexture)
    {
        g.saveState();
        g.reduceClipRegion(bounds.toNearestInt());
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        
        for (int x = (int)bounds.getX(); x < bounds.getRight(); x += 4) {
            for (int y = (int)bounds.getY(); y < bounds.getBottom(); y += 4) {
                g.fillRect(x, y, 1, 1);
            }
        }
        g.restoreState();
    }

    // 3. Crisp Flat Border
    g.setColour(juce::Colours::black.withAlpha(0.20f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Deep dark chassis background
    g.fillAll (juce::Colour(0xFF09090B)); 

    // Main Housing Panel
    g.setColour(juce::Colour(0xFF111114));
    g.fillRoundedRectangle(10, 10, 700, 390, 8); 
    g.setColour(juce::Colour(0xFF222228));
    g.drawRoundedRectangle(10, 10, 700, 390, 8, 1.5f);

    // Title Block
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f).withName("Helvetica").withStyle("Bold"));
    g.drawText("HOME :", 25, 20, 80, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText("DISTO", 105, 20, 80, 30, juce::Justification::centredLeft);

    // Divider Line
    g.setColour(juce::Colour(0xFF1E1E24));
    g.drawLine(25, 65, 695, 65, 2.0f); 

    // ==========================================
    // DISTINCT FLAT CARDS WITH VIBRANT HEX COLORS
    // ==========================================
    
    // Card 1: MODES Section (Vibrant Mint, Textured)[cite: 3]
    drawFlatCard(g, juce::Rectangle<float>(20, 80, 230, 175), juce::Colour(0xFF64FFDA), true);

    // Card 2: FILTER Section (Warm Amber/Gold, Untextured to keep original feel)[cite: 3]
    drawFlatCard(g, juce::Rectangle<float>(20, 265, 230, 125), juce::Colour(0xFFFFC107), false);

    // Card 3: DRIVE / TONE / PUNCH Section (Vibrant Coral/Peach, Textured)[cite: 3]
    drawFlatCard(g, juce::Rectangle<float>(260, 80, 260, 310), juce::Colour(0xFFFF9EA6), true);

    // Card 4: OUTPUT / MIX Section (Vibrant Periwinkle, Textured)[cite: 3]
    drawFlatCard(g, juce::Rectangle<float>(530, 80, 170, 310), juce::Colour(0xFFA3B8FF), true);


    // ==========================================
    // TEXT LABELS & OVERLAYS 
    // ==========================================
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));

    // --- CARD 1: MODES ---
    g.setColour(juce::Colour(0xFF111114)); // Dark text for bright background[cite: 3]
    g.drawText("MODE", 20, 90, 230, 20, juce::Justification::centred);

    // --- CARD 2: FILTER --- (Untouched styling)
    g.setColour(juce::Colour(0xFF2A1C00)); 
    g.drawText("FILTER", 20, 273, 230, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    g.drawText("LOW CUT", 35, 295, 80, 15, juce::Justification::left);
    g.drawText("HIGH CUT", 155, 295, 80, 15, juce::Justification::right);
    g.setColour(juce::Colour(0xFF4A3400));
    g.drawText(getFrequencyString(lowCutSlider.getValue()), 35, 310, 80, 15, juce::Justification::left); 
    g.drawText(getFrequencyString(highCutSlider.getValue()), 155, 310, 80, 15, juce::Justification::right); 

    // Mini filter graph aligned with sliders
    float lowProp = lowCutSlider.valueToProportionOfLength(lowCutSlider.getValue());
    float lowX = lowCutSlider.getX() + (lowProp * lowCutSlider.getWidth());
    float highProp = highCutSlider.valueToProportionOfLength(highCutSlider.getValue());
    float highX = highCutSlider.getX() + (highProp * highCutSlider.getWidth());
    float graphY = 345.0f;

    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.drawLine(35, graphY, 235, graphY, 2.0f); 

    juce::Path filterCurve; 
    filterCurve.startNewSubPath(35, 375);
    filterCurve.cubicTo(lowX - 6, 375, lowX - 6, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 6, graphY, highX + 6, 375, 235, 375);
    g.setColour(juce::Colour(0xFF4A3400));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f)); 

    // --- CARD 3: DRIVE, TONE, PUNCH ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF111114)); // Dark text for bright background[cite: 3]
    g.drawText("DRIVE", 260, 90, 260, 20, juce::Justification::centred); 
    g.drawText("TONE", 285, 260, 80, 20, juce::Justification::centred);
    g.drawText("PUNCH", 415, 260, 80, 20, juce::Justification::centred);

    // Descriptors
    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF111114).withAlpha(0.6f)); // Dark faded text[cite: 3]
    g.drawText("DARK",  275, 360, 40, 15, juce::Justification::left);
    g.drawText("BRIGHT", 335, 360, 40, 15, juce::Justification::right);
    g.drawText("SOFT",  405, 360, 40, 15, juce::Justification::left);
    g.drawText("HARD",   465, 360, 40, 15, juce::Justification::right);

    // --- CARD 4: OUTPUT, MIX ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF111114)); // Dark text for bright background[cite: 3]
    g.drawText("OUTPUT", 545, 90, 70, 20, juce::Justification::left);
    g.drawText("MIX", 580, 260, 70, 20, juce::Justification::centred); 

    // Descriptors
    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF111114).withAlpha(0.6f)); // Dark faded text[cite: 3]
    g.drawText("DRY", 565, 360, 30, 15, juce::Justification::left);
    g.drawText("WET", 635, 360, 30, 15, juce::Justification::right);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Header
    presetCombo.setBounds(210, 20, 300, 30);
    saveButton.setBounds(620, 20, 30, 30); 
    settingsButton.setBounds(660, 20, 30, 30); 

    // --- CARD 1: MODES ---
    for (int i = 0; i < 6; ++i) 
    {
        int col = i % 2; 
        int row = i / 2; 
        modeButtons[i].setBounds(35 + (col * 105), 118 + (row * 42), 95, 26);
    }

    // --- CARD 2: FILTER ---
    lowCutSlider.setBounds(30, 325, 90, 30);  
    highCutSlider.setBounds(150, 325, 90, 30); 

    // --- CARD 3: DRIVE, TONE & PUNCH ---
    driveKnob.setBounds(315, 115, 150, 150); 
    toneKnob.setBounds(290, 285, 70, 70);
    punchKnob.setBounds(420, 285, 70, 70);

    // --- CARD 4: OUTPUT & MIX ---
    outputKnob.setBounds(555, 125, 120, 120); 
    autoToggle.setBounds(620, 90, 65, 20); 
    mixKnob.setBounds(580, 290, 70, 70);
}