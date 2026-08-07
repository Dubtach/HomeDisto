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
    auto lowCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(44100.0, 80.0f);
    auto highCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 8000.0f);
    for (auto& stage : lowCutStages)  stage.state = lowCutCoeffs;
    for (auto& stage : highCutStages) stage.state = highCutCoeffs;
    lowShelfFilter.state = juce::dsp::IIR::Coefficients<float>::makeLowShelf(44100.0, 80.0f, 0.707f, 1.0f);
    highShelfFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf(44100.0, 8000.0f, 0.707f, 1.0f);
    bell1Filter.state = juce::dsp::IIR::Coefficients<float>::makePeakFilter(44100.0, 400.0f, 0.8f, 1.0f);
    bell2Filter.state = juce::dsp::IIR::Coefficients<float>::makePeakFilter(44100.0, 2500.0f, 0.8f, 1.0f);
    toneFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf(44100.0, 1000.0f, 0.707f, 1.0f);
    smoothFilter.state = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 20000.0f);
    dcBlockerFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(44100.0, 15.0f);
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
    params.push_back(std::make_unique<juce::AudioParameterBool>("AUTO", "Auto", true));

    // REDESIGNED: was a 2-knob focus-band filter (LOW_CUT/HIGH_CUT only).
    // Now a proper 4-band EQ: LOW and HIGH can each be a cut or a shelf,
    // plus two fully parametric bell/peak bands in between. See the header
    // comment above the filter members for why the old "only this band
    // gets distorted" trick had to go once shelves/bells entered the
    // picture (that trick only worked with pure cut filters).
    juce::NormalisableRange<float> lowRange(20.0f, 2000.0f, 1.0f, 0.3f);
    juce::NormalisableRange<float> highRange(500.0f, 20000.0f, 10.0f, 0.3f);
    juce::NormalisableRange<float> bell1Range(60.0f, 6000.0f, 1.0f, 0.3f);
    juce::NormalisableRange<float> bell2Range(200.0f, 16000.0f, 1.0f, 0.3f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_LOW_FREQ", "Low Freq", lowRange, 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("EQ_LOW_TYPE", "Low Type", juce::StringArray{ "Cut", "Shelf" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_LOW_GAIN", "Low Gain", -18.0f, 18.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_BELL1_FREQ", "Bell 1 Freq", bell1Range, 400.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_BELL1_GAIN", "Bell 1 Gain", -18.0f, 18.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_BELL1_Q", "Bell 1 Q", 0.2f, 8.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_BELL2_FREQ", "Bell 2 Freq", bell2Range, 2500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_BELL2_GAIN", "Bell 2 Gain", -18.0f, 18.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_BELL2_Q", "Bell 2 Q", 0.2f, 8.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_HIGH_FREQ", "High Freq", highRange, 8000.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("EQ_HIGH_TYPE", "High Type", juce::StringArray{ "Cut", "Shelf" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("EQ_HIGH_GAIN", "High Gain", -18.0f, 18.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("PUNCH", "Punch", 0.0f, 1.0f, 0.5f));
    // FIX: default changed to 100% wet (was 50%) -- Decapitator-style
    // "the effect IS the sound" default workflow. MIX still exists and
    // still works normally if someone wants to blend it down; this only
    // changes what the plugin defaults to and what the factory presets use.
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 1.0f));

    // HQ mode: switches the internal oversampling used for the distortion
    // stage from 2x to 4x, trading a little extra latency/CPU for cleaner
    // high-drive tones. Wired to the gear/settings button in the editor.
    params.push_back(std::make_unique<juce::AudioParameterBool>("HQ", "HQ Mode", false));

    // NEW: post-distortion low-pass that tames harsh upper-harmonic fizz the
    // waveshaper generates. 0 = off/full character, 1 = heaviest smoothing.
    // Defaults to a little bit on -- most of the "harsh out of the box"
    // complaints are exactly this, an unfiltered distortion tail.
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SMOOTH", "Smooth (De-Fizz)", 0.0f, 1.0f, 0.35f));

    // Controls how many cascaded stages EQ_LOW/EQ_HIGH use when in CUT mode
    // -- 12/24/48 dB/octave. Has no effect on a band currently set to Shelf.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "SLOPE", "Filter Slope", juce::StringArray{ "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 0));

    return { params.begin(), params.end() };
}

void HomeDistoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    for (auto& stage : lowCutStages)  stage.prepare(spec);
    for (auto& stage : highCutStages) stage.prepare(spec);
    lastSlopeStageCount = 1;
    lowShelfFilter.prepare(spec);
    highShelfFilter.prepare(spec);
    bell1Filter.prepare(spec);
    bell2Filter.prepare(spec);
    toneFilter.prepare(spec);
    smoothFilter.prepare(spec);
    dcBlockerFilter.prepare(spec);
    dcBlockerFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 15.0f);

    // FIX: front-load a generous cushion (8192 samples, or the host's
    // reported block size if that's bigger) instead of exactly the block
    // size the host happens to report right now. Reaper's Anticipative FX
    // processing can call processBlock() with much bigger blocks than
    // prepareToPlay() advertised; sizing for that up front means the
    // fallback growth path in ensureCapacityFor() (a real, if rare,
    // allocation on the audio thread) essentially never actually triggers.
    const int safeInitialBlockSize = juce::jmax(samplesPerBlock, 8192);
    dryBuffer.setSize(getTotalNumOutputChannels(), safeInitialBlockSize);

    smoothDrive.reset(sampleRate, 0.02);
    smoothOut.reset(sampleRate, 0.02);
    smoothMix.reset(sampleRate, 0.02);
    smoothPunch.reset(sampleRate, 0.02);

    // NEW: fast ~8ms ramp -- short enough to be inaudible as a fade, long
    // enough to smoothly cover the discontinuity when MODE changes.
    modeSwitchGain.reset(sampleRate, 0.008);
    modeSwitchGain.setCurrentAndTargetValue(1.0f);
    lastModeForClickGuard = -1;
    

    // NEW: seed all the block-rate EQ smoothers from the actual current
    // parameter values, so the very first block after load doesn't ramp in
    // from stale defaults.
    smoothedLowFreqHz   = apvts.getRawParameterValue("EQ_LOW_FREQ")->load();
    smoothedLowGainDb   = apvts.getRawParameterValue("EQ_LOW_GAIN")->load();
    smoothedBell1FreqHz = apvts.getRawParameterValue("EQ_BELL1_FREQ")->load();
    smoothedBell1GainDb = apvts.getRawParameterValue("EQ_BELL1_GAIN")->load();
    smoothedBell1Q      = apvts.getRawParameterValue("EQ_BELL1_Q")->load();
    smoothedBell2FreqHz = apvts.getRawParameterValue("EQ_BELL2_FREQ")->load();
    smoothedBell2GainDb = apvts.getRawParameterValue("EQ_BELL2_GAIN")->load();
    smoothedBell2Q      = apvts.getRawParameterValue("EQ_BELL2_Q")->load();
    smoothedHighFreqHz  = apvts.getRawParameterValue("EQ_HIGH_FREQ")->load();
    smoothedHighGainDb  = apvts.getRawParameterValue("EQ_HIGH_GAIN")->load();
    smoothedToneDb = apvts.getRawParameterValue("TONE")->load() * 6.0f;
    smoothedDeFizzHz = juce::jmap(apvts.getRawParameterValue("SMOOTH")->load(), 0.0f, 1.0f, 20000.0f, 3000.0f);

    // --- Oversampling for the waveshaper (fixes aliasing on the distortion) ---
    auto numChannels = (size_t) juce::jmax(1, getTotalNumOutputChannels());

    oversampling2x = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling4x = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);

    // FIX: preparedBlockSize must be reset to 0 here so ensureCapacityFor()
    // below actually (re)initializes the fresh oversampling objects instead
    // of thinking they're already sized correctly from a previous prepare.
    preparedBlockSize = 0;
    ensureCapacityFor(safeInitialBlockSize);

    lastHqState = apvts.getRawParameterValue("HQ")->load() > 0.5f;
    setLatencySamples((int) (lastHqState ? oversampling4x->getLatencyInSamples()
                                          : oversampling2x->getLatencyInSamples()));
}

