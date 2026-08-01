#include "PluginProcessor.h"
#include "PluginEditor.h"

// NEW: settings popup content. Owned by the CallOutBox that shows it (see
// showSettingsMenu below), so its parameter attachments just need to live as
// long as this component does.
class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel(HomeDistoAudioProcessor& proc)
    {
        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

        hqToggle.setButtonText("HQ Mode (4x Oversampling)");
        hqToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00E5FF));
        addAndMakeVisible(hqToggle);
        hqAttach = std::make_unique<ButtonAttachment>(proc.apvts, "HQ", hqToggle);

        smoothLabel.setText("Smooth (De-Fizz)", juce::dontSendNotification);
        smoothLabel.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        smoothLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
        addAndMakeVisible(smoothLabel);

        smoothSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        smoothSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 20);
        addAndMakeVisible(smoothSlider);
        smoothAttach = std::make_unique<SliderAttachment>(proc.apvts, "SMOOTH", smoothSlider);

        // NOTE: Filter Slope now lives directly in the Filter card on the
        // main interface (12/24/48 buttons next to LOW CUT/HIGH CUT) rather
        // than here -- it's a control people want quick access to while
        // shaping the band, not a set-and-forget setting. No duplicate
        // control here on purpose (see also: why AUTO isn't here either).

        // NOTE: no Auto-Gain control here on purpose -- it already has its
        // own toggle on the main interface (next to the knobs), so putting
        // it here too would just be a confusing duplicate control for the
        // same parameter.

        openPresetsButton.setButtonText("Open Presets Folder");
        openPresetsButton.onClick = [&proc] { proc.getPresetDirectory().revealToUser(); };
        addAndMakeVisible(openPresetsButton);

        setSize(260, 122);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF161618));
        g.setColour(juce::Colour(0xFF2A2A30));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(12);
        hqToggle.setBounds(b.removeFromTop(22));
        b.removeFromTop(10);
        smoothLabel.setBounds(b.removeFromTop(16));
        smoothSlider.setBounds(b.removeFromTop(24));
        b.removeFromTop(10);

        openPresetsButton.setBounds(b.removeFromTop(24));
    }

private:
    juce::ToggleButton hqToggle;
    juce::Label smoothLabel;
    juce::Slider smoothSlider;
    juce::TextButton openPresetsButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothAttach;
};

