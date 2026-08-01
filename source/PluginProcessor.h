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

    // NEW: DC blocker. TUBE mode intentionally clips the positive and
    // negative halves of the waveform differently (that asymmetry is what
    // makes it sound like a tube) -- but any asymmetric waveshaper shifts
    // the waveform's average away from zero. Left alone that eats headroom
    // and can produce a click when a note starts/stops or when the plugin
    // is bypassed/enabled. Always-on, fixed ~15 Hz, not user-controllable.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlockerFilter;

    // DSP Components (Wet Path)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowCutFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highCutFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> toneFilter;

    // NEW: post-distortion "de-fizz" filter. LOW_CUT/HIGH_CUT only shape what
    // goes INTO the waveshaper -- they do nothing about the harsh upper
    // harmonics the waveshaper itself generates. This runs after the
    // distortion stage (wet path only) and is controlled by the SMOOTH
    // parameter, exposed in the settings popup.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> smoothFilter;
    
    // REDESIGNED: LOW_CUT/HIGH_CUT used to filter the ENTIRE signal (both
    // wet and a phase-matched "dry" copy), which meant content outside the
    // band wasn't just "left undistorted" -- it was filtered out of the
    // output altogether, on both paths. That's a normal EQ, not a focus
    // control. Now LOW_CUT/HIGH_CUT only carve out the band that gets fed
    // to the distortion; everything outside that band is recovered by
    // subtraction (dry - band) and passed through completely untouched --
    // no filtering, no distortion, phase-exact by construction. See
    // processBlock() for the full explanation.
    juce::AudioBuffer<float> outOfBandBuffer;
    
    juce::AudioBuffer<float> dryBuffer;

    // Parameter Smoothers (Anti-zipper noise)
    juce::SmoothedValue<float> smoothDrive;
    juce::SmoothedValue<float> smoothOut;
    juce::SmoothedValue<float> smoothMix;
    juce::SmoothedValue<float> smoothPunch;
    juce::SmoothedValue<float> autoGainFactor;

    // NEW: filter-cutoff smoothing. LOW_CUT/HIGH_CUT/TONE/SMOOTH previously
    // recalculated their IIR coefficients every block straight from the raw,
    // un-smoothed parameter value -- a fast knob sweep or automation ramp
    // could jump the cutoff enough in one block to click. These are simple
    // one-pole smoothers on the *parameter values themselves*, updated once
    // per block (filter coefficients are already only recomputed once per
    // block, not per-sample, so this is smoothed at the right rate).
    float smoothedLowCutHz = 20.0f;
    float smoothedHighCutHz = 20000.0f;
    float smoothedToneDb = 0.0f;
    float smoothedDeFizzHz = 20000.0f;
    static float onePoleApproach(float current, float target, float coeff) noexcept
    {
        return current + (target - current) * coeff;
    }

    // Oversampling for the nonlinear waveshaping stage only (prevents aliasing
    // from the distortion curves). 2x is always-on; 4x kicks in when the user
    // enables "HQ" mode from the settings button in the editor.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling4x;
    bool lastHqState = false;

    // FIX: Oversampling's internal buffers are only sized for the block size
    // given to initProcessing(). Reaper's "Anticipative FX processing" (which
    // kicks in automatically once a plugin reports latency, as ours does now)
    // can call processBlock() with larger blocks than prepareToPlay ever saw.
    // Track the largest block size we've actually prepared for so we can grow
    // everything on demand instead of silently overrunning fixed buffers.
    // prepareToPlay() also front-loads a generous cushion (see .cpp) so this
    // growth path essentially never actually fires in practice -- growing on
    // the audio thread is a fallback of last resort, not the normal path.
    int preparedBlockSize = 0;
    void ensureCapacityFor(int numSamples);

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