void HomeDistoAudioProcessor::ensureCapacityFor(int numSamples)
{
    // FIX: this is the actual crackle/"electric spark" bug. Oversampling's
    // internal buffers, and our own drive/punch envelope scratch buffers,
    // were only ever sized once, in prepareToPlay(), for the block size the
    // host announced up front. Once oversampling made this plugin report
    // latency, Reaper's Anticipative FX processing started calling
    // processBlock() with larger blocks than that -- so those fixed-size
    // buffers got overrun, and the waveshaper ended up reading/writing
    // garbage memory. That garbage is what you were hearing as sparking/
    // crackling noise. This grows everything on demand instead.
    if (numSamples <= preparedBlockSize)
        return;

    preparedBlockSize = numSamples;

    if (oversampling2x != nullptr) { oversampling2x->initProcessing((size_t) preparedBlockSize); oversampling2x->reset(); }
    if (oversampling4x != nullptr) { oversampling4x->initProcessing((size_t) preparedBlockSize); oversampling4x->reset(); }

    driveEnvBuffer.resize(preparedBlockSize);
    punchEnvBuffer.resize(preparedBlockSize);
    modeSwitchEnvBuffer.resize(preparedBlockSize);
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
// FIX: juce::dsp::IIR::Coefficients does NOT store a0 -- its constructor
// normalizes everything by a0 and keeps exactly 5 values internally:
// [b0, b1, b2, a1, a2]. getRawCoefficients() therefore points at a 5-element
// array, not 6. The old code here wrote raw[0..5] (six values), with raw[3]
// hardcoded to 1.0f standing in for "a0" -- that overwrote the *real* a1
// coefficient with garbage, shifted a1/a2 into the wrong slots, and wrote
// raw[5] one element past the end of the array on every single block, for
// every filter, continuously. That out-of-bounds write is what was
// corrupting memory and producing the crackling/sparking noise.
void HomeDistoAudioProcessor::updateHighPass(juce::dsp::IIR::Coefficients<float>* state, float freq, double sampleRate) {
    if (!state) return;
    freq = juce::jmin(freq, (float)(sampleRate * 0.499));
    double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * 0.70710678f);
    double a0 = 1.0 + alpha;
    
    auto* raw = state->getRawCoefficients();
    raw[0] = (float)((1.0 + cos_w0) / (2.0 * a0));  // b0
    raw[1] = (float)(-(1.0 + cos_w0) / a0);          // b1
    raw[2] = (float)((1.0 + cos_w0) / (2.0 * a0));  // b2
    raw[3] = (float)(-2.0 * cos_w0 / a0);            // a1
    raw[4] = (float)((1.0 - alpha) / a0);            // a2
}

