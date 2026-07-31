#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Helvetica");

    setSize (720, 410);
    setLookAndFeel(&flatLaf);

    // Generate the premium noise texture once on startup[cite: 6]
    createNoiseTexture(); 

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
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF1A1A20));
    autoToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFF55555D)); 
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

void HomeDistoAudioProcessorEditor::createNoiseTexture()
{
    // Generate a subtle noise map
    noiseTexture = juce::Image(juce::Image::ARGB, 256, 256, true);
    juce::Graphics g(noiseTexture);
    juce::Random r;
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            auto alpha = (uint8_t)(r.nextFloat() * 12.0f); // Very faint noise
            noiseTexture.setPixelAt(x, y, juce::Colour((uint8_t)0, (uint8_t)0, (uint8_t)0, alpha));
        }
    }
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

// Flat 2D card with subtle noise texture[cite: 6]
void HomeDistoAudioProcessorEditor::drawTexturedCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour)
{
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 6.0f);

    // Apply the noise texture
    g.setTiledImageFill(noiseTexture, 0, 0, 1.0f);
    g.fillRoundedRectangle(bounds, 6.0f);

    // Crisp, flat 2D border
    g.setColour(juce::Colour(0xFFC0C0C5));
    g.drawRoundedRectangle(bounds, 6.0f, 1.5f);
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Premium light-grey main chassis background[cite: 6]
    g.fillAll (juce::Colour(0xFFD8D8DE)); 

    // Main Housing Panel[cite: 6]
    g.setColour(juce::Colour(0xFFE2E2E7));
    g.fillRoundedRectangle(10, 10, 700, 390, 8); 
    g.setColour(juce::Colour(0xFFC5C5CB));
    g.drawRoundedRectangle(10, 10, 700, 390, 8, 1.5f);

    // Title Block[cite: 6]
    g.setColour(juce::Colour(0xFF1A1A20));
    g.setFont(juce::FontOptions(22.0f).withName("Helvetica").withStyle("Bold"));
    g.drawText("HOME :", 25, 20, 80, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xFF55555D));
    g.drawText("DISTO", 105, 20, 80, 30, juce::Justification::centredLeft);

    // Divider Line[cite: 6]
    g.setColour(juce::Colour(0xFFC5C5CB));
    g.drawLine(25, 65, 695, 65, 2.0f); 

    // ==========================================
    // UNIFIED BRIGHTER SECTIONS WITH TEXTURE[cite: 6]
    // ==========================================
    juce::Colour cardColor = juce::Colour(0xFFF2F2F6); // Same base hue, slightly brighter
    
    drawTexturedCard(g, juce::Rectangle<float>(20, 80, 230, 175), cardColor);
    drawTexturedCard(g, juce::Rectangle<float>(20, 265, 230, 125), cardColor);
    drawTexturedCard(g, juce::Rectangle<float>(260, 80, 260, 310), cardColor);
    drawTexturedCard(g, juce::Rectangle<float>(530, 80, 170, 310), cardColor);

    // ==========================================
    // DARK TEXT LABELS[cite: 6]
    // ==========================================
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF1A1A20)); // Sharp dark color to prevent clashing

    // --- CARD 1: MODES ---
    g.drawText("MODE", 20, 90, 230, 20, juce::Justification::centred);

    // --- CARD 2: FILTER ---
    g.drawText("FILTER", 20, 273, 230, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    g.drawText("LOW CUT", 35, 295, 80, 15, juce::Justification::left);
    g.drawText("HIGH CUT", 155, 295, 80, 15, juce::Justification::right);
    g.setColour(juce::Colour(0xFF55555D));
    g.drawText(getFrequencyString(lowCutSlider.getValue()), 35, 310, 80, 15, juce::Justification::left);
    g.drawText(getFrequencyString(highCutSlider.getValue()), 155, 310, 80, 15, juce::Justification::right);

    // Mini filter graph[cite: 6]
    float lowProp = lowCutSlider.valueToProportionOfLength(lowCutSlider.getValue());
    float lowX = lowCutSlider.getX() + (lowProp * lowCutSlider.getWidth());
    float highProp = highCutSlider.valueToProportionOfLength(highCutSlider.getValue());
    float highX = highCutSlider.getX() + (highProp * highCutSlider.getWidth());
    float graphY = 345.0f;

    g.setColour(juce::Colour(0xFFD0D0D5));
    g.drawLine(35, graphY, 235, graphY, 2.0f); 

    juce::Path filterCurve; 
    filterCurve.startNewSubPath(35, 375);
    filterCurve.cubicTo(lowX - 6, 375, lowX - 6, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 6, graphY, highX + 6, 375, 235, 375);
    g.setColour(juce::Colour(0xFF1A1A20));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f));

    // --- CARD 3: DRIVE, TONE, PUNCH ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF1A1A20));
    g.drawText("DRIVE", 260, 90, 260, 20, juce::Justification::centred);
    g.drawText("TONE", 285, 260, 80, 20, juce::Justification::centred);
    g.drawText("PUNCH", 415, 260, 80, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF6A6A75));
    g.drawText("DARK",  275, 360, 40, 15, juce::Justification::left);
    g.drawText("BRIGHT", 335, 360, 40, 15, juce::Justification::right);
    g.drawText("SOFT",  405, 360, 40, 15, juce::Justification::left);
    g.drawText("HARD",   465, 360, 40, 15, juce::Justification::right);

    // --- CARD 4: OUTPUT, MIX ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF1A1A20));
    g.drawText("OUTPUT", 545, 90, 70, 20, juce::Justification::left);
    g.drawText("MIX", 580, 260, 70, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFF6A6A75));
    g.drawText("DRY", 565, 360, 30, 15, juce::Justification::left);
    g.drawText("WET", 635, 360, 30, 15, juce::Justification::right);
}

void HomeDistoAudioProcessorEditor::resized()
{
    presetCombo.setBounds(210, 20, 300, 30);
    saveButton.setBounds(620, 20, 30, 30); 
    settingsButton.setBounds(660, 20, 30, 30); 

    for (int i = 0; i < 6; ++i) 
    {
        int col = i % 2; 
        int row = i / 2; 
        modeButtons[i].setBounds(35 + (col * 105), 118 + (row * 42), 95, 26);
    }

    lowCutSlider.setBounds(30, 325, 90, 30);  
    highCutSlider.setBounds(150, 325, 90, 30); 

    driveKnob.setBounds(315, 115, 150, 150); 
    toneKnob.setBounds(290, 285, 70, 70);
    punchKnob.setBounds(420, 285, 70, 70);

    outputKnob.setBounds(555, 125, 120, 120); 
    autoToggle.setBounds(620, 90, 65, 20); 
    mixKnob.setBounds(580, 290, 70, 70);
}