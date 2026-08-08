#pragma once
#include <JuceHeader.h>
#include <array>

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

    // NEW: preset value locks. When set, loadPreset() below will restore
    // OUT/MIX to whatever the user had them at instead of taking the
    // preset's stored value. Plain atomics rather than APVTS parameters on
    // purpose -- this isn't something a host should automate or save as
    // part of "the sound", it's a UI convenience toggled by the lock icons
    // next to those two knobs. Only ever touched from the message thread
    // (UI button clicks, and loadPreset() which is itself only ever called
    // from UI callbacks), so plain std::atomic<bool> is sufficient here.
    std::atomic<bool> lockOutput { false };
    std::atomic<bool> lockMix { false };
    // NEW: locks the entire 4-band EQ (all 12 params: LOW freq/type/gain,
    // BELL1/BELL2 freq/gain/Q, HIGH freq/type/gain) across preset changes,
    // same idea as lockOutput/lockMix above.
    std::atomic<bool> lockEQ { false };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    // NEW: DC blocker. TUBE mode intentionally clips the positive and
    // negative halves of the waveform differently (that asymmetry is what
    // makes it sound like a tube) -- but any asymmetric waveshaper shifts
    // the waveform's average away from zero. Left alone that eats headroom
    // and can produce a click when a note starts/stops or when the plugin
    // is bypassed/enabled. Always-on, fixed ~15 Hz, not user-controllable.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlockerFilter;

    // REDESIGNED: this used to be a 2-knob "focus band" filter that used a
    // subtraction trick (isolate a band, distort only that, sum the
    // untouched rest back in). That trick only works with pure cut filters.
    // Now that LOW/HIGH can be shelves too, and there are 2 bell/peak bands,
    // there's no clean "everything else" to recover via subtraction anymore
    // (shelves/bells change gain rather than fully removing content). This
    // is now a standard 4-band EQ that shapes tone BEFORE the distortion
    // stage -- the same signal-chain design virtually every amp-sim/
    // distortion plugin uses. MIX is back to a plain dry/fully-processed
    // blend (see processBlock).
    //
    // Band 1 (LOW):  cut (cascaded highpass, slope-controlled) or shelf
    // Band 2 (BELL1): parametric peak/bell
    // Band 3 (BELL2): parametric peak/bell
    // Band 4 (HIGH): cut (cascaded lowpass, slope-controlled) or shelf
    static constexpr int kMaxFilterStages = 4;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>, kMaxFilterStages> lowCutStages;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>, kMaxFilterStages> highCutStages;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowShelfFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highShelfFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> bell1Filter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> bell2Filter;

    // FIX: at any MIX between 0-100%, dry and wet were being summed
    // directly -- but wet goes through the 4-band EQ (real IIR filters,
    // which all have a phase response) while dry stayed completely
    // unfiltered/phase-flat. Summing two signals with different phase
    // response at the same frequencies is exactly what comb filtering is;
    // it reads as "hollow", "phasey", "something missing" -- present at
    // any partial mix, gone at 0 or 100% because only one signal is
    // present then. Each of these mirrors its wet counterpart exactly,
    // SHARING the same Coefficients object (assigned in the constructor)
    // so they always process identically -- only the per-instance filter
    // state differs, which is what keeps dry and wet phase-matched.
    // Deliberate scope: only the EQ is mirrored, not TONE/SMOOTH/the
    // waveshaper -- those are the distortion stage's own character and
    // have nothing to phase-match against on a signal that was never
    // distorted in the first place.
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>, kMaxFilterStages> dryLowCutStages;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>, kMaxFilterStages> dryHighCutStages;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dryLowShelfFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dryHighShelfFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dryBell1Filter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dryBell2Filter;
    int lastSlopeStageCount = 1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> toneFilter;

    // NEW: post-distortion "de-fizz" filter. This runs after the distortion
    // stage and is controlled by the SMOOTH parameter in the settings popup.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> smoothFilter;
    
    juce::AudioBuffer<float> dryBuffer;

    // Parameter Smoothers (Anti-zipper noise)
    juce::SmoothedValue<float> smoothDrive;
    juce::SmoothedValue<float> smoothOut;
    juce::SmoothedValue<float> smoothMix;
    juce::SmoothedValue<float> smoothPunch;

    // NEW: mode-switch declick. See processBlock for why an abrupt curve
    // switch (especially into/out of FUZZ) clicks, and why a short
    // mute-and-refade around the switch instant fixes it.
    juce::SmoothedValue<float> modeSwitchGain;
    int lastModeForClickGuard = -1;
    juce::Array<float> modeSwitchEnvBuffer;

    // NEW: block-rate one-pole smoothing for every EQ band's freq/gain/Q --
    // same reasoning as before (a fast drag/automation ramp could otherwise
    // jump a cutoff/gain enough in one block to click), just extended to
    // cover the new bands.
    float smoothedLowFreqHz = 80.0f, smoothedLowGainDb = 0.0f;
    float smoothedBell1FreqHz = 400.0f, smoothedBell1GainDb = 0.0f, smoothedBell1Q = 0.8f;
    float smoothedBell2FreqHz = 2500.0f, smoothedBell2GainDb = 0.0f, smoothedBell2Q = 0.8f;
    float smoothedHighFreqHz = 8000.0f, smoothedHighGainDb = 0.0f;
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
    // NEW: low-shelf (for EQ_LOW in shelf mode) and peak/bell (for the two
    // BELL bands) -- same RBJ-cookbook-derived, 5-element-array-correct
    // approach as the other update* functions above.
    void updateLowShelf(juce::dsp::IIR::Coefficients<float>* state, float freq, float Q, float gain, double sampleRate);
    void updatePeak(juce::dsp::IIR::Coefficients<float>* state, float freq, float Q, float gain, double sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessor)
};