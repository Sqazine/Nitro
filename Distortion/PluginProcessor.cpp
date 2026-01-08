

#include "PluginProcessor.h"

DistortionAudioProcessor::DistortionAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         )
#endif
      ,
      mApvts(*this, nullptr),
      mDistortion(mApvts, "Distortion", "dB", 0.0f, 60.0f, 0.0f),
      mLevel(mApvts, "Level", "dB", -40.0f, 40.0f, 0.0f),
      mHighPassFrequency(mApvts, "High Pass Frequency", "Hz", 20.0f, 20000.0f, 20.0f),
      mLowPassFrequency(mApvts, "Low Pass Frequency", "Hz", 20.0f, 20000.0f, 20000.0f),
      mWetDry(mApvts, "Dry/Wet", "", 0.0f, 1.0f, 0.5f)
{
}

DistortionAudioProcessor::~DistortionAudioProcessor()
{
}

const juce::String DistortionAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DistortionAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool DistortionAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool DistortionAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double DistortionAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DistortionAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int DistortionAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DistortionAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String DistortionAudioProcessor::getProgramName(int index)
{
    return {};
}

void DistortionAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void DistortionAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    mSampleRate = static_cast<float>(spec.sampleRate);
    mMaxBlockSize = spec.maximumBlockSize;
    mNumChannels = spec.numChannels;

    mInputVolume.prepare(spec);
    mOutputVolume.prepare(spec);
    mLowPassFilter.prepare(spec);
    mHighPassFilter.prepare(spec);

    mOversampling->initProcessing(static_cast<size_t>(mMaxBlockSize));

    mOversampling->reset();
    mLowPassFilter.reset();
    mHighPassFilter.reset();
}

void DistortionAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DistortionAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

        // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void DistortionAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto numSamples = buffer.getNumSamples();

    for (auto i = juce::jmin(2, totalNumInputChannels); i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    float inputVol = mDistortion.getTargetValue();
    float outputVol = mLevel.getTargetValue();

    auto inputdB = juce::Decibels::decibelsToGain(inputVol);
    auto outputdB = juce::Decibels::decibelsToGain(outputVol);

    if (mInputVolume.getGainLinear() != inputdB)
        mInputVolume.setGainLinear(inputdB);
    if (mOutputVolume.getGainLinear() != outputdB)
        mOutputVolume.setGainLinear(outputdB);

    float freqLowPass = mLowPassFrequency.getTargetValue();
    *mLowPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(mSampleRate, freqLowPass);
    float freqHighPass = mHighPassFrequency.getTargetValue();
    *mHighPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(mSampleRate, freqHighPass);

    juce::dsp::AudioBlock<float> block(buffer);
    if (block.getNumChannels() > 2)
        block = block.getSubsetChannelBlock(0, 2);

    auto ctx = juce::dsp::ProcessContextReplacing<float>(block);

    juce::ScopedNoDenormals noDenormals;
    mInputVolume.process(ctx);
    mHighPassFilter.process(ctx);

    juce::dsp::AudioBlock<float> oversampledBlock = mOversampling->processSamplesUp(ctx.getInputBlock());
    auto waveshaperContext = juce::dsp::ProcessContextReplacing<float>(oversampledBlock);

    mWaveShapers.process(waveshaperContext);

    waveshaperContext.getOutputBlock() *= 0.7f;

    mOversampling->processSamplesDown(ctx.getOutputBlock());

    mLowPassFilter.process(ctx);
    mOutputVolume.process(ctx);
}

bool DistortionAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *DistortionAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void DistortionAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = mApvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DistortionAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(mApvts.state.getType()))
            mApvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DistortionAudioProcessor();
}
#endif