HomeDistoAudioProcessorEditor::HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName("Helvetica");

    setSize (720, 410);
    setLookAndFeel(&synthLaf); 

    // Replaced combo box with distinct functional buttons
    presetMenuButton.setButtonText("Select Preset...");
    presetMenuButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetMenuButton);

    presetUpButton.setName("PRESET_UP"); // <-- FIX: Use standard name instead of Unicode text
    presetUpButton.onClick = [this] { audioProcessor.prevPreset(); updatePresetName(); };
    addAndMakeVisible(presetUpButton);

    presetDownButton.setName("PRESET_DOWN"); // <-- FIX: Use standard name instead of Unicode text
    presetDownButton.onClick = [this] { audioProcessor.nextPreset(); updatePresetName(); };
    addAndMakeVisible(presetDownButton);

    updatePresetName(); // Load active name state right away

    saveButton.setName("SAVE");
    addAndMakeVisible(saveButton);
    
    saveButton.onClick = [this] {
        auto alert = std::make_unique<juce::AlertWindow>("Save Preset", "Enter a name for the preset:", juce::AlertWindow::NoIcon);
        alert->addTextEditor("presetName", "New Preset", "Preset Name:");
        alert->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));

        auto* rawAlert = alert.release();
        rawAlert->enterModalState(true, juce::ModalCallbackFunction::create([this, rawAlert](int result) {
            if (result == 1) 
            {
                juce::String presetName = rawAlert->getTextEditorContents("presetName");
                if (presetName.isNotEmpty()) 
                {
                    audioProcessor.savePreset(presetName);
                    updatePresetName(); 
                }
            }
            delete rawAlert;
        }));
    };
    
    bypassButton.setName("BYPASS");
    bypassButton.setClickingTogglesState(true);
    addAndMakeVisible(bypassButton);
    bypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "BYPASS", bypassButton);
    bypassButton.onClick = [this] { repaint(); };
    
    // FIX: previously this button was drawn and clickable but had no
    // onClick handler at all -- clicking it did nothing. It now opens a
    // proper settings popup (HQ mode, de-fizz smoothing, auto-gain, and a
    // shortcut to the presets folder) instead of being a single hardwired
    // toggle.
    settingsButton.setName("SETTINGS");
    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this] {
        auto panel = std::make_unique<SettingsPanel>(audioProcessor);
        auto bounds = settingsButton.getScreenBounds();
        juce::CallOutBox::launchAsynchronously(std::move(panel), bounds, nullptr);
    };

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

    // NEW: lock icons -- when locked, changing presets leaves this knob's
    // value alone instead of jumping to whatever the preset stored.
    outputLockButton.setName("LOCK");
    outputLockButton.setClickingTogglesState(true);
    outputLockButton.setTooltip("Lock OUTPUT: presets won't change this value");
    addAndMakeVisible(outputLockButton);
    outputLockButton.onClick = [this] { audioProcessor.lockOutput.store(outputLockButton.getToggleState()); };

    mixLockButton.setName("LOCK");
    mixLockButton.setClickingTogglesState(true);
    mixLockButton.setTooltip("Lock MIX: presets won't change this value");
    addAndMakeVisible(mixLockButton);
    mixLockButton.onClick = [this] { audioProcessor.lockMix.store(mixLockButton.getToggleState()); };

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

    // NEW: filter slope buttons (12/24/48 dB/oct), right in the Filter card.
    for (int i = 0; i < 3; ++i)
    {
        slopeButtons[i].setButtonText(slopeButtonLabels[i]);
        slopeButtons[i].setRadioGroupId(101);
        slopeButtons[i].setClickingTogglesState(true);
        addAndMakeVisible(slopeButtons[i]);

        slopeButtons[i].onClick = [this, i] {
            audioProcessor.apvts.getParameter("SLOPE")->setValueNotifyingHost(i / 2.0f);
            repaint(); // the visual curve steepness depends on slope too
        };
    }

    audioProcessor.apvts.addParameterListener("SLOPE", this);
    int initialSlope = (int) audioProcessor.apvts.getRawParameterValue("SLOPE")->load();
    slopeButtons[juce::jlimit(0, 2, initialSlope)].setToggleState(true, juce::dontSendNotification);
}

HomeDistoAudioProcessorEditor::~HomeDistoAudioProcessorEditor()
{
    audioProcessor.apvts.removeParameterListener("MODE", this);
    audioProcessor.apvts.removeParameterListener("SLOPE", this);
    setLookAndFeel(nullptr); 
}

void HomeDistoAudioProcessorEditor::updatePresetName()
{
    if (audioProcessor.currentPresetFile.existsAsFile())
        presetMenuButton.setButtonText(audioProcessor.currentPresetFile.getFileNameWithoutExtension());
    else
        presetMenuButton.setButtonText("Select Preset...");
}

void HomeDistoAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    auto categories = audioProcessor.getAllPresetsCategorized();

    for (auto& pair : categories)
    {
        juce::PopupMenu categoryMenu;
        for (auto& file : pair.second)
        {
            categoryMenu.addItem(file.getFileNameWithoutExtension(), [this, file]() {
                audioProcessor.loadPreset(file);
                updatePresetName();
            });
        }
        // Submenu label corresponds perfectly to folder name
        menu.addSubMenu(pair.first, categoryMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetMenuButton));
}

void HomeDistoAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "MODE")
    {
        juce::MessageManager::callAsync([this, newValue]() {
            modeButtons[(int)newValue].setToggleState(true, juce::dontSendNotification);
        });
    }
    else if (parameterID == "SLOPE")
    {
        juce::MessageManager::callAsync([this, newValue]() {
            int idx = juce::jlimit(0, 2, (int) newValue);
            slopeButtons[idx].setToggleState(true, juce::dontSendNotification);
            repaint(); // visual curve steepness depends on slope
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

    g.setFont(juce::FontOptions(22.0f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colours::white);
    
    int homeWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "HOME : ");
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
    // FIX: left-aligned and narrowed (was centred across the full card
    // width) to make room for the 12/24/48 slope buttons in the same row.
    drawCardText("FILTER", 32, 273, 80, 20, juce::Justification::left);

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

    // NEW: corner sharpness now actually reflects the selected filter
    // slope instead of being a fixed, meaningless curve. Gentler roll-off
    // (12 dB/oct) draws a wide, soft corner; steeper slopes (24/48 dB/oct)
    // draw a progressively tighter, more abrupt corner -- visually a
    // steeper wall, matching what the filter is actually doing.
    int slopeIdx = juce::jlimit(0, 2, (int) audioProcessor.apvts.getRawParameterValue("SLOPE")->load());
    float cornerOffset = slopeIdx == 0 ? 10.0f : (slopeIdx == 1 ? 5.0f : 1.5f);

    juce::Path filterCurve; 
    filterCurve.startNewSubPath(35, 375);
    filterCurve.cubicTo(lowX - cornerOffset, 375, lowX - cornerOffset, graphY, lowX, graphY);
    filterCurve.lineTo(highX, graphY);
    filterCurve.cubicTo(highX + cornerOffset, graphY, highX + cornerOffset, 375, 235, 375);
    
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

void HomeDistoAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    if (bypassButton.getToggleState())
    {
        g.excludeClipRegion(bypassButton.getBounds());

        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.fillRoundedRectangle(10, 10, 700, 390, 8);
        
        g.setFont(juce::FontOptions(48.0f).withName("Helvetica").withStyle("Bold"));
        g.setColour(juce::Colours::white);
        g.drawText("BYPASSED", 10, 10, 700, 390, juce::Justification::centred);
    }
}

void HomeDistoAudioProcessorEditor::resized()
{
    // Mathematically split preset bounds for dedicated arrow buttons
    auto presetArea = juce::Rectangle<int>(210, 20, 300, 30);
    auto arrowsArea = presetArea.removeFromRight(20); 
    
    presetUpButton.setBounds(arrowsArea.removeFromTop(15));
    presetDownButton.setBounds(arrowsArea);
    
    presetArea.removeFromRight(4); // clean gap
    presetMenuButton.setBounds(presetArea);
    
    saveButton.setBounds(520, 20, 30, 30); 
    bypassButton.setBounds(620, 20, 30, 30);
    settingsButton.setBounds(660, 20, 30, 30); 

    for (int i = 0; i < 6; ++i) 
    {
        int col = i % 2; 
        int row = i / 2; 
        modeButtons[i].setBounds(35 + (col * 105), 115 + (row * 44), 95, 32);
    }

    lowCutSlider.setBounds(30, 325, 90, 30);  
    highCutSlider.setBounds(150, 325, 90, 30); 

    // NEW: slope buttons sit in the FILTER title row, right side of the card.
    for (int i = 0; i < 3; ++i)
        slopeButtons[i].setBounds(122 + i * 38, 274, 34, 18);

    driveKnob.setBounds(315, 115, 150, 150); 
    toneKnob.setBounds(290, 285, 70, 70);
    punchKnob.setBounds(420, 285, 70, 70);

    outputKnob.setBounds(555, 125, 120, 120); 
    // FIX: was overlapping the knob's corner directly -- moved clear to the
    // right of the knob with a real gap, vertically centered on it.
    outputLockButton.setBounds(555 + 120 - 18, 125, 18, 18);
    
    autoToggle.setBounds(615, 90, 75, 20); 
    
    mixKnob.setBounds(580, 290, 70, 70);
    // FIX: same spacing fix as outputLockButton above.
    mixLockButton.setBounds(580 + 70 + 8, 290, 18, 18);
}