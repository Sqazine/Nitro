
#include "PluginProcessor.h"
#include "Common/Utils.h"
ChorusAudioProcessor::ChorusAudioProcessor()
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
	mParamDelay(mApvts, "Delay", "ms", 10.0f, 50.0f, 30.0f, [](float value) { return value * 0.001f; }),
	mParamWidth(mApvts, "Width", "ms", 10.0f, 50.0f, 20.0f, [](float value) { return value * 0.001f; }),
	mParamDepth(mApvts, "Depth", "", 0.0f, 1.0f, 1.0f),
	mParamNumVoices(mApvts, "Number of Voices", "", { "2", "3", "4", "5" }, 0, [](float value) { return value + 2; }),
	mParamFrequency(mApvts, "LFO Frequency", "Hz", 0.05f, 2.0f, 0.2f),
	mParamWaveform(mApvts, "LFO Waveform", "", mWaveformItemsUI, Waveform::SINE),
	mParamInterpolation(mApvts, "Interpolation", "", mInterpolationItemsUI, Interpolation::LINEAR),
	mParamStereo(mApvts, "Stereo", "", true)
{
	mApvts.state = juce::ValueTree(juce::Identifier(getName().removeCharacters("- ")));
}

ChorusAudioProcessor::~ChorusAudioProcessor()
{
}

const juce::String ChorusAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool ChorusAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool ChorusAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool ChorusAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double ChorusAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int ChorusAudioProcessor::getNumPrograms()
{
	return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
	// so this should be at least 1, even if you're not really implementing programs.
}

int ChorusAudioProcessor::getCurrentProgram()
{
	return 0;
}

void ChorusAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String ChorusAudioProcessor::getProgramName(int index)
{
	return {};
}

void ChorusAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void ChorusAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	constexpr double smoothTime = 1e-3;
	mParamDelay.reset(sampleRate, smoothTime);
	mParamWidth.reset(sampleRate, smoothTime);
	mParamDepth.reset(sampleRate, smoothTime);
	mParamFrequency.reset(sampleRate, smoothTime);
	mParamNumVoices.reset(sampleRate, smoothTime);
	mParamWaveform.reset(sampleRate, smoothTime);
	mParamInterpolation.reset(sampleRate, smoothTime);
	mParamStereo.reset(sampleRate, smoothTime);

	float maxDelayTime = mParamDelay.maxValue + mParamWidth.maxValue;
	mDelayBufferSamples = (int32_t)(maxDelayTime * sampleRate) + 1;
	if (mDelayBufferSamples < 1)
		mDelayBufferSamples = 1;

	mDelayBufferChannels = getTotalNumOutputChannels();
	mDelayBuffer.setSize(mDelayBufferChannels, mDelayBufferSamples);
	mDelayBuffer.clear();

	mDelayWritePosition = 0;
	mLfoPhase = 0.0f;
	mInverseSampleRate = 1.0f / sampleRate;
}

void ChorusAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChorusAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
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

void ChorusAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
	ScopedNoDenormals noDenormals;

	const int32_t numInputChannels = getTotalNumInputChannels();
	const int32_t numOutputChannels = getTotalNumOutputChannels();
	const int32_t numSamples = buffer.getNumSamples();

	float currentDelay = mParamDelay.getNextValue();
	float currentWidth = mParamWidth.getNextValue();
	float currentDepth = mParamDepth.getNextValue();
	float currentFrequency = mParamFrequency.getNextValue();
	int numVoices = (int)mParamNumVoices.getTargetValue();
	bool stereo = (bool)mParamStereo.getTargetValue();

	int32_t localWritePosition;
	float phase;

	for (int32_t channel = 0; channel < numInputChannels; ++channel)
	{
		float *channelData = buffer.getWritePointer(channel);
		float *delayData = mDelayBuffer.getWritePointer(channel);

		localWritePosition = mDelayWritePosition;

		phase = mLfoPhase;

		for (int32_t sample = 0; sample < numSamples; ++sample)
		{
			const float in = channelData[sample];
			float out = 0.0f;
			float phaseOffset = 0.0f;
			float weight = 0.0f;

			for (int32_t voice = 0; voice < numVoices - 1; ++voice)
			{
				if (stereo && numVoices > 2)
				{
					weight = (float)voice / (float)(numVoices - 2);
					if (channel != 0)
						weight = 1.0f - weight;
				}
				else
				{
					weight = 1.0f;
				}

				float localDelayTime = (currentDelay + currentWidth * Lfo(phase + phaseOffset, (Waveform)mParamWaveform.getTargetValue())) * getSampleRate();

				float readPosition = fmodf(localWritePosition - localDelayTime + mDelayBufferSamples, mDelayBufferSamples);

				int32_t localReadPosition = floorf(readPosition);

				switch ((int32_t)mParamInterpolation.getTargetValue())
				{
				case Interpolation::NEAREST_NEIGHBOUR:
				{
					float closestSample = delayData[localReadPosition % mDelayBufferSamples];
					out = closestSample;
					break;
				}
				case Interpolation::LINEAR:
				{
					float fraction = readPosition - (float)localReadPosition;
					float delayed0 = delayData[(localReadPosition + 0)];
					float delayed1 = delayData[(localReadPosition + 1) % mDelayBufferSamples];
					out = delayed0 = fraction * (delayed1 - delayed0);
					break;
				}
				case Interpolation::CUBIC:
				{
					float fraction = readPosition - (float)localReadPosition;
					float fractionSqrt = fraction * fraction;
					float fractionCube = fractionSqrt * fraction;

					float sample0 = delayData[(localReadPosition - 1 + mDelayBufferSamples) % mDelayBufferSamples];
					float sample1 = delayData[(localReadPosition + 0)];
					float sample2 = delayData[(localReadPosition + 1) % mDelayBufferSamples];
					float sample3 = delayData[(localReadPosition + 2) % mDelayBufferSamples];

					float a0 = -0.5f * sample0 + 1.5f * sample1 - 1.5f * sample2 + 0.5f * sample3;
					float a1 = sample0 - 2.5f * sample1 + 2.0f * sample2 - 0.5f * sample3;
					float a2 = -0.5f * sample0 + 0.5f * sample2;
					float a3 = sample1;
					out = a0 * fractionCube + a1 * fractionSqrt + a2 * fraction + a3;
					break;
				}
				}

				if (stereo && numVoices == 2)
					channelData[sample] = (channel == 0) ? in : out * currentDepth;
				else
					channelData[sample] += out * currentDepth * weight;

				if (numVoices == 3)
					phaseOffset += 0.25f;
				else if (numVoices > 3)
					phaseOffset += 1.0f / (float)(numVoices - 1);
			}

			delayData[localWritePosition] = in;

			if (++localWritePosition >= mDelayBufferSamples)
				localWritePosition -= mDelayBufferSamples;
			phase += currentFrequency * mInverseSampleRate;
			if (phase >= 1.0f)
				phase -= 1.0f;
		}
	}

	mDelayWritePosition = localWritePosition;
	mLfoPhase = phase;

	for (int32_t channel = numInputChannels; channel < numOutputChannels; ++channel)
		buffer.clear(channel, 0, numSamples);
}

bool ChorusAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *ChorusAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}

void ChorusAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void ChorusAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
	return new ChorusAudioProcessor();
}
#endif
