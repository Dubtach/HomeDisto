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
    
    // Distortion Engine
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", 1.0f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SHAPE", "Shape", 0.1f, 1.0f, 0.5f));
    
    // FIXED: Changed to Gain ranges (-15dB to +15dB) for a proper 3-band EQ
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LOW", "Low", -15.0f, 15.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MID", "Mid", -15.0f, 15.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("HIGH", "High", -15.0f, 15.0f, 0.0f));
    
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

    lowEQ.prepare(spec);
    midEQ.prepare(spec);
    highEQ.prepare(spec);
    
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

    // FIXED: We are now fetching ALL parameters from the UI!
    float drive = apvts.getRawParameterValue("DRIVE")->load();
    float shape = apvts.getRawParameterValue("SHAPE")->load();
    float lowDB = apvts.getRawParameterValue("LOW")->load();
    float midDB = apvts.getRawParameterValue("MID")->load();
    float highDB = apvts.getRawParameterValue("HIGH")->load();
    float revAmount = apvts.getRawParameterValue("REVERB")->load();
    float sat = apvts.getRawParameterValue("SAT")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float outVol = apvts.getRawParameterValue("OUT")->load();

    // Update EQ Coefficients
    *lowEQ.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(getSampleRate(), 250.0f, 0.707f, juce::Decibels::decibelsToGain(lowDB));
    *midEQ.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), 1000.0f, 0.707f, juce::Decibels::decibelsToGain(midDB));
    *highEQ.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), 4000.0f, 0.707f, juce::Decibels::decibelsToGain(highDB));

    // Update Reverb Settings
    reverbParams.roomSize = revAmount;
    reverbParams.wetLevel = revAmount;
    reverbParams.dryLevel = 1.0f - (revAmount * 0.5f);
    reverb.setParameters(reverbParams);

    // Copy dry buffer for Mix
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // 1. Apply Drive & Asymmetric Shape Distortion
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float in = channelData[sample] * drive; 
            
            // The SHAPE slider now limits positive wave peaks to create asymmetry
            if (in > 0.0f)
                channelData[sample] = std::tanh(in) * shape;
            else
                channelData[sample] = std::tanh(in);
        }
    }

    // 2. Process 3-Band EQ
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    lowEQ.process(context);
    midEQ.process(context);
    highEQ.process(context);

    // 3. Apply Saturation Color Stage
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = std::tanh(channelData[sample] * sat);
        }
    }

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

// Global filter entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HomeDistoAudioProcessor();
}