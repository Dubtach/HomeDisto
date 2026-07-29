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
    // WaveShaper for Distortion (Soft Clipping / Tanh)
    driveShaper.functionToUse = [](float x) { return std::tanh(x); };
}

HomeDistoAudioProcessor::~HomeDistoAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout HomeDistoAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Distortion Engine
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", 1.0f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SHAPE", "Shape", 0.1f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LOW", "Low", 20.0f, 20000.0f, 250.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MID", "Mid", 20.0f, 20000.0f, 1000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("HIGH", "High", 20.0f, 20000.0f, 4000.0f));
    
    // Domestic Color
    params.push_back(std::make_unique<juce::AudioParameterFloat>("REVERB", "Reverb", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SAT", "Saturation", 1.0f, 5.0f, 1.0f));
    
    // Output
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("OUT", "Output", 0.0f, 2.0f, 1.0f));

    return { params.begin(), params.end() };
}

void HomeDistoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    driveShaper.prepare(spec);
    lowPass.prepare(spec);
    lowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    highPass.prepare(spec);
    highPass.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    bandPass.prepare(spec);
    bandPass.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    
    reverb.setSampleRate(sampleRate);
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

    // Get Parameters
    float drive = apvts.getRawParameterValue("DRIVE")->load();
    float lowFreq = apvts.getRawParameterValue("LOW")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float outVol = apvts.getRawParameterValue("OUT")->load();
    float revAmount = apvts.getRawParameterValue("REVERB")->load();

    // Update Filter and Reverb Settings
    lowPass.setCutoffFrequency(lowFreq);
    reverbParams.roomSize = revAmount;
    reverbParams.wetLevel = revAmount;
    reverbParams.dryLevel = 1.0f - (revAmount * 0.5f);
    reverb.setParameters(reverbParams);

    // Copy dry buffer for Mix
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // 1. Apply Drive
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= drive; 
        }
    }
    
    // 2. WaveShaper Distortion
    driveShaper.process(context);

    // 3. Filters
    lowPass.process(context);

    // 4. Reverb Processing
    if (totalNumInputChannels == 1)
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
    else if (totalNumInputChannels == 2)
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());

    // 5. Dry/Wet Mix & Output Gain
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* dryData = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float wetSignal = channelData[sample];
            float drySignal = dryData[sample];
            channelData[sample] = (drySignal * (1.0f - mix) + wetSignal * mix) * outVol;
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

juce::AudioProcessorEditor* HomeDistoAudioProcessor::createEditor()
{
    return new HomeDistoAudioProcessorEditor (*this);
}