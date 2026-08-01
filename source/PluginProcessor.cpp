#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include <cmath>

HomeDistoAudioProcessor::HomeDistoAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", createParameters())
#endif
{
    FactoryPresets::generateDefaults(*this);

    // Pre-allocate filter states to prevent null-dereferences
    lowCutFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(44100.0, 20.0f);
    highCutFilter.state = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 20000.0f);
    toneFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf(44100.0, 1000.0f, 0.707f, 1.0f);
    
    dryLowCut.state = lowCutFilter.state;
    dryHighCut.state = highCutFilter.state;
    dryTone.state = toneFilter.state;
}

HomeDistoAudioProcessor::~HomeDistoAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout HomeDistoAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("BYPASS", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", 0.0f, 24.0f, 6.7f));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("MODE", "Mode", 
        juce::StringArray{"PUNCH", "TUBE", "TAPE", "DIGITAL", "CRUNCH", "FUZZ"}, 0));
        
    params.push_back(std::make_unique<juce::AudioParameterFloat>("OUT", "Output", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("AUTO", "Auto", false));

    juce::NormalisableRange<float> lowRange(20.0f, 1000.0f, 1.0f, 0.3f);
    juce::NormalisableRange<float> highRange(1000.0f, 20000.0f, 10.0f, 0.3f);
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LOW_CUT", "Low Cut", lowRange, 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("HIGH_CUT", "High Cut", highRange, 8500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("PUNCH", "Punch", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 0.5f));

    return { params.begin(), params.end() };
}

void HomeDistoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    lowCutFilter.prepare(spec);
    highCutFilter.prepare(spec);
    toneFilter.prepare(spec);
    
    dryLowCut.prepare(spec);
    dryHighCut.prepare(spec);
    dryTone.prepare(spec);

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);

    smoothDrive.reset(sampleRate, 0.02);
    smoothOut.reset(sampleRate, 0.02);
    smoothMix.reset(sampleRate, 0.02);
    smoothPunch.reset(sampleRate, 0.02);
    
    autoGainFactor.reset(sampleRate, 0.05);
    autoGainFactor.setCurrentAndTargetValue(1.0f);
}

void HomeDistoAudioProcessor::releaseResources() {}

bool HomeDistoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

// -----------------------------------------------------------------------------
void HomeDistoAudioProcessor::updateHighPass(juce::dsp::IIR::Coefficients<float>* state, float freq, double sampleRate) {
    if (!state) return;
    freq = juce::jmin(freq, (float)(sampleRate * 0.499));
    double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * 0.70710678f);
    double a0 = 1.0 + alpha;
    
    auto* raw = state->getRawCoefficients();
    raw[0] = (float)((1.0 + cos_w0) / (2.0 * a0));
    raw[1] = (float)(-(1.0 + cos_w0) / a0);      
    raw[2] = (float)((1.0 + cos_w0) / (2.0 * a0));
    raw[3] = 1.0f;                                
    raw[4] = (float)(-2.0 * cos_w0 / a0);          
    raw[5] = (float)((1.0 - alpha) / a0);         
}

void HomeDistoAudioProcessor::updateLowPass(juce::dsp::IIR::Coefficients<float>* state, float freq, double sampleRate) {
    if (!state) return;
    freq = juce::jmin(freq, (float)(sampleRate * 0.499));
    double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * 0.70710678f);
    double a0 = 1.0 + alpha;
    
    auto* raw = state->getRawCoefficients();
    raw[0] = (float)((1.0 - cos_w0) / (2.0 * a0));
    raw[1] = (float)((1.0 - cos_w0) / a0);
    raw[2] = (float)((1.0 - cos_w0) / (2.0 * a0));
    raw[3] = 1.0f;
    raw[4] = (float)(-2.0 * cos_w0 / a0);
    raw[5] = (float)((1.0 - alpha) / a0);
}

