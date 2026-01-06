

#include "PluginProcessor.h"


DelayAudioProcessor::DelayAudioProcessor()
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
	mDelayParamDelayTime(mDelayParameters, "Time", "s", 0.0f, 5.0f, 0.1f),
	mDelayParamFeedback(mDelayParameters, "Feedback", "", 0.0f, 0.9f, 0.7f),
	mDelayParamMix(mDelayParameters, "Mix", "", 0.0f, 1.0f, 1.0f)
{
	mDelayParameters.state = juce::ValueTree(juce::Identifier(getName()));
}

DelayAudioProcessor::~DelayAudioProcessor()
{
}


const juce::String DelayAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool DelayAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool DelayAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool DelayAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double DelayAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int DelayAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
	// so this should be at least 1, even if you're not really implementing programs.
}

int DelayAudioProcessor::getCurrentProgram()
{
	return 0;
}

void DelayAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String DelayAudioProcessor::getProgramName(int index)
{
	return {};
}

void DelayAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}


void DelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
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

	mDelayWritePositions = 0;
}

void DelayAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void DelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;

	const int numInputChannels = getTotalNumInputChannels();
	const int numOutputChannels = getTotalNumOutputChannels();
	const int numSamples = buffer.getNumSamples();

	float currentDelayTime = mDelayParamDelayTime.getTargetValue() * (float)getSampleRate();
	float currentFeedback = mDelayParamFeedback.getNextValue();
	float currentMix = mDelayParamMix.getNextValue();

	int localWritePosition;

	for (int channel = 0; channel < numInputChannels; ++channel)
	{
		float* channelData = buffer.getWritePointer(channel);
		float* delayData = mDelayBuffer.getWritePointer(channel);
		localWritePosition = mDelayWritePositions;

		for (int sample = 0; sample < numSamples; ++sample)
		{
			const float in = channelData[sample];
			float out = 0.0f;

			float readPosition = fmodf((float)localWritePosition - currentDelayTime + (float)mDelayBufferSamples, mDelayBufferSamples);

			int localReadPosition = floorf(readPosition);

			if (localReadPosition != localWritePosition)
			{
				float fraction = readPosition - (float)localReadPosition;
				float delayed1 = delayData[(localReadPosition + 0)];
				float delayed2 = delayData[(localReadPosition + 1) % mDelayBufferSamples];
				out = delayed1 + fraction * (delayed2 - delayed1);

				channelData[sample] = in + currentMix * (out - in);
				delayData[localWritePosition] = in + out * currentFeedback;
			}

			if (++localWritePosition >= mDelayBufferSamples)
				localWritePosition -= mDelayBufferSamples;
		}

	}

	mDelayWritePositions = localWritePosition;
	
	for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
		buffer.clear(channel, 0, numSamples);
}


bool DelayAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DelayAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}


void DelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	auto state = mDelayParameters.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void DelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
	return new DelayAudioProcessor();
}
#endif