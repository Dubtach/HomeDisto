#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (700, 480); // Scaled down canvas
    setLookAndFeel(&flatLaf);

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

    // Setup Interactive Filter Graph Sliders
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
    // Flat Dark Background
    g.fillAll (juce::Colour(0xFF101012)); 

    // Inner Plugin Bezel
    g.setColour(juce::Colour(0xFF141416));
    g.fillRoundedRectangle(15, 15, 670, 450, 12);
    g.setColour(juce::Colour(0xFF1C1C20));
    g.drawRoundedRectangle(15, 15, 670, 450, 12, 1.5f);

    // --- Header Title ---
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(26.0f).withStyle("Bold"));
    g.drawText("GOD", 30, 25, 120, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xFFA855F7));
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText("DISTORTION", 30, 50, 120, 20, juce::Justification::centredLeft);

    // --- Main Labels ---
    g.setColour(juce::Colour(0xFF777777));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("MODE", 40, 95, 100, 20, juce::Justification::centredLeft);
    g.drawText("DRIVE", 270, 80, 160, 20, juce::Justification::centred);
    g.drawText("OUTPUT", 520, 90, 80, 20, juce::Justification::centred); // Output Label

    // --- Filter Box Panel ---
    g.setColour(juce::Colour(0xFF101012));
    g.fillRoundedRectangle(30, 290, 290, 130, 8); 
    
    // Dynamic Filter Text Readouts
    g.setColour(juce::Colour(0xFF777777));
    g.drawText("FILTER", 30, 300, 290, 20, juce::Justification::centred);
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("LOW CUT", 45, 330, 80, 20, juce::Justification::left);
    g.drawText("HIGH CUT", 225, 330, 80, 20, juce::Justification::right);
    g.setColour(juce::Colours::white);
    g.drawText(getFrequencyString(lowCutSlider.getValue()), 45, 350, 80, 20, juce::Justification::left);
    g.drawText(getFrequencyString(highCutSlider.getValue()), 225, 350, 80, 20, juce::Justification::right);

    // Filter Graph Math
    float lowX = lowCutSlider.getX() + (lowCutSlider.getWidth() * ((lowCutSlider.getValue() - 20.0f) / 980.0f));
    float highX = highCutSlider.getX() + (highCutSlider.getWidth() * ((highCutSlider.getValue() - 1000.0f) / 19000.0f));
    float graphY = 390.0f; 

    // Draw Graph Background Line
    g.setColour(juce::Colour(0xFF2A2A30));
    g.drawLine(45, graphY, 305, graphY, 2.0f);

    // Draw Dynamic Purple Curve
    juce::Path filterCurve;
    filterCurve.startNewSubPath(45, 420);
    filterCurve.cubicTo(lowX - 10, 420, lowX - 10, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + 10, graphY, highX + 10, 420, 305, 420);
    
    g.setColour(juce::Colour(0xFFA855F7));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f));
    g.fillPath(filterCurve, juce::AffineTransform().translated(0,0)); // Faint fill if desired: g.setColour(juce::Colour(0x33A855F7)); g.fillPath(filterCurve);

    // --- Bottom Right Labels ---
    g.setColour(juce::Colour(0xFF777777));
    g.drawText("TONE",  360, 310, 70, 20, juce::Justification::centred);
    g.drawText("PUNCH", 460, 310, 70, 20, juce::Justification::centred);
    g.drawText("MIX",   560, 310, 70, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f));
    g.drawText("DARK     BRIGHT", 360, 400, 70, 20, juce::Justification::centred);
    g.drawText("SOFT     HARD",   460, 400, 70, 20, juce::Justification::centred);
    g.drawText("DRY     WET",     560, 400, 70, 20, juce::Justification::centred);
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Mode List (Vertical stack)
    for (int i = 0; i < 4; ++i)
        modeButtons[i].setBounds(40, 120 + (i * 35), 110, 30);

    // Main Drive
    driveKnob.setBounds(270, 110, 160, 160);

    // Top Right Output
    outputKnob.setBounds(520, 115, 80, 80);
    autoToggle.setBounds(520, 200, 80, 30);

    // Interactive Filter Sliders (Placed over the visual graph)
    lowCutSlider.setBounds(45, 370, 120, 40);  // Left half of graph
    highCutSlider.setBounds(185, 370, 120, 40); // Right half of graph

    // Bottom Right
    toneKnob.setBounds(360, 335, 70, 70);
    punchKnob.setBounds(460, 335, 70, 70);
    mixKnob.setBounds(560, 335, 70, 70);
}