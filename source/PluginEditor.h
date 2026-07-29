#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class HomeDistoAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor&);
    ~HomeDistoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    HomeDistoAudioProcessor& audioProcessor;

    // UI Elements
    juce::Slider driveKnob;
    juce::Slider shapeSlider;
    juce::Slider lowKnob;
    juce::Slider midKnob;
    juce::Slider highKnob;
    
    juce::Slider reverbSlider;
    juce::Slider satKnob;
    
    juce::Slider mixKnob;
    juce::Slider outputKnob;

    // Attachments (connect UI to DSP)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttach, shapeAttach, lowAttach, midAttach, highAttach;
    std::unique_ptr<SliderAttachment> revAttach, satAttach, mixAttach, outAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessorEditor)
};