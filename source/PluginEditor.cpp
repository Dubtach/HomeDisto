#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// NEW: preset browser content, replacing the plain native PopupMenu with
// something that actually matches the plugin's look. Owned by the
// CallOutBox that shows it.
// NEW: preset rows previously rendered with JUCE's stock default TextButton
// look (the browser doesn't share the main plugin's custom LookAndFeel),
// which centres button text -- fine for a single word, but reads oddly for
// a list of preset names. This gives rows proper left-aligned, list-style
// text instead.
class PresetRowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto base = button.findColour(juce::TextButton::buttonColourId);
        g.setColour(shouldDrawButtonAsHighlighted ? base.brighter(0.25f) : base);
        g.fillRoundedRectangle(bounds, 3.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setFont(juce::FontOptions(12.0f));
        g.setColour(button.findColour(juce::TextButton::textColourOffId));
        g.drawText(button.getButtonText(), button.getLocalBounds().reduced(10, 0), juce::Justification::centredLeft);
    }
};

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
            // NEW: "0. Default" is pinned as its own always-visible button
            // above the search box, not buried in the scrollable list --
            // a one-click sane starting point every time the plugin loads.
            if (pair.first == "0. Default" && !pair.second.isEmpty())
            {
                juce::File f = pair.second.getFirst();
                const bool isDefaultActive = (processor.currentPresetFile == f);
                defaultButton.setButtonText(isDefaultActive ? "DEFAULT  •  Active" : "DEFAULT  •  Quick Start");
                defaultButton.setColour(juce::TextButton::buttonColourId,
                                        isDefaultActive ? juce::Colour(0xFF00FF87).withAlpha(0.22f)
                                                        : juce::Colour(0xFF00FF87).withAlpha(0.12f));
                defaultButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF00FF87));
                defaultButton.setLookAndFeel(&rowLnf);
                defaultButton.onClick = [this, f] {
                    processor.loadPreset(f);
                    editor.updatePresetName();
                    if (auto* cob = findParentComponentOfClass<juce::CallOutBox>())
                        cob->dismiss();
                };
                addAndMakeVisible(defaultButton);
                continue;
            }

            CategoryUI cat;
            cat.name = pair.first;
            // Strip the leading sort-order prefix ("1. Guitars" -> "Guitars")
            // for the chip label and header -- the number is only there to
            // control folder sort order, showing it is just noise.
            // FIX: this was showing blank for the "User" category. JUCE's
            // fromFirstOccurrenceOf() returns an EMPTY string when the
            // substring isn't found at all -- it does NOT fall back to the
            // original string. "User" has no ". " in it (no numeric sort
            // prefix), so it was silently blanking out every time. Only
            // strip when the prefix is actually present.
            cat.displayName = pair.first.contains(". ")
                ? pair.first.fromFirstOccurrenceOf(". ", false, false)
                : pair.first;

            auto header = std::make_unique<juce::Label>();
            header->setText(cat.displayName, juce::dontSendNotification);
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
                btn->setLookAndFeel(&rowLnf);
                btn->onClick = [this, f] {
                    processor.loadPreset(f);
                    editor.updatePresetName();
                    if (auto* cob = findParentComponentOfClass<juce::CallOutBox>())
                        cob->dismiss();
                };
                content->addAndMakeVisible(*btn);
                if (isActive) activeRow = btn.get();
                cat.rows.push_back(std::move(btn));
            }

            categoryUIs.push_back(std::move(cat));
        }

        // NEW: one chip per category, always visible above the scrollable
        // list -- fixes "hard to see how many categories there are, have to
        // scroll to find one." Clicking a chip jumps the list straight to
        // that category instead of scrolling blind.
        for (auto& cat : categoryUIs)
        {
            auto chip = std::make_unique<juce::TextButton>(cat.displayName);
            chip->setName("PRESET_CHIP");
            chip->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A1A1E));
            chip->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.75f));
            juce::String targetName = cat.name;
            chip->onClick = [this, targetName] { scrollToCategory(targetName); };
            addAndMakeVisible(*chip);
            categoryChips.push_back(std::move(chip));
        }

        setSize(300, 440);
        rebuildLayout();

        if (activeRow != nullptr)
        {
            // Centre the active row in the visible area rather than just
            // scrolling it to the top edge, so the couple of presets before
            // AND after it are visible too -- exactly what makes stepping
            // to the next one without rescrolling possible.
            int rowY = activeRow->getBounds().getCentreY();
            int targetY = juce::jmax(0, rowY - viewport.getHeight() / 2);
            viewport.setViewPosition(0, targetY);
        }
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
        defaultButton.setBounds(b.removeFromTop(26));
        b.removeFromTop(8);

        // Category chips: wrap into rows, 3 per row.
        {
            int chipW = (b.getWidth() - 8) / 3;
            int x = b.getX(), y = b.getY();
            int col = 0;
            for (auto& chip : categoryChips)
            {
                chip->setBounds(x + col * (chipW + 4), y, chipW, 22);
                if (++col >= 3) { col = 0; y += 26; }
            }
            int rows = (int) std::ceil(categoryChips.size() / 3.0);
            b.removeFromTop(rows * 26 + 4);
        }

        viewport.setBounds(b);
        rebuildLayout();
    }

