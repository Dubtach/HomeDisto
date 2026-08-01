#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace FactoryPresets
{
    // Sets a single parameter's stored value directly on an APVTS-shaped
    // ValueTree copy (root "Parameters", children are <PARAM id=".." value=".."/>).
    inline void setTreeParam(juce::ValueTree& tree, const juce::String& id, float value)
    {
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto child = tree.getChild(i);
            if (child.getProperty("id").toString() == id)
            {
                child.setProperty("value", value, nullptr);
                return;
            }
        }
    }

    inline void generateDefaults(HomeDistoAudioProcessor& processor)
    {
        // FIX: this used to generate presets by repeatedly mutating the LIVE,
        // host-connected APVTS via setValueNotifyingHost() -- once per
        // parameter, across all ~30 factory presets -- then restoring the
        // prior state afterward via setStateInformation(). That fires real
        // host parameter-change notifications during plugin construction,
        // before the host has even finished instantiating the plugin, which
        // some DAWs log as automation/undo activity. It also did a full
        // state save + apply + restore cycle 30 times on first load.
        //
        // Presets are now built entirely off-line: take one snapshot of the
        // default parameter tree, then for each preset work on an in-memory
        // copy of it and write that copy straight to disk. The live
        // processor/host-facing state is never touched.

        juce::ValueTree baselineTree = processor.apvts.copyState();

        // Scratch tree that setParam/setMode below write into; pointed at a
        // fresh copy of baselineTree for the duration of each configureLambda.
        juce::ValueTree* currentTree = nullptr;

        auto createDefault = [&](const juce::String& category, const juce::String& name, std::function<void()> configureLambda)
        {
            juce::File categoryDir = processor.getPresetDirectory().getChildFile(category);
            if (!categoryDir.exists())
                categoryDir.createDirectory();

            juce::File presetFile = categoryDir.getChildFile(name + ".xml");
            if (!presetFile.existsAsFile())
            {
                juce::ValueTree presetTree = baselineTree.createCopy();
                currentTree = &presetTree;

                configureLambda();

                currentTree = nullptr;

                if (auto xml = presetTree.createXml())
                    xml->writeTo(presetFile);
            }
        };

        auto setParam = [&](const juce::String& id, float realValue) {
            if (currentTree != nullptr)
                setTreeParam(*currentTree, id, realValue);
        };

        auto setMode = [&](float modeIndex) {
            // AudioParameterChoice stores its actual (denormalized) value as
            // the choice index itself, so this is the index directly -- no
            // 0-1 normalization needed here.
            if (currentTree != nullptr)
                setTreeParam(*currentTree, "MODE", modeIndex);
        };

        // ==========================================
        // 01-05: MASTERING & MIX BUS
        // ==========================================
        createDefault("01. Mastering & Bus", "01. Bus - Master Glue", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.0f); setParam("MIX", 0.25f);
            setParam("TONE", 0.05f); setParam("PUNCH", 0.1f);
            setParam("LOW_CUT", 20.0f); setParam("HIGH_CUT", 20000.0f);
        });
        createDefault("01. Mastering & Bus", "02. Bus - Subtle Tube Heat", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 4.5f); setParam("MIX", 0.3f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.0f);
            setParam("LOW_CUT", 25.0f); setParam("HIGH_CUT", 18500.0f);
        });
        createDefault("01. Mastering & Bus", "03. Bus - Vintage Console", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 6.0f); setParam("MIX", 0.15f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.2f);
            setParam("LOW_CUT", 30.0f); setParam("HIGH_CUT", 16000.0f);
        });
        createDefault("01. Mastering & Bus", "04. Bus - Tape Mojo", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 5.0f); setParam("MIX", 0.4f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.0f);
            setParam("LOW_CUT", 25.0f); setParam("HIGH_CUT", 15000.0f);
        });
        createDefault("01. Mastering & Bus", "05. Bus - Mastering Punch", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 4.0f); setParam("MIX", 0.2f);
            setParam("TONE", 0.15f); setParam("PUNCH", 0.4f);
            setParam("LOW_CUT", 20.0f); setParam("HIGH_CUT", 20000.0f);
        });

        // ==========================================
        // 06-10: VOCALS
        // ==========================================
        createDefault("02. Vocals", "06. Vocal - Tube Presence", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 4.5f); setParam("MIX", 0.4f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.1f);
            setParam("LOW_CUT", 80.0f); setParam("HIGH_CUT", 20000.0f);
        });
        createDefault("02. Vocals", "07. Vocal - Saturation Air", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.5f); setParam("MIX", 0.5f);
            setParam("TONE", 0.4f); setParam("PUNCH", 0.0f);
            setParam("LOW_CUT", 95.0f); setParam("HIGH_CUT", 20000.0f);
        });
        createDefault("02. Vocals", "08. Vocal - Smooth Overdrive", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 6.0f); setParam("MIX", 0.45f);
            setParam("TONE", -0.05f); setParam("PUNCH", 0.2f);
            setParam("LOW_CUT", 100.0f); setParam("HIGH_CUT", 14000.0f);
        });
        createDefault("02. Vocals", "09. Vocal - Aggressive Rap", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 8.0f); setParam("MIX", 0.35f);
            setParam("TONE", 0.3f); setParam("PUNCH", 0.5f);
            setParam("LOW_CUT", 75.0f); setParam("HIGH_CUT", 18000.0f);
        });
        createDefault("02. Vocals", "10. Vocal - Lo-Fi Radio", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 14.0f); setParam("MIX", 0.8f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.1f);
            setParam("LOW_CUT", 400.0f); setParam("HIGH_CUT", 3500.0f);
        });

        // ==========================================
        // 11-15: BASS
        // ==========================================
        createDefault("03. Bass", "11. Bass - Modern Grunt", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 0.6f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.8f);
            setParam("LOW_CUT", 40.0f); setParam("HIGH_CUT", 8000.0f);
        });
        createDefault("03. Bass", "12. Bass - Vintage Amp", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 12.0f); setParam("MIX", 0.55f);
            setParam("TONE", -0.2f); setParam("PUNCH", 0.3f);
            setParam("LOW_CUT", 35.0f); setParam("HIGH_CUT", 5500.0f);
        });
        createDefault("03. Bass", "13. Bass - Parallel Smash", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 20.0f); setParam("MIX", 0.25f);
            setParam("TONE", 0.2f); setParam("PUNCH", 1.0f);
            setParam("LOW_CUT", 20.0f); setParam("HIGH_CUT", 20000.0f);
        });
        createDefault("03. Bass", "14. Bass - Fuzz Sub", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 18.0f); setParam("MIX", 0.45f);
            setParam("TONE", -0.4f); setParam("PUNCH", 0.1f);
            setParam("LOW_CUT", 20.0f); setParam("HIGH_CUT", 3000.0f);
        });
        createDefault("03. Bass", "15. Bass - Tube Exciter", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 8.0f); setParam("MIX", 0.4f);
            setParam("TONE", 0.3f); setParam("PUNCH", 0.2f);
            setParam("LOW_CUT", 50.0f); setParam("HIGH_CUT", 12000.0f);
        });

        // ==========================================
        // 16-20: GUITARS
        // ==========================================
        createDefault("04. Guitars", "16. Guitar - Classic Overdrive", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 14.0f); setParam("MIX", 1.0f);
            setParam("TONE", 0.15f); setParam("PUNCH", 0.3f);
            setParam("LOW_CUT", 80.0f); setParam("HIGH_CUT", 6500.0f);
        });
        createDefault("04. Guitars", "17. Guitar - Heavy Chug", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 18.0f); setParam("MIX", 1.0f);
            setParam("TONE", 0.25f); setParam("PUNCH", 0.7f);
            setParam("LOW_CUT", 100.0f); setParam("HIGH_CUT", 8000.0f);
        });
        createDefault("04. Guitars", "18. Guitar - Doom Fuzz", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", -0.3f); setParam("PUNCH", 0.8f);
            setParam("LOW_CUT", 30.0f); setParam("HIGH_CUT", 4500.0f);
        });
        createDefault("04. Guitars", "19. Guitar - Screaming Lead", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 16.0f); setParam("MIX", 0.9f);
            setParam("TONE", 0.4f); setParam("PUNCH", 0.5f);
            setParam("LOW_CUT", 120.0f); setParam("HIGH_CUT", 9500.0f);
        });
        createDefault("04. Guitars", "20. Guitar - Tape Fuzz", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.2f);
            setParam("LOW_CUT", 60.0f); setParam("HIGH_CUT", 6000.0f);
        });

        // ==========================================
        // 21-25: DRUMS
        // ==========================================
        createDefault("05. Drums", "21. Drums - Parallel Snap", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 18.0f); setParam("MIX", 0.25f);
            setParam("TONE", 0.3f); setParam("PUNCH", 1.0f);
            setParam("LOW_CUT", 20.0f); setParam("HIGH_CUT", 20000.0f);
        });
        createDefault("05. Drums", "22. Drums - Bus Crush", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 12.0f); setParam("MIX", 0.3f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.9f);
            setParam("LOW_CUT", 30.0f); setParam("HIGH_CUT", 16000.0f);
        });
        createDefault("05. Drums", "23. Drums - Snare Crackifier", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 0.4f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.6f);
            setParam("LOW_CUT", 150.0f); setParam("HIGH_CUT", 12000.0f);
        });
        createDefault("05. Drums", "24. Drums - Kick Sub Maker", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 6.0f); setParam("MIX", 0.4f);
            setParam("TONE", -0.3f); setParam("PUNCH", 0.2f);
            setParam("LOW_CUT", 20.0f); setParam("HIGH_CUT", 500.0f);
        });
        createDefault("05. Drums", "25. Drums - Breakbeat Trash", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 15.0f); setParam("MIX", 0.65f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.4f);
            setParam("LOW_CUT", 150.0f); setParam("HIGH_CUT", 7000.0f);
        });

        // ==========================================
        // 26-30: SYNTHS & CREATIVE FX
        // ==========================================
        createDefault("06. Synths & FX", "26. Synths - Wavefold Lead", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 15.0f); setParam("MIX", 0.8f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.3f);
            setParam("LOW_CUT", 60.0f); setParam("HIGH_CUT", 15000.0f);
        });
        createDefault("06. Synths & FX", "27. Synths - Analog Warm Pad", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 7.0f); setParam("MIX", 0.5f);
            setParam("TONE", -0.15f); setParam("PUNCH", 0.0f);
            setParam("LOW_CUT", 60.0f); setParam("HIGH_CUT", 10000.0f);
        });
        createDefault("06. Synths & FX", "28. Synths - Acid Screamer", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 16.0f); setParam("MIX", 0.8f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.7f);
            setParam("LOW_CUT", 80.0f); setParam("HIGH_CUT", 18000.0f);
        });
        createDefault("06. Synths & FX", "29. FX - 8-Bit Chiptune", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", 0.8f); setParam("PUNCH", 1.0f);
            setParam("LOW_CUT", 150.0f); setParam("HIGH_CUT", 8000.0f);
        });
        createDefault("06. Synths & FX", "30. FX - Total Destruction", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", -0.8f); setParam("PUNCH", 1.0f);
            setParam("LOW_CUT", 200.0f); setParam("HIGH_CUT", 4000.0f);
        });
    }
}