void HomeDistoAudioProcessor::updateHighShelf(juce::dsp::IIR::Coefficients<float>* state, float freq, float Q, float gain, double sampleRate) {
    if (!state) return;
    freq = juce::jmin(freq, (float)(sampleRate * 0.499));
    double A = std::sqrt(juce::jmax(0.0001f, gain));
    double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double a0 = (A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha;
    
    auto* raw = state->getRawCoefficients();
    raw[0] = (float)(A * ((A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha) / a0);
    raw[1] = (float)(-2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0) / a0);
    raw[2] = (float)(A * ((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha) / a0);
    raw[3] = 1.0f;
    raw[4] = (float)(2.0 * ((A - 1.0) - (A + 1.0) * cos_w0) / a0);
    raw[5] = (float)(((A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha) / a0);
}
// -----------------------------------------------------------------------------

void HomeDistoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    if (numSamples == 0) return;

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1)
    {
        for (int i = 1; i < totalNumOutputChannels; ++i)
            buffer.copyFrom(i, 0, buffer, 0, 0, numSamples);
    }
    else
    {
        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, numSamples);
    }

    if (apvts.getRawParameterValue("BYPASS")->load() > 0.5f)
        return;

    float driveDb = apvts.getRawParameterValue("DRIVE")->load();
    float lowCut = apvts.getRawParameterValue("LOW_CUT")->load();
    float highCut = apvts.getRawParameterValue("HIGH_CUT")->load();
    float tone = apvts.getRawParameterValue("TONE")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float punch = apvts.getRawParameterValue("PUNCH")->load();
    float outDb = apvts.getRawParameterValue("OUT")->load();
    int mode = juce::roundToInt(apvts.getRawParameterValue("MODE")->load());

    double sr = getSampleRate();
    if (sr <= 0.0) sr = 44100.0;

    smoothDrive.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    smoothOut.setTargetValue(juce::Decibels::decibelsToGain(outDb));
    smoothMix.setTargetValue(mix);
    smoothPunch.setTargetValue(punch);

    updateHighPass(lowCutFilter.state.get(), juce::jmax(20.0f, lowCut), sr);
    updateLowPass(highCutFilter.state.get(), juce::jmin(20000.0f, highCut), sr);
    float toneFreq = 1000.0f; 
    float toneDb = tone * 6.0f; 
    updateHighShelf(toneFilter.state.get(), toneFreq, 0.707f, juce::Decibels::decibelsToGain(toneDb), sr);

    if (dryBuffer.getNumChannels() < totalNumOutputChannels || dryBuffer.getNumSamples() < numSamples)
    {
        dryBuffer.setSize(juce::jmax(1, totalNumOutputChannels), numSamples, false, false, true);
    }

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    lowCutFilter.process(context);
    highCutFilter.process(context);

    juce::dsp::AudioBlock<float> dryBlock = juce::dsp::AudioBlock<float>(dryBuffer).getSubBlock(0, (size_t)numSamples);
    juce::dsp::ProcessContextReplacing<float> dryContext (dryBlock);
    dryLowCut.process(dryContext);
    dryHighCut.process(dryContext);

    // --- FIX: PRE-EQ BLOWUP PROTECTION ---
    // Protects against IIR filters exploding due to rapid automation sweeps
    bool wetBlewUp = false;
    bool dryBlewUp = false;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
        auto* wData = buffer.getWritePointer(ch);
        auto* dData = dryBuffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s) {
            if (!std::isfinite(wData[s]) || std::abs(wData[s]) > 24.0f) {
                wData[s] = 0.0f;
                wetBlewUp = true;
            }
            if (!std::isfinite(dData[s]) || std::abs(dData[s]) > 24.0f) {
                dData[s] = 0.0f;
                dryBlewUp = true;
            }
        }
    }
    // Snap filters back to reality if state history gets corrupted
    if (wetBlewUp) {
        lowCutFilter.reset();
        highCutFilter.reset();
    }
    if (dryBlewUp) {
        dryLowCut.reset();
        dryHighCut.reset();
    }

    // Measure inRMS *after* sanitation to guarantee it receives finite numbers
    float inRMS = 0.0f;
    if (totalNumOutputChannels > 0) {
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            inRMS += buffer.getRMSLevel(ch, 0, numSamples);
        inRMS /= (float)totalNumOutputChannels;
    }

    auto writePointers = buffer.getArrayOfWritePointers();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentDrive = smoothDrive.getNextValue();
        float currentPunch = smoothPunch.getNextValue();
        
        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            float x = writePointers[channel][sample] * currentDrive;
            float out = 0.0f;

            switch (mode)
            {
                case 0: out = std::tanh(x) * (1.0f + currentPunch * 0.5f); break;
                case 1: out = x > 0.0f ? std::tanh(x) : std::tanh(x * 0.8f); break;
                case 2: out = (2.0f / juce::MathConstants<float>::pi) * std::atan(x); break;
                case 3: out = std::max(-1.0f, std::min(1.0f, x)); break;
                case 4: out = std::sin(x); break;
                case 5: 
                    out = x > 0.0f ? 1.0f : -1.0f;
                    // FIX: Replaced !isnan with isfinite to prevent Inf parameters from causing std::exp UB
                    if (std::isfinite(x)) out *= (1.0f - std::exp(-std::abs(x * (1.0f + currentPunch))));
                    break;
                default: out = std::tanh(x);
            }
            writePointers[channel][sample] = out;
        }
    }

    toneFilter.process(context);
    dryTone.process(dryContext);

    // --- FIX: POST-EQ BLOWUP PROTECTION ---
    wetBlewUp = false;
    dryBlewUp = false;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
        auto* wData = buffer.getWritePointer(ch);
        auto* dData = dryBuffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s) {
            if (!std::isfinite(wData[s]) || std::abs(wData[s]) > 24.0f) {
                wData[s] = 0.0f;
                wetBlewUp = true;
            }
            if (!std::isfinite(dData[s]) || std::abs(dData[s]) > 24.0f) {
                dData[s] = 0.0f;
                dryBlewUp = true;
            }
        }
    }
    if (wetBlewUp) toneFilter.reset();
    if (dryBlewUp) dryTone.reset();

    float outRMS = 0.0f;
    if (totalNumOutputChannels > 0) {
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            outRMS += buffer.getRMSLevel(ch, 0, numSamples);
        outRMS /= (float)totalNumOutputChannels;
    }

    // FIX: Hard-check RMS for valid floats to ensure smoother targets never inherit NaN/Inf targets
    bool autoGainActive = apvts.getRawParameterValue("AUTO")->load() > 0.5f;
    if (autoGainActive && outRMS > 0.0001f && inRMS > 0.0001f && std::isfinite(outRMS) && std::isfinite(inRMS)) {
        // Clamp gain scaling to a strict +/- 24dB threshold
        float target = juce::jlimit(0.063f, 15.8f, inRMS / outRMS); 
        autoGainFactor.setTargetValue(target);
    } else {
        autoGainFactor.setTargetValue(1.0f);
    }

    auto dryPointers = dryBuffer.getArrayOfReadPointers();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentMix = smoothMix.getNextValue();
        float currentOut = smoothOut.getNextValue();
        float currentAutoGain = autoGainFactor.getNextValue();

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            float wetSignal = writePointers[channel][sample] * currentAutoGain;
            float drySignal = dryPointers[channel][sample];
            
            // Absolute final safety net: intercept rogue values immediately prior to host output
            float finalOut = (drySignal * (1.0f - currentMix) + wetSignal * currentMix) * currentOut;
            if (!std::isfinite(finalOut)) finalOut = 0.0f;
            
            writePointers[channel][sample] = finalOut;
        }
    }
}

void HomeDistoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HomeDistoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::File HomeDistoAudioProcessor::getPresetDirectory()
{
    juce::File documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::File presetDir = documentsDir.getChildFile("HomeDisto").getChildFile("Presets");
    
    if (!presetDir.exists())
        presetDir.createDirectory();
        
    return presetDir;
}

std::map<juce::String, juce::Array<juce::File>> HomeDistoAudioProcessor::getAllPresetsCategorized()
{
    std::map<juce::String, juce::Array<juce::File>> categories;
    juce::File dir = getPresetDirectory();
    
    auto files = dir.findChildFiles(juce::File::findFiles, true, "*.xml");
    for (auto& file : files)
    {
        juce::String category = file.getParentDirectory().getFileName();
        if (category == dir.getFileName()) category = "Uncategorized";
        categories[category].add(file);
    }
    return categories;
}

juce::Array<juce::File> HomeDistoAudioProcessor::getFlatPresetList()
{
    juce::Array<juce::File> list;
    auto categories = getAllPresetsCategorized();
    for (auto& pair : categories)
        for (auto& file : pair.second)
            list.add(file);
    return list;
}

void HomeDistoAudioProcessor::savePreset(const juce::String& name)
{
    juce::File userDir = getPresetDirectory().getChildFile("User");
    if (!userDir.exists()) userDir.createDirectory();
    
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    
    if (xml != nullptr)
    {
        juce::File presetFile = userDir.getChildFile(name + ".xml");
        xml->writeTo(presetFile);
        currentPresetFile = presetFile;
    }
}

void HomeDistoAudioProcessor::loadPreset(const juce::File& file)
{
    if (file.existsAsFile())
    {
        std::unique_ptr<juce::XmlElement> xmlState = juce::XmlDocument::parse(file);
        
        if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            currentPresetFile = file;
        }
    }
}

void HomeDistoAudioProcessor::nextPreset()
{
    auto list = getFlatPresetList();
    if (list.isEmpty()) return;
    int index = list.indexOf(currentPresetFile);
    index = (index + 1) % list.size(); 
    loadPreset(list[index]);
}

void HomeDistoAudioProcessor::prevPreset()
{
    auto list = getFlatPresetList();
    if (list.isEmpty()) return;
    int index = list.indexOf(currentPresetFile);
    index = (index - 1 < 0) ? list.size() - 1 : index - 1; 
    loadPreset(list[index]);
}

juce::AudioProcessorEditor* HomeDistoAudioProcessor::createEditor()
{
    return new HomeDistoAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HomeDistoAudioProcessor();
}