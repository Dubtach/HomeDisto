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
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFFFFFF));
    autoToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFF888888)); 
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

// Helper to render rich shaded cards with soft drop shadows, inner bevels, and premium textures
void HomeDistoAudioProcessorEditor::drawShadedCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour)
{
    // 1. Enhanced 3D Drop Shadows
    for (int i = 1; i <= 8; ++i)
    {
        g.setColour(juce::Colours::black.withAlpha(0.08f * (9 - i)));
        g.fillRoundedRectangle(bounds.expanded((float)i * 0.5f).translated(0.0f, 2.0f + i * 0.6f), 10.0f);
    }

    // 2. Rich Vertical Gradient (Base is brightened per requirements)
    juce::ColourGradient grad(baseColour.brighter(0.2f), bounds.getX(), bounds.getY(),
                               baseColour.darker(0.15f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, 8.0f);

    // 3. Premium Grid/Mesh Texture Overlay
    // Draws a subtle intersecting pattern to emulate hardware casing
    g.setColour(juce::Colours::black.withAlpha(0.05f));
    for (float y = bounds.getY() + 4.0f; y < bounds.getBottom() - 2.0f; y += 4.0f)
        g.drawLine(bounds.getX() + 2.0f, y, bounds.getRight() - 2.0f, y, 1.5f);
    for (float x = bounds.getX() + 4.0f; x < bounds.getRight() - 2.0f; x += 4.0f)
        g.drawLine(x, bounds.getY() + 2.0f, x, bounds.getBottom() - 2.0f, 1.5f);

    // 4. Subtle Inner Highlight Line (Top Bevel for 3D edge catch)
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.5f);

    // 5. Dark Outer Border Line
    g.setColour(juce::Colours::black.withAlpha(0.40f));
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
    // DISTINCT SHADED CARDS WITH BRIGHTER COLORS
    // ==========================================
    
    // Card 1: MODES Section (Brighter Green)
    drawShadedCard(g, juce::Rectangle<float>(20, 80, 230, 175), juce::Colour(0xFF4A9E48));

    // Card 2: FILTER Section (Brighter Amber/Gold)
    drawShadedCard(g, juce::Rectangle<float>(20, 265, 230, 125), juce::Colour(0xFFFFD54F));

    // Card 3: DRIVE / TONE / PUNCH Section (Brighter Burnt Orange)
    drawShadedCard(g, juce::Rectangle<float>(260, 80, 260, 310), juce::Colour(0xFFFF8F66));

    // Card 4: OUTPUT / MIX Section (Brighter Red)
    drawShadedCard(g, juce::Rectangle<float>(530, 80, 170, 310), juce::Colour(0xFFD93838));

    // ==========================================
    // TEXT LABELS & OVERLAYS (Engraved Dark Text)
    // ==========================================
    
    // Helper lambda to draw dark text with a white drop shadow (Engraved 3D look)
    auto drawEngravedText = [&](const juce::String& text, int x, int y, int w, int h, juce::Justification just, float alpha = 0.85f) {
        g.setColour(juce::Colours::white.withAlpha(0.4f));     // Bottom highlight
        g.drawText(text, x, y + 1, w, h, just);
        g.setColour(juce::Colours::black.withAlpha(alpha));    // Dark foreground
        g.drawText(text, x, y, w, h, just);
    };

    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));

    // --- CARD 1: MODES ---
    drawEngravedText("MODE", 20, 90, 230, 20, juce::Justification::centred);

    // --- CARD 2: FILTER ---
    drawEngravedText("FILTER", 20, 273, 230, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    drawEngravedText("LOW CUT", 35, 295, 80, 15, juce::Justification::left, 0.75f);
    drawEngravedText("HIGH CUT", 155, 295, 80, 15, juce::Justification::right, 0.75f);
    
    drawEngravedText(getFrequencyString(lowCutSlider.getValue()), 35, 310, 80, 15, juce::Justification::left, 0.9f); 
    drawEngravedText(getFrequencyString(highCutSlider.getValue()), 155, 310, 80, 15, juce::Justification::right, 0.9f); 

    // Mini filter graph aligned with sliders
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
    
    // Graph line with Engraved Shadow
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f), juce::AffineTransform::translation(0, 1.0f));
    g.setColour(juce::Colour(0xFF202020));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f)); 

    // --- CARD 3: DRIVE, TONE, PUNCH ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    drawEngravedText("DRIVE", 260, 90, 260, 20, juce::Justification::centred); 
    drawEngravedText("TONE", 285, 260, 80, 20, juce::Justification::centred);
    drawEngravedText("PUNCH", 415, 260, 80, 20, juce::Justification::centred);

    // Descriptors
    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    drawEngravedText("DARK",  275, 360, 40, 15, juce::Justification::left, 0.65f);
    drawEngravedText("BRIGHT", 335, 360, 40, 15, juce::Justification::right, 0.65f);
    drawEngravedText("SOFT",  405, 360, 40, 15, juce::Justification::left, 0.65f);
    drawEngravedText("HARD",   465, 360, 40, 15, juce::Justification::right, 0.65f);

    // --- CARD 4: OUTPUT, MIX ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    drawEngravedText("OUTPUT", 545, 90, 70, 20, juce::Justification::left);
    drawEngravedText("MIX", 580, 260, 70, 20, juce::Justification::centred); 

    // Descriptors
    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    drawEngravedText("DRY", 565, 360, 30, 15, juce::Justification::left, 0.65f);
    drawEngravedText("WET", 635, 360, 30, 15, juce::Justification::right, 0.65f);
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