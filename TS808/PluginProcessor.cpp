#include "PluginProcessor.h"

TS808AudioProcessor::TS808AudioProcessor()
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
      mDrive(mApvts, "Drive", "%", 0.0f, 100.0f, 0.0f),
      mTone(mApvts, "Tone", "%", 0.0f, 100.0f, 50.0f),
      mLevel(mApvts, "Level", "%", 0.0f, 100.0f, 50.0f)
{
}

TS808AudioProcessor::~TS808AudioProcessor()
{
}

const juce::String TS808AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TS808AudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool TS808AudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool TS808AudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double TS808AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TS808AudioProcessor::getNumPrograms()
{
    return 1;
}

int TS808AudioProcessor::getCurrentProgram()
{
    return 0;
}

void TS808AudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String TS808AudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void TS808AudioProcessor::changeProgramName(int /*index*/, const juce::String &/*newName*/)
{
}

void TS808AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampleRate = static_cast<float>(sampleRate);

    mOversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    mOversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    mOversampling->reset();

    for (auto &m : mModel)
    {
        m.prepare(sampleRate, 4);
        m.reset();
    }
}

void TS808AudioProcessor::releaseResources()
{
    mOversampling.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TS808AudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void TS808AudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &/*midiMessages*/)
{
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    for (auto i = juce::jmin(2, totalNumInputChannels); i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    float drive = mDrive.getTargetValue() / 100.0f;
    float tone = mTone.getTargetValue() / 100.0f;
    float level = mLevel.getTargetValue() / 100.0f;

    for (auto &m : mModel)
        m.setParams(drive, tone, level);

    juce::ScopedNoDenormals noDenormals;

    juce::dsp::AudioBlock<float> block(buffer);
    if (block.getNumChannels() > 2)
        block = block.getSubsetChannelBlock(0, 2);

    auto osBlock = mOversampling->processSamplesUp(block);

    auto numOsChannels = osBlock.getNumChannels();
    auto numOsSamples = osBlock.getNumSamples();

    for (size_t ch = 0; ch < numOsChannels && ch < 2; ++ch)
    {
        auto *samples = osBlock.getChannelPointer(ch);
        auto &model = mModel[ch];

        for (size_t i = 0; i < numOsSamples; ++i)
            samples[i] = model.processSample(samples[i]);
    }

    mOversampling->processSamplesDown(block);
}

bool TS808AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *TS808AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void TS808AudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = mApvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TS808AudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(mApvts.state.getType()))
            mApvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new TS808AudioProcessor();
}
#endif