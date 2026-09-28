#include "PluginProcessor.h"

DS2AudioProcessor::DS2AudioProcessor()
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
      mDrive(mApvts, "Dist", "%", 0.0f, 100.0f, 0.0f),
      mTone(mApvts, "Tone", "%", 0.0f, 100.0f, 50.0f),
      mLevel(mApvts, "Level", "%", 0.0f, 100.0f, 50.0f),
      mMode(mApvts, "Mode", "", juce::StringArray{ "Turbo I", "Turbo II"}, 0)
{
}

DS2AudioProcessor::~DS2AudioProcessor()
{
}

const juce::String DS2AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DS2AudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool DS2AudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool DS2AudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double DS2AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DS2AudioProcessor::getNumPrograms()
{
    return 1;
}

int DS2AudioProcessor::getCurrentProgram()
{
    return 0;
}

void DS2AudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String DS2AudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void DS2AudioProcessor::changeProgramName(int /*index*/, const juce::String &/*newName*/)
{
}

void DS2AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
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

void DS2AudioProcessor::releaseResources()
{
    mOversampling.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DS2AudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
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

void DS2AudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &/*midiMessages*/)
{
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    for (auto i = juce::jmin(2, totalNumInputChannels); i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    float drive = mDrive.getTargetValue() / 100.0f;
    float tone = mTone.getTargetValue() / 100.0f;
    float level = mLevel.getTargetValue() / 100.0f;

    TurboMode mode;
    int idx = static_cast<int>(mMode.getTargetValue() + 0.5f);
    if (idx >= 1)
        mode = TurboMode::TurboII;
    else if (idx >= 0)
        mode = TurboMode::TurboI;

    for (auto &m : mModel)
        m.setParams(drive, tone, level, mode);

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

bool DS2AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *DS2AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void DS2AudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = mApvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DS2AudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(mApvts.state.getType()))
            mApvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new DS2AudioProcessor();
}
#endif