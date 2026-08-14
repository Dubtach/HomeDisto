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
        const auto presetRoot = processor.getPresetDirectory();

        // Factory presets are completely regenerated on startup so the library
        // can be intentionally improved without leaving stale factory files.
        // User presets live in the separate "User" folder and are never touched.
        for (auto child : presetRoot.findChildFiles(juce::File::findDirectories, false))
            if (child.getFileName() != "User")
                child.deleteRecursively();

        juce::ValueTree baselineTree = processor.apvts.copyState();
        juce::ValueTree* currentTree = nullptr;

        auto createDefault = [&](const juce::String& category,
                                 const juce::String& name,
                                 std::function<void()> configureLambda)
        {
            auto categoryDir = presetRoot.getChildFile(category);
            categoryDir.createDirectory();

            auto presetFile = categoryDir.getChildFile(name + ".xml");

            auto presetTree = baselineTree.createCopy();
            currentTree = &presetTree;
            configureLambda();
            currentTree = nullptr;

            if (auto xml = presetTree.createXml())
                xml->writeTo(presetFile);
        };

        auto setParam = [&](const juce::String& id, float value)
        {
            if (currentTree != nullptr)
                setTreeParam(*currentTree, id, value);
        };

        auto setMode = [&](float value) { setParam("MODE", value); };
        auto setDrive = [&](float value) { setParam("DRIVE", value); };
        auto setTone = [&](float value) { setParam("TONE", value); };
        auto setPunch = [&](float value) { setParam("PUNCH", value); };
        auto setMix = [&](float value) { setParam("MIX", value); };
        auto setOut = [&](float value) { setParam("OUT", value); };
        auto setHQ = [&](bool enabled) { setParam("HQ", enabled ? 1.0f : 0.0f); };
        auto setAuto = [&](bool enabled) { setParam("AUTO", enabled ? 1.0f : 0.0f); };
        auto setSmooth = [&](float value) { setParam("SMOOTH", value); };
        auto setSlope = [&](int value) { setParam("SLOPE", (float) value); };

        auto setLow = [&](float freq, bool shelf, float gain)
        {
            setParam("EQ_LOW_FREQ", freq);
            setParam("EQ_LOW_TYPE", shelf ? 1.0f : 0.0f);
            setParam("EQ_LOW_GAIN", gain);
        };

        auto setHigh = [&](float freq, bool shelf, float gain)
        {
            setParam("EQ_HIGH_FREQ", freq);
            setParam("EQ_HIGH_TYPE", shelf ? 1.0f : 0.0f);
            setParam("EQ_HIGH_GAIN", gain);
        };

        auto setBell1 = [&](float freq, float gain, float q)
        {
            setParam("EQ_BELL1_FREQ", freq);
            setParam("EQ_BELL1_GAIN", gain);
            setParam("EQ_BELL1_Q", q);
        };

        auto setBell2 = [&](float freq, float gain, float q)
        {
            setParam("EQ_BELL2_FREQ", freq);
            setParam("EQ_BELL2_GAIN", gain);
            setParam("EQ_BELL2_Q", q);
        };

        // ================================================================
        // 0. DEFAULT
        // ================================================================
        createDefault("0. Default", "Default", [&]()
        {
            setMode(0); setDrive(6.0f); setTone(0.0f); setPunch(0.35f); setMix(1.0f); setOut(-3.4f);
            setAuto(true); setHQ(false); setSmooth(0.35f); setSlope(1);
            setLow(75.0f, false, 0.0f); setBell1(350.0f, -0.5f, 0.8f);
            setBell2(3000.0f, 0.5f, 0.8f); setHigh(18000.0f, false, 0.0f);
        });

        // ================================================================
        // 1. ESSENTIALS
        // ================================================================
        createDefault("1. Essentials", "01 Clean Heat", [&]()
        {
            setMode(1); setDrive(2.5f); setTone(0.05f); setPunch(0.05f); setMix(1.0f); setOut(-1.2f);
            setAuto(true); setHQ(false); setSmooth(0.55f); setSlope(1);
            setLow(45.0f, false, 0.0f); setBell1(220.0f, -0.2f, 0.7f);
            setBell2(4500.0f, 0.3f, 0.7f); setHigh(16000.0f, true, 0.2f);
        });
        createDefault("1. Essentials", "02 Warmth", [&]()
        {
            setMode(2); setDrive(3.5f); setTone(-0.1f); setPunch(0.05f); setMix(1.0f); setOut(-1.5f);
            setAuto(true); setHQ(false); setSmooth(0.65f); setSlope(1);
            setLow(35.0f, true, 0.5f); setBell1(250.0f, 0.2f, 0.7f);
            setBell2(3500.0f, -0.2f, 0.8f); setHigh(13000.0f, true, -0.4f);
        });
        createDefault("1. Essentials", "03 Bite", [&]()
        {
            setMode(0); setDrive(5.5f); setTone(0.25f); setPunch(0.25f); setMix(1.0f); setOut(-3.0f);
            setAuto(true); setHQ(false); setSmooth(0.30f); setSlope(1);
            setLow(70.0f, false, 0.0f); setBell1(350.0f, -0.4f, 0.8f);
            setBell2(2800.0f, 1.0f, 0.9f); setHigh(10000.0f, true, 0.2f);
        });
        createDefault("1. Essentials", "04 Glue", [&]()
        {
            setMode(2); setDrive(4.0f); setTone(0.0f); setPunch(0.1f); setMix(1.0f); setOut(-1.8f);
            setAuto(true); setHQ(true); setSmooth(0.60f); setSlope(2);
            setLow(30.0f, false, 0.0f); setBell1(300.0f, -0.5f, 0.7f);
            setBell2(8000.0f, 0.25f, 0.6f); setHigh(16000.0f, true, 0.1f);
        });
        createDefault("1. Essentials", "05 Crunch", [&]()
        {
            setMode(4); setDrive(10.0f); setTone(0.15f); setPunch(0.45f); setMix(1.0f); setOut(-5.8f);
            setAuto(true); setHQ(true); setSmooth(0.30f); setSlope(2);
            setLow(80.0f, false, 0.0f); setBell1(280.0f, -0.7f, 0.8f);
            setBell2(3200.0f, 0.8f, 0.9f); setHigh(9000.0f, false, 0.0f);
        });
        createDefault("1. Essentials", "06 Heavy", [&]()
        {
            setMode(3); setDrive(16.0f); setTone(0.2f); setPunch(0.6f); setMix(1.0f); setOut(-10.0f);
            setAuto(true); setHQ(true); setSmooth(0.22f); setSlope(2);
            setLow(90.0f, false, 0.0f); setBell1(250.0f, -0.9f, 0.8f);
            setBell2(3500.0f, 0.9f, 1.0f); setHigh(8500.0f, false, 0.0f);
        });
        createDefault("1. Essentials", "07 Fuzz", [&]()
        {
            setMode(5); setDrive(20.0f); setTone(-0.1f); setPunch(0.7f); setMix(1.0f); setOut(-12.5f);
            setAuto(true); setHQ(true); setSmooth(0.18f); setSlope(1);
            setLow(60.0f, true, 0.6f); setBell1(180.0f, 0.3f, 0.8f);
            setBell2(1400.0f, -0.5f, 1.0f); setHigh(6500.0f, false, 0.0f);
        });
        createDefault("1. Essentials", "08 Destroy", [&]()
        {
            setMode(5); setDrive(23.0f); setTone(0.35f); setPunch(0.95f); setMix(1.0f); setOut(-15.0f);
            setAuto(false); setHQ(true); setSmooth(0.05f); setSlope(0);
            setLow(75.0f, false, 0.0f); setBell1(220.0f, -1.0f, 0.9f);
            setBell2(2500.0f, 1.2f, 1.2f); setHigh(7000.0f, false, 0.0f);
        });

        // ================================================================
        // 2. GUITARS
        // ================================================================
        const auto guitar = [&](const juce::String& n, int m, float d, float t, float p, float o,
                                float lf, float lG, float b1f, float b1G, float b2f, float b2G,
                                float hf, float hG, float smooth, int slope)
        {
            createDefault("2. Guitars", n, [&]()
            {
                setMode((float)m); setDrive(d); setTone(t); setPunch(p); setMix(1.0f); setOut(o);
                setAuto(true); setHQ(true); setSmooth(smooth); setSlope(slope);
                setLow(lf, false, lG); setBell1(b1f, b1G, 0.8f);
                setBell2(b2f, b2G, 1.0f); setHigh(hf, false, hG);
            });
        };
        guitar("01 Clean Amp", 1, 3.0f, 0.0f, 0.05f, -1.3f, 65, 0.0f, 250, -0.2f, 4200, 0.4f, 16000, 0.2f, 0.55f, 1);
        guitar("02 Edge of Breakup", 1, 7.0f, 0.08f, 0.2f, -3.7f, 75, 0.0f, 300, -0.4f, 2800, 0.7f, 9500, 0.0f, 0.45f, 1);
        guitar("03 Blues Drive", 1, 10.0f, 0.05f, 0.25f, -5.3f, 80, 0.0f, 450, -0.5f, 3200, 0.8f, 8500, 0.0f, 0.40f, 1);
        guitar("04 Classic Rock", 4, 12.0f, 0.12f, 0.4f, -7.0f, 85, 0.0f, 280, -0.7f, 2800, 0.8f, 9000, 0.0f, 0.32f, 2);
        guitar("05 Tight Rhythm", 3, 15.0f, 0.15f, 0.6f, -9.0f, 100, 0.0f, 250, -1.0f, 4200, 1.0f, 8500, 0.0f, 0.22f, 2);
        guitar("06 Modern Chug", 4, 18.0f, 0.20f, 0.7f, -10.8f, 110, 0.0f, 300, -1.0f, 3500, 1.0f, 7800, 0.0f, 0.18f, 2);
        guitar("07 Lead Lift", 0, 14.0f, 0.35f, 0.45f, -8.4f, 120, 0.0f, 500, -0.6f, 3000, 1.4f, 10500, 0.4f, 0.28f, 1);
        guitar("08 Doom Fuzz", 5, 22.0f, -0.25f, 0.8f, -14.0f, 50, 1.0f, 160, 0.6f, 1000, -0.8f, 4500, 0.0f, 0.10f, 1);

        // ================================================================
        // 3. BASS
        // ================================================================
        const auto bass = [&](const juce::String& n, int m, float d, float t, float p, float o,
                              float lf, float lG, float b1f, float b1G, float b2f, float b2G,
                              float hf, float hG, float smooth, int slope)
        {
            createDefault("3. Bass", n, [&]()
            {
                setMode((float)m); setDrive(d); setTone(t); setPunch(p); setMix(1.0f); setOut(o);
                setAuto(true); setHQ(true); setSmooth(smooth); setSlope(slope);
                setLow(lf, true, lG); setBell1(b1f, b1G, 0.8f);
                setBell2(b2f, b2G, 1.0f); setHigh(hf, false, hG);
            });
        };
        bass("01 Clean Weight", 0, 2.0f, 0.0f, 0.05f, -0.8f, 45, 0.9f, 120, 0.2f, 900, 0.2f, 9000, 0.0f, 0.60f, 1);
        bass("02 Tube Roundness", 1, 5.0f, -0.1f, 0.15f, -2.4f, 45, 1.0f, 180, 0.5f, 700, -0.3f, 6500, 0.0f, 0.55f, 1);
        bass("03 Modern Growl", 0, 10.0f, 0.1f, 0.6f, -6.0f, 55, 1.1f, 300, -0.8f, 1000, 1.0f, 8500, 0.0f, 0.28f, 2);
        bass("04 Pick Bite", 4, 12.0f, 0.22f, 0.55f, -7.3f, 50, 0.8f, 220, 0.3f, 2200, 1.0f, 10000, 0.0f, 0.24f, 2);
        bass("05 Fuzz Sub", 5, 18.0f, -0.35f, 0.5f, -10.8f, 35, 1.2f, 80, 0.6f, 800, -0.9f, 3500, 0.0f, 0.12f, 1);
        bass("06 Grind Bass", 3, 15.0f, 0.15f, 0.75f, -10.2f, 55, 0.8f, 200, -0.7f, 1800, 1.2f, 8000, 0.0f, 0.20f, 2);

        // ================================================================
        // 4. DRUMS
        // ================================================================
        const auto drums = [&](const juce::String& n, int m, float d, float t, float p, float o,
                               float lf, float lG, float b1f, float b1G, float b2f, float b2G,
                               float hf, float hG, float smooth, int slope)
        {
            createDefault("4. Drums", n, [&]()
            {
                setMode((float)m); setDrive(d); setTone(t); setPunch(p); setMix(1.0f); setOut(o);
                setAuto(true); setHQ(true); setSmooth(smooth); setSlope(slope);
                setLow(lf, false, lG); setBell1(b1f, b1G, 0.8f);
                setBell2(b2f, b2G, 1.0f); setHigh(hf, false, hG);
            });
        };
        drums("01 Drum Bus Glue", 2, 4.0f, -0.05f, 0.2f, -1.8f, 30, 0.0f, 180, 0.3f, 6500, 0.4f, 16000, 0.0f, 0.65f, 2);
        drums("02 Punchy Bus", 0, 8.0f, 0.1f, 0.7f, -4.8f, 28, 0.0f, 120, 0.4f, 4500, 0.9f, 15000, 0.0f, 0.32f, 2);
        drums("03 Snare Crack", 4, 10.0f, 0.45f, 0.65f, -6.8f, 120, 0.0f, 220, 0.8f, 4800, 1.4f, 12000, 0.0f, 0.25f, 2);
        drums("04 Kick Weight", 1, 6.0f, -0.2f, 0.25f, -3.0f, 45, 0.0f, 75, 1.0f, 2500, 0.3f, 7000, 0.0f, 0.60f, 1);
        drums("05 Room Crush", 3, 14.0f, 0.1f, 0.75f, -9.0f, 50, 0.0f, 300, -0.5f, 6000, 1.0f, 14000, -0.5f, 0.18f, 2);
        drums("06 Trash Kit", 5, 18.0f, 0.25f, 0.5f, -12.0f, 120, 0.0f, 700, 0.8f, 3000, 1.0f, 6500, 0.0f, 0.08f, 1);
        drums("07 Hi-Hat Sizzle", 3, 8.0f, 0.45f, 0.25f, -6.0f, 350, 0.0f, 6000, 0.8f, 10000, 1.0f, 15000, 0.8f, 0.25f, 2);

        // ================================================================
        // 5. VOCALS
        // ================================================================
        const auto vocals = [&](const juce::String& n, int m, float d, float t, float p, float o,
                                float lf, float lG, float b1f, float b1G, float b2f, float b2G,
                                float hf, float hG, float smooth, int slope)
        {
            createDefault("5. Vocals", n, [&]()
            {
                setMode((float)m); setDrive(d); setTone(t); setPunch(p); setMix(1.0f); setOut(o);
                setAuto(true); setHQ(true); setSmooth(smooth); setSlope(slope);
                setLow(lf, false, lG); setBell1(b1f, b1G, 0.8f);
                setBell2(b2f, b2G, 1.0f); setHigh(hf, true, hG);
            });
        };
        vocals("01 Vocal Polish", 1, 3.0f, 0.05f, 0.05f, -1.4f, 85, 0.0f, 300, -0.6f, 4200, 1.0f, 16000, 0.4f, 0.65f, 2);
        vocals("02 Tube Presence", 1, 5.0f, 0.18f, 0.12f, -2.8f, 90, 0.0f, 300, -0.8f, 4000, 1.2f, 15000, 0.7f, 0.55f, 2);
        vocals("03 Saturation Air", 2, 4.0f, 0.2f, 0.0f, -1.9f, 95, 0.0f, 250, -0.7f, 5200, 0.8f, 11000, 1.0f, 0.65f, 1);
        vocals("04 Rap Bite", 3, 8.0f, 0.3f, 0.5f, -6.0f, 80, 0.0f, 1000, 0.5f, 4500, 1.0f, 17000, 0.7f, 0.30f, 2);
        vocals("05 Intimate Grit", 0, 6.0f, -0.05f, 0.15f, -3.2f, 95, 0.0f, 350, -0.8f, 3000, 0.8f, 12000, 0.3f, 0.50f, 2);
        vocals("06 Lo-Fi Radio", 3, 13.0f, 0.1f, 0.15f, -8.5f, 450, 0.0f, 1400, 1.0f, 2800, 0.7f, 3800, 0.0f, 0.08f, 1);
        vocals("07 Megaphone", 5, 15.0f, 0.25f, 0.25f, -10.0f, 450, 0.0f, 1200, 1.2f, 2600, 0.6f, 4200, 0.0f, 0.05f, 1);

        // ================================================================
        // 6. SYNTHS & FX
        // ================================================================
        const auto fx = [&](const juce::String& n, int m, float d, float t, float p, float o,
                            float lf, bool ls, float lG, float b1f, float b1G, float b2f, float b2G,
                            float hf, bool hs, float hG, float smooth, int slope, bool hq)
        {
            createDefault("6. Synths & FX", n, [&]()
            {
                setMode((float)m); setDrive(d); setTone(t); setPunch(p); setMix(1.0f); setOut(o);
                setAuto(true); setHQ(hq); setSmooth(smooth); setSlope(slope);
                setLow(lf, ls, lG); setBell1(b1f, b1G, 0.9f);
                setBell2(b2f, b2G, 1.1f); setHigh(hf, hs, hG);
            });
        };
        fx("01 Analog Pad", 2, 6.0f, -0.1f, 0.0f, -2.7f, 55, true, 0.5f, 220, 0.3f, 4200, -0.4f, 11000, true, -0.3f, 0.60f, 2, true);
        fx("02 Mono Bass Synth", 0, 9.0f, 0.05f, 0.45f, -5.7f, 35, true, 0.8f, 120, 0.2f, 900, 0.8f, 8500, false, 0.0f, 0.35f, 2, true);
        fx("03 Acid Bite", 5, 16.0f, 0.55f, 0.75f, -11.5f, 70, false, 0.0f, 900, 0.8f, 3800, 1.0f, 12000, false, 0.0f, 0.10f, 1, true);
        fx("04 Digital Lead", 3, 17.0f, 0.35f, 0.55f, -11.0f, 80, false, 0.0f, 500, -0.5f, 3500, 1.3f, 12000, false, 0.0f, 0.15f, 2, true);
        fx("05 Chiptune", 3, 22.0f, 0.8f, 0.9f, -15.0f, 180, false, 0.0f, 1500, 1.0f, 4200, 1.1f, 8000, false, 0.0f, 0.03f, 1, true);
        fx("06 Broken Speaker", 5, 19.0f, 0.2f, 0.55f, -13.0f, 300, false, 0.0f, 1400, 1.2f, 3000, 0.9f, 4200, false, 0.0f, 0.06f, 1, false);
        fx("07 Glitch Texture", 4, 20.0f, 0.0f, 0.8f, -13.5f, 120, false, 0.0f, 650, -0.6f, 5200, 1.4f, 9000, true, -0.5f, 0.04f, 2, true);
        fx("08 Piano Tape", 2, 2.5f, -0.1f, 0.0f, -0.7f, 35, false, 0.0f, 220, 0.3f, 3200, -0.3f, 12000, true, -0.2f, 0.70f, 2, true);

        // ================================================================
        // 7. MASTERING & BUS
        // ================================================================
        const auto bus = [&](const juce::String& n, int m, float d, float t, float p, float o,
                             float lf, float lG, float b1f, float b1G, float b2f, float b2G,
                             float hf, bool hs, float hG, float smooth, int slope)
        {
            createDefault("7. Mastering & Bus", n, [&]()
            {
                setMode((float)m); setDrive(d); setTone(t); setPunch(p); setMix(1.0f); setOut(o);
                setAuto(true); setHQ(true); setSmooth(smooth); setSlope(slope);
                setLow(lf, false, lG); setBell1(b1f, b1G, 0.8f);
                setBell2(b2f, b2G, 0.7f); setHigh(hf, hs, hG);
            });
        };
        bus("01 Transparent Glue", 2, 2.5f, 0.0f, 0.05f, -1.0f, 25, 0.0f, 180, 0.0f, 8000, 0.2f, 18000, true, 0.1f, 0.75f, 2);
        bus("02 Tape Polish", 2, 4.0f, -0.05f, 0.1f, -1.7f, 25, 0.0f, 250, 0.2f, 7000, 0.2f, 16000, true, -0.2f, 0.70f, 2);
        bus("03 Tube Console", 1, 4.5f, 0.05f, 0.1f, -2.2f, 30, 0.2f, 180, 0.4f, 4500, 0.4f, 16000, true, -0.3f, 0.65f, 2);
        bus("04 Punch Bus", 0, 5.0f, 0.1f, 0.35f, -3.0f, 22, 0.0f, 300, -0.5f, 4500, 0.7f, 15000, true, 0.2f, 0.50f, 2);
        bus("05 Analog Sheen", 1, 3.5f, 0.1f, 0.05f, -1.6f, 25, 0.0f, 150, 0.3f, 7000, 0.5f, 14000, true, 0.5f, 0.78f, 2);
        bus("06 Dark Tape", 2, 5.0f, -0.15f, 0.0f, -2.1f, 30, 0.0f, 180, 0.3f, 6000, -0.3f, 12000, true, -1.0f, 0.72f, 2);
        bus("07 Mastering Bite", 0, 3.5f, 0.18f, 0.25f, -2.3f, 20, 0.0f, 350, -0.4f, 5000, 0.7f, 17000, true, 0.2f, 0.45f, 2);
        bus("08 Safety Limiting Tone", 3, 5.0f, 0.05f, 0.1f, -3.0f, 25, 0.0f, 250, -0.6f, 6000, 0.5f, 15000, true, 0.0f, 0.55f, 2);
    }
}