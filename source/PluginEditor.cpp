#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Helvetica");

    setSize (720, 410);
    setLookAndFeel(&synthLaf); 

    presetCombo.addItemList({"Huge Punch", "Tape Saturation", "Digital Crush", "Warm Tube", "Heavy Fuzz"}, 1);
    presetCombo.setSelectedId(1, juce::dontSendNotification);
    presetCombo.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetCombo);

    saveButton.setName("SAVE");
    addAndMakeVisible(saveButton);
    
    bypassButton.setName("BYPASS");
    addAndMakeVisible(bypassButton);
    
    settingsButton.setName("SETTINGS");
    addAndMakeVisible(settingsButton);

    auto setupKnob = [this](juce::Slider& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attach, juce::Colour glowColour) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, glowColour); 
        addAndMakeVisible(slider);
        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, slider);
    };

    setupKnob(driveKnob, "DRIVE", driveAttach, juce::Colour(0xFFB900FF)); 
    setupKnob(toneKnob, "TONE", toneAttach, juce::Colour(0xFFB900FF));
    setupKnob(punchKnob, "PUNCH", punchAttach, juce::Colour(0xFFB900FF));

    setupKnob(outputKnob, "OUT", outAttach, juce::Colour(0xFFFF007F)); 
    setupKnob(mixKnob, "MIX", mixAttach, juce::Colour(0xFFFF007F));

    autoToggle.setButtonText("AUTO");
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFF007F)); 
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

