

#include "PluginProcessor.h"


DistortionAudioProcessor::DistortionAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
    , parameters(*this, nullptr, juce::Identifier("Distortion"), {
        std::make_unique<juce::AudioParameterFloat>(IDs::inputVolume,"distortion",juce::NormalisableRange<float>(0.0f,60.0f),0.0f," dB",juce::AudioProcessorParameter::genericParameter,[](float value,int)
            {
            return static_cast<juce::String>(round(value * 100.f) / 100.f);
            },
            nullptr),
        std::make_unique<juce::AudioParameterFloat>(IDs::outputVolume,"level",juce::NormalisableRange<float>(-40.0f,40.0f),0.0f," dB",juce::AudioProcessorParameter::genericParameter,[](float value,int)
            {
            return static_cast<juce::String>(round(value * 100.f) / 100.f);
            },
            nullptr),
        std::make_unique<juce::AudioParameterFloat>(IDs::HPFreq,"highpass freq",juce::NormalisableRange<float>(20.0f,20000.0f,0.01f,0.2299f),0.0f," Hz",juce::AudioProcessorParameter::genericParameter,[](float value,int)
            {
            return static_cast<juce::String>(round(value * 100.f) / 100.f);
            },
            nullptr),
        std::make_unique<juce::AudioParameterFloat>(IDs::LPFreq,"lowpass freq",juce::NormalisableRange<float>(20.0f,20000.0f,0.01f,0.2299f),20000.0f," Hz",juce::AudioProcessorParameter::genericParameter,[](float value,int)
            {
            return static_cast<juce::String>(round(value * 100.f) / 100.f);
            },
            nullptr),
        std::make_unique<juce::AudioParameterFloat>(IDs::wetDry,"mix",juce::NormalisableRange<float>(0.0f,1.0f),0.5f,juce::String(),juce::AudioProcessorParameter::genericParameter,[](float value,int)
            {
            return static_cast<juce::String>(round(value * 100.f * 100.0f) / 100.f);
            },
            nullptr)
        })
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
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
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

    sampleRate = static_cast<float>(spec.sampleRate);
    maxBlockSize = spec.maximumBlockSize;
    numChannels = spec.numChannels;

    inputVolume.prepare(spec);
    outputVolume.prepare(spec);
    lowPassFilter.prepare(spec);
    highPassFilter.prepare(spec);

    oversampling->initProcessing(static_cast<size_t>(maxBlockSize));

    oversampling->reset();
    lowPassFilter.reset();
    highPassFilter.reset();
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
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
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

    float inputVol = *parameters.getRawParameterValue(IDs::inputVolume);
    float outputVol = *parameters.getRawParameterValue(IDs::outputVolume);

    auto inputdB = juce::Decibels::decibelsToGain(inputVol);
    auto outputdB = juce::Decibels::decibelsToGain(outputVol);

    if (inputVolume.getGainLinear() != inputdB)
        inputVolume.setGainLinear(inputdB);
    if (outputVolume.getGainLinear() != outputdB)
        outputVolume.setGainLinear(outputdB);

    float freqLowPass = *parameters.getRawParameterValue(IDs::LPFreq);
    *lowPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, freqLowPass);
    float freqHighPass = *parameters.getRawParameterValue(IDs::HPFreq);
    *highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, freqHighPass);

    juce::dsp::AudioBlock<float> block(buffer);
    if (block.getNumChannels() > 2)
        block = block.getSubsetChannelBlock(0, 2);

    auto ctx = juce::dsp::ProcessContextReplacing<float>(block);

    juce::ScopedNoDenormals noDenormals;
    inputVolume.process(ctx);
    highPassFilter.process(ctx);

    juce::dsp::AudioBlock<float> oversampledBlock = oversampling->processSamplesUp(ctx.getInputBlock());
    auto waveshaperContext = juce::dsp::ProcessContextReplacing<float>(oversampledBlock);

    waveShapers.process(waveshaperContext);

    waveshaperContext.getOutputBlock() *= 0.7f;

    oversampling->processSamplesDown(ctx.getOutputBlock());

    lowPassFilter.process(ctx);
    outputVolume.process(ctx);
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
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DistortionAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DistortionAudioProcessor();
}
#endif