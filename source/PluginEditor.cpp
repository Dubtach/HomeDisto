#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Helvetica");

    setSize (720, 410);
    setLookAndFeel(&flatLaf); //[cite: 4]

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

    setupKnob(driveKnob, "DRIVE", driveAttach); //[cite: 4]
    setupKnob(outputKnob, "OUT", outAttach); 
    setupKnob(toneKnob, "TONE", toneAttach);
    setupKnob(punchKnob, "PUNCH", punchAttach);
    setupKnob(mixKnob, "MIX", mixAttach);

    autoToggle.setButtonText("AUTO");
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFFFFFF));
    autoToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFF888888)); 
    addAndMakeVisible(autoToggle);
    autoAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "AUTO", autoToggle);

    for (int i = 0; i < 6; ++i) //[cite: 4]
    {
        modeButtons[i].setButtonText(modeNames[i]);
        modeButtons[i].setRadioGroupId(100);
        modeButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(modeButtons[i]);
        
        modeButtons[i].onClick = [this, i] {
            audioProcessor.apvts.getParameter("MODE")->setValueNotifyingHost(i / 5.0f);
        };
    }
    
    audioProcessor.apvts.addParameterListener("MODE", this); //[cite: 4]
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
    audioProcessor.apvts.removeParameterListener("MODE", this); //[cite: 4]
    setLookAndFeel(nullptr); 
}

void HomeDistoAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "MODE") //[cite: 4]
    {
        juce::MessageManager::callAsync([this, newValue]() {
            modeButtons[(int)newValue].setToggleState(true, juce::dontSendNotification);
        });
    }
}

juce::String HomeDistoAudioProcessorEditor::getFrequencyString(float hz)
{
    if (hz >= 1000.0f) //[cite: 4]
        return juce::String(hz / 1000.0f, 1) + " kHz";
    return juce::String(juce::roundToInt(hz)) + " Hz";
}

