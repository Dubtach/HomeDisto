#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    lowCutFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
    highCutFilter.state = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 20000.0f);
    toneFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1000.0f, 0.707f, 1.0f);

    lowCutFilter.prepare(spec);
    highCutFilter.prepare(spec);
    toneFilter.prepare(spec);

    dryBuffer.setSize(spec.numChannels, samplesPerBlock);
}

void HomeDistoAudioProcessor::releaseResources() {}

bool HomeDistoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void HomeDistoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (apvts.getRawParameterValue("BYPASS")->load() > 0.5f)
        return;

    float driveDb = apvts.getRawParameterValue("DRIVE")->load();
    float driveGain = juce::Decibels::decibelsToGain(driveDb);
    float lowCut = apvts.getRawParameterValue("LOW_CUT")->load();
    float highCut = apvts.getRawParameterValue("HIGH_CUT")->load();
    float tone = apvts.getRawParameterValue("TONE")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float punch = apvts.getRawParameterValue("PUNCH")->load();
    float outDb = apvts.getRawParameterValue("OUT")->load();
    float outGain = juce::Decibels::decibelsToGain(outDb);
    int mode = juce::roundToInt(apvts.getRawParameterValue("MODE")->load());

    *lowCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), juce::jmax(20.0f, lowCut));
    *highCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), juce::jmin(20000.0f, highCut));
    
    float toneFreq = 1000.0f; 
    float toneDb = tone * 6.0f; 
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), toneFreq, 0.707f, juce::Decibels::decibelsToGain(toneDb));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    lowCutFilter.process(context);
    highCutFilter.process(context);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float x = channelData[sample] * driveGain;
            float out = 0.0f;

            switch (mode)
            {
                case 0: out = std::tanh(x) * (1.0f + punch * 0.5f); break;
                case 1: out = x > 0.0f ? std::tanh(x) : std::tanh(x * 0.8f); break;
                case 2: out = (2.0f / juce::MathConstants<float>::pi) * std::atan(x); break;
                case 3: out = std::max(-1.0f, std::min(1.0f, x)); break;
                case 4: out = std::sin(x); break;
                case 5: 
                    out = x > 0.0f ? 1.0f : -1.0f;
                    out *= (1.0f - std::exp(-std::abs(x * (1.0f + punch))));
                    break;
                default: out = std::tanh(x);
            }
            channelData[sample] = out;
        }
    }

    toneFilter.process(context);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* dryData = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float wetSignal = channelData[sample];
            float drySignal = dryData[sample];
            channelData[sample] = (drySignal * (1.0f - mix) + wetSignal * mix) * outGain;
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

void HomeDistoAudioProcessor::savePreset(const juce::String& name)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    
    if (xml != nullptr)
    {
        juce::File presetFile = getPresetDirectory().getChildFile(name + ".xml");
        xml->writeTo(presetFile);
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
        }
    }
}

juce::AudioProcessorEditor* HomeDistoAudioProcessor::createEditor()
{
    return new HomeDistoAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HomeDistoAudioProcessor();
}