private:
    struct CategoryUI
    {
        juce::String name;         // full folder name, e.g. "1. Guitars" (for lookups)
        juce::String displayName;  // stripped, e.g. "Guitars" (for display)
        std::unique_ptr<juce::Label> header;
        std::vector<std::unique_ptr<juce::TextButton>> rows;
    };

    void scrollToCategory(const juce::String& categoryName)
    {
        for (auto& cat : categoryUIs)
        {
            if (cat.name == categoryName && cat.header->isVisible())
            {
                viewport.setViewPosition(0, juce::jmax(0, cat.header->getY() - 4));
                return;
            }
        }
    }

    void rebuildLayout()
    {
        auto filter = searchBox.getText().trim().toLowerCase();
        int width = juce::jmax(1, viewport.getWidth() - 8);
        int y = 4;
        const int rowHeight = 24;
        const int headerHeight = 22;

        const bool showDefault = filter.isEmpty() || defaultButton.getButtonText().toLowerCase().contains(filter)
                                 || filter == "default";
        defaultButton.setVisible(showDefault);

        bool anyPresetVisible = showDefault;
        for (auto& cat : categoryUIs)
            for (auto& row : cat.rows)
                if (filter.isEmpty() || row->getButtonText().toLowerCase().contains(filter))
                    anyPresetVisible = true;

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

        if (!anyPresetVisible)
        {
            static const juce::String noResultsText = "No presets found";
            if (noResultsLabel == nullptr)
            {
                noResultsLabel = std::make_unique<juce::Label>();
                noResultsLabel->setText(noResultsText, juce::dontSendNotification);
                noResultsLabel->setJustificationType(juce::Justification::centred);
                noResultsLabel->setFont(juce::FontOptions(12.0f));
                noResultsLabel->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.45f));
                content->addAndMakeVisible(*noResultsLabel);
            }
            noResultsLabel->setVisible(true);
            noResultsLabel->setBounds(0, 8, width, 28);
        }
        else if (noResultsLabel != nullptr)
        {
            noResultsLabel->setVisible(false);
        }
    }

    HomeDistoAudioProcessorEditor& editor;
    HomeDistoAudioProcessor& processor;
    // Declared before the buttons that use it, so it's destroyed last --
    // a LookAndFeel must outlive any component still pointing at it.
    PresetRowLookAndFeel rowLnf;
    juce::Label titleLabel;
    juce::TextEditor searchBox;
    juce::TextButton defaultButton;
    std::vector<std::unique_ptr<juce::TextButton>> categoryChips;
    juce::Viewport viewport;
    std::unique_ptr<juce::Component> content;
    std::vector<CategoryUI> categoryUIs;
    juce::TextButton* activeRow = nullptr; // for auto-scroll-to-active on open
    std::unique_ptr<juce::Label> noResultsLabel;
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
                juce::String presetName = rawAlert->getTextEditorContents("presetName").trim();
                if (presetName.isNotEmpty())
                {
                    auto userDir = audioProcessor.getPresetDirectory().getChildFile("User");
                    auto cleanName = presetName.replaceCharacters("\\/:*?\"<>|", "_________").trim();
                    auto targetFile = userDir.getChildFile(cleanName + ".xml");

                    if (targetFile.existsAsFile())
                    {
                        auto confirm = std::make_unique<juce::AlertWindow>(
                            "Overwrite Preset?",
                            "A user preset with this name already exists. Overwrite it?",
                            juce::AlertWindow::WarningIcon);
                        confirm->addButton("Overwrite", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
                        confirm->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));

                        auto* confirmAlert = confirm.release();
                        confirmAlert->enterModalState(true, juce::ModalCallbackFunction::create(
                            [this, confirmAlert, presetName](int confirmResult)
                            {
                                if (confirmResult == 1)
                                {
                                    audioProcessor.savePreset(presetName, true);
                                    updatePresetName();
                                }
                                delete confirmAlert;
                            }));
                    }
                    else if (!audioProcessor.savePreset(presetName, false))
                    {
                        updatePresetName();
                    }
                    else
                    {
                        updatePresetName();
                    }
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

    // Quick access to license activation/status, placed next to SETTINGS.
    licenseButton.setName("LICENSE");
    licenseButton.setClickingTogglesState(false);
    addAndMakeVisible(licenseButton);
    licenseButton.onClick = [this] { showActivationDialog(); };

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

    // NEW: EQ reset -- flattens all 4 bands back to neutral (LOW/HIGH set
    // to Cut at the extreme edges of their range, where a cut is
    // inaudible; both bells at 0 dB gain), i.e. a flat line across the
    // whole graph.
    eqResetButton.setName("RESET_EQ");
    eqResetButton.setTooltip("Reset EQ to flat");
    addAndMakeVisible(eqResetButton);
    eqResetButton.onClick = [this] {
        auto& ap = audioProcessor.apvts;
        auto setP = [&ap](const juce::String& id, float normalised) {
            if (auto* p = ap.getParameter(id)) p->setValueNotifyingHost(normalised);
        };
        // Cut/Shelf choice params: normalised 0 = Cut.
        setP("EQ_LOW_TYPE", 0.0f);
        setP("EQ_HIGH_TYPE", 0.0f);
        if (auto* p = ap.getParameter("EQ_LOW_FREQ"))  p->setValueNotifyingHost(p->convertTo0to1(20.0f));
        if (auto* p = ap.getParameter("EQ_LOW_GAIN"))  p->setValueNotifyingHost(p->convertTo0to1(0.0f));
        if (auto* p = ap.getParameter("EQ_HIGH_FREQ")) p->setValueNotifyingHost(p->convertTo0to1(20000.0f));
        if (auto* p = ap.getParameter("EQ_HIGH_GAIN")) p->setValueNotifyingHost(p->convertTo0to1(0.0f));
        if (auto* p = ap.getParameter("EQ_BELL1_GAIN")) p->setValueNotifyingHost(p->convertTo0to1(0.0f));
        if (auto* p = ap.getParameter("EQ_BELL2_GAIN")) p->setValueNotifyingHost(p->convertTo0to1(0.0f));
        repaint();
    };

    // NEW: EQ lock -- same idea as the OUTPUT/MIX locks, but for the whole
    // EQ (all 4 nodes) across preset changes.
    eqLockButton.setName("LOCK_EQ");
    eqLockButton.setClickingTogglesState(true);
    eqLockButton.setTooltip("Lock EQ: presets won't change these nodes");
    addAndMakeVisible(eqLockButton);
    eqLockButton.onClick = [this] { audioProcessor.lockEQ.store(eqLockButton.getToggleState()); };

    // FIX: this is the actual "loses lock state on hide/reopen" bug. The
    // processor (and its lockOutput/lockMix/lockEQ atomics) survives the
    // editor being closed and reopened -- only the editor component itself
    // gets destroyed and recreated. But these buttons never read back the
    // processor's current values on construction, so a brand new editor
    // always started them looking unlocked regardless of what they
    // actually were -- and clicking one to "re-lock" it would actually
    // toggle it OFF, since it was never really off to begin with.
    outputLockButton.setToggleState(audioProcessor.lockOutput.load(), juce::dontSendNotification);
    mixLockButton.setToggleState(audioProcessor.lockMix.load(), juce::dontSendNotification);
    eqLockButton.setToggleState(audioProcessor.lockEQ.load(), juce::dontSendNotification);

    autoToggle.setButtonText("AUTO");
    autoToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFF007F)); 
    autoToggle.setTooltip("Auto Gain: DRIVE/TONE/PUNCH/MODE automatically nudge OUTPUT to keep loudness steady");
    addAndMakeVisible(autoToggle);
    autoAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "AUTO", autoToggle);

    for (int i = 0; i < 6; ++i) 
    {
        modeButtons[i].setButtonText(modeNames[i]);
        modeButtons[i].setName("MODE_BTN");
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

    // FIX: the on-screen 12/24/48 slope buttons are gone -- SLOPE is
    // controlled directly on the graph now (shift+drag LOW/HIGH), so a
    // separate widget for it was redundant. Still a real APVTS parameter,
    // still needs a listener so automation/preset loads repaint the curve.
    audioProcessor.apvts.addParameterListener("SLOPE", this);

    // NEW: AUTO reworked entirely -- see applyAutoGainCompensation() for
    // the full explanation. DRIVE/TONE/PUNCH need their own listeners here
    // (MODE's is already registered above); AUTO itself needs one too, so
    // toggling it on can (re)establish its baseline.
    audioProcessor.apvts.addParameterListener("DRIVE", this);
    audioProcessor.apvts.addParameterListener("TONE", this);
    audioProcessor.apvts.addParameterListener("PUNCH", this);
    audioProcessor.apvts.addParameterListener("MIX", this);
    audioProcessor.apvts.addParameterListener("AUTO", this);
    recalibrateAutoBaseline();
    updateLicenseUI();
}