// Helper to render rich shaded cards with soft drop shadows, textures, and inner bevels
void HomeDistoAudioProcessorEditor::drawShadedCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour)
{
    // 1. Soft Outer Drop Shadows[cite: 4]
    for (int i = 1; i <= 6; ++i)
    {
        g.setColour(juce::Colours::black.withAlpha(0.06f * (7 - i)));
        g.fillRoundedRectangle(bounds.expanded((float)i * 0.8f).translated(0.0f, 1.5f + i * 0.5f), 10.0f);
    }

    // 2. Rich Vertical Gradient (Shaded from light top to dark base)[cite: 4]
    juce::ColourGradient grad(baseColour.brighter(0.15f), bounds.getX(), bounds.getY(),
                               baseColour.darker(0.35f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, 8.0f);

    // 3. Premium Texture (Subtle Diagonal Lines)
    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());
    g.setColour(juce::Colours::black.withAlpha(0.06f));
    for (float i = -bounds.getHeight(); i < bounds.getWidth() + bounds.getHeight(); i += 6.0f)
    {
        g.drawLine(bounds.getX() + i, bounds.getY(), bounds.getX() + i - bounds.getHeight(), bounds.getBottom(), 1.5f);
    }
    g.restoreState();

    // 4. Subtle Inner Highlight Line (Top Bevel)[cite: 4]
    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // 5. Dark Outer Border Line[cite: 4]
    g.setColour(juce::Colours::black.withAlpha(0.40f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Deep dark chassis background[cite: 4]
    g.fillAll (juce::Colour(0xFF09090B)); 

    // Main Housing Panel
    g.setColour(juce::Colour(0xFF111114));
    g.fillRoundedRectangle(10, 10, 700, 390, 8); //[cite: 4]
    g.setColour(juce::Colour(0xFF222228));
    g.drawRoundedRectangle(10, 10, 700, 390, 8, 1.5f);

    // Title Block[cite: 4]
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f).withName("Helvetica").withStyle("Bold"));
    g.drawText("HOME :", 25, 20, 80, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText("DISTO", 105, 20, 80, 30, juce::Justification::centredLeft);

    // Divider Line[cite: 4]
    g.setColour(juce::Colour(0xFF1E1E24));
    g.drawLine(25, 65, 695, 65, 2.0f); 

    // ==========================================
    // DISTINCT SHADED CARDS WITH PREMIUM HEX COLORS
    // ==========================================
    
    // Card 1: MODES Section (Deep Slate Blue)
    drawShadedCard(g, juce::Rectangle<float>(20, 80, 230, 175), juce::Colour(0xFF151821));

    // Card 2: FILTER Section (#FFC107 - Warm Amber/Gold)[cite: 4]
    drawShadedCard(g, juce::Rectangle<float>(20, 265, 230, 125), juce::Colour(0xFFFFC107));

    // Card 3: DRIVE / TONE / PUNCH Section (Deep Graphite)
    drawShadedCard(g, juce::Rectangle<float>(260, 80, 260, 310), juce::Colour(0xFF1E1E22));

    // Card 4: OUTPUT / MIX Section (Deep Burgundy)
    drawShadedCard(g, juce::Rectangle<float>(530, 80, 170, 310), juce::Colour(0xFF261418));


    // ==========================================
    // TEXT LABELS & OVERLAYS 
    // ==========================================
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));

    // --- CARD 1: MODES ---
    // Using bright off-white text to contrast perfectly with the dark slate background
    g.setColour(juce::Colour(0xFFE5E5EA));
    g.drawText("MODE", 20, 90, 230, 20, juce::Justification::centred);

    // --- CARD 2: FILTER ---
    // Darker text remains for the bright #FFC107 background to ensure legibility[cite: 4]
    g.setColour(juce::Colour(0xFF2A1C00)); 
    g.drawText("FILTER", 20, 273, 230, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    g.drawText("LOW CUT", 35, 295, 80, 15, juce::Justification::left);
    g.drawText("HIGH CUT", 155, 295, 80, 15, juce::Justification::right);
    g.setColour(juce::Colour(0xFF4A3400));
    g.drawText(getFrequencyString(lowCutSlider.getValue()), 35, 310, 80, 15, juce::Justification::left); //[cite: 4]
    g.drawText(getFrequencyString(highCutSlider.getValue()), 155, 310, 80, 15, juce::Justification::right); //[cite: 4]

    // Mini filter graph aligned with sliders[cite: 4]
    float lowProp = lowCutSlider.valueToProportionOfLength(lowCutSlider.getValue());
    float lowX = lowCutSlider.getX() + (lowProp * lowCutSlider.getWidth());
    float highProp = highCutSlider.valueToProportionOfLength(highCutSlider.getValue());
    float highX = highCutSlider.getX() + (highProp * highCutSlider.getWidth());
    float graphY = 345.0f;

    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.drawLine(35, graphY, 235, graphY, 2.0f); 

    juce::Path filterCurve; //[cite: 4]
    filterCurve.startNewSubPath(35, 375);
    filterCurve.cubicTo(lowX - 6, 375, lowX - 6, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 6, graphY, highX + 6, 375, 235, 375);
    g.setColour(juce::Colour(0xFF4A3400));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f)); //[cite: 4]

    // --- CARD 3: DRIVE, TONE, PUNCH ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    // Using bright off-white text to contrast perfectly with the dark graphite background
    g.setColour(juce::Colour(0xFFE5E5EA));
    g.drawText("DRIVE", 260, 90, 260, 20, juce::Justification::centred); //[cite: 4]
    g.drawText("TONE", 285, 260, 80, 20, juce::Justification::centred);
    g.drawText("PUNCH", 415, 260, 80, 20, juce::Justification::centred);

    // Descriptors[cite: 4]
    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFFA5A5B0));
    g.drawText("DARK",  275, 360, 40, 15, juce::Justification::left);
    g.drawText("BRIGHT", 335, 360, 40, 15, juce::Justification::right);
    g.drawText("SOFT",  405, 360, 40, 15, juce::Justification::left);
    g.drawText("HARD",   465, 360, 40, 15, juce::Justification::right);

    // --- CARD 4: OUTPUT, MIX ---
    g.setFont(juce::FontOptions(13.0f).withName("Helvetica").withStyle("Bold"));
    // Using bright off-white text to contrast perfectly with the deep burgundy background
    g.setColour(juce::Colour(0xFFE5E5EA));
    g.drawText("OUTPUT", 545, 90, 70, 20, juce::Justification::left);
    g.drawText("MIX", 580, 260, 70, 20, juce::Justification::centred); //[cite: 4]

    // Descriptors[cite: 4]
    g.setFont(juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colour(0xFFA5A5B0));
    g.drawText("DRY", 565, 360, 30, 15, juce::Justification::left);
    g.drawText("WET", 635, 360, 30, 15, juce::Justification::right);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Header[cite: 4]
    presetCombo.setBounds(210, 20, 300, 30);
    saveButton.setBounds(620, 20, 30, 30); 
    settingsButton.setBounds(660, 20, 30, 30); 

    // --- CARD 1: MODES ---
    for (int i = 0; i < 6; ++i) //[cite: 4]
    {
        int col = i % 2; 
        int row = i / 2; 
        modeButtons[i].setBounds(35 + (col * 105), 118 + (row * 42), 95, 26);
    }

    // --- CARD 2: FILTER ---
    lowCutSlider.setBounds(30, 325, 90, 30);  //[cite: 4]
    highCutSlider.setBounds(150, 325, 90, 30); 

    // --- CARD 3: DRIVE, TONE & PUNCH ---
    driveKnob.setBounds(315, 115, 150, 150); //[cite: 4]
    toneKnob.setBounds(290, 285, 70, 70);
    punchKnob.setBounds(420, 285, 70, 70);

    // --- CARD 4: OUTPUT & MIX ---
    outputKnob.setBounds(555, 125, 120, 120); //[cite: 4]
    autoToggle.setBounds(620, 90, 65, 20); 
    mixKnob.setBounds(580, 290, 70, 70);
}