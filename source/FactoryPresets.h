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
        // Presets are built entirely off-line: take one snapshot of the
        // default parameter tree, then for each preset work on an in-memory
        // copy of it and write that copy straight to disk. The live
        // processor/host-facing state is never touched (see git history for
        // why -- this used to mutate the live, host-connected APVTS during
        // construction, which is unsafe).

        juce::ValueTree baselineTree = processor.apvts.copyState();
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

        // Small helper for the 4-band EQ so every preset call reads the
        // same way: low/high can be Cut (type 0) or Shelf (type 1, with a
        // gain); bells are always freq/gain/Q.
        auto setLow = [&](float freq, bool shelf, float gainDb) {
            setParam("EQ_LOW_FREQ", freq);
            setParam("EQ_LOW_TYPE", shelf ? 1.0f : 0.0f);
            setParam("EQ_LOW_GAIN", gainDb);
        };
        auto setHigh = [&](float freq, bool shelf, float gainDb) {
            setParam("EQ_HIGH_FREQ", freq);
            setParam("EQ_HIGH_TYPE", shelf ? 1.0f : 0.0f);
            setParam("EQ_HIGH_GAIN", gainDb);
        };
        auto setBell1 = [&](float freq, float gainDb, float q) {
            setParam("EQ_BELL1_FREQ", freq); setParam("EQ_BELL1_GAIN", gainDb); setParam("EQ_BELL1_Q", q);
        };
        auto setBell2 = [&](float freq, float gainDb, float q) {
            setParam("EQ_BELL2_FREQ", freq); setParam("EQ_BELL2_GAIN", gainDb); setParam("EQ_BELL2_Q", q);
        };

        // ==========================================
        // 01-05: MASTERING & MIX BUS
        // Subtle by design -- bus/mastering presets should glue and add
        // character without announcing themselves. Small moves, gentle Qs.
        // ==========================================
        createDefault("01. Mastering & Bus", "01. Bus - Master Glue", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.0f); setParam("MIX", 0.25f);
            setParam("TONE", 0.05f); setParam("PUNCH", 0.1f);
            setLow(25.0f, false, 0.0f);
            setBell1(300.0f, -1.5f, 0.7f);   // clear a touch of low-mid buildup
            setBell2(9000.0f, 1.0f, 0.6f);   // gentle air
            setHigh(20000.0f, false, 0.0f);
        });
        createDefault("01. Mastering & Bus", "02. Bus - Subtle Tube Heat", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 4.5f); setParam("MIX", 0.3f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.0f);
            setLow(30.0f, false, 0.0f);
            setBell1(180.0f, 1.0f, 0.7f);    // warmth
            setBell2(6000.0f, 0.5f, 0.6f);
            setHigh(18500.0f, true, 1.0f);   // gentle top-end lift
        });
        createDefault("01. Mastering & Bus", "03. Bus - Vintage Console", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 6.0f); setParam("MIX", 0.15f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.2f);
            setLow(35.0f, true, 1.0f);       // console low-end weight
            setBell1(120.0f, 1.5f, 0.7f);
            setBell2(3000.0f, 1.0f, 0.7f);
            setHigh(16000.0f, true, -1.5f);  // vintage darkness
        });
        createDefault("01. Mastering & Bus", "04. Bus - Tape Mojo", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 5.0f); setParam("MIX", 0.4f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.0f);
            setLow(28.0f, false, 0.0f);
            setBell1(150.0f, 1.0f, 0.7f);
            setBell2(8000.0f, -0.5f, 0.6f);
            setHigh(15000.0f, true, -2.0f);  // classic tape rolloff
        });
        createDefault("01. Mastering & Bus", "05. Bus - Mastering Punch", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 4.0f); setParam("MIX", 0.2f);
            setParam("TONE", 0.15f); setParam("PUNCH", 0.4f);
            setLow(22.0f, false, 0.0f);
            setBell1(400.0f, -1.0f, 0.8f);
            setBell2(5000.0f, 1.5f, 0.7f);   // punch/presence lift
            setHigh(20000.0f, true, 1.0f);
        });

        // ==========================================
        // 06-10: VOCALS
        // Rumble control below, presence bells in the 3-5kHz range where
        // vocals cut through, air on top where flattering.
        // ==========================================
        createDefault("02. Vocals", "06. Vocal - Tube Presence", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 4.5f); setParam("MIX", 0.4f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.1f);
            setLow(90.0f, false, 0.0f);
            setBell1(300.0f, -2.0f, 0.8f);   // clear boxiness
            setBell2(4000.0f, 3.0f, 1.0f);   // presence
            setHigh(20000.0f, true, 2.0f);   // air
        });
        createDefault("02. Vocals", "07. Vocal - Saturation Air", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.5f); setParam("MIX", 0.5f);
            setParam("TONE", 0.4f); setParam("PUNCH", 0.0f);
            setLow(95.0f, false, 0.0f);
            setBell1(250.0f, -1.5f, 0.8f);
            setBell2(5000.0f, 2.0f, 0.9f);
            setHigh(11000.0f, true, 3.0f);   // signature airy top
        });
        createDefault("02. Vocals", "08. Vocal - Smooth Overdrive", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 6.0f); setParam("MIX", 0.45f);
            setParam("TONE", -0.05f); setParam("PUNCH", 0.2f);
            setLow(100.0f, false, 0.0f);
            setBell1(350.0f, -2.0f, 0.8f);
            setBell2(3000.0f, 2.0f, 0.8f);
            setHigh(14000.0f, false, 0.0f);
        });
        createDefault("02. Vocals", "09. Vocal - Aggressive Rap", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 8.0f); setParam("MIX", 0.35f);
            setParam("TONE", 0.3f); setParam("PUNCH", 0.5f);
            setLow(80.0f, false, 0.0f);
            setBell1(1000.0f, 1.5f, 0.9f);   // bite
            setBell2(4500.0f, 2.5f, 1.0f);   // edge
            setHigh(18000.0f, true, 2.0f);
        });
        createDefault("02. Vocals", "10. Vocal - Lo-Fi Radio", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 14.0f); setParam("MIX", 0.8f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.1f);
            setLow(400.0f, false, 0.0f);     // real bandpass -- keep as Cut
            setBell1(1500.0f, 3.0f, 1.6f);   // telephone honk
            setBell2(2800.0f, 1.5f, 1.4f);
            setHigh(3500.0f, false, 0.0f);
        });

        // ==========================================
        // 11-15: BASS
        // Low SHELF (not cut) where weight matters -- bass presets need
        // sub-boost, not just a highpass. Mud cut ~300-500Hz, growl/definition
        // bell ~800Hz-1.2kHz.
        // ==========================================
        createDefault("03. Bass", "11. Bass - Modern Grunt", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 0.6f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.8f);
            setLow(60.0f, true, 3.0f);       // sub weight
            setBell1(400.0f, -2.5f, 0.8f);   // mud
            setBell2(900.0f, 2.0f, 1.0f);    // growl
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("03. Bass", "12. Bass - Vintage Amp", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 12.0f); setParam("MIX", 0.55f);
            setParam("TONE", -0.2f); setParam("PUNCH", 0.3f);
            setLow(50.0f, true, 2.0f);
            setBell1(200.0f, 1.5f, 0.7f);
            setBell2(700.0f, -1.0f, 0.9f);
            setHigh(5500.0f, false, 0.0f);
        });
        createDefault("03. Bass", "13. Bass - Parallel Smash", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 20.0f); setParam("MIX", 0.25f);
            setParam("TONE", 0.2f); setParam("PUNCH", 1.0f);
            setLow(20.0f, false, 0.0f);      // full range -- this is a parallel blend
            setBell1(100.0f, 2.0f, 0.8f);
            setBell2(3000.0f, 2.0f, 1.0f);   // aggressive top for the blend to cut through
            setHigh(20000.0f, false, 0.0f);
        });
        createDefault("03. Bass", "14. Bass - Fuzz Sub", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 18.0f); setParam("MIX", 0.45f);
            setParam("TONE", -0.4f); setParam("PUNCH", 0.1f);
            setLow(30.0f, true, 2.0f);
            setBell1(80.0f, 2.0f, 0.7f);
            setBell2(800.0f, -2.0f, 0.9f);   // declutter fuzz mids
            setHigh(3000.0f, false, 0.0f);
        });
        createDefault("03. Bass", "15. Bass - Tube Exciter", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 8.0f); setParam("MIX", 0.4f);
            setParam("TONE", 0.3f); setParam("PUNCH", 0.2f);
            setLow(50.0f, false, 0.0f);
            setBell1(250.0f, 1.0f, 0.7f);
            setBell2(2500.0f, 2.5f, 1.0f);   // exciter harmonics
            setHigh(12000.0f, false, 0.0f);
        });

        // ==========================================
        // 16-20: GUITARS
        // Low cut to clear amp rumble, presence bell 2.5-3.5kHz for cut-
        // through, high cut to tame fizz depending on how hot the drive is.
        // ==========================================
        createDefault("04. Guitars", "16. Guitar - Classic Overdrive", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 14.0f); setParam("MIX", 1.0f);
            setParam("TONE", 0.15f); setParam("PUNCH", 0.3f);
            setLow(80.0f, false, 0.0f);
            setBell1(400.0f, -1.5f, 0.8f);
            setBell2(2500.0f, 2.0f, 0.9f);
            setHigh(6500.0f, false, 0.0f);
        });
        createDefault("04. Guitars", "17. Guitar - Heavy Chug", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 18.0f); setParam("MIX", 1.0f);
            setParam("TONE", 0.25f); setParam("PUNCH", 0.7f);
            setLow(100.0f, false, 0.0f);
            setBell1(250.0f, -2.0f, 0.8f);   // tighten
            setBell2(3500.0f, 2.0f, 1.0f);   // pick attack
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("04. Guitars", "18. Guitar - Doom Fuzz", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", -0.3f); setParam("PUNCH", 0.8f);
            setLow(50.0f, true, 2.0f);       // heavy low-end shelf
            setBell1(150.0f, 1.5f, 0.7f);
            setBell2(1000.0f, -1.5f, 0.9f);  // classic doom mid-scoop
            setHigh(4500.0f, false, 0.0f);
        });
        createDefault("04. Guitars", "19. Guitar - Screaming Lead", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 16.0f); setParam("MIX", 0.9f);
            setParam("TONE", 0.4f); setParam("PUNCH", 0.5f);
            setLow(120.0f, false, 0.0f);
            setBell1(500.0f, -2.0f, 0.8f);
            setBell2(3000.0f, 3.0f, 1.2f);   // scream presence
            setHigh(9500.0f, false, 0.0f);
        });
        createDefault("04. Guitars", "20. Guitar - Tape Fuzz", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.2f);
            setLow(60.0f, false, 0.0f);
            setBell1(200.0f, 1.0f, 0.7f);
            setBell2(2000.0f, -1.0f, 0.9f);
            setHigh(6000.0f, false, 0.0f);
        });

        // ==========================================
        // 21-25: DRUMS
        // Very element-specific: kick wants a real low shelf, snare wants
        // crack around 4-6kHz, buses stay closer to full-range.
        // ==========================================
        createDefault("05. Drums", "21. Drums - Parallel Snap", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 18.0f); setParam("MIX", 0.25f);
            setParam("TONE", 0.3f); setParam("PUNCH", 1.0f);
            setLow(20.0f, false, 0.0f);
            setBell1(150.0f, 2.0f, 0.8f);    // thump
            setBell2(5000.0f, 3.0f, 1.1f);   // snap/click
            setHigh(20000.0f, false, 0.0f);
        });
        createDefault("05. Drums", "22. Drums - Bus Crush", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 12.0f); setParam("MIX", 0.3f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.9f);
            setLow(30.0f, false, 0.0f);
            setBell1(400.0f, -1.0f, 0.8f);
            setBell2(6000.0f, 2.0f, 1.0f);
            setHigh(16000.0f, false, 0.0f);
        });
        createDefault("05. Drums", "23. Drums - Snare Crackifier", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 0.4f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.6f);
            setLow(150.0f, false, 0.0f);
            setBell1(250.0f, 2.0f, 0.8f);    // body
            setBell2(4500.0f, 3.0f, 1.1f);   // crack
            setHigh(12000.0f, false, 0.0f);
        });
        createDefault("05. Drums", "24. Drums - Kick Sub Maker", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 6.0f); setParam("MIX", 0.4f);
            setParam("TONE", -0.3f); setParam("PUNCH", 0.2f);
            setLow(45.0f, true, 4.0f);       // real sub-boost shelf, not just a cut
            setBell1(80.0f, 2.0f, 0.7f);
            setBell2(2500.0f, 1.0f, 1.0f);   // beater click
            setHigh(500.0f, false, 0.0f);
        });
        createDefault("05. Drums", "25. Drums - Breakbeat Trash", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 15.0f); setParam("MIX", 0.65f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.4f);
            setLow(150.0f, false, 0.0f);
            setBell1(300.0f, 2.0f, 0.8f);
            setBell2(3000.0f, 2.5f, 1.0f);
            setHigh(7000.0f, false, 0.0f);
        });

        // ==========================================
        // 26-30: SYNTHS & CREATIVE FX
        // More latitude for character moves -- these are meant to be
        // opinionated starting points, not neutral ones.
        // ==========================================
        createDefault("06. Synths & FX", "26. Synths - Wavefold Lead", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 15.0f); setParam("MIX", 0.8f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.3f);
            setLow(60.0f, false, 0.0f);
            setBell1(300.0f, -1.0f, 0.8f);
            setBell2(3000.0f, 2.0f, 1.0f);
            setHigh(15000.0f, false, 0.0f);
        });
        createDefault("06. Synths & FX", "27. Synths - Analog Warm Pad", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 7.0f); setParam("MIX", 0.5f);
            setParam("TONE", -0.15f); setParam("PUNCH", 0.0f);
            setLow(60.0f, true, 1.5f);
            setBell1(200.0f, 1.0f, 0.7f);
            setBell2(4000.0f, -1.0f, 0.8f);  // soften
            setHigh(10000.0f, false, 0.0f);
        });
        createDefault("06. Synths & FX", "28. Synths - Acid Screamer", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 16.0f); setParam("MIX", 0.8f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.7f);
            setLow(80.0f, false, 0.0f);
            setBell1(1000.0f, 2.0f, 2.0f);   // resonant emphasis
            setBell2(5000.0f, 2.0f, 1.2f);
            setHigh(18000.0f, false, 0.0f);
        });
        createDefault("06. Synths & FX", "29. FX - 8-Bit Chiptune", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", 0.8f); setParam("PUNCH", 1.0f);
            setLow(150.0f, false, 0.0f);
            setBell1(1500.0f, 2.0f, 1.2f);
            setBell2(4000.0f, 2.0f, 1.2f);
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("06. Synths & FX", "30. FX - Total Destruction", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f);
            setParam("TONE", -0.8f); setParam("PUNCH", 1.0f);
            setLow(80.0f, true, 3.0f);
            setBell1(200.0f, 3.0f, 0.8f);
            setBell2(2000.0f, -2.0f, 1.0f);
            setHigh(4000.0f, false, 0.0f);
        });
    }
}