#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// NEW: preset browser content, replacing the plain native PopupMenu with
// something that actually matches the plugin's look, plus a search box
// (useful now that there are 30 presets across 6 categories). Owned by the
// CallOutBox that shows it.
class PresetBrowserPanel : public juce::Component
{
public:
    PresetBrowserPanel(HomeDistoAudioProcessorEditor& ed, HomeDistoAudioProcessor& proc)
        : editor(ed), processor(proc)
    {
        titleLabel.setText("PRESETS", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        searchBox.setTextToShowWhenEmpty("Search presets...", juce::Colours::white.withAlpha(0.35f));
        searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF09090B));
        searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2A2A30));
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFF00FF87));
        searchBox.onTextChange = [this] { rebuildLayout(); };
        addAndMakeVisible(searchBox);

        content = std::make_unique<juce::Component>();
        viewport.setViewedComponent(content.get(), false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        auto categories = processor.getAllPresetsCategorized();
        for (auto& pair : categories)
        {
            CategoryUI cat;
            cat.name = pair.first;

            auto header = std::make_unique<juce::Label>();
            header->setText(pair.first, juce::dontSendNotification);
            header->setFont(juce::FontOptions(11.5f).withStyle("Bold"));
            header->setColour(juce::Label::textColourId, juce::Colour(0xFF00FF87));
            content->addAndMakeVisible(*header);
            cat.header = std::move(header);

            for (auto& file : pair.second)
            {
                juce::File f = file;
                auto btn = std::make_unique<juce::TextButton>(f.getFileNameWithoutExtension());
                btn->setName("PRESET_ROW");
                bool isActive = (processor.currentPresetFile == f);
                btn->setColour(juce::TextButton::buttonColourId, isActive ? juce::Colour(0xFF00FF87).withAlpha(0.18f) : juce::Colour(0xFF1A1A1E));
                btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF00FF87).withAlpha(0.25f));
                btn->setColour(juce::TextButton::textColourOffId, isActive ? juce::Colour(0xFF00FF87) : juce::Colours::white.withAlpha(0.85f));
                btn->onClick = [this, f] {
                    processor.loadPreset(f);
                    editor.updatePresetName();
                    if (auto* cob = findParentComponentOfClass<juce::CallOutBox>())
                        cob->dismiss();
                };
                content->addAndMakeVisible(*btn);
                cat.rows.push_back(std::move(btn));
            }

            categoryUIs.push_back(std::move(cat));
        }

        setSize(260, 400);
        rebuildLayout();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF161618));
        g.setColour(juce::Colour(0xFF2A2A30));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(10);
        titleLabel.setBounds(b.removeFromTop(20));
        b.removeFromTop(4);
        searchBox.setBounds(b.removeFromTop(24));
        b.removeFromTop(6);
        viewport.setBounds(b);
        rebuildLayout();
    }

