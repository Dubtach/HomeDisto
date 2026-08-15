#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace FactoryPresets
{
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
            if (currentTree != nullptr)
                setTreeParam(*currentTree, "MODE", modeIndex);
        };
        // FIX: MIX is now always 100% (Decapitator-style -- the effect IS
        // the sound, not a blend), so every preset's OUT trim needed
        // recalculating: the old trims assumed each preset's own (often
        // much lower) MIX amount diluting the wet signal, which no longer
        // applies once wet is the only signal.
        //
        // Rather than re-estimating loudness by hand again, every OUT value
        // below is computed from the exact same formula AUTO itself uses
        // (see HomeDistoAudioProcessorEditor::computeAutoCompDb) applied to
        // that preset's own DRIVE/TONE/PUNCH/MODE. This isn't just
        // convenient -- it's self-consistent by construction: since AUTO is
        // also on by default now, and recalibrates its baseline right after
        // a preset loads (see recalibrateAutoBaseline()), a preset's stored
        // OUT and AUTO's live baseline are mathematically identical the
        // moment it loads. There's no jump between "what the preset baked
        // in" and "what AUTO would compute live" because they're the same
        // number by definition.
        auto setOut = [&](float outDb) {
            setParam("OUT", outDb);

            // Refine the original factory library without changing its core
            // voicing: explicitly set the newer quality controls so factory
            // presets do not inherit whatever defaults happen to be active.
            auto getCurrent = [&](const juce::String& id, float fallback) {
                if (currentTree == nullptr)
                    return fallback;

                for (int i = 0; i < currentTree->getNumChildren(); ++i)
                {
                    auto child = currentTree->getChild(i);
                    if (child.getProperty("id").toString() == id)
                        return (float) child.getProperty("value");
                }

                return fallback;
            };

            const float drive = getCurrent("DRIVE", 6.0f);
            const int mode = juce::roundToInt(getCurrent("MODE", 0.0f));

            float smooth = 0.55f;
            if (drive >= 18.0f) smooth = (mode == 5 ? 0.10f : 0.18f);
            else if (drive >= 12.0f) smooth = (mode == 5 ? 0.16f : 0.25f);
            else if (drive >= 7.0f) smooth = 0.38f;
            setParam("SMOOTH", smooth);

            setParam("HQ", drive >= 10.0f ? 1.0f : 0.0f);

            const int slope = drive >= 14.0f ? (mode == 5 ? 1 : 2)
                                             : (drive >= 6.0f ? 1 : 0);
            setParam("SLOPE", (float) slope);

            setParam("AUTO", 1.0f);
        };
        auto setLow = [&](float freq, bool shelf, float gainDb) {
            setParam("EQ_LOW_FREQ", freq); setParam("EQ_LOW_TYPE", shelf ? 1.0f : 0.0f); setParam("EQ_LOW_GAIN", gainDb);
        };
        auto setHigh = [&](float freq, bool shelf, float gainDb) {
            setParam("EQ_HIGH_FREQ", freq); setParam("EQ_HIGH_TYPE", shelf ? 1.0f : 0.0f); setParam("EQ_HIGH_GAIN", gainDb);
        };
        auto setBell1 = [&](float freq, float gainDb, float q) {
            setParam("EQ_BELL1_FREQ", freq); setParam("EQ_BELL1_GAIN", gainDb); setParam("EQ_BELL1_Q", q);
        };
        auto setBell2 = [&](float freq, float gainDb, float q) {
            setParam("EQ_BELL2_FREQ", freq); setParam("EQ_BELL2_GAIN", gainDb); setParam("EQ_BELL2_Q", q);
        };

        // ==========================================
        // 0. DEFAULT -- pinned at the top of the preset browser (not part
        // of the scrollable category list), a sane, moderate, versatile
        // starting point for anything.
        // ==========================================
        createDefault("0. Default", "Default", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 6.0f); setParam("MIX", 1.0f); setOut(-3.4f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.3f);
            setLow(20.0f, false, 0.0f);
            setBell1(400.0f, -0.4f, 0.8f);
            setBell2(3000.0f, 0.4f, 0.8f);
            setHigh(20000.0f, false, 0.0f);
        });

        // ==========================================
        // 0. HIGHLIGHTS -- shown first (right after the pinned Default),
        // one flagship pick from each other category so a new user can
        // hear the range of what this plugin does without having to dig
        // through all 53 presets first. Each one duplicates the exact
        // settings of its source preset (same category it's borrowed
        // from noted in the comment) rather than being a new sound -- this
        // is a shortcut/showcase, not new content.
        // ==========================================
        createDefault("0. Highlights", "1. Subtle Glue (from Mastering & Bus)", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.0f); setParam("MIX", 1.0f); setOut(-1.2f);
            setParam("TONE", 0.05f); setParam("PUNCH", 0.1f);
            setLow(25.0f, false, 0.0f);
            setBell1(300.0f, -0.7f, 0.7f);
            setBell2(9000.0f, 0.4f, 0.6f);
            setHigh(20000.0f, false, 0.0f);
        });
        createDefault("0. Highlights", "2. Modern Bass Grunt (from Bass)", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 1.0f); setOut(-6.3f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.8f);
            setLow(60.0f, true, 1.4f);
            setBell1(400.0f, -1.1f, 0.8f);
            setBell2(900.0f, 0.9f, 1.0f);
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("0. Highlights", "3. Snare Crack (from Drums)", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 1.0f); setOut(-6.8f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.6f);
            setLow(150.0f, false, 0.0f);
            setBell1(250.0f, 0.9f, 0.8f);
            setBell2(4500.0f, 1.4f, 1.1f);
            setHigh(12000.0f, false, 0.0f);
        });
        createDefault("0. Highlights", "4. Tube Vocal Presence (from Vocals)", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 4.5f); setParam("MIX", 1.0f); setOut(-2.6f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.1f);
            setLow(90.0f, false, 0.0f);
            setBell1(300.0f, -0.9f, 0.8f);
            setBell2(4000.0f, 1.4f, 1.0f);
            setHigh(20000.0f, true, 0.9f);
        });
        createDefault("0. Highlights", "5. Heavy Guitar Chug (from Guitars)", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 18.0f); setParam("MIX", 1.0f); setOut(-10.8f);
            setParam("TONE", 0.25f); setParam("PUNCH", 0.7f);
            setLow(100.0f, false, 0.0f);
            setBell1(250.0f, -0.9f, 0.8f);
            setBell2(3500.0f, 0.9f, 1.0f);
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("0. Highlights", "6. Acid Screamer FX (from Synths & FX)", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 16.0f); setParam("MIX", 1.0f); setOut(-11.5f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.7f);
            setLow(80.0f, false, 0.0f);
            setBell1(1000.0f, 0.9f, 2.0f);
            setBell2(5000.0f, 0.9f, 1.2f);
            setHigh(18000.0f, false, 0.0f);
        });
        createDefault("0. Highlights", "7. Warm Piano (from Synths & FX)", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 3.0f); setParam("MIX", 1.0f); setOut(-1.7f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.1f);
            setLow(35.0f, false, 0.0f);
            setBell1(250.0f, 0.3f, 0.7f);
            setBell2(4000.0f, 0.4f, 0.8f);
            setHigh(10000.0f, true, 0.3f);
        });

        // ==========================================
        // 5. GUITARS
        // FIX: several of these were at 100% MIX, which -- combined with
        // heavy drive -- read as harsh/unpleasant as a *default* starting
        // point rather than a deliberate creative choice. Backed off to
        // 75-90% so the dry signal's transients still poke through.
        // ==========================================
        createDefault("5. Guitars", "1. Classic Overdrive", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 14.0f); setParam("MIX", 1.0f); setOut(-7.6f);
            setParam("TONE", 0.15f); setParam("PUNCH", 0.3f);
            setLow(80.0f, false, 0.0f);
            setBell1(400.0f, -0.7f, 0.8f);
            setBell2(2500.0f, 0.9f, 0.9f);
            setHigh(6500.0f, false, 0.0f);
        });
        createDefault("5. Guitars", "2. Heavy Chug", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 18.0f); setParam("MIX", 1.0f); setOut(-10.8f);
            setParam("TONE", 0.25f); setParam("PUNCH", 0.7f);
            setLow(100.0f, false, 0.0f);
            setBell1(250.0f, -0.9f, 0.8f);
            setBell2(3500.0f, 0.9f, 1.0f);
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("5. Guitars", "3. Doom Fuzz", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f); setOut(-14.9f);
            setParam("TONE", -0.3f); setParam("PUNCH", 0.8f);
            setLow(50.0f, true, 0.9f);
            setBell1(150.0f, 0.7f, 0.7f);
            setBell2(1000.0f, -0.7f, 0.9f);
            setHigh(4500.0f, false, 0.0f);
        });
        createDefault("5. Guitars", "4. Screaming Lead", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 16.0f); setParam("MIX", 1.0f); setOut(-9.1f);
            setParam("TONE", 0.4f); setParam("PUNCH", 0.5f);
            setLow(120.0f, false, 0.0f);
            setBell1(500.0f, -0.9f, 0.8f);
            setBell2(3000.0f, 1.4f, 1.2f);
            setHigh(9500.0f, false, 0.0f);
        });
        createDefault("5. Guitars", "5. Tape Fuzz", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f); setOut(-11.7f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.2f);
            setLow(60.0f, false, 0.0f);
            setBell1(200.0f, 0.4f, 0.7f);
            setBell2(2000.0f, -0.4f, 0.9f);
            setHigh(6000.0f, false, 0.0f);
        });
        createDefault("5. Guitars", "6. Blues Breaker", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 10.0f); setParam("MIX", 1.0f); setOut(-5.4f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.2f);
            setLow(90.0f, false, 0.0f);
            setBell1(800.0f, 0.4f, 0.9f);
            setBell2(3000.0f, 0.7f, 1.0f);
            setHigh(7000.0f, false, 0.0f);
        });
        createDefault("5. Guitars", "7. Modern Djent", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 16.0f); setParam("MIX", 1.0f); setOut(-10.6f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.6f);
            setLow(100.0f, false, 0.0f);
            setBell1(300.0f, -0.9f, 0.8f);
            setBell2(4500.0f, 1.1f, 1.1f);
            setHigh(9000.0f, false, 0.0f);
        });
        createDefault("5. Guitars", "8. Acoustic Warmth", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 2.0f); setParam("MIX", 1.0f); setOut(-1.1f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.0f);
            setLow(80.0f, false, 0.0f);
            setBell1(200.0f, 0.4f, 0.7f);
            setBell2(6000.0f, 0.4f, 0.7f);
            setHigh(12000.0f, true, 0.4f);
        });
        createDefault("5. Guitars", "9. Acoustic Fingerstyle", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 1.5f); setParam("MIX", 1.0f); setOut(-0.8f);
            setParam("TONE", 0.05f); setParam("PUNCH", 0.0f);
            setLow(90.0f, false, 0.0f);
            setBell1(300.0f, 0.3f, 0.7f);
            setBell2(8000.0f, 0.4f, 0.7f);
            setHigh(14000.0f, true, 0.3f);
        });
        createDefault("5. Guitars", "10. Acoustic Strum Body", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 2.5f); setParam("MIX", 1.0f); setOut(-0.8f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.0f);
            setLow(70.0f, false, 0.0f);
            setBell1(180.0f, 0.4f, 0.7f);
            setBell2(5000.0f, 0.3f, 0.7f);
            setHigh(16000.0f, false, 0.0f);
        });

        // ==========================================
        // 2. BASS
        // ==========================================
        createDefault("2. Bass", "1. Modern Grunt", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 1.0f); setOut(-6.3f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.8f);
            setLow(60.0f, true, 1.4f);
            setBell1(400.0f, -1.1f, 0.8f);
            setBell2(900.0f, 0.9f, 1.0f);
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("2. Bass", "2. Vintage Amp", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 12.0f); setParam("MIX", 1.0f); setOut(-6.3f);
            setParam("TONE", -0.2f); setParam("PUNCH", 0.3f);
            setLow(50.0f, true, 0.9f);
            setBell1(200.0f, 0.7f, 0.7f);
            setBell2(700.0f, -0.4f, 0.9f);
            setHigh(5500.0f, false, 0.0f);
        });
        createDefault("2. Bass", "3. Parallel Smash", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 20.0f); setParam("MIX", 1.0f); setOut(-12.2f);
            setParam("TONE", 0.2f); setParam("PUNCH", 1.0f);
            setLow(20.0f, false, 0.0f);
            setBell1(100.0f, 0.9f, 0.8f);
            setBell2(3000.0f, 0.9f, 1.0f);
            setHigh(20000.0f, false, 0.0f);
        });
        createDefault("2. Bass", "4. Fuzz Sub", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 18.0f); setParam("MIX", 1.0f); setOut(-10.8f);
            setParam("TONE", -0.4f); setParam("PUNCH", 0.1f);
            setLow(30.0f, true, 0.9f);
            setBell1(80.0f, 0.9f, 0.7f);
            setBell2(800.0f, -0.9f, 0.9f);
            setHigh(3000.0f, false, 0.0f);
        });
        createDefault("2. Bass", "5. Tube Exciter", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 8.0f); setParam("MIX", 1.0f); setOut(-4.6f);
            setParam("TONE", 0.3f); setParam("PUNCH", 0.2f);
            setLow(50.0f, false, 0.0f);
            setBell1(250.0f, 0.4f, 0.7f);
            setBell2(2500.0f, 1.1f, 1.0f);
            setHigh(12000.0f, false, 0.0f);
        });
        createDefault("2. Bass", "6. Synth Reese", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 14.0f); setParam("MIX", 1.0f); setOut(-9.4f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.3f);
            setLow(40.0f, true, 0.9f);
            setBell1(500.0f, -0.4f, 0.8f);
            setBell2(1500.0f, 0.9f, 1.2f);
            setHigh(6000.0f, false, 0.0f);
        });
        createDefault("2. Bass", "7. Pick Bite", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 12.0f); setParam("MIX", 1.0f); setOut(-7.4f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.5f);
            setLow(50.0f, false, 0.0f);
            setBell1(200.0f, 0.4f, 0.7f);
            setBell2(2000.0f, 1.1f, 1.0f);
            setHigh(10000.0f, false, 0.0f);
        });
        createDefault("2. Bass", "8. Sub Clean", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 2.0f); setParam("MIX", 1.0f); setOut(-1.0f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.0f);
            setLow(50.0f, true, 0.9f);
            setHigh(8000.0f, false, 0.0f);
        });

        // ==========================================
        // 3. DRUMS
        // ==========================================
        createDefault("3. Drums", "1. Parallel Snap", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 18.0f); setParam("MIX", 1.0f); setOut(-10.8f);
            setParam("TONE", 0.3f); setParam("PUNCH", 1.0f);
            setLow(20.0f, false, 0.0f);
            setBell1(150.0f, 0.9f, 0.8f);
            setBell2(5000.0f, 1.4f, 1.1f);
            setHigh(20000.0f, false, 0.0f);
        });
        createDefault("3. Drums", "2. Bus Crush", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 12.0f); setParam("MIX", 1.0f); setOut(-8.9f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.9f);
            setLow(30.0f, false, 0.0f);
            setBell1(400.0f, -0.4f, 0.8f);
            setBell2(6000.0f, 0.9f, 1.0f);
            setHigh(16000.0f, false, 0.0f);
        });
        createDefault("3. Drums", "3. Snare Crackifier", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 10.0f); setParam("MIX", 1.0f); setOut(-6.8f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.6f);
            setLow(150.0f, false, 0.0f);
            setBell1(250.0f, 0.9f, 0.8f);
            setBell2(4500.0f, 1.4f, 1.1f);
            setHigh(12000.0f, false, 0.0f);
        });
        createDefault("3. Drums", "4. Kick Sub Maker", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 6.0f); setParam("MIX", 1.0f); setOut(-3.0f);
            setParam("TONE", -0.3f); setParam("PUNCH", 0.2f);
            setLow(45.0f, true, 1.8f);
            setBell1(80.0f, 0.9f, 0.7f);
            setBell2(2500.0f, 0.4f, 1.0f);
            setHigh(500.0f, false, 0.0f);
        });
        createDefault("3. Drums", "5. Breakbeat Trash", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 15.0f); setParam("MIX", 1.0f); setOut(-10.3f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.4f);
            setLow(150.0f, false, 0.0f);
            setBell1(300.0f, 0.9f, 0.8f);
            setBell2(3000.0f, 1.1f, 1.0f);
            setHigh(7000.0f, false, 0.0f);
        });
        createDefault("3. Drums", "6. Room Glue", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 4.0f); setParam("MIX", 1.0f); setOut(-1.6f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.1f);
            setLow(40.0f, false, 0.0f);
            setBell1(150.0f, 0.4f, 0.7f);
            setBell2(3000.0f, -0.4f, 0.8f);
            setHigh(12000.0f, true, -0.4f);
        });
        createDefault("3. Drums", "7. Hi-Hat Sizzle", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 8.0f); setParam("MIX", 1.0f); setOut(-6.3f);
            setParam("TONE", 0.4f); setParam("PUNCH", 0.3f);
            setLow(400.0f, false, 0.0f);
            setBell1(6000.0f, 0.9f, 1.0f);
            setBell2(10000.0f, 0.9f, 1.0f);
            setHigh(15000.0f, true, 0.9f);
        });
        createDefault("3. Drums", "8. Tom Thump", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 8.0f); setParam("MIX", 1.0f); setOut(-4.5f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.4f);
            setLow(70.0f, true, 1.4f);
            setBell1(120.0f, 0.9f, 0.8f);
            setBell2(2500.0f, -0.4f, 0.9f);
            setHigh(4000.0f, false, 0.0f);
        });

        // ==========================================
        // 4. VOCALS
        // ==========================================
        createDefault("4. Vocals", "1. Tube Presence", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 4.5f); setParam("MIX", 1.0f); setOut(-2.6f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.1f);
            setLow(90.0f, false, 0.0f);
            setBell1(300.0f, -0.9f, 0.8f);
            setBell2(4000.0f, 1.4f, 1.0f);
            setHigh(20000.0f, true, 0.9f);
        });
        createDefault("4. Vocals", "2. Saturation Air", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.5f); setParam("MIX", 1.0f); setOut(-1.6f);
            setParam("TONE", 0.4f); setParam("PUNCH", 0.0f);
            setLow(95.0f, false, 0.0f);
            setBell1(250.0f, -0.7f, 0.8f);
            setBell2(5000.0f, 0.9f, 0.9f);
            setHigh(11000.0f, true, 1.4f);
        });
        createDefault("4. Vocals", "3. Smooth Overdrive", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 6.0f); setParam("MIX", 1.0f); setOut(-3.3f);
            setParam("TONE", -0.05f); setParam("PUNCH", 0.2f);
            setLow(100.0f, false, 0.0f);
            setBell1(350.0f, -0.9f, 0.8f);
            setBell2(3000.0f, 0.9f, 0.8f);
            setHigh(14000.0f, false, 0.0f);
        });
        createDefault("4. Vocals", "4. Aggressive Rap", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 8.0f); setParam("MIX", 1.0f); setOut(-6.5f);
            setParam("TONE", 0.3f); setParam("PUNCH", 0.5f);
            setLow(80.0f, false, 0.0f);
            setBell1(1000.0f, 0.7f, 0.9f);
            setBell2(4500.0f, 1.1f, 1.0f);
            setHigh(18000.0f, true, 0.9f);
        });
        createDefault("4. Vocals", "5. Lo-Fi Radio", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 14.0f); setParam("MIX", 1.0f); setOut(-8.7f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.1f);
            setLow(400.0f, false, 0.0f);
            setBell1(1500.0f, 1.4f, 1.6f);
            setBell2(2800.0f, 0.7f, 1.4f);
            setHigh(3500.0f, false, 0.0f);
        });
        createDefault("4. Vocals", "6. Podcast Warmth", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.0f); setParam("MIX", 1.0f); setOut(-1.1f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.0f);
            setLow(90.0f, false, 0.0f);
            setBell1(300.0f, -0.4f, 0.8f);
            setBell2(3000.0f, 0.4f, 0.8f);
            setHigh(10000.0f, true, 0.4f);
        });
        createDefault("4. Vocals", "7. Telephone", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 10.0f); setParam("MIX", 1.0f); setOut(-6.8f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.2f);
            setLow(500.0f, false, 0.0f);
            setBell1(1200.0f, 1.4f, 1.5f);
            setBell2(2000.0f, 0.4f, 1.2f);
            setHigh(2500.0f, false, 0.0f);
        });
        createDefault("4. Vocals", "8. Doubler Grit", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 5.0f); setParam("MIX", 1.0f); setOut(-2.9f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.2f);
            setLow(100.0f, false, 0.0f);
            setBell1(400.0f, -0.7f, 0.8f);
            setBell2(5000.0f, 0.9f, 1.0f);
            setHigh(12000.0f, true, 0.7f);
        });

        // ==========================================
        // 6. SYNTHS & FX
        // ==========================================
        createDefault("6. Synths & FX", "1. Wavefold Lead", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 15.0f); setParam("MIX", 1.0f); setOut(-8.5f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.3f);
            setLow(60.0f, false, 0.0f);
            setBell1(300.0f, -0.4f, 0.8f);
            setBell2(3000.0f, 0.9f, 1.0f);
            setHigh(15000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "2. Analog Warm Pad", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 7.0f); setParam("MIX", 1.0f); setOut(-2.9f);
            setParam("TONE", -0.15f); setParam("PUNCH", 0.0f);
            setLow(60.0f, true, 0.7f);
            setBell1(200.0f, 0.4f, 0.7f);
            setBell2(4000.0f, -0.4f, 0.8f);
            setHigh(10000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "3. Acid Screamer", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 16.0f); setParam("MIX", 1.0f); setOut(-11.5f);
            setParam("TONE", 0.5f); setParam("PUNCH", 0.7f);
            setLow(80.0f, false, 0.0f);
            setBell1(1000.0f, 0.9f, 2.0f);
            setBell2(5000.0f, 0.9f, 1.2f);
            setHigh(18000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "4. 8-Bit Chiptune", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f); setOut(-15.7f);
            setParam("TONE", 0.8f); setParam("PUNCH", 1.0f);
            setLow(150.0f, false, 0.0f);
            setBell1(1500.0f, 0.9f, 1.2f);
            setBell2(4000.0f, 0.9f, 1.2f);
            setHigh(8000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "5. Total Destruction", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 24.0f); setParam("MIX", 1.0f); setOut(-14.8f);
            setParam("TONE", -0.8f); setParam("PUNCH", 1.0f);
            setLow(80.0f, true, 1.4f);
            setBell1(200.0f, 1.4f, 0.8f);
            setBell2(2000.0f, -0.9f, 1.0f);
            setHigh(4000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "6. Bitcrush Chaos", [&]() {
            setMode(3.0f); // DIGITAL
            setParam("DRIVE", 20.0f); setParam("MIX", 1.0f); setOut(-12.7f);
            setParam("TONE", 0.3f); setParam("PUNCH", 0.6f);
            setLow(80.0f, false, 0.0f);
            setBell1(1000.0f, 0.9f, 1.0f);
            setBell2(6000.0f, 0.9f, 1.0f);
            setHigh(12000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "7. Radio Static", [&]() {
            setMode(5.0f); // FUZZ
            setParam("DRIVE", 20.0f); setParam("MIX", 1.0f); setOut(-12.6f);
            setParam("TONE", 0.2f); setParam("PUNCH", 0.3f);
            setLow(300.0f, false, 0.0f);
            setBell1(1500.0f, 1.4f, 2.0f);
            setBell2(3500.0f, 0.7f, 1.3f);
            setHigh(5000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "8. Supersaw Grind", [&]() {
            setMode(4.0f); // CRUNCH
            setParam("DRIVE", 12.0f); setParam("MIX", 1.0f); setOut(-7.2f);
            setParam("TONE", 0.15f); setParam("PUNCH", 0.4f);
            setLow(60.0f, false, 0.0f);
            setBell1(400.0f, -0.4f, 0.8f);
            setBell2(5000.0f, 0.9f, 1.0f);
            setHigh(16000.0f, false, 0.0f);
        });
        createDefault("6. Synths & FX", "9. Piano - Warm Tape", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 2.0f); setParam("MIX", 1.0f); setOut(-0.5f);
            setParam("TONE", -0.05f); setParam("PUNCH", 0.0f);
            setLow(40.0f, false, 0.0f);
            setBell1(200.0f, 0.3f, 0.7f);
            setBell2(3000.0f, -0.3f, 0.7f);
            setHigh(12000.0f, true, -0.3f);
        });
        createDefault("6. Synths & FX", "10. Piano - Tube Glow", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 3.0f); setParam("MIX", 1.0f); setOut(-1.7f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.1f);
            setLow(35.0f, false, 0.0f);
            setBell1(250.0f, 0.3f, 0.7f);
            setBell2(4000.0f, 0.4f, 0.8f);
            setHigh(10000.0f, true, 0.3f);
        });

        // ==========================================
        // 1. MASTERING & BUS
        // ==========================================
        createDefault("1. Mastering & Bus", "1. Master Glue", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 3.0f); setParam("MIX", 1.0f); setOut(-1.2f);
            setParam("TONE", 0.05f); setParam("PUNCH", 0.1f);
            setLow(25.0f, false, 0.0f);
            setBell1(300.0f, -0.7f, 0.7f);
            setBell2(9000.0f, 0.4f, 0.6f);
            setHigh(20000.0f, false, 0.0f);
        });
        createDefault("1. Mastering & Bus", "2. Subtle Tube Heat", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 4.5f); setParam("MIX", 1.0f); setOut(-2.3f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.0f);
            setLow(30.0f, false, 0.0f);
            setBell1(180.0f, 0.4f, 0.7f);
            setBell2(6000.0f, 0.2f, 0.6f);
            setHigh(18500.0f, true, 0.4f);
        });
        createDefault("1. Mastering & Bus", "3. Vintage Console", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 6.0f); setParam("MIX", 1.0f); setOut(-3.3f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.2f);
            setLow(35.0f, true, 0.4f);
            setBell1(120.0f, 0.7f, 0.7f);
            setBell2(3000.0f, 0.4f, 0.7f);
            setHigh(16000.0f, true, -0.7f);
        });
        createDefault("1. Mastering & Bus", "4. Tape Mojo", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 5.0f); setParam("MIX", 1.0f); setOut(-1.9f);
            setParam("TONE", -0.1f); setParam("PUNCH", 0.0f);
            setLow(28.0f, false, 0.0f);
            setBell1(150.0f, 0.4f, 0.7f);
            setBell2(8000.0f, -0.2f, 0.6f);
            setHigh(15000.0f, true, -0.9f);
        });
        createDefault("1. Mastering & Bus", "5. Mastering Punch", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 4.0f); setParam("MIX", 1.0f); setOut(-2.7f);
            setParam("TONE", 0.15f); setParam("PUNCH", 0.4f);
            setLow(22.0f, false, 0.0f);
            setBell1(400.0f, -0.4f, 0.8f);
            setBell2(5000.0f, 0.7f, 0.7f);
            setHigh(20000.0f, true, 0.4f);
        });
        createDefault("1. Mastering & Bus", "6. Analog Sheen", [&]() {
            setMode(1.0f); // TUBE
            setParam("DRIVE", 3.0f); setParam("MIX", 1.0f); setOut(-1.6f);
            setParam("TONE", 0.1f); setParam("PUNCH", 0.0f);
            setLow(25.0f, false, 0.0f);
            setBell1(150.0f, 0.4f, 0.7f);
            setBell2(8000.0f, 0.4f, 0.6f);
            setHigh(14000.0f, true, 0.7f);
        });
        createDefault("1. Mastering & Bus", "7. Gentle Compression Feel", [&]() {
            setMode(0.0f); // PUNCH
            setParam("DRIVE", 2.5f); setParam("MIX", 1.0f); setOut(-1.4f);
            setParam("TONE", 0.0f); setParam("PUNCH", 0.1f);
            setLow(20.0f, false, 0.0f);
            setBell1(300.0f, -0.4f, 0.7f);
            setHigh(10000.0f, true, 0.2f);
        });
        createDefault("1. Mastering & Bus", "8. Loudness Maximizer Prep", [&]() {
            setMode(2.0f); // TAPE
            setParam("DRIVE", 5.0f); setParam("MIX", 1.0f); setOut(-2.2f);
            setParam("TONE", 0.05f); setParam("PUNCH", 0.1f);
            setLow(20.0f, false, 0.0f);
            setBell1(250.0f, -0.7f, 0.7f);
            setBell2(4000.0f, 0.7f, 0.8f);
            setHigh(12000.0f, true, 0.4f);
        });
    }
}