HomeDistoAudioProcessorEditor::~HomeDistoAudioProcessorEditor()
{
    audioProcessor.apvts.removeParameterListener("MODE", this);
    audioProcessor.apvts.removeParameterListener("SLOPE", this);
    audioProcessor.apvts.removeParameterListener("EQ_LOW_TYPE", this);
    audioProcessor.apvts.removeParameterListener("EQ_HIGH_TYPE", this);
    audioProcessor.apvts.removeParameterListener("DRIVE", this);
    audioProcessor.apvts.removeParameterListener("TONE", this);
    audioProcessor.apvts.removeParameterListener("PUNCH", this);
    audioProcessor.apvts.removeParameterListener("MIX", this);
    audioProcessor.apvts.removeParameterListener("AUTO", this);
    setLookAndFeel(nullptr); 
}

void HomeDistoAudioProcessorEditor::updateLicenseUI()
{
    const bool unlocked = audioProcessor.getLicenseManager().isActivated();
    eqResetButton.setEnabled(unlocked);
    eqLockButton.setEnabled(unlocked);
    licenseButton.setToggleState(unlocked, juce::dontSendNotification);
    licenseButton.setTooltip(unlocked
                                  ? "Home-Disto is activated - view license status"
                                  : "Unlock EQ - enter your activation code");
    repaint(20, 265, 230, 125);
}

void HomeDistoAudioProcessorEditor::showActivationDialog()
{
    if (audioProcessor.getLicenseManager().isActivated())
    {
        auto message = juce::String("Home-Disto EQ is activated.");
        auto name = audioProcessor.getLicenseManager().getLicenseeName();
        if (name.isNotEmpty())
            message += "\n\nLicensed to: " + name;

        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Home-Disto Activated",
                                               message);
        return;
    }

    auto alert = std::make_unique<juce::AlertWindow>(
        "Unlock Home-Disto EQ",
        "Enter the activation code from your purchase email.",
        juce::AlertWindow::NoIcon);
    alert->addTextEditor("activationCode", "", "Activation Code:", false);
    if (auto* editor = alert->getTextEditor("activationCode"))
    {
        editor->setMultiLine(false);
        editor->setReturnKeyStartsNewLine(false);
    }
    alert->addButton("ACTIVATE", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto* rawAlert = alert.release();
    rawAlert->enterModalState(true, juce::ModalCallbackFunction::create([this, rawAlert](int result)
    {
        if (result == 1)
        {
            const auto code = rawAlert->getTextEditorContents("activationCode").trim();
            if (audioProcessor.getLicenseManager().activate(code))
            {
                updateLicenseUI();
                auto message = juce::String("EQ is now unlocked.");
                auto name = audioProcessor.getLicenseManager().getLicenseeName();
                if (name.isNotEmpty())
                    message += "\n\nLicensed to: " + name;
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                       "Home-Disto Activated", message);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Activation Failed",
                                                       "That activation code is not valid for Home-Disto.");
            }
        }
        delete rawAlert;
    }));
}

void HomeDistoAudioProcessorEditor::refreshFromProcessorState()
{
    updatePresetName();
    outputLockButton.setToggleState(audioProcessor.lockOutput.load(), juce::dontSendNotification);
    mixLockButton.setToggleState(audioProcessor.lockMix.load(), juce::dontSendNotification);
    eqLockButton.setToggleState(audioProcessor.lockEQ.load(), juce::dontSendNotification);
    updateLicenseUI();

    // Refresh the editor-facing controls after a host state restore, not just
    // after the editor itself is constructed.
    for (int i = 0; i < 6; ++i)
        modeButtons[i].setToggleState(i == (int) audioProcessor.apvts.getRawParameterValue("MODE")->load(),
                                      juce::dontSendNotification);
    repaint();
}

