

#include "PluginProcessor.h"

SimpleEQAudioProcessor::SimpleEQAudioProcessor()
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
	  mApvts(*this, nullptr), mLowCutFreq(mApvts, "Low Cut Freq", "Hz", 20.0f, 20000.0f, 20.0f), mLowCutQuality(mApvts, "Low Cut Quality", "", 0.71f, 10.0f, 0.1f), mHighCutFreq(mApvts, "High Cut Freq", "Hz", 20.0f, 20000.0f, 20000.0f), mHighCutQuality(mApvts, "High Cut Quality", "", 0.71f, 10.0f, 0.1f)
{
}

SimpleEQAudioProcessor::~SimpleEQAudioProcessor()
{
}

const juce::String SimpleEQAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool SimpleEQAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool SimpleEQAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool SimpleEQAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double SimpleEQAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int SimpleEQAudioProcessor::getNumPrograms()
{
	return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
			  // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQAudioProcessor::getCurrentProgram()
{
	return 0;
}

void SimpleEQAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String SimpleEQAudioProcessor::getProgramName(int index)
{
	return {};
}

void SimpleEQAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void SimpleEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	juce::dsp::ProcessSpec spec;
	spec.maximumBlockSize = samplesPerBlock;
	spec.numChannels = 1;
	spec.sampleRate = sampleRate;

	mLeftChain.prepare(spec);
	mRightChain.prepare(spec);

	auto lowCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(),
																				mLowCutFreq.getTargetValue(),
																				mLowCutQuality.getTargetValue());
	mLeftChain.get<0>().coefficients = *lowCutCoefficients;
	mRightChain.get<0>().coefficients = *lowCutCoefficients;

	auto highCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(),
																				mHighCutFreq.getTargetValue(),
																				mHighCutQuality.getTargetValue());
	mLeftChain.get<1>().coefficients = *highCutCoefficients;
	mRightChain.get<1>().coefficients = *highCutCoefficients;
}

void SimpleEQAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
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

void SimpleEQAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	auto lowCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(),
																				mLowCutFreq.getTargetValue(),
																				mLowCutQuality.getTargetValue());
	mLeftChain.get<0>().coefficients = *lowCutCoefficients;
	mRightChain.get<0>().coefficients = *lowCutCoefficients;

	auto highCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(),
																				mHighCutFreq.getTargetValue(),
																				mHighCutQuality.getTargetValue());
	mLeftChain.get<1>().coefficients = *highCutCoefficients;
	mRightChain.get<1>().coefficients = *highCutCoefficients;

	juce::dsp::AudioBlock<float> block(buffer);

	auto leftBlock = block.getSingleChannelBlock(0);
	auto rightBlock = block.getSingleChannelBlock(1);

	juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
	juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

	mLeftChain.process(leftContext);
	mRightChain.process(rightContext);
}

bool SimpleEQAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *SimpleEQAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}

void SimpleEQAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void SimpleEQAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
	return new SimpleEQAudioProcessor();
}
#endif