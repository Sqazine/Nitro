

#include "PluginProcessor.h"


PingPongDelayAudioProcessor::PingPongDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
	: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
	),
#endif
	mDelayParameters(*this,nullptr),
	mDelayParamBalance(mDelayParameters, "Balance", "", -1.0f, 1.0f, 0.0f),
	mDelayParamDelayTime(mDelayParameters, "Time", "s", 0.0f, 5.0f, 0.1f),
	mDelayParamFeedback(mDelayParameters, "Feedback", "", 0.0f, 0.9f, 0.7f),
	mDelayParamMix(mDelayParameters, "Mix", "", 0.0f, 1.0f, 1.0f)
{
	mDelayParameters.state = juce::ValueTree(juce::Identifier(getName()));
}

PingPongDelayAudioProcessor::~PingPongDelayAudioProcessor()
{
}


const juce::String PingPongDelayAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool PingPongDelayAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool PingPongDelayAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool PingPongDelayAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double PingPongDelayAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int PingPongDelayAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
	// so this should be at least 1, even if you're not really implementing programs.
}

int PingPongDelayAudioProcessor::getCurrentProgram()
{
	return 0;
}

void PingPongDelayAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String PingPongDelayAudioProcessor::getProgramName(int index)
{
	return {};
}

void PingPongDelayAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}


void PingPongDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	const double smoothTime = 1e-3;
	mDelayParamDelayTime.reset(sampleRate, smoothTime);
	mDelayParamFeedback.reset(sampleRate, smoothTime);
	mDelayParamMix.reset(sampleRate, smoothTime);

	float maxDelayTime = mDelayParamDelayTime.maxValue;
	mDelayBufferSamples = maxDelayTime * sampleRate + 1;
	if (mDelayBufferSamples < 1)
		mDelayBufferSamples = 1;

	mDelayBufferChannels = getTotalNumInputChannels();
	mDelayBuffer.setSize(mDelayBufferChannels, mDelayBufferSamples);
	mDelayBuffer.clear();

	mDelayWritePosition = 0;
}

void PingPongDelayAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PingPongDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void PingPongDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();
	auto numSamples = buffer.getNumSamples();

	float currentBalance = mDelayParamBalance.getNextValue() * 0.5f + 0.5f;
	float currentDelayTime = mDelayParamDelayTime.getTargetValue() * (float)getSampleRate();
	float currentFeedback = mDelayParamFeedback.getNextValue();
	float currentMix = mDelayParamMix.getNextValue();

	int localWritePosition = mDelayWritePosition;

	float* channelDataL = buffer.getWritePointer(0);
	float* channelDataR = buffer.getWritePointer(1);
	float* delayDataL = mDelayBuffer.getWritePointer(0);
	float* delayDataR = mDelayBuffer.getWritePointer(1);

	for (int sample = 0; sample < numSamples; ++sample)
	{
		const float inL = (1.0f - currentBalance) * channelDataL[sample];
		const float inR = currentBalance * channelDataR[sample];
		float outL = 0.0f;
		float outR = 0.0f;

		float readPosition = fmodf((float)localWritePosition - currentDelayTime + (float)mDelayBufferSamples, mDelayBufferSamples);

		int localReadPosition = floorf(readPosition);

		if (localReadPosition != localWritePosition)
		{
			float fraction = readPosition - (float)localReadPosition;
			float delayed1L = delayDataL[(localReadPosition + 0)];
			float delayed1R = delayDataR[(localReadPosition + 0)];
			float delayed2L = delayDataL[(localReadPosition + 1) % mDelayBufferSamples];
			float delayed2R = delayDataR[(localReadPosition + 1) % mDelayBufferSamples];

			outL = delayed1L + fraction * (delayed2L - delayed1L);
			outR = delayed1R + fraction * (delayed2R - delayed1R);

			channelDataL[sample] = inL + (outL - inL) * currentMix;
			channelDataR[sample] = inR + (outR - inR) * currentMix;
			delayDataL[localWritePosition] = inL + outR * currentFeedback;
			delayDataR[localWritePosition] = inR + outL * currentFeedback;
		}

		if (++localWritePosition >= mDelayBufferSamples)
			localWritePosition -= mDelayBufferSamples;
	}

	mDelayWritePosition = localWritePosition;

	for (int channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
		buffer.clear(channel, 0, numSamples);
}


bool PingPongDelayAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PingPongDelayAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}


void PingPongDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	auto state = mDelayParameters.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void PingPongDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(mDelayParameters.state.getType()))
			mDelayParameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}


// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new PingPongDelayAudioProcessor();
}
#endif