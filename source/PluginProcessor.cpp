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
    auto lowCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(44100.0, 20.0f);
    auto highCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 20000.0f);
    for (auto& stage : lowCutStages)  stage.state = lowCutCoeffs;
    for (auto& stage : highCutStages) stage.state = highCutCoeffs;
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
    params.push_back(std::make_unique<juce::AudioParameterBool>("AUTO", "Auto", false));

    juce::NormalisableRange<float> lowRange(20.0f, 1000.0f, 1.0f, 0.3f);
    juce::NormalisableRange<float> highRange(1000.0f, 20000.0f, 10.0f, 0.3f);
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LOW_CUT", "Low Cut", lowRange, 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("HIGH_CUT", "High Cut", highRange, 8500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("PUNCH", "Punch", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 0.5f));

    // HQ mode: switches the internal oversampling used for the distortion
    // stage from 2x to 4x, trading a little extra latency/CPU for cleaner
    // high-drive tones. Wired to the gear/settings button in the editor.
    params.push_back(std::make_unique<juce::AudioParameterBool>("HQ", "HQ Mode", false));

    // NEW: post-distortion low-pass that tames harsh upper-harmonic fizz the
    // waveshaper generates. 0 = off/full character, 1 = heaviest smoothing.
    // Defaults to a little bit on -- most of the "harsh out of the box"
    // complaints are exactly this, an unfiltered distortion tail.
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SMOOTH", "Smooth (De-Fizz)", 0.0f, 1.0f, 0.35f));

    // NEW: controls how many cascaded stages LOW_CUT/HIGH_CUT use --
    // 12/24/48 dB/octave. Exposed in the settings popup rather than the main
    // interface since it's a "shape the tool" control, not something you'd
    // sweep during a mix.
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
    outOfBandBuffer.setSize(getTotalNumOutputChannels(), safeInitialBlockSize);

    smoothDrive.reset(sampleRate, 0.02);
    smoothOut.reset(sampleRate, 0.02);
    smoothMix.reset(sampleRate, 0.02);
    smoothPunch.reset(sampleRate, 0.02);
    
    autoGainFactor.reset(sampleRate, 0.05);
    autoGainFactor.setCurrentAndTargetValue(1.0f);

    // NEW: seed the block-rate filter-cutoff smoothers from the actual
    // current parameter values, so the very first block after load doesn't
    // ramp in from stale defaults.
    smoothedLowCutHz = apvts.getRawParameterValue("LOW_CUT")->load();
    smoothedHighCutHz = apvts.getRawParameterValue("HIGH_CUT")->load();
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
    float lowCut = apvts.getRawParameterValue("LOW_CUT")->load();
    float highCut = apvts.getRawParameterValue("HIGH_CUT")->load();
    float tone = apvts.getRawParameterValue("TONE")->load();
    float mix = apvts.getRawParameterValue("MIX")->load();
    float punch = apvts.getRawParameterValue("PUNCH")->load();
    float outDb = apvts.getRawParameterValue("OUT")->load();
    int mode = juce::roundToInt(apvts.getRawParameterValue("MODE")->load());

    double sr = getSampleRate();
    if (sr <= 0.0) sr = 44100.0;

    smoothDrive.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    smoothOut.setTargetValue(juce::Decibels::decibelsToGain(outDb));
    smoothMix.setTargetValue(mix);
    smoothPunch.setTargetValue(punch);

    // NEW: one-pole smoothing on the filter-cutoff parameters themselves,
    // at block rate, before they're turned into IIR coefficients. Without
    // this, a fast knob sweep or automation ramp on LOW_CUT/HIGH_CUT/TONE/
    // SMOOTH could jump the cutoff enough in a single block to click.
    // coeff ~0.25 settles in roughly 8-12 blocks (a couple hundred ms),
    // fast enough to feel responsive, slow enough to not click.
    constexpr float filterSmoothingCoeff = 0.25f;
    smoothedLowCutHz  = onePoleApproach(smoothedLowCutHz,  lowCut,        filterSmoothingCoeff);
    smoothedHighCutHz = onePoleApproach(smoothedHighCutHz, highCut,       filterSmoothingCoeff);
    smoothedToneDb    = onePoleApproach(smoothedToneDb,    tone * 6.0f,   filterSmoothingCoeff);

    updateHighPass(lowCutStages[0].state.get(), juce::jmax(20.0f, smoothedLowCutHz), sr);
    updateLowPass(highCutStages[0].state.get(), juce::jmin(20000.0f, smoothedHighCutHz), sr);
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
    if (outOfBandBuffer.getNumChannels() < totalNumOutputChannels || outOfBandBuffer.getNumSamples() < numSamples)
    {
        outOfBandBuffer.setSize(juce::jmax(1, totalNumOutputChannels), numSamples, false, false, true);
    }

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // REDESIGNED FILTER BEHAVIOR:
    // LOW_CUT/HIGH_CUT now define a "focus band" -- only the audio inside
    // that range gets fed to the distortion at all. Everything outside it
    // is recovered by subtracting the band-passed signal from the original
    // ("outOfBand = dry - band") and passed straight to the output with NO
    // filtering and NO distortion applied to it whatsoever. Because it's a
    // literal subtraction rather than a second independent filter, the two
    // pieces always sum back to the original exactly -- no comb filtering,
    // no gaps, regardless of how steep or gentle the band edges are.
    // NEW: SLOPE picks how many identical stages get cascaded for a
    // steeper roll-off: 1 stage = 12 dB/oct, 2 = 24, 4 = 48.
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

    for (int i = 0; i < slopeStageCount; ++i) lowCutStages[i].process(context);   // buffer now holds only content above LOW_CUT
    for (int i = 0; i < slopeStageCount; ++i) highCutStages[i].process(context);  // buffer now holds only the LOW_CUT..HIGH_CUT band

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* dry = dryBuffer.getWritePointer(ch);
        auto* band = buffer.getWritePointer(ch);
        auto* outOfBand = outOfBandBuffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s)
            outOfBand[s] = dry[s] - band[s];
    }

    // --- FIX: PRE-EQ BLOWUP PROTECTION ---
    // Protects against IIR filters exploding due to rapid automation sweeps.
    // Only the focus band and its derived out-of-band complement can carry
    // filter-instability artifacts here (dryBuffer itself is untouched raw
    // audio and doesn't need sanitizing).
    bool wetBlewUp = false;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
        auto* wData = buffer.getWritePointer(ch);
        auto* oData = outOfBandBuffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s) {
            if (!std::isfinite(wData[s]) || std::abs(wData[s]) > 24.0f) {
                wData[s] = 0.0f;
                wetBlewUp = true;
            }
            if (!std::isfinite(oData[s]) || std::abs(oData[s]) > 24.0f) {
                oData[s] = 0.0f;
            }
        }
    }
    // Snap filters back to reality if state history gets corrupted
    if (wetBlewUp) {
        for (auto& stage : lowCutStages)  stage.reset();
        for (auto& stage : highCutStages) stage.reset();
    }

    // Measure inRMS *after* sanitation to guarantee it receives finite numbers
    float inRMS = 0.0f;
    if (totalNumOutputChannels > 0) {
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            inRMS += buffer.getRMSLevel(ch, 0, numSamples);
        inRMS /= (float)totalNumOutputChannels;
    }

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

    // Pre-compute the smoothed DRIVE/PUNCH envelopes at block rate; these get
    // held across however many oversampled sub-samples correspond to each
    // original sample.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        driveEnvBuffer.set(sample, smoothDrive.getNextValue());
        punchEnvBuffer.set(sample, smoothPunch.getNextValue());
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

    float outRMS = 0.0f;
    if (totalNumOutputChannels > 0) {
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            outRMS += buffer.getRMSLevel(ch, 0, numSamples);
        outRMS /= (float)totalNumOutputChannels;
    }

    // FIX: Hard-check RMS for valid floats to ensure smoother targets never inherit NaN/Inf targets
    bool autoGainActive = apvts.getRawParameterValue("AUTO")->load() > 0.5f;
    if (autoGainActive && outRMS > 0.0001f && inRMS > 0.0001f && std::isfinite(outRMS) && std::isfinite(inRMS)) {
        // Clamp gain scaling to a strict +/- 24dB threshold
        float target = juce::jlimit(0.063f, 15.8f, inRMS / outRMS); 
        autoGainFactor.setTargetValue(target);
    } else {
        autoGainFactor.setTargetValue(1.0f);
    }

    auto dryPointers = dryBuffer.getArrayOfReadPointers();
    auto outOfBandPointers = outOfBandBuffer.getArrayOfReadPointers();
    auto writePointers = buffer.getArrayOfWritePointers();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentMix = smoothMix.getNextValue();
        float currentOut = smoothOut.getNextValue();
        float currentAutoGain = autoGainFactor.getNextValue();

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            // REDESIGNED: "wet" is no longer the whole signal run through
            // distortion -- it's the untouched out-of-band content plus the
            // distorted focus band, recombined. At MIX=1 you hear the full
            // original signal with only the LOW_CUT..HIGH_CUT range altered;
            // at MIX=0 you hear the literal, unfiltered original (drySignal
            // is now genuinely raw -- it never went through
            // LOW_CUT/HIGH_CUT/TONE at all, unlike before).
            float distortedBand = writePointers[channel][sample] * currentAutoGain;
            float wetSignal = outOfBandPointers[channel][sample] + distortedBand;
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

        juce::MessageManager::callAsync ([this, newState]() mutable
        {
            apvts.replaceState (newState);
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
            // NEW: capture the current OUT/MIX values before the preset
            // overwrites them, so locked knobs can be restored afterward.
            float savedOutDb = apvts.getRawParameterValue("OUT")->load();
            float savedMix   = apvts.getRawParameterValue("MIX")->load();

            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            currentPresetFile = file;

            if (lockOutput.load())
                if (auto* p = apvts.getParameter("OUT"))
                    p->setValueNotifyingHost(p->convertTo0to1(savedOutDb));

            if (lockMix.load())
                if (auto* p = apvts.getParameter("MIX"))
                    p->setValueNotifyingHost(p->convertTo0to1(savedMix));
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