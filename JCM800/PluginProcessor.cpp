
#include "PluginProcessor.h"

JCM800AudioProcessor::JCM800AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
     , mApvts(*this, nullptr)
     , mGain(mApvts, "Gain", "GAIN", 0.0f, 1.0f, 0.5f)
     , mBass(mApvts, "Bass", "BASS", 0.0f, 1.0f, 0.5f)
     , mMiddle(mApvts, "Middle", "MID", 0.0f, 1.0f, 0.5f)
     , mHigh(mApvts, "High", "HIGH", 0.0f, 1.0f, 0.5f)
     , mVolume(mApvts, "Volume", "VOL", 0.0f, 1.0f, 0.5f)
     , mPresence(mApvts, "Presence", "PRES", 0.0f, 1.0f, 0.5f)
{
}

JCM800AudioProcessor::~JCM800AudioProcessor()
{
}

const juce::String JCM800AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JCM800AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool JCM800AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool JCM800AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double JCM800AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int JCM800AudioProcessor::getNumPrograms()
{
    return 1;
}

int JCM800AudioProcessor::getCurrentProgram()
{
    return 0;
}

void JCM800AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String JCM800AudioProcessor::getProgramName (int index)
{
    return {};
}

void JCM800AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void JCM800AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mProcessChain.reset();

    juce::dsp::ProcessSpec spec;
    spec.numChannels = 2;
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;

    mProcessChain.prepare(spec);

    auto &waveShaper = mProcessChain.get<waveShaperIndex>();
    
    waveShaper.functionToUse = [](float x) -> float {
        // JCM800风格的失真曲线 - 软削波结合指数特性
        float drive = 15.0f;
        x *= drive;
        
        if (x > 0.0f) {
            return 1.0f - std::exp(-x);
        } else {
            return -1.0f + std::exp(x);
        }
    };

    mPreviousGain = mGain.getTargetValue();
    mPreviousBass = mBass.getTargetValue();
    mPreviousMiddle = mMiddle.getTargetValue();
    mPreviousHigh = mHigh.getTargetValue();
    mPreviousPresence = mPresence.getTargetValue();
}

void JCM800AudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool JCM800AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void JCM800AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto currentGain = mGain.getTargetValue();
    auto currentBass = mBass.getTargetValue();
    auto currentMiddle = mMiddle.getTargetValue();
    auto currentHigh = mHigh.getTargetValue();
    auto currentVolume = mVolume.getTargetValue();
    auto currentPresence = mPresence.getTargetValue();

    juce::dsp::AudioBlock<float> block(buffer);
    if (block.getNumChannels() > 2)
        block = block.getSubsetChannelBlock(0, 2);

    auto &preGainStage = mProcessChain.get<preGainStageIndex>();
    auto &bassFilter = mProcessChain.get<bassFilterIndex>();
    auto &middleFilter = mProcessChain.get<middleFilterIndex>();
    auto &highFilter = mProcessChain.get<highFilterIndex>();
    auto &presenceFilter = mProcessChain.get<presenceFilterIndex>();
    auto &volumeStage = mProcessChain.get<volumeStageIndex>();

    preGainStage.setGainLinear(currentGain * 10.0f);

    // 低音处理 - 带通滤波器，中心频率约80Hz
    if (currentBass != mPreviousBass) {
        float bassFreq = 80.0f;
        float q = 0.707f;
        float gain = 1.0f + (currentBass - 0.5f) * 10.0f;
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), bassFreq, q, gain);
        bassFilter.coefficients = coeffs;
        mPreviousBass = currentBass;
    }

    // 中音处理 - 带通滤波器，中心频率约800Hz
    if (currentMiddle != mPreviousMiddle) {
        float middleFreq = 800.0f;
        float q = 0.707f;
        float gain = 1.0f + (currentMiddle - 0.5f) * 10.0f;
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), middleFreq, q, gain);
        middleFilter.coefficients = coeffs;
        mPreviousMiddle = currentMiddle;
    }

    // 高音处理 - 带通滤波器，中心频率约3kHz
    if (currentHigh != mPreviousHigh) {
        float highFreq = 3000.0f;
        float q = 0.707f;
        float gain = 1.0f + (currentHigh - 0.5f) * 10.0f;
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), highFreq, q, gain);
        highFilter.coefficients = coeffs;
        mPreviousHigh = currentHigh;
    }

    // 临场感处理
    if (currentPresence != mPreviousPresence) {
        float presenceFreq = 5000.0f + (currentPresence * 3000.0f);
        float q = 0.707f;
        float gain = 1.0f + (currentPresence - 0.5f) * 8.0f;
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), presenceFreq, q, gain);
        presenceFilter.coefficients = coeffs;
        mPreviousPresence = currentPresence;
    }

    volumeStage.setGainLinear(currentVolume);

    juce::dsp::ProcessContextReplacing<float> context(block);
    mProcessChain.process(context);
}

bool JCM800AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* JCM800AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void JCM800AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = mApvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void JCM800AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(mApvts.state.getType())) {
        mApvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JCM800AudioProcessor();
}
#endif
