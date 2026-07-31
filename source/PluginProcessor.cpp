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
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", 0.0f, 24.0f, 6.7f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("MODE", "Mode", juce::StringArray{"PUNCH", "TUBE", "TAPE", "DIGITAL"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("OUT", "Output", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("AUTO", "Auto", false));

    // Added Skew (logarithmic feel) to frequency bands for smoother dragging
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

    float driveDb = apvts.getRawParameterValue("DRIVE")->load();
    float driveGain = juce::Decibels::decibelsToGain(driveDb);
    float lowCut = apvts.getRawParameterValue("LOW_CUT")->load();
    float highCut = apvts.getRawParameterValue("HIGH_CUT")->load();
    float tone = apvts.getRawParameterValue("TONE")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float outDb = apvts.getRawParameterValue("OUT")->load();
    float outGain = juce::Decibels::decibelsToGain(outDb);

    *lowCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), juce::jmax(20.0f, lowCut));
    *highCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), juce::jmin(20000.0f, highCut));
    
    float toneFreq = 1000.0f; 
    float toneDb = tone * 6.0f; 
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), toneFreq, 0.707f, juce::Decibels::decibelsToGain(toneDb));

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    lowCutFilter.process(context);
    highCutFilter.process(context);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = std::tanh(channelData[sample] * driveGain);
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

juce::AudioProcessorEditor* HomeDistoAudioProcessor::createEditor()
{
    return new HomeDistoAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HomeDistoAudioProcessor();
}