void HomeDistoAudioProcessorEditor::drawShadedCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour)
{
    for (int i = 1; i <= 7; ++i)
    {
        g.setColour(juce::Colours::black.withAlpha(0.12f * (8 - i)));
        g.fillRoundedRectangle(bounds.expanded((float)i * 0.4f).translated(0.0f, 2.0f + i * 0.5f), 8.0f);
    }

    juce::ColourGradient grad(baseColour, bounds.getX(), bounds.getY(),
                               baseColour.darker(0.20f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colours::black.withAlpha(0.12f));
    for (float y = bounds.getY() + 4.0f; y < bounds.getBottom() - 2.0f; y += 4.0f)
        g.drawLine(bounds.getX() + 2.0f, y, bounds.getRight() - 2.0f, y, 1.2f);
    for (float x = bounds.getX() + 4.0f; x < bounds.getRight() - 2.0f; x += 4.0f)
        g.drawLine(x, bounds.getY() + 2.0f, x, bounds.getBottom() - 2.0f, 1.2f);

    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.drawRoundedRectangle(bounds, 6.0f, 2.0f);
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour(0xFF09090B));

    g.setColour(juce::Colour(0xFF111114));
    g.fillRoundedRectangle(10, 10, 700, 390, 8); 
    g.setColour(juce::Colour(0xFF222228));
    g.drawRoundedRectangle(10, 10, 700, 390, 8, 1.5f);

    // FIX: Using getStringWidthFloat to calculate font width for modern JUCE compatibility
    g.setFont(juce::FontOptions(22.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colours::white);
    int homeWidth = juce::roundToInt(juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), "HOME : "));
    g.drawText("HOME : ", 25, 20, homeWidth, 30, juce::Justification::centredLeft);
    
    g.setColour(juce::Colour(0xFF00E5FF)); 
    g.drawText("DISTO", 25 + homeWidth, 20, 80, 30, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xFF1E1E24));
    g.drawLine(25, 65, 695, 65, 2.0f); 

    drawShadedCard(g, juce::Rectangle<float>(20, 80, 230, 175), juce::Colour(0xFF00E5FF));
    drawShadedCard(g, juce::Rectangle<float>(20, 265, 230, 125), juce::Colour(0xFF00FF87));
    drawShadedCard(g, juce::Rectangle<float>(260, 80, 260, 310), juce::Colour(0xFFB900FF));
    drawShadedCard(g, juce::Rectangle<float>(530, 80, 170, 310), juce::Colour(0xFFFF007F));

    auto drawCardText = [&](const juce::String& text, int x, int y, int w, int h, juce::Justification just) {
        g.setColour(juce::Colours::black.withAlpha(0.35f));    
        g.drawText(text, x, y + 1.5f, w, h, just);
        g.setColour(juce::Colour(0xFF09090B));                 
        g.drawText(text, x, y, w, h, just);
    };

    g.setFont(juce::FontOptions(14.0f).withName("Helvetica").withStyle("Bold"));

    drawCardText("MODE", 20, 90, 230, 20, juce::Justification::centred);
    drawCardText("FILTER", 20, 273, 230, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(11.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("LOW CUT", 35, 295, 80, 15, juce::Justification::left);
    drawCardText("HIGH CUT", 155, 295, 80, 15, juce::Justification::right);
    
    drawCardText(getFrequencyString(lowCutSlider.getValue()), 35, 310, 80, 15, juce::Justification::left); 
    drawCardText(getFrequencyString(highCutSlider.getValue()), 155, 310, 80, 15, juce::Justification::right); 

    float lowProp = lowCutSlider.valueToProportionOfLength(lowCutSlider.getValue());
    float lowX = lowCutSlider.getX() + (lowProp * lowCutSlider.getWidth());
    float highProp = highCutSlider.valueToProportionOfLength(highCutSlider.getValue());
    float highX = highCutSlider.getX() + (highProp * highCutSlider.getWidth());
    float graphY = 345.0f;

    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.drawLine(35, graphY, 235, graphY, 2.0f); 

    juce::Path filterCurve; 
    filterCurve.startNewSubPath(35, 375);
    filterCurve.cubicTo(lowX - 6, 375, lowX - 6, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 6, graphY, highX + 6, 375, 235, 375);
    
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.strokePath(filterCurve, juce::PathStrokeType(3.5f), juce::AffineTransform::translation(0, 1.5f));
    g.setColour(juce::Colour(0xFF09090B));
    g.strokePath(filterCurve, juce::PathStrokeType(3.0f)); 

    g.setFont(juce::FontOptions(14.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("DRIVE", 260, 90, 260, 20, juce::Justification::centred); 
    drawCardText("TONE", 285, 260, 80, 20, juce::Justification::centred);
    drawCardText("PUNCH", 415, 260, 80, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("DARK",  275, 360, 40, 15, juce::Justification::left);
    drawCardText("BRIGHT", 335, 360, 40, 15, juce::Justification::right);
    drawCardText("SOFT",  405, 360, 40, 15, juce::Justification::left);
    drawCardText("HARD",   465, 360, 40, 15, juce::Justification::right);

    g.setFont(juce::FontOptions(14.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("OUTPUT", 545, 90, 70, 20, juce::Justification::left);
    drawCardText("MIX", 580, 260, 70, 20, juce::Justification::centred); 

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("DRY", 565, 360, 30, 15, juce::Justification::left);
    drawCardText("WET", 635, 360, 30, 15, juce::Justification::right);
}

void HomeDistoAudioProcessorEditor::resized()
{
    presetCombo.setBounds(210, 20, 300, 30);
    
    // Positioned the 3 buttons with exact 40px symmetric gaps between them
    saveButton.setBounds(520, 20, 30, 30); 
    bypassButton.setBounds(590, 20, 30, 30);
    settingsButton.setBounds(660, 20, 30, 30); 

    // MODES: Adjusted grid for chunkier buttons (Height 32, wider spacing)
    for (int i = 0; i < 6; ++i) 
    {
        int col = i % 2; 
        int row = i / 2; 
        modeButtons[i].setBounds(35 + (col * 105), 115 + (row * 44), 95, 32);
    }

    lowCutSlider.setBounds(30, 325, 90, 30);  
    highCutSlider.setBounds(150, 325, 90, 30); 

    driveKnob.setBounds(315, 115, 150, 150); 
    toneKnob.setBounds(290, 285, 70, 70);
    punchKnob.setBounds(420, 285, 70, 70);

    outputKnob.setBounds(555, 125, 120, 120); 
    
    // Pixel-perfect alignment: Uses exact same Y axis (90) and Height (20) as the "OUTPUT" label
    autoToggle.setBounds(615, 90, 75, 20); 
    
    mixKnob.setBounds(580, 290, 70, 70);
}