private:
    struct CategoryUI
    {
        juce::String name;
        std::unique_ptr<juce::Label> header;
        std::vector<std::unique_ptr<juce::TextButton>> rows;
    };

    void rebuildLayout()
    {
        auto filter = searchBox.getText().trim().toLowerCase();
        int width = juce::jmax(1, viewport.getWidth() - 8);
        int y = 4;
        const int rowHeight = 24;
        const int headerHeight = 22;

        for (auto& cat : categoryUIs)
        {
            bool anyVisibleInCategory = false;
            for (auto& row : cat.rows)
                if (filter.isEmpty() || row->getButtonText().toLowerCase().contains(filter))
                    anyVisibleInCategory = true;

            cat.header->setVisible(anyVisibleInCategory);
            if (anyVisibleInCategory)
            {
                cat.header->setBounds(2, y, width - 4, headerHeight);
                y += headerHeight;
            }

            for (auto& row : cat.rows)
            {
                bool matches = filter.isEmpty() || row->getButtonText().toLowerCase().contains(filter);
                row->setVisible(matches);
                if (matches)
                {
                    row->setBounds(0, y, width, rowHeight - 2);
                    y += rowHeight;
                }
            }
            if (anyVisibleInCategory) y += 6;
        }

        content->setSize(width, juce::jmax(y, viewport.getHeight()));
    }

    HomeDistoAudioProcessorEditor& editor;
    HomeDistoAudioProcessor& processor;
    juce::Label titleLabel;
    juce::TextEditor searchBox;
    juce::Viewport viewport;
    std::unique_ptr<juce::Component> content;
    std::vector<CategoryUI> categoryUIs;
};

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
        smoothSlider.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; };

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

    // NEW: every knob now shows its actual value in a small popup while
    // hovering or dragging (setPopupDisplayEnabled with showOnlyWhileDragging
    // = false means it appears on hover too, not just while adjusting).
    // Each knob gets its own formatting function so the number shown
    // actually means something -- percentage for DRIVE/MIX/PUNCH, signed dB
    // around 0 for OUT/TONE -- rather than a raw internal parameter value.
    auto setupKnob = [this](juce::Slider& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attach,
                             juce::Colour glowColour, std::function<juce::String(double)> formatter) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, glowColour); 
        addAndMakeVisible(slider);
        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, slider);
        slider.textFromValueFunction = std::move(formatter);
        slider.setPopupDisplayEnabled(true, false, this);
    };

    // DRIVE: shown as % of its 0-24 dB range rather than raw dB, per request.
    setupKnob(driveKnob, "DRIVE", driveAttach, juce::Colour(0xFFB900FF),
        [](double v) { return juce::String(juce::roundToInt(v / 24.0 * 100.0)) + "%"; });

    // TONE: internally -1..1, actually drives a +/-6 dB shelf -- show the
    // real dB value it produces, signed, 0 dB dead center.
    setupKnob(toneKnob, "TONE", toneAttach, juce::Colour(0xFFB900FF),
        [](double v) {
            double db = v * 6.0;
            juce::String s = juce::String(db, 1);
            if (db > 0.0) s = "+" + s;
            return s + " dB";
        });

    // PUNCH: plain 0-1 amount -- percentage.
    setupKnob(punchKnob, "PUNCH", punchAttach, juce::Colour(0xFFB900FF),
        [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });

    // OUTPUT: real dB value, signed, 0 dB dead center (range is already
    // -24..+24 dB with 0 as the default/middle).
    setupKnob(outputKnob, "OUT", outAttach, juce::Colour(0xFFFF007F),
        [](double v) {
            juce::String s = juce::String(v, 1);
            if (v > 0.0) s = "+" + s;
            return s + " dB";
        });

    // MIX: 0-1 dry/wet ratio -- percentage, per request.
    setupKnob(mixKnob, "MIX", mixAttach, juce::Colour(0xFFFF007F),
        [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });

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

    // REDESIGNED: now 8 sliders (freq/gain for LOW, BELL1, BELL2, HIGH; Q
    // for the bells is wheel-controlled, see mouseWheelMove) instead of 2.
    // All still real Sliders purely as the APVTS-attached data model --
    // invisible, never shown; the EQ graph itself is the control surface.
    // Invisible components are excluded from hit-testing entirely, so
    // clicks in the graph area correctly fall through to this component's
    // own mouse handlers instead of being swallowed by a slider underneath.
    auto wireEqSlider = [this](juce::Slider& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attach) {
        addChildComponent(slider); // added but not shown
        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, slider);
        slider.onValueChange = [this] { repaint(); };
    };

    wireEqSlider(lowFreqSlider,  "EQ_LOW_FREQ",  lowFreqAttach);
    wireEqSlider(lowGainSlider,  "EQ_LOW_GAIN",  lowGainAttach);
    wireEqSlider(bell1FreqSlider, "EQ_BELL1_FREQ", bell1FreqAttach);
    wireEqSlider(bell1GainSlider, "EQ_BELL1_GAIN", bell1GainAttach);
    wireEqSlider(bell1QSlider,    "EQ_BELL1_Q",    bell1QAttach);
    wireEqSlider(bell2FreqSlider, "EQ_BELL2_FREQ", bell2FreqAttach);
    wireEqSlider(bell2GainSlider, "EQ_BELL2_GAIN", bell2GainAttach);
    wireEqSlider(bell2QSlider,    "EQ_BELL2_Q",    bell2QAttach);
    wireEqSlider(highFreqSlider, "EQ_HIGH_FREQ", highFreqAttach);
    wireEqSlider(highGainSlider, "EQ_HIGH_GAIN", highGainAttach);

    // EQ_LOW_TYPE/EQ_HIGH_TYPE (Cut vs Shelf) aren't sliders -- they're
    // toggled by clicking (not dragging) the LOW/HIGH node; see mouseUp.
    // Repaint whenever they change via automation/preset load too.
    audioProcessor.apvts.addParameterListener("EQ_LOW_TYPE", this);
    audioProcessor.apvts.addParameterListener("EQ_HIGH_TYPE", this);

    // NEW: filter slope buttons (12/24/48 dB/oct) styled as a single
    // segmented control (see SLOPE_BTN handling in the LookAndFeel) rather
    // than three separate boxes.
    for (int i = 0; i < 3; ++i)
    {
        slopeButtons[i].setName("SLOPE_BTN");
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
    audioProcessor.apvts.removeParameterListener("EQ_LOW_TYPE", this);
    audioProcessor.apvts.removeParameterListener("EQ_HIGH_TYPE", this);
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
    // FIX: was a plain native PopupMenu with OS-styled submenus -- replaced
    // with a proper themed, searchable browser that actually matches the
    // rest of the plugin (and highlights the currently active preset).
    auto panel = std::make_unique<PresetBrowserPanel>(*this, audioProcessor);
    auto bounds = presetMenuButton.getScreenBounds();
    juce::CallOutBox::launchAsynchronously(std::move(panel), bounds, nullptr);
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
    else if (parameterID == "EQ_LOW_TYPE" || parameterID == "EQ_HIGH_TYPE")
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }
}

