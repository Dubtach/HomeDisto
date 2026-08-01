#pragma once
#include <JuceHeader.h>

class HomeDistoAudioProcessor  : public juce::AudioProcessor
{
public:
    HomeDistoAudioProcessor();
    ~HomeDistoAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Home:Disto"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Preset System Methods
    juce::File getPresetDirectory();
    void savePreset(const juce::String& name);
    void loadPreset(const juce::File& file);
    
    std::map<juce::String, juce::Array<juce::File>> getAllPresetsCategorized();
    juce::Array<juce::File> getFlatPresetList();
    void nextPreset();
    void prevPreset();
    
    juce::File currentPresetFile;
    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    // DSP Components (Wet Path)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowCutFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highCutFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> toneFilter;
    
    // DSP Components (Dry Path - matched to prevent comb filtering)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dryLowCut;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dryHighCut;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dryTone;
    
    juce::AudioBuffer<float> dryBuffer;

    // Parameter Smoothers (Anti-zipper noise)
    juce::SmoothedValue<float> smoothDrive;
    juce::SmoothedValue<float> smoothOut;
    juce::SmoothedValue<float> smoothMix;
    juce::SmoothedValue<float> smoothPunch;
    juce::SmoothedValue<float> autoGainFactor;

    // Oversampling for the nonlinear waveshaping stage only (prevents aliasing
    // from the distortion curves). 2x is always-on; 4x kicks in when the user
    // enables "HQ" mode from the settings button in the editor.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling4x;
    bool lastHqState = false;

    // Per-block scratch buffers for smoothed drive/punch values at the
    // (non-oversampled) block rate, indexed and held across oversampled
    // sub-samples. Sized in prepareToPlay to avoid realtime allocation.
    juce::Array<float> driveEnvBuffer, punchEnvBuffer;

    // Real-time safe IIR coefficient updates (prevents Heap Allocation)
    void updateHighPass(juce::dsp::IIR::Coefficients<float>* state, float freq, double sampleRate);
    void updateLowPass(juce::dsp::IIR::Coefficients<float>* state, float freq, double sampleRate);
    void updateHighShelf(juce::dsp::IIR::Coefficients<float>* state, float freq, float Q, float gain, double sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessor)
};