void HomeDistoAudioProcessorEditor::updatePresetName()
{
    if (audioProcessor.currentPresetFile.existsAsFile())
    {
        auto name = audioProcessor.currentPresetFile.getFileNameWithoutExtension();
        if (audioProcessor.isCurrentPresetModified())
            name += "*";
        presetMenuButton.setButtonText(name);
    }
    else
        presetMenuButton.setButtonText("Custom");

    // NEW: recalibrate AUTO's baseline to the just-loaded preset's own
    // DRIVE/TONE/PUNCH/MODE values, WITHOUT touching OUTPUT. Presets already
    // carry their own carefully-tuned OUTPUT trim; without this, loading a
    // preset would immediately fire AUTO's compensation logic (since
    // replaceState() touches every parameter, including these) and shove
    // OUTPUT away from what the preset intended. This runs synchronously,
    // before the deferred parameterChanged callbacks triggered by the load
    // get a chance to run, so by the time they do, there's no baseline
    // mismatch left for them to "correct."
    recalibrateAutoBaseline();
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
    // Mark the preset display as modified when real parameter values differ
    // from the loaded reference state. Session-only flags (locks/path) are
    // deliberately excluded by the processor comparison.
    juce::MessageManager::callAsync([this]() {
        if (audioProcessor.currentPresetFile.existsAsFile())
        {
            auto name = audioProcessor.currentPresetFile.getFileNameWithoutExtension();
            if (audioProcessor.isCurrentPresetModified())
                name += "*";
            presetMenuButton.setButtonText(name);
        }
        else
        {
            presetMenuButton.setButtonText("Custom");
        }
    });
    if (parameterID == "MODE")
    {
        juce::MessageManager::callAsync([this, newValue]() {
            modeButtons[(int)newValue].setToggleState(true, juce::dontSendNotification);
            if (audioProcessor.apvts.getRawParameterValue("AUTO")->load() > 0.5f)
                applyAutoGainCompensation();
        });
    }
    else if (parameterID == "DRIVE" || parameterID == "TONE" || parameterID == "PUNCH" || parameterID == "MIX")
    {
        // AUTO -- these four (plus MODE above) are exactly the knobs whose
        // loudness AUTO is meant to compensate for. MIX added per request:
        // pulling MIX down reduces the compensation proportionally (see
        // computeAutoCompDb) so turning it down doesn't just make
        // everything quieter with nothing balancing it.
        juce::MessageManager::callAsync([this]() {
            if (audioProcessor.apvts.getRawParameterValue("AUTO")->load() > 0.5f)
                applyAutoGainCompensation();
        });
    }
    else if (parameterID == "AUTO")
    {
        // Turning AUTO on shouldn't itself jump the OUTPUT knob -- just
        // establish where "no compensation yet" is, starting from
        // wherever DRIVE/TONE/PUNCH/MODE happen to already be.
        juce::MessageManager::callAsync([this, newValue]() {
            if (newValue > 0.5f) recalibrateAutoBaseline();
        });
    }
    else if (parameterID == "SLOPE")
    {
        juce::MessageManager::callAsync([this]() { repaint(); }); // curve steepness depends on slope
    }
    else if (parameterID == "EQ_LOW_TYPE" || parameterID == "EQ_HIGH_TYPE")
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }
}

// NEW: AUTO, reworked entirely. It used to be RMS-based makeup gain
// computed on the audio thread; now it's a UI-layer "linked knobs"
// convenience -- DRIVE/TONE/PUNCH/MODE each nudge the OUTPUT knob to keep
// perceived loudness roughly steady, exactly the way a person would
// manually compensate by ear, done automatically. When AUTO is off none of
// this runs at all (checked at every call site above).
//
// Honest caveat: these coefficients are principled engineering estimates
// (DRIVE dominates since it's the main gain-adding control but saturation
// partially self-compresses so it isn't compensated 1:1; TONE and PUNCH
// contribute much less since they don't add anywhere near as much energy;
// each MODE gets a fixed offset based on how "hot" that curve tends to run,
// reusing the same judgement calls made tuning the factory presets' OUTPUT
// trims) -- not a measured loudness model, since there's no way to render
// real audio through the DSP in this environment to verify by ear.
float HomeDistoAudioProcessorEditor::computeAutoCompDb() const
{
    float driveDb = audioProcessor.apvts.getRawParameterValue("DRIVE")->load();
    float toneDb = audioProcessor.apvts.getRawParameterValue("TONE")->load() * 6.0f;
    float punch = audioProcessor.apvts.getRawParameterValue("PUNCH")->load();
    float mix = audioProcessor.apvts.getRawParameterValue("MIX")->load();
    int mode = juce::jlimit(0, 5, (int) audioProcessor.apvts.getRawParameterValue("MODE")->load());

    // PUNCH, TUBE, TAPE, DIGITAL, CRUNCH, FUZZ
    static const float modeOffsetDb[6] = { 0.0f, 0.0f, 0.5f, -1.5f, -0.5f, -2.0f };

    float rawComp = (-0.5f * driveDb) + (-0.15f * toneDb) + (-1.5f * punch) + modeOffsetDb[mode];

    // NEW: MIX now scales the whole compensation. Physically motivated: at
    // MIX=0 there's no wet (distorted) signal in the output at all, just
    // dry, so there's nothing to compensate for -- compensation should be
    // 0. At MIX=1 the full DRIVE/TONE/PUNCH/MODE compensation applies, same
    // as before. Linear in between. Without this, pulling MIX down would
    // just make everything quieter with nothing correcting for it, which
    // is exactly the "balance" problem turning MIX down was supposed to
    // avoid.
    return rawComp * mix;
}

void HomeDistoAudioProcessorEditor::recalibrateAutoBaseline()
{
    lastAutoCompDb = computeAutoCompDb();
}

void HomeDistoAudioProcessorEditor::applyAutoGainCompensation()
{
    // Respect the OUTPUT lock -- locking means "presets and automatic
    // changes alike leave this knob alone," not just presets.
    if (audioProcessor.lockOutput.load())
        return;

    float newComp = computeAutoCompDb();
    float delta = newComp - lastAutoCompDb;
    lastAutoCompDb = newComp;
    if (std::abs(delta) < 0.001f) return;

    auto* outParam = audioProcessor.apvts.getParameter("OUT");
    if (outParam == nullptr) return;

    float currentOutDb = audioProcessor.apvts.getRawParameterValue("OUT")->load();
    float newOutDb = juce::jlimit(-24.0f, 24.0f, currentOutDb + delta);
    outParam->setValueNotifyingHost(outParam->convertTo0to1(newOutDb));
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
    if (!audioProcessor.getLicenseManager().isActivated())
        return;

    auto* p = audioProcessor.apvts.getParameter(typeParamID);
    if (p == nullptr) return;
    int current = (int) audioProcessor.apvts.getRawParameterValue(typeParamID)->load();
    int next = 1 - current; // 2 choices (Cut/Shelf) -- normalized value == index for a 2-choice param
    p->setValueNotifyingHost((float) next);
}

