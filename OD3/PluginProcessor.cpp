#include "PluginProcessor.h"

OD3AudioProcessor::OD3AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput ("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         )
#endif
      ,
      mApvts (*this, nullptr),
      mDrive (mApvts, "Drive", "%", 0.0f, 100.0f, 0.0f),
      mTone (mApvts, "Tone", "%", 0.0f, 100.0f, 50.0f),
      mMid (mApvts, "Mid", "%", 0.0f, 100.0f, 50.0f),
      mLevel (mApvts, "Level", "%", 0.0f, 100.0f, 50.0f)
{
}

OD3AudioProcessor::~OD3AudioProcessor()
{
}

const juce::String OD3AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool OD3AudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool OD3AudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool OD3AudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double OD3AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int OD3AudioProcessor::getNumPrograms()
{
    return 1;
}

int OD3AudioProcessor::getCurrentProgram()
{
    return 0;
}

void OD3AudioProcessor::setCurrentProgram (int /*index*/)
{
}

const juce::String OD3AudioProcessor::getProgramName (int /*index*/)
{
    return {};
}

void OD3AudioProcessor::changeProgramName (int /*index*/, const juce::String &/*newName*/)
{
}

void OD3AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mSampleRate = static_cast<float> (sampleRate);

    mOversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    mOversampling->initProcessing (static_cast<size_t> (samplesPerBlock));
    mOversampling->reset();

    for (auto &m : mModel)
    {
        m.prepare (sampleRate, 4);
        m.reset();
    }
}

void OD3AudioProcessor::releaseResources()
{
    mOversampling.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OD3AudioProcessor::isBusesLayoutSupported (const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
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

void OD3AudioProcessor::processBlock (juce::AudioBuffer<float> &buffer, juce::MidiBuffer &/*midiMessages*/)
{
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    for (auto i = juce::jmin (2, totalNumInputChannels); i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    float drive = mDrive.getTargetValue() / 100.0f;
    float tone = mTone.getTargetValue() / 100.0f;
    float mid  = mMid.getTargetValue() / 100.0f;
    float level = mLevel.getTargetValue() / 100.0f;

    for (auto &m : mModel)
        m.setParams (drive, tone, mid, level);

    juce::ScopedNoDenormals noDenormals;

    juce::dsp::AudioBlock<float> block (buffer);
    if (block.getNumChannels() > 2)
        block = block.getSubsetChannelBlock (0, 2);

    auto osBlock = mOversampling->processSamplesUp (block);

    auto numOsChannels = osBlock.getNumChannels();
    auto numOsSamples = osBlock.getNumSamples();

    for (size_t ch = 0; ch < numOsChannels && ch < 2; ++ch)
    {
        auto *samples = osBlock.getChannelPointer (ch);
        auto &model = mModel[ch];

        for (size_t i = 0; i < numOsSamples; ++i)
            samples[i] = model.processSample (samples[i]);
    }

    mOversampling->processSamplesDown (block);
}

bool OD3AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *OD3AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void OD3AudioProcessor::getStateInformation (juce::MemoryBlock &destData)
{
    auto state = mApvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void OD3AudioProcessor::setStateInformation (const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (mApvts.state.getType()))
            mApvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new OD3AudioProcessor();
}
#endif