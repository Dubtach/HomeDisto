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
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", 0.0f, 10.0f, 5.0f));
    
    juce::StringArray algos = { "Warm", "Punch", "Tape", "Digital", "Fuzz" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("ALGO", "Algorithm", algos, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LOW_CUT", "Low Cut", 20.0f, 1000.0f, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("HIGH_CUT", "High Cut", 1000.0f, 20000.0f, 20000.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", -10.0f, 10.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("PUNCH", "Punch", 0.0f, 10.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("TEXTURE", "Texture", 0.0f, 10.0f, 0.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("OUT", "Output", -24.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("AUTO", "Auto", false));

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

    // Fetch parameters
    float drive = apvts.getRawParameterValue("DRIVE")->load();
    float lowCut = apvts.getRawParameterValue("LOW_CUT")->load();
    float highCut = apvts.getRawParameterValue("HIGH_CUT")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float outVol = apvts.getRawParameterValue("OUT")->load();

    // Update Filter Coefficients
    *lowCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), lowCut);
    *highCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), highCut);

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // 1. Basic Distortion Pass (Placeholder for algorithms)
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = std::tanh(channelData[sample] * (1.0f + drive));
        }
    }

    // 2. Filters
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    lowCutFilter.process(context);
    highCutFilter.process(context);

    // 3. Dry/Wet Mix & Output Gain
    float outGainLinear = juce::Decibels::decibelsToGain(outVol);
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* dryData = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float wetSignal = channelData[sample];
            float drySignal = dryData[sample];
            channelData[sample] = (drySignal * (1.0f - mix) + wetSignal * mix) * outGainLinear;
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HomeDistoAudioProcessor();
}