// --- Shared freq/gain <-> pixel mapping for the EQ graph -------------------
// X uses a single shared log-frequency axis (20 Hz-20 kHz) for ALL bands,
// rather than each parameter's own (differently-skewed) NormalisableRange --
// otherwise e.g. a bell centered at 1 kHz and the low band's corner at 1 kHz
// would render at different X positions even though they're the same
// frequency. Y is +/-18 dB gain.
float HomeDistoAudioProcessorEditor::freqToX(float hz) const
{
    float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
    float logHz = std::log10(juce::jlimit(20.0f, 20000.0f, hz));
    float prop = (logHz - logMin) / (logMax - logMin);
    return filterGraphLeft + prop * (filterGraphRight - filterGraphLeft);
}

float HomeDistoAudioProcessorEditor::xToFreq(float x) const
{
    float prop = juce::jlimit(0.0f, 1.0f, (x - filterGraphLeft) / (filterGraphRight - filterGraphLeft));
    float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
    return std::pow(10.0f, logMin + prop * (logMax - logMin));
}

float HomeDistoAudioProcessorEditor::gainToY(float db) const
{
    float prop = (juce::jlimit(-18.0f, 18.0f, db) + 18.0f) / 36.0f;
    return filterGraphBottomY - prop * (filterGraphBottomY - filterGraphTopY);
}

float HomeDistoAudioProcessorEditor::yToGain(float y) const
{
    float prop = juce::jlimit(0.0f, 1.0f, (filterGraphBottomY - y) / (filterGraphBottomY - filterGraphTopY));
    return prop * 36.0f - 18.0f;
}

juce::Point<float> HomeDistoAudioProcessorEditor::lowHandlePos()
{
    int type = (int) audioProcessor.apvts.getRawParameterValue("EQ_LOW_TYPE")->load();
    float gainDb = (type == 0) ? 0.0f : (float) lowGainSlider.getValue(); // Cut = 0 dB node (dive drawn separately)
    return { freqToX((float) lowFreqSlider.getValue()), gainToY(gainDb) };
}

