

#include "PluginProcessor.h"
#include "Common/Utils.h"

FlangerAudioProcessor::FlangerAudioProcessor()
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
	apvts(*this, nullptr),
	mDelay(apvts, "Delay", "ms", 1.0f, 20.0f, 2.5f, [](float value) { return value * 0.001f; }),
	mWidth(apvts, "Width", "ms", 1.0f, 20.0f, 10.0f, [](float value) { return value * 0.001f; }),
	mDepth(apvts, "Depth", "", 0.0f, 1.0f, 1.0f),
	mFeedback(apvts, "Feedback", "", 0.0f, 1.0f, 1.0f),
	mInverted(apvts, "Inverted mode", "", false, [](float value) {return value * (-2.0f) + 1.0f; }),
	mFrequency(apvts, "LFO Frequency", "Hz", 0.05f, 2.0f, 0.2f),
	mWaveForm(apvts, "LFO waveform", "", mWaveformItemsUI, Waveform::SINE),
	mInterpolation(apvts, "Interpolation", "", mInterpolationItemsUI, Interpolation::LINEAR),
	mStereo(apvts, "Stereo", "")

{
	apvts.state = juce::ValueTree(juce::Identifier(getName()));
}

FlangerAudioProcessor::~FlangerAudioProcessor()
{
}


const juce::String FlangerAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool FlangerAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool FlangerAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool FlangerAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double FlangerAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int FlangerAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
	// so this should be at least 1, even if you're not really implementing programs.
}

int FlangerAudioProcessor::getCurrentProgram()
{
	return 0;
}

void FlangerAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String FlangerAudioProcessor::getProgramName(int index)
{
	return {};
}

void FlangerAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}


void FlangerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	constexpr double smoothTime = 1e-3;
	mDelay.reset(sampleRate, smoothTime);
	mWidth.reset(sampleRate, smoothTime);
	mDepth.reset(sampleRate, smoothTime);
	mFeedback.reset(sampleRate, smoothTime);
	mInverted.reset(sampleRate, smoothTime);
	mFrequency.reset(sampleRate, smoothTime);
	mWaveForm.reset(sampleRate, smoothTime);
	mInterpolation.reset(sampleRate, smoothTime);
	mStereo.reset(sampleRate, smoothTime);

	float maxDelayTime = mDelay.maxValue + mWidth.maxValue;
	mDelayBufferSamples = maxDelayTime * sampleRate;
	if (mDelayBufferSamples < 1)
		mDelayBufferSamples = 1;

	mDelayBufferChannels = getTotalNumInputChannels();
	mDelayBuffer.setSize(mDelayBufferChannels, mDelayBufferSamples);
	mDelayBuffer.clear();

	mDelayWritePosition = 0;
	mLfoPhase = 0.0f;
	mInverseSampleRate = 1.0f / sampleRate;
}

void FlangerAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FlangerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void FlangerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();
	auto numSamples = buffer.getNumSamples();

	float curDelay = mDelay.getNextValue();
	float curWidth = mWidth.getNextValue();
	float curDepth = mDepth.getNextValue();
	float curFeedback = mFeedback.getNextValue();
	float curInverted = mInverted.getNextValue();
	float curFrequency = mFrequency.getNextValue();


	int localWritePosition;
	float phase;
	float phaseMain;

	for (int channel = 0; channel < totalNumInputChannels; ++channel)
	{
		float* channelData = buffer.getWritePointer(channel);
		float* delayData = mDelayBuffer.getWritePointer(channel);
		localWritePosition = mDelayWritePosition;

		if (mStereo.getTargetValue() && channel != 0)
			phase = fmodf(phase + 0.25f, 1.0f);

		for (int sample = 0; sample < numSamples; ++sample)
		{
			const float inData = channelData[sample];
			float outData = 0.0f;

			float localDelayTime = (curDelay + curWidth * Lfo(phase, (Waveform)mWaveForm.getTargetValue())) * getSampleRate();

			float readPosition = fmodf(localWritePosition - localDelayTime + mDelayBufferSamples, mDelayBufferSamples);

			int localReadPosition = floorf(readPosition);

			switch ((int)mInterpolation.getTargetValue())
			{
			case NEAREST_NEIGHBOUR:
			{
				float closestSample = delayData[localReadPosition % mDelayBufferSamples];
				outData = closestSample;
				break;
			}
			case LINEAR:
			{
				float fraction = readPosition - localReadPosition;
				float delayed0 = delayData[(localReadPosition + 0)];
				float delayed1 = delayData[(localReadPosition + 1) % mDelayBufferSamples];
				outData = delayed0 + fraction * (delayed1 - delayed0);
				break;
			}
			case CUBIC:
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
				outData = a0 * fractionCube + a1 * fractionSqrt + a2 * fraction + a3;
				break;
			}
			}

			channelData[sample] = inData + outData * curDepth * curInverted;
			delayData[localWritePosition] = inData + outData * curFeedback;

			if (++localWritePosition >= mDelayBufferSamples)
				localWritePosition -= mDelayBufferSamples;

			phase += curFrequency * mInverseSampleRate;
			if (phase >= 1.0f)
				phase -= 1.0f;
		}

		if (channel == 0)
			phaseMain = phase;
	}

	mDelayWritePosition = localWritePosition;
	mLfoPhase = phaseMain;
}


bool FlangerAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FlangerAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}


void FlangerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void FlangerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new FlangerAudioProcessor();
}
#endif
