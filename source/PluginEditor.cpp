#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 600);
    setLookAndFeel(&flatLaf);

    auto setupKnob = [this](juce::Slider& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attach) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, slider);
    };

    setupKnob(driveKnob, "DRIVE", driveAttach);
    setupKnob(topOutKnob, "OUT", outAttach); 
    setupKnob(toneKnob, "TONE", toneAttach);
    setupKnob(punchKnob, "PUNCH", punchAttach);
    setupKnob(mixKnob, "MIX", mixAttach);

    autoToggle.setButtonText("AUTO");
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFA855F7));
    addAndMakeVisible(autoToggle);
    autoAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "AUTO", autoToggle);

    modeCombo.addItemList({"PUNCH", "TUBE", "TAPE", "DIGITAL"}, 1);
    modeCombo.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modeCombo);
    modeAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "MODE", modeCombo);
}

HomeDistoAudioProcessorEditor::~HomeDistoAudioProcessorEditor()
{
    setLookAndFeel(nullptr); 
}

void HomeDistoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Flat Dark Background
    g.fillAll (juce::Colour(0xFF101012)); 

    // Inner Plugin Bezel (Solid Flat Shape)
    g.setColour(juce::Colour(0xFF18181A));
    g.fillRoundedRectangle(20, 20, 760, 560, 16);
    g.setColour(juce::Colour(0xFF202024));
    g.drawRoundedRectangle(20, 20, 760, 560, 16, 2.0f);

    // --- Header Section ---
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(28.0f).withStyle("Bold"));
    g.drawText("GOD", 50, 40, 150, 30, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xFFA855F7));
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText("DISTORTION", 50, 70, 150, 20, juce::Justification::centredLeft);

    // Preset Bar Placeholder
    g.setColour(juce::Colour(0xFF101012));
    g.fillRoundedRectangle(280, 40, 240, 40, 6);
    g.setColour(juce::Colours::white);
    g.drawText("<    Huge Punch    >", 280, 40, 240, 40, juce::Justification::centred);

    // --- Labels ---
    g.setColour(juce::Colour(0xFFAAAAAA));
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("MODE", 50, 120, 180, 20, juce::Justification::centred);
    g.drawText("DRIVE", 300, 120, 200, 20, juce::Justification::centred);
    g.drawText("MIX", 580, 120, 150, 20, juce::Justification::centred); // Top knob

    // Lower Panel Areas (Solid flat grouping boxes)
    g.setColour(juce::Colour(0xFF101012));
    g.fillRoundedRectangle(40, 340, 340, 160, 8); // Filter Box
    
    // Filter Graph Line (2D Graphic)
    g.setColour(juce::Colour(0xFF2A2A30));
    g.drawLine(60, 450, 360, 450, 2.0f);
    juce::Path filterCurve;
    filterCurve.startNewSubPath(60, 490);
    filterCurve.cubicTo(120, 490, 100, 410, 160, 410);
    filterCurve.lineTo(260, 410);
    filterCurve.cubicTo(320, 410, 300, 490, 360, 490);
    g.setColour(juce::Colour(0xFFA855F7));
    g.strokePath(filterCurve, juce::PathStrokeType(2.0f));

    g.setColour(juce::Colour(0xFFAAAAAA));
    g.drawText("FILTER", 40, 350, 340, 20, juce::Justification::centred);
    g.setColour(juce::Colour(0xFFA855F7));
    g.drawText("LOW CUT", 60, 380, 80, 20, juce::Justification::left);
    g.drawText("HIGH CUT", 280, 380, 80, 20, juce::Justification::right);
    g.setColour(juce::Colours::white);
    g.drawText("120 Hz", 60, 400, 80, 20, juce::Justification::left);
    g.drawText("8.5 kHz", 280, 400, 80, 20, juce::Justification::right);

    // Bottom Right Knobs Labels
    g.setColour(juce::Colour(0xFFAAAAAA));
    g.drawText("TONE",  430, 360, 80, 20, juce::Justification::centred);
    g.drawText("PUNCH", 540, 360, 80, 20, juce::Justification::centred);
    g.drawText("MIX",   650, 360, 80, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f));
    g.drawText("DARK       BRIGHT", 430, 470, 80, 20, juce::Justification::centred);
    g.drawText("SOFT       HARD", 540, 470, 80, 20, juce::Justification::centred);
    g.drawText("DRY       WET", 650, 470, 80, 20, juce::Justification::centred);

    // --- Footer ---
    g.setColour(juce::Colour(0xFF444444));
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText("SERIOUS DISTORTION. ZERO COMPLEXITY.", 200, 530, 400, 20, juce::Justification::centred);
    g.drawText("ASONG AUDIO", 600, 530, 150, 20, juce::Justification::centredRight);
}

void HomeDistoAudioProcessorEditor::resized()
{
    modeCombo.setBounds(50, 150, 180, 40);
    
    // Main Drive
    driveKnob.setBounds(300, 150, 200, 200);

    // Top Right
    topOutKnob.setBounds(610, 150, 90, 90);
    autoToggle.setBounds(610, 260, 90, 30);

    // Bottom Right Knobs
    toneKnob.setBounds(430, 390, 80, 80);
    punchKnob.setBounds(540, 390, 80, 80);
    mixKnob.setBounds(650, 390, 80, 80);
}