void HomeDistoAudioProcessor::updateLowPass(juce::dsp::IIR::Coefficients<float>* state, float freq, double sampleRate) {
    if (!state) return;
    freq = juce::jmin(freq, (float)(sampleRate * 0.499));
    double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * 0.70710678f);
    double a0 = 1.0 + alpha;
    
    auto* raw = state->getRawCoefficients();
    raw[0] = (float)((1.0 - cos_w0) / (2.0 * a0));  // b0
    raw[1] = (float)((1.0 - cos_w0) / a0);           // b1
    raw[2] = (float)((1.0 - cos_w0) / (2.0 * a0));  // b2
    raw[3] = (float)(-2.0 * cos_w0 / a0);            // a1
    raw[4] = (float)((1.0 - alpha) / a0);            // a2
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
    raw[0] = (float)(A * ((A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha) / a0); // b0
    raw[1] = (float)(-2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0) / a0);                        // b1
    raw[2] = (float)(A * ((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha) / a0); // b2
    raw[3] = (float)(2.0 * ((A - 1.0) - (A + 1.0) * cos_w0) / a0);                             // a1
    raw[4] = (float)(((A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha) / a0);      // a2
}

// NEW: low-shelf (RBJ cookbook), for EQ_LOW when set to Shelf mode.
void HomeDistoAudioProcessor::updateLowShelf(juce::dsp::IIR::Coefficients<float>* state, float freq, float Q, float gain, double sampleRate) {
    if (!state) return;
    freq = juce::jmin(freq, (float)(sampleRate * 0.499));
    double A = std::sqrt(juce::jmax(0.0001f, gain));
    double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double a0 = (A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha;

    auto* raw = state->getRawCoefficients();
    raw[0] = (float)(A * ((A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha) / a0); // b0
    raw[1] = (float)(2.0 * A * ((A - 1.0) - (A + 1.0) * cos_w0) / a0);                         // b1
    raw[2] = (float)(A * ((A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha) / a0); // b2
    raw[3] = (float)(-2.0 * ((A - 1.0) + (A + 1.0) * cos_w0) / a0);                            // a1
    raw[4] = (float)(((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha) / a0);      // a2
}

// NEW: peaking/bell (RBJ cookbook), for the two BELL bands.
void HomeDistoAudioProcessor::updatePeak(juce::dsp::IIR::Coefficients<float>* state, float freq, float Q, float gain, double sampleRate) {
    if (!state) return;
    freq = juce::jmin(freq, (float)(sampleRate * 0.499));
    double A = std::sqrt(juce::jmax(0.0001f, gain));
    double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double cos_w0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * (double) juce::jmax(0.05f, Q));
    double a0 = 1.0 + alpha / A;

    auto* raw = state->getRawCoefficients();
    raw[0] = (float)((1.0 + alpha * A) / a0);  // b0
    raw[1] = (float)((-2.0 * cos_w0) / a0);     // b1
    raw[2] = (float)((1.0 - alpha * A) / a0);   // b2
    raw[3] = (float)((-2.0 * cos_w0) / a0);     // a1
    raw[4] = (float)((1.0 - alpha / A) / a0);   // a2
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
    float tone = apvts.getRawParameterValue("TONE")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float punch = apvts.getRawParameterValue("PUNCH")->load();
    float outDb = apvts.getRawParameterValue("OUT")->load();
    int mode = juce::roundToInt(apvts.getRawParameterValue("MODE")->load());

    // 4-band EQ parameters.
    float lowFreqRaw  = apvts.getRawParameterValue("EQ_LOW_FREQ")->load();
    float highFreqRaw = apvts.getRawParameterValue("EQ_HIGH_FREQ")->load();
    int lowType  = juce::roundToInt(apvts.getRawParameterValue("EQ_LOW_TYPE")->load());  // 0=Cut, 1=Shelf
    int highType = juce::roundToInt(apvts.getRawParameterValue("EQ_HIGH_TYPE")->load()); // 0=Cut, 1=Shelf
    float lowGainDb  = apvts.getRawParameterValue("EQ_LOW_GAIN")->load();
    float highGainDb = apvts.getRawParameterValue("EQ_HIGH_GAIN")->load();
    float bell1FreqRaw = apvts.getRawParameterValue("EQ_BELL1_FREQ")->load();
    float bell1GainDb  = apvts.getRawParameterValue("EQ_BELL1_GAIN")->load();
    float bell1Q       = apvts.getRawParameterValue("EQ_BELL1_Q")->load();
    float bell2FreqRaw = apvts.getRawParameterValue("EQ_BELL2_FREQ")->load();
    float bell2GainDb  = apvts.getRawParameterValue("EQ_BELL2_GAIN")->load();
    float bell2Q       = apvts.getRawParameterValue("EQ_BELL2_Q")->load();

    // FIX: LOW/HIGH could previously cross or pass straight through each
    // other with no constraint -- confusing to interact with and, in CUT
    // mode, produces a nonsensical inverted band. Enforce a minimum
    // separation, ratio-based since the frequency skew is logarithmic (a
    // fixed Hz gap would be meaningless at 15 kHz and huge at 30 Hz).
    constexpr float minLowHighRatio = 1.05f;
    if (highFreqRaw < lowFreqRaw * minLowHighRatio)
    {
        float mid = std::sqrt(lowFreqRaw * highFreqRaw);
        lowFreqRaw  = mid / std::sqrt(minLowHighRatio);
        highFreqRaw = mid * std::sqrt(minLowHighRatio);
    }

    double sr = getSampleRate();
    if (sr <= 0.0) sr = 44100.0;

    smoothDrive.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    smoothOut.setTargetValue(juce::Decibels::decibelsToGain(outDb));
    smoothMix.setTargetValue(mix);
    smoothPunch.setTargetValue(punch);

    // One-pole smoothing on every EQ band's freq/gain/Q at block rate, same
    // reasoning as before: a fast drag or automation ramp could otherwise
    // jump a value enough in a single block to click.
    constexpr float filterSmoothingCoeff = 0.25f;
    smoothedLowFreqHz   = onePoleApproach(smoothedLowFreqHz,   lowFreqRaw,   filterSmoothingCoeff);
    smoothedLowGainDb   = onePoleApproach(smoothedLowGainDb,   lowGainDb,    filterSmoothingCoeff);
    smoothedBell1FreqHz = onePoleApproach(smoothedBell1FreqHz, bell1FreqRaw, filterSmoothingCoeff);
    smoothedBell1GainDb = onePoleApproach(smoothedBell1GainDb, bell1GainDb,  filterSmoothingCoeff);
    smoothedBell1Q      = onePoleApproach(smoothedBell1Q,      bell1Q,       filterSmoothingCoeff);
    smoothedBell2FreqHz = onePoleApproach(smoothedBell2FreqHz, bell2FreqRaw, filterSmoothingCoeff);
    smoothedBell2GainDb = onePoleApproach(smoothedBell2GainDb, bell2GainDb,  filterSmoothingCoeff);
    smoothedBell2Q      = onePoleApproach(smoothedBell2Q,      bell2Q,       filterSmoothingCoeff);
    smoothedHighFreqHz  = onePoleApproach(smoothedHighFreqHz,  highFreqRaw,  filterSmoothingCoeff);
    smoothedHighGainDb  = onePoleApproach(smoothedHighGainDb,  highGainDb,   filterSmoothingCoeff);
    smoothedToneDb      = onePoleApproach(smoothedToneDb,      tone * 6.0f,  filterSmoothingCoeff);

    updateHighPass(lowCutStages[0].state.get(),  juce::jmax(20.0f, smoothedLowFreqHz), sr);
    updateLowPass(highCutStages[0].state.get(),  juce::jmin(20000.0f, smoothedHighFreqHz), sr);
    updateLowShelf(lowShelfFilter.state.get(),   juce::jmax(20.0f, smoothedLowFreqHz), 0.707f, juce::Decibels::decibelsToGain(smoothedLowGainDb), sr);
    updateHighShelf(highShelfFilter.state.get(), juce::jmin(20000.0f, smoothedHighFreqHz), 0.707f, juce::Decibels::decibelsToGain(smoothedHighGainDb), sr);
    updatePeak(bell1Filter.state.get(), smoothedBell1FreqHz, smoothedBell1Q, juce::Decibels::decibelsToGain(smoothedBell1GainDb), sr);
    updatePeak(bell2Filter.state.get(), smoothedBell2FreqHz, smoothedBell2Q, juce::Decibels::decibelsToGain(smoothedBell2GainDb), sr);

    float toneFreq = 1000.0f; 
    updateHighShelf(toneFilter.state.get(), toneFreq, 0.707f, juce::Decibels::decibelsToGain(smoothedToneDb), sr);

    float smoothAmt = apvts.getRawParameterValue("SMOOTH")->load();
    float smoothFreqTarget = juce::jmap(smoothAmt, 0.0f, 1.0f, 20000.0f, 3000.0f);
    smoothedDeFizzHz = onePoleApproach(smoothedDeFizzHz, smoothFreqTarget, filterSmoothingCoeff);
    updateLowPass(smoothFilter.state.get(), smoothedDeFizzHz, sr);

    // FIX: grow the oversampling + envelope scratch buffers if this block is
    // bigger than anything we've prepared for (see ensureCapacityFor for why).
    ensureCapacityFor(numSamples);

    if (dryBuffer.getNumChannels() < totalNumOutputChannels || dryBuffer.getNumSamples() < numSamples)
    {
        dryBuffer.setSize(juce::jmax(1, totalNumOutputChannels), numSamples, false, false, true);
    }

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // REDESIGNED: this is now a standard 4-band EQ shaping tone BEFORE the
    // distortion stage, applied to the whole wet signal (see the header
    // comment above the filter members for why the old "only this band
    // gets distorted" subtraction trick isn't compatible with shelves/bells).
    bell1Filter.process(context);
    bell2Filter.process(context);

    // SLOPE picks how many identical cut stages get cascaded for a
    // steeper roll-off: 1 stage = 12 dB/oct, 2 = 24, 4 = 48. Only relevant
    // when a band is actually in Cut mode.
    int slopeIndex = juce::roundToInt(apvts.getRawParameterValue("SLOPE")->load());
    int slopeStageCount = (slopeIndex <= 0) ? 1 : (slopeIndex == 1 ? 2 : 4);
    if (slopeStageCount != lastSlopeStageCount)
    {
        // Stages that were sitting idle may hold stale filter history;
        // reset everything on a slope change so it doesn't click.
        for (auto& stage : lowCutStages)  stage.reset();
        for (auto& stage : highCutStages) stage.reset();
        lastSlopeStageCount = slopeStageCount;
    }

    if (lowType == 0) { for (int i = 0; i < slopeStageCount; ++i) lowCutStages[i].process(context); }
    else               { lowShelfFilter.process(context); }

    if (highType == 0) { for (int i = 0; i < slopeStageCount; ++i) highCutStages[i].process(context); }
    else                { highShelfFilter.process(context); }

    // --- FIX: PRE-EQ BLOWUP PROTECTION ---
    // Protects against IIR filters exploding due to rapid automation sweeps.
    bool wetBlewUp = false;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
        auto* wData = buffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s) {
            if (!std::isfinite(wData[s]) || std::abs(wData[s]) > 24.0f) {
                wData[s] = 0.0f;
                wetBlewUp = true;
            }
        }
    }
    // Snap filters back to reality if state history gets corrupted
    if (wetBlewUp) {
        for (auto& stage : lowCutStages)  stage.reset();
        for (auto& stage : highCutStages) stage.reset();
        lowShelfFilter.reset();
        highShelfFilter.reset();
        bell1Filter.reset();
        bell2Filter.reset();
    }

    // REDESIGNED: AUTO used to be RMS-based makeup gain computed here on
    // the audio thread (measure input RMS, measure output RMS after
    // distortion, apply the ratio as smoothed gain). That's been removed
    // entirely -- AUTO is now a UI-layer feature that automatically moves
    // the OUTPUT knob in response to DRIVE/TONE/PUNCH/MODE changes (see
    // HomeDistoAudioProcessorEditor::applyAutoGainCompensation). The audio
    // thread no longer does anything for AUTO at all.

    // --- Waveshaping stage, run oversampled to avoid aliasing ---
    // FIX: previously this ran directly at the project sample rate, which
    // means every nonlinear curve below (tanh/atan/hard-clip/sin/fuzz) was
    // generating harmonics well above Nyquist that folded back down as
    // audible aliasing, especially at high DRIVE. Now the block is upsampled
    // before shaping and decimated back down afterward.
    bool hqOn = apvts.getRawParameterValue("HQ")->load() > 0.5f;
    auto& activeOversampling = hqOn ? *oversampling4x : *oversampling2x;

    if (hqOn != lastHqState)
    {
        oversampling2x->reset();
        oversampling4x->reset();
        setLatencySamples((int) activeOversampling.getLatencyInSamples());
        lastHqState = hqOn;
    }

    // FIX: mode switches had zero crossfade -- the waveshaper would jump
    // straight from one curve to a completely different one on the very
    // next sample. Most curves output ~0 for ~0 input, but FUZZ adds a DC
    // bias (x + 0.35) before saturating, so it does NOT output ~0 near
    // silence -- switching into or out of FUZZ is exactly the case with
    // the biggest step discontinuity, which is why that one clicked loudest.
    // Rather than crossfading two entire curves (expensive, fiddly), a
    // short mute-and-refade around the switch instant hides the
    // discontinuity in near-silence instead of letting it click through.
    if (mode != lastModeForClickGuard)
    {
        modeSwitchGain.setCurrentAndTargetValue(0.0f);
        modeSwitchGain.setTargetValue(1.0f);
        lastModeForClickGuard = mode;
    }

    // Pre-compute the smoothed DRIVE/PUNCH/mode-switch envelopes at block
    // rate; these get held across however many oversampled sub-samples
    // correspond to each original sample.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        driveEnvBuffer.set(sample, smoothDrive.getNextValue());
        punchEnvBuffer.set(sample, smoothPunch.getNextValue());
        modeSwitchEnvBuffer.set(sample, modeSwitchGain.getNextValue());
    }

    auto oversampledBlock = activeOversampling.processSamplesUp(block);
    int factor = (int) activeOversampling.getOversamplingFactor();
    auto numUpSamples = oversampledBlock.getNumSamples();
    auto numUpChannels = oversampledBlock.getNumChannels();

    for (size_t sample = 0; sample < numUpSamples; ++sample)
    {
        int origIndex = juce::jlimit(0, numSamples - 1, (int) (sample / (size_t) factor));
        float currentDrive = driveEnvBuffer[origIndex];
        float currentPunch = punchEnvBuffer[origIndex];
        float currentSwitchGain = modeSwitchEnvBuffer[origIndex];

        for (size_t channel = 0; channel < numUpChannels; ++channel)
        {
            auto* channelData = oversampledBlock.getChannelPointer(channel);
            float x = channelData[sample] * currentDrive;
            float out = 0.0f;

            // REDESIGNED: modes were sharing too much DNA -- PUNCH/TUBE/TAPE
            // were all mild variations on tanh, and DIGITAL/FUZZ were both
            // "smooth-ish hard saturation" once oversampled. Each mode now
            // uses a genuinely different waveshaping mechanism so they're
            // distinguishable by ear, not just by name. CRUNCH is untouched
            // -- it was already doing its own thing (wavefolding).
            switch (mode)
            {
                case 0:
                    // PUNCH: the plain reference clipper. Symmetric tanh --
                    // deliberately the "baseline" the other modes deviate
                    // from, so it stays simple.
                    out = std::tanh(x) * (1.0f + currentPunch * 0.5f);
                    break;

                case 1:
                {
                    // TUBE: real even-harmonic warmth via a quadratic bias
                    // term before saturating (x + k*x^2), not just a
                    // slightly-different tanh slope on one side. This is
                    // what actually gives tube-style asymmetric coloration;
                    // the old version was too subtle to hear next to PUNCH.
                    // The DC blocker downstream safely removes the offset
                    // this bias creates.
                    float biased = x + 0.22f * x * x;
                    out = std::tanh(biased);
                    break;
                }

                case 2:
                {
                    // TAPE: classic cubic soft-clip (x - x^3/3, clamped
                    // beyond +/-1). FIX: the old atan-based curve compresses
                    // signal even at low levels (its slope is under 1 almost
                    // immediately), which is exactly why TAPE sounded
                    // quieter than the other modes at matched DRIVE. Cubic
                    // soft-clip has unity slope at x=0 -- quiet passages
                    // pass through at the same level as other modes -- and
                    // only rounds off once you approach the knee, which is
                    // also a more distinct (rounder, lower-order-harmonic)
                    // character than tanh.
                    if (x >= 1.0f)       out = 2.0f / 3.0f;
                    else if (x <= -1.0f) out = -2.0f / 3.0f;
                    else                 out = x - (x * x * x) / 3.0f;
                    break;
                }

                case 3:
                {
                    // DIGITAL: brick-wall clip PLUS bit-depth quantization.
                    // FIX: a plain hard clip alone shares too much harmonic
                    // character with FUZZ's saturation once both are
                    // oversampled and driven hard. Quantizing on top gives
                    // DIGITAL an actual stepped/gritty texture that reads as
                    // "digital" and can't be mistaken for a smooth fuzz.
                    float clipped = juce::jlimit(-1.0f, 1.0f, x);
                    const float levels = 14.0f;
                    out = std::round(clipped * levels) / levels;
                    break;
                }

                case 4:
                    // CRUNCH: unchanged -- wavefolding already gives it a
                    // complex, metallic identity nothing else here has.
                    out = std::sin(x);
                    break;

                case 5:
                {
                    // FUZZ: asymmetric bias before saturating -- real fuzz
                    // pedals are biased/"starved" transistor circuits, which
                    // is what gives fuzz its gated, sputtery character
                    // rather than a clean symmetric saturation. FIX: the old
                    // version was symmetric and, once smooth, sat too close
                    // to DIGITAL's (now-quantized) hard clip. The DC blocker
                    // downstream handles the resulting offset safely.
                    float biasedX = x + 0.35f;
                    out = biasedX > 0.0f ? 1.0f : -1.0f;
                    if (std::isfinite(biasedX))
                        out *= (1.0f - std::exp(-std::abs(biasedX * (1.0f + currentPunch))));
                    break;
                }

                default: out = std::tanh(x);
            }

            // FIX: PUNCH previously only had an effect in modes 0 and 5 (the
            // cases above that reference currentPunch directly). Every other
            // mode ignored the knob entirely. This adds a generic
            // punch-scaled, asymmetric harmonic boost on top of every mode's
            // curve so the knob is always audible, everywhere.
            // FIX: was 0.25f -- that added a lot of hard-edged harmonic
            // energy on every mode by default and was a real contributor to
            // the "harsh" complaint. 0.12f keeps PUNCH audible without
            // stacking that much extra grit on top of the mode's own curve.
            out += currentPunch * 0.12f * out * std::abs(out);
            // FIX: this used to clamp to +/-4.0 (about +12 dBFS), which is
            // "safe" only in the sense of not being NaN/Inf -- it still let
            // a genuinely ear-splitting signal through to the mix stage.
            // +/-2.0 still gives the waveshaper/punch stage real headroom to
            // work with before the final output limiter (added below) takes
            // over completely.
            out = juce::jlimit(-2.0f, 2.0f, out);

            // Mask the mode-switch discontinuity: ~1 (inaudible) during
            // normal playback, briefly dips to 0 and ramps back up right at
            // the instant MODE changes.
            out *= currentSwitchGain;

            channelData[sample] = out;
        }
    }

    activeOversampling.processSamplesDown(block);

    // NEW: DC blocker, right off the back of the waveshaper -- this is
    // exactly where TUBE mode's intentional asymmetry (and any future
    // asymmetric curve) introduces a DC offset. Doing it here, before
    // SMOOTH/TONE, keeps those stages working on a zero-centered signal.
    dcBlockerFilter.process(context);

    // NEW: de-fizz -- tames the harsh upper-harmonic content the waveshaper
    // just generated, before the user-controlled TONE shelf gets applied.
    smoothFilter.process(context);

    toneFilter.process(context);

    // --- FIX: POST-EQ BLOWUP PROTECTION ---
    wetBlewUp = false;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
        auto* wData = buffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s) {
            if (!std::isfinite(wData[s]) || std::abs(wData[s]) > 24.0f) {
                wData[s] = 0.0f;
                wetBlewUp = true;
            }
        }
    }
    if (wetBlewUp) toneFilter.reset();

    auto dryPointers = dryBuffer.getArrayOfReadPointers();
    auto writePointers = buffer.getArrayOfWritePointers();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentMix = smoothMix.getNextValue();
        float currentOut = smoothOut.getNextValue();

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            // REDESIGNED: back to a standard dry/fully-processed blend now
            // that the 4-band EQ shapes the whole wet signal before
            // distortion (rather than a subtracted "only this band gets
            // distorted" focus region, which isn't compatible with
            // shelves/bell bands -- see the header comment above the filter
            // members). At MIX=1 you hear the fully EQ'd + distorted
            // signal; at MIX=0 the literal unprocessed original.
            float wetSignal = writePointers[channel][sample];
            float drySignal = dryPointers[channel][sample];
            
            // Absolute final safety net: intercept rogue values immediately prior to host output
            float finalOut = (drySignal * (1.0f - currentMix) + wetSignal * currentMix) * currentOut;
            if (!std::isfinite(finalOut)) finalOut = 0.0f;

            // FIX: there was no actual output ceiling anywhere in the chain.
            // DRIVE (up to +24 dB) feeding the shaper, plus OUTPUT (up to
            // another +24 dB) on top, meant the final level could land well
            // past 0 dBFS with nothing to stop it -- which is exactly what
            // pegs a DAW meter red the moment the plugin is inserted/played,
            // regardless of what state/preset it happens to load in.
            // tanh() is ~identity for anything under about -6 dBFS (fully
            // transparent for normal signal levels) and smoothly limits
            // anything hotter than that toward +/-1.0, so it can never blast
            // out past unity no matter what the knobs/preset are doing.
            finalOut = std::tanh(finalOut);

            writePointers[channel][sample] = finalOut;
        }
    }
}

void HomeDistoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // FIX: lockOutput/lockMix/lockEQ were plain in-memory atomics with no
    // persistence at all -- they survive the editor being closed/reopened
    // (the processor itself stays alive for that), but a real session
    // save/close/reopen, or any host that reconstructs the processor,
    // silently lost them. They're intentionally NOT APVTS parameters (a
    // host shouldn't be able to automate "lock this knob"), but they still
    // need to be part of the saved state -- stored as plain custom
    // properties directly on the same ValueTree, so they ride along with
    // everything else through copyState()/replaceState().
    apvts.state.setProperty("lockOutput", lockOutput.load(), nullptr);
    apvts.state.setProperty("lockMix", lockMix.load(), nullptr);
    apvts.state.setProperty("lockEQ", lockEQ.load(), nullptr);

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HomeDistoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        // FIX: hosts are permitted to call setStateInformation() from any
        // thread -- some genuinely do this on a background project-loading
        // thread rather than the message thread. apvts.replaceState() swaps
        // out the entire parameter ValueTree and rewires its internal
        // listeners; it is only safe to call on the message thread. Doing
        // that swap concurrently with the audio thread reading parameters
        // is exactly the "XML parser overwriting variables the audio thread
        // is reading" crash scenario. Parsing the XML into a standalone
        // ValueTree is self-contained and safe on any thread; only the
        // actual replaceState() call needs to be deferred.
        juce::ValueTree newState = juce::ValueTree::fromXml (*xmlState);

        // Read the lock flags now (cheap, thread-safe -- just reading
        // properties off a standalone ValueTree), defaulting to off for
        // older saves made before this existed.
        bool newLockOutput = newState.hasProperty("lockOutput") && (bool) newState.getProperty("lockOutput");
        bool newLockMix    = newState.hasProperty("lockMix")    && (bool) newState.getProperty("lockMix");
        bool newLockEQ     = newState.hasProperty("lockEQ")     && (bool) newState.getProperty("lockEQ");

        juce::MessageManager::callAsync ([this, newState, newLockOutput, newLockMix, newLockEQ]() mutable
        {
            apvts.replaceState (newState);
            lockOutput.store(newLockOutput);
            lockMix.store(newLockMix);
            lockEQ.store(newLockEQ);
        });
    }
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
    // NOTE: unlike setStateInformation (which a host can call from any
    // thread), this is only ever invoked from editor button/menu callbacks,
    // which JUCE guarantees run on the message thread -- so calling
    // apvts.replaceState() directly here is safe as written. If this is
    // ever called from anywhere else, route it through the same
    // MessageManager::callAsync pattern used in setStateInformation.
    if (file.existsAsFile())
    {
        std::unique_ptr<juce::XmlElement> xmlState = juce::XmlDocument::parse(file);
        
        if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        {
            // NEW: capture the current OUT/MIX/EQ values before the preset
            // overwrites them, so locked knobs can be restored afterward.
            float savedOutDb = apvts.getRawParameterValue("OUT")->load();
            float savedMix   = apvts.getRawParameterValue("MIX")->load();

            static const char* eqParamIDs[] = {
                "EQ_LOW_FREQ", "EQ_LOW_TYPE", "EQ_LOW_GAIN",
                "EQ_BELL1_FREQ", "EQ_BELL1_GAIN", "EQ_BELL1_Q",
                "EQ_BELL2_FREQ", "EQ_BELL2_GAIN", "EQ_BELL2_Q",
                "EQ_HIGH_FREQ", "EQ_HIGH_TYPE", "EQ_HIGH_GAIN"
            };
            float savedEq[12];
            for (int i = 0; i < 12; ++i)
                savedEq[i] = apvts.getRawParameterValue(eqParamIDs[i])->load();

            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            currentPresetFile = file;

            if (lockOutput.load())
                if (auto* p = apvts.getParameter("OUT"))
                    p->setValueNotifyingHost(p->convertTo0to1(savedOutDb));

            if (lockMix.load())
                if (auto* p = apvts.getParameter("MIX"))
                    p->setValueNotifyingHost(p->convertTo0to1(savedMix));

            if (lockEQ.load())
            {
                for (int i = 0; i < 12; ++i)
                    if (auto* p = apvts.getParameter(eqParamIDs[i]))
                        p->setValueNotifyingHost(p->convertTo0to1(savedEq[i]));
            }
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