// NEW: replaces the old on-screen 12/24/48 dB/oct slope buttons entirely --
// shift+drag on LOW/HIGH scrubs through these 3 options directly (see
// mouseDrag). SLOPE is still a real, 3-choice APVTS parameter underneath;
// this just steps it by +1/-1 with wraparound.
void HomeDistoAudioProcessorEditor::cycleSlope (int direction)
{
    // 1. Get the current slope index (0 = 12 dB, 1 = 24 dB, 2 = 48 dB)
    int currentIndex = juce::roundToInt(audioProcessor.apvts.getRawParameterValue("SLOPE")->load());
    
    // 2. Add the drag direction and clamp it strictly between 0 and 2
    // This entirely prevents the annoying 48 -> 12 or 12 -> 48 wrap-around.
    int newIndex = juce::jlimit(0, 2, currentIndex + direction);
    
    // 3. Update the parameter only if the boundary hasn't been hit yet
    if (newIndex != currentIndex)
    {
        if (auto* param = audioProcessor.apvts.getParameter("SLOPE"))
        {
            // Normalize the 0-2 index back to APVTS's expected 0.0 - 1.0 range
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1((float)newIndex));
            param->endChangeGesture();
        }
    }
}

namespace
{
    constexpr float kFilterHandleHitRadius = 11.0f;
}

void HomeDistoAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const juce::Rectangle<float> eqBounds(20.0f, 265.0f, 230.0f, 125.0f);
    if (!audioProcessor.getLicenseManager().isActivated() && eqBounds.contains(e.position))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }

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
    const juce::Rectangle<float> eqBounds(20.0f, 265.0f, 230.0f, 125.0f);
    if (!audioProcessor.getLicenseManager().isActivated() && eqBounds.contains(e.position))
    {
        draggingFilterHandle = FilterHandle::none;
        rightButtonDrag = false;
        showActivationDialog();
        return;
    }

    auto pos = e.position;
    if      (pos.getDistanceFrom(lowHandlePos())   < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::low;
    else if (pos.getDistanceFrom(bell1HandlePos()) < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::bell1;
    else if (pos.getDistanceFrom(bell2HandlePos()) < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::bell2;
    else if (pos.getDistanceFrom(highHandlePos())  < kFilterHandleHitRadius) draggingFilterHandle = FilterHandle::high;
    else draggingFilterHandle = FilterHandle::none;

    // Left button is always a drag (position, or shift+drag for
    // Q/slope -- see mouseDrag); right button is reserved for a single
    // discrete action on LOW/HIGH: a click (not a drag) toggles Cut/Shelf.
    rightButtonDrag = e.mods.isRightButtonDown();
    slopeDragAccum = 0.0f;

    lastDragPos = pos;

    repaint();
}

void HomeDistoAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (!audioProcessor.getLicenseManager().isActivated()) return;
    if (draggingFilterHandle == FilterHandle::none) return;
    if (rightButtonDrag) return; // right button has no drag gesture -- see mouseUp for its click action

    // Shift+drag adjusts the node's "shape" parameter instead of moving
    // it: Q (bump width) on a bell, SLOPE (roll-off steepness) on
    // LOW/HIGH -- drag up to increase, down to decrease, in both cases.
    // FIX: slope scrubbing used to be a right-drag; moved to shift+drag so
    // it lives on the same button/gesture as the bell Q control, and right
    // stays a single, simple click-only action.
    if (e.mods.isShiftDown())
    {
        if (draggingFilterHandle == FilterHandle::bell1 || draggingFilterHandle == FilterHandle::bell2)
        {
            juce::Slider& qSlider = (draggingFilterHandle == FilterHandle::bell1) ? bell1QSlider : bell2QSlider;
            float deltaY = lastDragPos.y - e.position.y; // positive = dragged up
            float q = juce::jlimit(0.2f, 8.0f, (float) qSlider.getValue() + deltaY * 0.05f);
            qSlider.setValue(q, juce::sendNotificationSync);
            lastDragPos = e.position;
            repaint();
            return;
        }
        if (draggingFilterHandle == FilterHandle::low || draggingFilterHandle == FilterHandle::high)
        {
            float deltaY = lastDragPos.y - e.position.y; // positive = dragged up
            slopeDragAccum += deltaY;
            constexpr float pxPerStep = 26.0f;
            while (slopeDragAccum > pxPerStep)  { cycleSlope(+1); slopeDragAccum -= pxPerStep; }
            while (slopeDragAccum < -pxPerStep) { cycleSlope(-1); slopeDragAccum += pxPerStep; }
            lastDragPos = e.position;
            repaint();
            return;
        }
    }
    lastDragPos = e.position;

    float newFreq = xToFreq(e.position.x);
    float newGain = yToGain(e.position.y);

    // FIX: only LOW vs HIGH was ever prevented from crossing -- BELL1 and
    // BELL2 had no ordering constraint at all (could pass through each
    // other, or past LOW/HIGH), which both looks wrong and kinks the curve
    // backwards since it's drawn low->bell1->bell2->high in that fixed
    // order regardless of where the nodes actually are. Each node is now
    // clamped against its immediate neighbours only (low<bell1<bell2<high),
    // which also transitively keeps low<high without needing a separate
    // direct check.
    constexpr float minRatio = 1.05f;

    switch (draggingFilterHandle)
    {
        case FilterHandle::low:
        {
            float bell1Now = (float) bell1FreqSlider.getValue();
            newFreq = juce::jmin(newFreq, bell1Now / minRatio);
            lowFreqSlider.setValue(newFreq, juce::sendNotificationSync);
            int type = (int) audioProcessor.apvts.getRawParameterValue("EQ_LOW_TYPE")->load();
            if (type == 1) lowGainSlider.setValue(newGain, juce::sendNotificationSync); // Shelf: Y = gain
            break;
        }
        case FilterHandle::bell1:
        {
            float lowNow = (float) lowFreqSlider.getValue();
            float bell2Now = (float) bell2FreqSlider.getValue();
            // Defensive: if low and bell2 ever end up close enough that
            // these bounds would invert, jlimit's min<=max precondition
            // would be violated -- jmin/jmax guarantees valid ordering
            // either way.
            float lo = lowNow * minRatio, hi = bell2Now / minRatio;
            newFreq = juce::jlimit(juce::jmin(lo, hi), juce::jmax(lo, hi), newFreq);
            bell1FreqSlider.setValue(newFreq, juce::sendNotificationSync);
            bell1GainSlider.setValue(newGain, juce::sendNotificationSync);
            break;
        }
        case FilterHandle::bell2:
        {
            float bell1Now = (float) bell1FreqSlider.getValue();
            float highNow = (float) highFreqSlider.getValue();
            float lo = bell1Now * minRatio, hi = highNow / minRatio;
            newFreq = juce::jlimit(juce::jmin(lo, hi), juce::jmax(lo, hi), newFreq);
            bell2FreqSlider.setValue(newFreq, juce::sendNotificationSync);
            bell2GainSlider.setValue(newGain, juce::sendNotificationSync);
            break;
        }
        case FilterHandle::high:
        {
            float bell2Now = (float) bell2FreqSlider.getValue();
            newFreq = juce::jmax(newFreq, bell2Now * minRatio);
            highFreqSlider.setValue(newFreq, juce::sendNotificationSync);
            int type = (int) audioProcessor.apvts.getRawParameterValue("EQ_HIGH_TYPE")->load();
            if (type == 1) highGainSlider.setValue(newGain, juce::sendNotificationSync);
            break;
        }
        default: break;
    }

    repaint();
}

void HomeDistoAudioProcessorEditor::mouseUp (const juce::MouseEvent& e)
{
    if (!audioProcessor.getLicenseManager().isActivated())
    {
        draggingFilterHandle = FilterHandle::none;
        rightButtonDrag = false;
        return;
    }

    // Right button is a single, simple gesture: a click (not a drag) on
    // LOW/HIGH toggles Cut/Shelf. Left button never gets here for that
    // purpose -- it's exclusively drag (position, or shift+drag for
    // Q/slope, handled entirely in mouseDrag).
    bool wasClick = e.getDistanceFromDragStart() < 4;
    if (wasClick && rightButtonDrag && draggingFilterHandle == FilterHandle::low)
        toggleBandType("EQ_LOW_TYPE");
    else if (wasClick && rightButtonDrag && draggingFilterHandle == FilterHandle::high)
        toggleBandType("EQ_HIGH_TYPE");

    draggingFilterHandle = FilterHandle::none;
    rightButtonDrag = false;
    repaint();
}

void HomeDistoAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (!audioProcessor.getLicenseManager().isActivated())
        return;

    // NEW: double-clicking any node resets it to baseline (0 dB) --
    // matches exactly what the EQ reset button does for that one node:
    // LOW/HIGH get their gain zeroed AND their frequency returned to the
    // neutral edge of their range (20 Hz / 20000 Hz, where a Cut is
    // inaudible); bells just get their gain zeroed, keeping whatever
    // freq/Q they were already set to.
    auto pos = e.position;
    auto& ap = audioProcessor.apvts;
    auto setNorm = [&ap](const juce::String& id, float realValue) {
        if (auto* p = ap.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(realValue));
    };

    if (pos.getDistanceFrom(lowHandlePos()) < kFilterHandleHitRadius)
    {
        setNorm("EQ_LOW_FREQ", 20.0f);
        setNorm("EQ_LOW_GAIN", 0.0f);
    }
    else if (pos.getDistanceFrom(bell1HandlePos()) < kFilterHandleHitRadius)
    {
        setNorm("EQ_BELL1_GAIN", 0.0f);
    }
    else if (pos.getDistanceFrom(bell2HandlePos()) < kFilterHandleHitRadius)
    {
        setNorm("EQ_BELL2_GAIN", 0.0f);
    }
    else if (pos.getDistanceFrom(highHandlePos()) < kFilterHandleHitRadius)
    {
        setNorm("EQ_HIGH_FREQ", 20000.0f);
        setNorm("EQ_HIGH_GAIN", 0.0f);
    }
    else return;

    draggingFilterHandle = FilterHandle::none;
    repaint();
}

void HomeDistoAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (!audioProcessor.getLicenseManager().isActivated())
        return;

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

void HomeDistoAudioProcessorEditor::drawTrackedText(juce::Graphics& g, const juce::String& text, float x, float y, float height, float targetWidth, const juce::Font& font)
{
    g.setFont(font);
    if (text.isEmpty()) return;

    // FIX: this previously measured the whole string's width in one call,
    // then advanced character-by-character using EACH character's width
    // measured in isolation. Those two numbers don't actually agree --
    // measuring a glyph on its own (no kerning context) gives a different
    // width than measuring it as part of a string -- so the drawn total
    // silently overshot the target width. Summing the same per-character
    // measurements used for drawing guarantees the two totals match.
    juce::Array<float> charWidths;
    float naturalWidth = 0.0f;
    for (int i = 0; i < text.length(); ++i)
    {
        float w = (float) juce::GlyphArrangement::getStringWidthInt(font, text.substring(i, i + 1));
        charWidths.add(w);
        naturalWidth += w;
    }

    int numGaps = text.length() - 1;
    float extraPerGap = numGaps > 0 ? (targetWidth - naturalWidth) / (float) numGaps : 0.0f;

    float cx = x;
    for (int i = 0; i < text.length(); ++i)
    {
        juce::String ch = text.substring(i, i + 1);
        g.drawText(ch, (int) cx, (int) y, 24, (int) height, juce::Justification::centredLeft);
        cx += charWidths[i] + extraPerGap;
    }
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

    // NEW: soft glass-like highlight along the top edge -- a small detail
    // that keeps the saturated card colours from reading as flat blocks.
    {
        juce::Rectangle<float> sheenArea = bounds.withHeight(bounds.getHeight() * 0.42f);
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.10f), sheenArea.getX(), sheenArea.getY(),
                                    juce::Colours::white.withAlpha(0.0f), sheenArea.getX(), sheenArea.getBottom(), false);
        juce::Path sheenPath;
        sheenPath.addRoundedRectangle(bounds, 6.0f);
        g.saveState();
        g.reduceClipRegion(sheenPath);
        g.setGradientFill(sheen);
        g.fillRect(sheenArea);
        g.restoreState();
    }

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

    // REDESIGNED (again): the embedded logo image was a decorative,
    // hand-drawn circuit-dot display face -- cool as a standalone piece of
    // art, but it clashed with the rest of this interface, which is clean
    // flat Helvetica everywhere else. Back to vector text, but done with
    // more care this time: mixed case (matching how the name actually
    // reads, not shouted in all-caps), one clean weight, tight kerning
    // between the two halves, and a company credit line underneath.
    juce::Font titleFont = juce::FontOptions(26.0f).withName("Helvetica").withStyle("Bold");
    g.setFont(titleFont);

    juce::String homeText = "Home-";
    juce::String distoText = "Disto";
    int homeWidth = juce::GlyphArrangement::getStringWidthInt(titleFont, homeText);
    int distoWidth = juce::GlyphArrangement::getStringWidthInt(titleFont, distoText);

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawText(homeText, 26, 15, homeWidth, 32, juce::Justification::centredLeft);
    g.drawText(distoText, 26 + homeWidth, 15, 110, 32, juce::Justification::centredLeft);

    g.setColour(juce::Colours::white);
    g.drawText(homeText, 25, 14, homeWidth, 32, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xFF00FF87));
    g.drawText(distoText, 25 + homeWidth, 14, 110, 32, juce::Justification::centredLeft);

    // NEW: company credit line underneath. FIX: rather than guessing a
    // number of literal spaces between letters (which only ever
    // coincidentally matches the title's width), this measures the actual
    // "HomeDisto" width and tracks each letter out by the exact amount
    // needed to match it precisely -- exact regardless of what font or
    // text either line ends up using later.
    juce::Font subtitleFont = juce::FontOptions(9.0f).withName("Helvetica").withStyle("Bold");
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    drawTrackedText(g, "DUBTACH DSP", 26.0f, 47.0f, 12.0f, (float) (homeWidth + distoWidth), subtitleFont);

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
    // NEW: small underline beneath each card's primary title -- a detail
    // that gives the label a bit more typographic structure.
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawLine(115.0f, 109.0f, 155.0f, 109.0f, 1.2f);

    // FIX: renamed from FILTER -- this is a real 4-band EQ now (Low
    // Cut/Shelf, 2 Bell bands, High Cut/Shelf), not just a filter.
    drawCardText("EQ", 20, 271, 230, 18, juce::Justification::centred);
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawLine(125.0f, 288.0f, 145.0f, 288.0f, 1.2f);

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

    // NEW: faint inset border around the graph area so it reads as a
    // distinct "scope" panel rather than just floating in the card.
    {
        juce::Rectangle<float> scopeBounds(filterGraphLeft - 4.0f, filterGraphTopY - 4.0f,
                                            (filterGraphRight - filterGraphLeft) + 8.0f,
                                            (filterGraphBottomY - filterGraphTopY) + 8.0f);
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(scopeBounds, 4.0f);
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.drawRoundedRectangle(scopeBounds, 4.0f, 1.0f);
    }

    // NEW: faint frequency reference gridlines at 100 Hz / 1 kHz / 10 kHz --
    // a small, standard EQ-plugin detail, kept deliberately subtle (very
    // low alpha, no labels) so it reads as texture rather than clutter.
    g.setColour(juce::Colours::black.withAlpha(0.08f));
    for (float refHz : { 100.0f, 1000.0f, 10000.0f })
    {
        float gx = freqToX(refHz);
        g.drawLine(gx, filterGraphTopY, gx, filterGraphBottomY, 1.0f);
    }

    // 0 dB reference line.
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.drawLine(filterGraphLeft, filterGraphMidY, filterGraphRight, filterGraphMidY, 1.5f);

    // FIX: this used to be a handful of fixed Bezier control points based
    // only on node position, which is exactly why changing a bell's Q did
    // nothing visible -- Q was never part of the curve math at all. This
    // instead samples the actual combined dB response across the frequency
    // axis (summing each band's contribution in dB, the same way real EQ
    // plugins draw their curves) and connects the samples -- so Q, gain,
    // and slope all visibly shape the line now, not just node position.
    int slopeIdx = juce::jlimit(0, 2, (int) audioProcessor.apvts.getRawParameterValue("SLOPE")->load());
    float slopeDbPerOct = 12.0f * (float) (slopeIdx == 0 ? 1 : (slopeIdx == 1 ? 2 : 4));

    float lowFreqHz  = (float) lowFreqSlider.getValue();
    float lowGainDb  = (float) lowGainSlider.getValue();
    float highFreqHz = (float) highFreqSlider.getValue();
    float highGainDb = (float) highGainSlider.getValue();
    float bell1FreqHz = (float) bell1FreqSlider.getValue();
    float bell1GainDb = (float) bell1GainSlider.getValue();
    float bell1QVal    = (float) bell1QSlider.getValue();
    float bell2FreqHz = (float) bell2FreqSlider.getValue();
    float bell2GainDb = (float) bell2GainSlider.getValue();
    float bell2QVal    = (float) bell2QSlider.getValue();

    // Gaussian-in-octaves bump: higher Q -> narrower bandwidth -> tighter,
    // more surgical peak. Lower Q -> wide, gentle bump. Matches the
    // shift-drag/scroll-wheel Q control directly.
    auto bellContribution = [](float hz, float centerHz, float gainDb, float q) {
        float octaves = std::log2(hz / centerHz);
        float bandwidthOct = 1.0f / juce::jmax(0.15f, q);
        float t = octaves / juce::jmax(0.05f, bandwidthOct);
        return gainDb * std::exp(-t * t);
    };

    juce::Path eqCurve;
    const int numSamples = 120;
    for (int i = 0; i <= numSamples; ++i)
    {
        float x = filterGraphLeft + (filterGraphRight - filterGraphLeft) * (float) i / (float) numSamples;
        float hz = xToFreq(x);
        float gainDb = 0.0f;

        if (lowType == 0) // Cut: real slope-based roll-off below the corner
        {
            if (hz < lowFreqHz)
                gainDb += -slopeDbPerOct * std::log2(lowFreqHz / hz);
        }
        else // Shelf: smooth sigmoid transition centred on the corner freq
        {
            float t = 1.0f / (1.0f + std::exp(std::log2(hz / lowFreqHz) * 3.0f));
            gainDb += lowGainDb * t;
        }

        if (highType == 0) // Cut
        {
            if (hz > highFreqHz)
                gainDb += -slopeDbPerOct * std::log2(hz / highFreqHz);
        }
        else // Shelf
        {
            float t = 1.0f / (1.0f + std::exp(-std::log2(hz / highFreqHz) * 3.0f));
            gainDb += highGainDb * t;
        }

        gainDb += bellContribution(hz, bell1FreqHz, bell1GainDb, bell1QVal);
        gainDb += bellContribution(hz, bell2FreqHz, bell2GainDb, bell2QVal);

        float y = gainToY(gainDb); // already clamps to the +/-18dB graph range
        if (i == 0) eqCurve.startNewSubPath(x, y);
        else eqCurve.lineTo(x, y);
    }

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
        static const char* slopeLabels[3] = { "12dB/oct", "24dB/oct", "48dB/oct" };
        juce::String slopeLabel = slopeLabels[slopeIdx];

        if (shownHandle == FilterHandle::low)
        {
            pt = lowPt;
            text = getFrequencyString((float) lowFreqSlider.getValue());
            if (lowType == 1) text += " / " + juce::String((float) lowGainSlider.getValue(), 1) + " dB";
            else text += " (Cut, " + slopeLabel + ")";
        }
        else if (shownHandle == FilterHandle::high)
        {
            pt = highPt;
            text = getFrequencyString((float) highFreqSlider.getValue());
            if (highType == 1) text += " / " + juce::String((float) highGainSlider.getValue(), 1) + " dB";
            else text += " (Cut, " + slopeLabel + ")";
        }
        else if (shownHandle == FilterHandle::bell1)
        {
            pt = bell1Pt;
            text = getFrequencyString((float) bell1FreqSlider.getValue()) + " / "
                 + juce::String((float) bell1GainSlider.getValue(), 1) + " dB / Q "
                 + juce::String((float) bell1QSlider.getValue(), 2);
        }
        else
        {
            pt = bell2Pt;
            text = getFrequencyString((float) bell2FreqSlider.getValue()) + " / "
                 + juce::String((float) bell2GainSlider.getValue(), 1) + " dB / Q "
                 + juce::String((float) bell2QSlider.getValue(), 2);
        }

        g.setFont(juce::FontOptions(11.0f).withName("Helvetica").withStyle("Bold"));
        float textWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 14.0f;
        juce::Rectangle<float> bubble(pt.x - textWidth * 0.5f, pt.y - 26.0f, textWidth, 16.0f);
        bubble = bubble.constrainedWithin(juce::Rectangle<float>(filterGraphLeft, 272.0f, filterGraphRight - filterGraphLeft, 120.0f));

        g.setColour(juce::Colour(0xFF161618));
        g.fillRoundedRectangle(bubble, 3.0f);
        g.setColour(juce::Colour(0xFF00FF87).withAlpha(0.5f));
        g.drawRoundedRectangle(bubble, 3.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.drawText(text, bubble, juce::Justification::centred);
    }

    g.setFont(juce::FontOptions(14.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("DRIVE", 260, 90, 260, 20, juce::Justification::centred); 
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawLine(370.0f, 109.0f, 410.0f, 109.0f, 1.2f);
    drawCardText("TONE", 285, 260, 80, 20, juce::Justification::centred);
    drawCardText("PUNCH", 415, 260, 80, 20, juce::Justification::centred);

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("DARK",  275, 360, 40, 15, juce::Justification::left);
    drawCardText("BRIGHT", 335, 360, 40, 15, juce::Justification::right);
    drawCardText("SOFT",  405, 360, 40, 15, juce::Justification::left);
    drawCardText("HARD",   465, 360, 40, 15, juce::Justification::right);

    g.setFont(juce::FontOptions(14.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("OUTPUT", 545, 90, 70, 20, juce::Justification::left);
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawLine(545.0f, 109.0f, 598.0f, 109.0f, 1.2f);
    drawCardText("MIX", 580, 260, 70, 20, juce::Justification::centred); 

    g.setFont(juce::FontOptions(10.0f).withName("Helvetica").withStyle("Bold"));
    drawCardText("DRY", 565, 360, 30, 15, juce::Justification::left);
    drawCardText("WET", 635, 360, 30, 15, juce::Justification::right);
}

void HomeDistoAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    if (!audioProcessor.getLicenseManager().isActivated())
    {
        const juce::Rectangle<float> eqBounds(20.0f, 265.0f, 230.0f, 125.0f);

        g.setColour(juce::Colour(0xFF09090B).withAlpha(0.92f));
        g.fillRoundedRectangle(eqBounds, 6.0f);
        g.setColour(juce::Colour(0xFF00FF87).withAlpha(0.35f));
        g.drawRoundedRectangle(eqBounds.reduced(1.0f), 6.0f, 1.5f);

        const auto centre = eqBounds.getCentre();
        g.setColour(juce::Colour(0xFF00FF87));
        juce::Path shackle;
        shackle.addCentredArc(centre.x, centre.y - 8.0f, 8.0f, 8.0f, 0.0f,
                              juce::MathConstants<float>::pi,
                              juce::MathConstants<float>::twoPi, true);
        g.strokePath(shackle, juce::PathStrokeType(2.2f));
        g.fillRoundedRectangle(centre.x - 10.0f, centre.y - 1.0f, 20.0f, 16.0f, 3.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(18.0f).withName("Helvetica").withStyle("Bold"));
        g.drawText("EQ LOCKED", 35, (int) centre.y + 20, 200, 24, juce::Justification::centred);

        g.setColour(juce::Colours::white.withAlpha(0.62f));
        g.setFont(juce::FontOptions(10.5f).withName("Helvetica").withStyle("Bold"));
        g.drawText("Activate your copy to unlock the EQ", 32, (int) centre.y + 46, 206, 18, juce::Justification::centred);
        g.setColour(juce::Colour(0xFF00FF87).withAlpha(0.9f));
        g.drawText("CLICK TO ENTER CODE", 35, (int) centre.y + 68, 200, 18, juce::Justification::centred);
    }

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
    licenseButton.setBounds(584, 20, 30, 30);
    settingsButton.setBounds(620, 20, 30, 30);
    bypassButton.setBounds(660, 20, 30, 30); 

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

    // EQ reset/lock icons, top-right corner of the EQ card (title text is
    // centred and short, so the corners are clear). 18x18, independent
    // (no shared backdrop -- that made on/off harder to tell apart, not
    // easier).
    eqResetButton.setBounds(202, 268, 18, 18);
    eqLockButton.setBounds(222, 268, 18, 18);

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