juce::Point<float> HomeDistoAudioProcessorEditor::highHandlePos()
{
    int type = (int) audioProcessor.apvts.getRawParameterValue("EQ_HIGH_TYPE")->load();
    float gainDb = (type == 0) ? 0.0f : (float) highGainSlider.getValue();
    return { freqToX((float) highFreqSlider.getValue()), gainToY(gainDb) };
}

juce::Point<float> HomeDistoAudioProcessorEditor::bell1HandlePos()
{
    return { freqToX((float) bell1FreqSlider.getValue()), gainToY((float) bell1GainSlider.getValue()) };
}

juce::Point<float> HomeDistoAudioProcessorEditor::bell2HandlePos()
{
    return { freqToX((float) bell2FreqSlider.getValue()), gainToY((float) bell2GainSlider.getValue()) };
}

void HomeDistoAudioProcessorEditor::toggleBandType(const juce::String& typeParamID)
{
    auto* p = audioProcessor.apvts.getParameter(typeParamID);
    if (p == nullptr) return;
    int current = (int) audioProcessor.apvts.getRawParameterValue(typeParamID)->load();
    int next = 1 - current; // 2 choices (Cut/Shelf) -- normalized value == index for a 2-choice param
    p->setValueNotifyingHost((float) next);
}

namespace
{
    constexpr float kFilterHandleHitRadius = 11.0f;
}

void HomeDistoAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    auto pos = e.position;
    FilterHandle newHover = FilterHandle::none;
    if      (pos.getDistanceFrom(lowHandlePos())   < kFilterHandleHitRadius) newHover = FilterHandle::low;
    else if (pos.getDistanceFrom(bell1HandlePos()) < kFilterHandleHitRadius) newHover = FilterHandle::bell1;
    else if (pos.getDistanceFrom(bell2HandlePos()) < kFilterHandleHitRadius) newHover = FilterHandle::bell2;
    else if (pos.getDistanceFrom(highHandlePos())  < kFilterHandleHitRadius) newHover = FilterHandle::high;

    if (newHover != hoveredFilterHandle)
    {
        hoveredFilterHandle = newHover;
        setMouseCursor(newHover == FilterHandle::none ? juce::MouseCursor::NormalCursor
                                                        : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void HomeDistoAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    auto pos = e.position;
    if      (pos.getDistanceFrom(lowHandlePos())   < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::low;
    else if (pos.getDistanceFrom(bell1HandlePos()) < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::bell1;
    else if (pos.getDistanceFrom(bell2HandlePos()) < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::bell2;
    else if (pos.getDistanceFrom(highHandlePos())  < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::high;
    else draggingFilterHandle = FilterHandle::none;

    repaint();
}

void HomeDistoAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingFilterHandle == FilterHandle::none) return;

    float newFreq = xToFreq(e.position.x);
    float newGain = yToGain(e.position.y);

    // FIX: LOW/HIGH could previously be dragged past each other. Clamp each
    // against the other's current frequency (ratio-based, matching the
    // defensive clamp on the DSP side) so they can't cross.
    constexpr float minRatio = 1.05f;

    switch (draggingFilterHandle)
    {
        case FilterHandle::low:
        {
            float highNow = (float) highFreqSlider.getValue();
            newFreq = juce::jmin(newFreq, highNow / minRatio);
            lowFreqSlider.setValue(newFreq, juce::sendNotificationSync);
            int type = (int) audioProcessor.apvts.getRawParameterValue("EQ_LOW_TYPE")->load();
            if (type == 1) lowGainSlider.setValue(newGain, juce::sendNotificationSync); // Shelf: Y = gain
            break;
        }
        case FilterHandle::high:
        {
            float lowNow = (float) lowFreqSlider.getValue();
            newFreq = juce::jmax(newFreq, lowNow * minRatio);
            highFreqSlider.setValue(newFreq, juce::sendNotificationSync);
            int type = (int) audioProcessor.apvts.getRawParameterValue("EQ_HIGH_TYPE")->load();
            if (type == 1) highGainSlider.setValue(newGain, juce::sendNotificationSync);
            break;
        }
        case FilterHandle::bell1:
            bell1FreqSlider.setValue(newFreq, juce::sendNotificationSync);
            bell1GainSlider.setValue(newGain, juce::sendNotificationSync);
            break;
        case FilterHandle::bell2:
            bell2FreqSlider.setValue(newFreq, juce::sendNotificationSync);
            bell2GainSlider.setValue(newGain, juce::sendNotificationSync);
            break;
        default: break;
    }

    repaint();
}

void HomeDistoAudioProcessorEditor::mouseUp (const juce::MouseEvent& e)
{
    // NEW: clicking (not dragging) the LOW or HIGH node toggles it between
    // Cut and Shelf -- keeps the graph uncluttered with extra buttons.
    bool wasClick = e.getDistanceFromDragStart() < 4;
    if (wasClick && draggingFilterHandle == FilterHandle::low)
        toggleBandType("EQ_LOW_TYPE");
    else if (wasClick && draggingFilterHandle == FilterHandle::high)
        toggleBandType("EQ_HIGH_TYPE");

    draggingFilterHandle = FilterHandle::none;
    repaint();
}

void HomeDistoAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto pos = e.position;
    juce::Slider* qSlider = nullptr;

    if (pos.getDistanceFrom(bell1HandlePos()) < kFilterHandleHitRadius) qSlider = &bell1QSlider;
    else if (pos.getDistanceFrom(bell2HandlePos()) < kFilterHandleHitRadius) qSlider = &bell2QSlider;

    if (qSlider != nullptr)
    {
        float q = (float) qSlider->getValue();
        q = juce::jlimit(0.2f, 8.0f, q + wheel.deltaY * 2.0f);
        qSlider->setValue(q, juce::sendNotificationSync);
        repaint();
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

    // REDESIGNED: was a single flat weight, "HOME : DISTO" with an awkward
    // literal " : " for separation. Now a proper small logotype: a drop
    // shadow for depth (same technique drawShadedCard already uses
    // elsewhere, for consistency), colour alone carrying the HOME/DISTO
    // split instead of a colon character, and a small tracked-out subtitle
    // underneath for a more "designed plugin" feel.
    juce::Font titleFont = juce::FontOptions(25.0f).withName("Helvetica").withStyle("Bold");
    g.setFont(titleFont);

    juce::String homeText = "HOME";
    juce::String distoText = "DISTO";
    int homeWidth = juce::GlyphArrangement::getStringWidthInt(titleFont, homeText);
    int gap = 7;

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawText(homeText, 26, 21, homeWidth, 30, juce::Justification::centredLeft);
    g.drawText(distoText, 26 + homeWidth + gap, 21, 100, 30, juce::Justification::centredLeft);

    g.setColour(juce::Colours::white);
    g.drawText(homeText, 25, 20, homeWidth, 30, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xFF00FF87));
    g.drawText(distoText, 25 + homeWidth + gap, 20, 100, 30, juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(8.5f).withName("Helvetica").withStyle("Bold"));
    g.setColour(juce::Colours::white.withAlpha(0.32f));
    g.drawText("D I S T O R T I O N   E N G I N E", 26, 51, 320, 12, juce::Justification::centredLeft);

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
    // FIX: renamed from FILTER -- this is a real 4-band EQ now (Low
    // Cut/Shelf, 2 Bell bands, High Cut/Shelf), not just a filter.
    drawCardText("EQ", 20, 271, 230, 18, juce::Justification::centred);

    // NEW: shared pill-shaped track behind the slope segmented control --
    // drawn here (once, in the parent) so all 3 buttons read as one control.
    // Matches the bounds set in resized().
    {
        juce::Rectangle<float> slopeTrack(76.0f, 291.0f, 118.0f, 16.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(slopeTrack, 3.0f);
    }

    // REDESIGNED: 4 draggable nodes now (was 2) -- LOW, BELL1, BELL2, HIGH.
    // Click (no drag) a LOW/HIGH node to toggle Cut<->Shelf; drag to move
    // it. Bell nodes always drag freely in freq+gain; scroll wheel over one
    // adjusts its Q.
    auto lowPt   = lowHandlePos();
    auto bell1Pt = bell1HandlePos();
    auto bell2Pt = bell2HandlePos();
    auto highPt  = highHandlePos();
    int lowType  = (int) audioProcessor.apvts.getRawParameterValue("EQ_LOW_TYPE")->load();
    int highType = (int) audioProcessor.apvts.getRawParameterValue("EQ_HIGH_TYPE")->load();

    // 0 dB reference line.
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.drawLine(filterGraphLeft, filterGraphMidY, filterGraphRight, filterGraphMidY, 1.5f);

    // Corner sharpness for CUT-mode bands reflects the selected filter
    // slope: gentler roll-off (12 dB/oct) draws a wide, soft dive to the
    // floor; steeper slopes (24/48 dB/oct) draw a progressively tighter one.
    int slopeIdx = juce::jlimit(0, 2, (int) audioProcessor.apvts.getRawParameterValue("SLOPE")->load());
    float cornerOffset = slopeIdx == 0 ? 16.0f : (slopeIdx == 1 ? 8.0f : 2.5f);

    juce::Path eqCurve;

    if (lowType == 0) // Cut: dive to the floor left of the node
    {
        eqCurve.startNewSubPath(filterGraphLeft, filterGraphBottomY);
        eqCurve.cubicTo(lowPt.x - cornerOffset, filterGraphBottomY, lowPt.x - cornerOffset, lowPt.y, lowPt.x, lowPt.y);
    }
    else // Shelf: approach the node's gain level smoothly
    {
        eqCurve.startNewSubPath(filterGraphLeft, lowPt.y);
        eqCurve.lineTo(lowPt.x, lowPt.y);
    }

    eqCurve.cubicTo((lowPt.x + bell1Pt.x) * 0.5f, lowPt.y, (lowPt.x + bell1Pt.x) * 0.5f, bell1Pt.y, bell1Pt.x, bell1Pt.y);
    eqCurve.cubicTo((bell1Pt.x + bell2Pt.x) * 0.5f, bell1Pt.y, (bell1Pt.x + bell2Pt.x) * 0.5f, bell2Pt.y, bell2Pt.x, bell2Pt.y);
    eqCurve.cubicTo((bell2Pt.x + highPt.x) * 0.5f, bell2Pt.y, (bell2Pt.x + highPt.x) * 0.5f, highPt.y, highPt.x, highPt.y);

    if (highType == 0) // Cut: dive to the floor right of the node
        eqCurve.cubicTo(highPt.x + cornerOffset, highPt.y, highPt.x + cornerOffset, filterGraphBottomY, filterGraphRight, filterGraphBottomY);
    else // Shelf
        eqCurve.lineTo(filterGraphRight, highPt.y);

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.strokePath(eqCurve, juce::PathStrokeType(3.5f), juce::AffineTransform::translation(0, 1.5f));
    g.setColour(juce::Colour(0xFF09090B));
    g.strokePath(eqCurve, juce::PathStrokeType(3.0f)); 

    // Fill under the curve for a proper "EQ scope" look. FIX: colour
    // matches the EQ card's own green accent instead of a mismatched cyan.
    juce::Path fillPath = eqCurve;
    fillPath.lineTo(filterGraphRight, filterGraphBottomY);
    fillPath.lineTo(filterGraphLeft, filterGraphBottomY);
    fillPath.closeSubPath();
    g.setColour(juce::Colour(0xFF00FF87).withAlpha(0.07f));
    g.fillPath(fillPath);

    // Draggable handles. Bigger and glowing when hovered/dragged so it's
    // obvious they're grabbable. FIX: colour now matches the EQ card's own
    // green accent (was a mismatched cyan borrowed from a different card).
    auto drawHandle = [&](juce::Point<float> pos, bool active) {
        float r = active ? 7.0f : 5.5f;
        if (active)
        {
            g.setColour(juce::Colour(0xFF00FF87).withAlpha(0.35f));
            g.fillEllipse(pos.x - r - 5.0f, pos.y - r - 5.0f, (r + 5.0f) * 2.0f, (r + 5.0f) * 2.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(pos.x - r, pos.y - r + 1.5f, r * 2.0f, r * 2.0f);
        g.setColour(active ? juce::Colour(0xFF00FF87) : juce::Colours::white);
        g.fillEllipse(pos.x - r, pos.y - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(0xFF09090B));
        g.drawEllipse(pos.x - r, pos.y - r, r * 2.0f, r * 2.0f, 1.2f);
    };

    bool lowActive   = (hoveredFilterHandle == FilterHandle::low)   || (draggingFilterHandle == FilterHandle::low);
    bool bell1Active = (hoveredFilterHandle == FilterHandle::bell1) || (draggingFilterHandle == FilterHandle::bell1);
    bool bell2Active = (hoveredFilterHandle == FilterHandle::bell2) || (draggingFilterHandle == FilterHandle::bell2);
    bool highActive  = (hoveredFilterHandle == FilterHandle::high)  || (draggingFilterHandle == FilterHandle::high);
    drawHandle(lowPt, lowActive);
    drawHandle(bell1Pt, bell1Active);
    drawHandle(bell2Pt, bell2Active);
    drawHandle(highPt, highActive);

    // Live value readout only while actively interacting with a handle --
    // no permanent text clutter otherwise.
    FilterHandle shownHandle = hoveredFilterHandle != FilterHandle::none ? hoveredFilterHandle : draggingFilterHandle;
    if (shownHandle != FilterHandle::none)
    {
        juce::Point<float> pt;
        juce::String text;

        if (shownHandle == FilterHandle::low)
        {
            pt = lowPt;
            text = getFrequencyString((float) lowFreqSlider.getValue());
            if (lowType == 1) text += " / " + juce::String((float) lowGainSlider.getValue(), 1) + " dB";
            else text += " (Cut)";
        }
        else if (shownHandle == FilterHandle::high)
        {
            pt = highPt;
            text = getFrequencyString((float) highFreqSlider.getValue());
            if (highType == 1) text += " / " + juce::String((float) highGainSlider.getValue(), 1) + " dB";
            else text += " (Cut)";
        }
        else if (shownHandle == FilterHandle::bell1)
        {
            pt = bell1Pt;
            text = getFrequencyString((float) bell1FreqSlider.getValue()) + " / " + juce::String((float) bell1GainSlider.getValue(), 1) + " dB";
        }
        else
        {
            pt = bell2Pt;
            text = getFrequencyString((float) bell2FreqSlider.getValue()) + " / " + juce::String((float) bell2GainSlider.getValue(), 1) + " dB";
        }

        g.setFont(juce::FontOptions(11.0f).withName("Helvetica").withStyle("Bold"));
        float textWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 14.0f;
        juce::Rectangle<float> bubble(pt.x - textWidth * 0.5f, pt.y - 26.0f, textWidth, 16.0f);
        bubble = bubble.constrainedWithin(juce::Rectangle<float>(filterGraphLeft, 292.0f, filterGraphRight - filterGraphLeft, 100.0f));

        g.setColour(juce::Colour(0xFF161618));
        g.fillRoundedRectangle(bubble, 3.0f);
        g.setColour(juce::Colour(0xFF00FF87).withAlpha(0.5f));
        g.drawRoundedRectangle(bubble, 3.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.drawText(text, bubble, juce::Justification::centred);
    }

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

    // NOTE: all 8 EQ band sliders (low/bell1/bell2/high freq+gain) are
    // invisible data-model-only components now -- no bounds needed, the
    // graph itself is drawn/hit-tested via freqToX/gainToY (see paint() and
    // the mouse handlers), not via each slider's own on-screen size.

    // Slope buttons sit in their own centred row directly under the EQ
    // title. Bounds must match the slopeTrack rectangle drawn in paint().
    for (int i = 0; i < 3; ++i)
        slopeButtons[i].setBounds(76 + i * 42, 291, 34, 16);

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
    mixLockButton.setBounds(580 + 60 + 6, 290, 18, 18);
}