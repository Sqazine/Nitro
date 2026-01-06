

#include "PluginProcessor.h"


SimpleEQAudioProcessor::SimpleEQAudioProcessor()
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
{
	addParameter(lowCutFreq = new juce::AudioParameterFloat("lowCutFreq",
		"lowCutFreq",
		juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
		20.0f,
		juce::String(),
		juce::AudioProcessorParameter::genericParameter,
		[](float value, int) {
			return (value < 1000.0f) ? juce::String(value, 0) + "Hz" : juce::String(value / 1000.0f, 1) + "kHz";
		}));

	addParameter(lowCutQuality = new juce::AudioParameterFloat("lowCutQuality",
		"lowCutQuality",
		juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.25f),
		0.71f));

	addParameter(highCutFreq = new juce::AudioParameterFloat("highCutFreq",
		"highCutFreq",
		juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
		20000.0f,
		juce::String(),
		juce::AudioProcessorParameter::genericParameter,
		[](float value, int) {
			return (value < 1000.0f) ? juce::String(value, 0) + "Hz" : juce::String(value / 1000.0f, 1) + "kHz";
		}));

	addParameter(highCutQuality = new juce::AudioParameterFloat("highCutQuality",
		"highCutQuality",
		juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.25f),
		0.71f));
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
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
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

void SimpleEQAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}


void SimpleEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	juce::dsp::ProcessSpec spec;
	spec.maximumBlockSize = samplesPerBlock;
	spec.numChannels = 1;
	spec.sampleRate = sampleRate;

	leftChain.prepare(spec);
	rightChain.prepare(spec);

	auto lowCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(),
		*lowCutFreq,
		*lowCutQuality); 
	leftChain.get<0>().coefficients = *lowCutCoefficients;
	rightChain.get<0>().coefficients = *lowCutCoefficients;

	auto highCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(),
		*highCutFreq,
		*highCutQuality);
	leftChain.get<1>().coefficients = *highCutCoefficients;
	rightChain.get<1>().coefficients = *highCutCoefficients;
}

void SimpleEQAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void SimpleEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	auto lowCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(),
		*lowCutFreq,
		*lowCutQuality);
	leftChain.get<0>().coefficients = *lowCutCoefficients;
	rightChain.get<0>().coefficients = *lowCutCoefficients;

	auto highCutCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(),
		*highCutFreq,
		*highCutQuality);
	leftChain.get<1>().coefficients = *highCutCoefficients;
	rightChain.get<1>().coefficients = *highCutCoefficients;

	juce::dsp::AudioBlock<float> block(buffer);

	auto leftBlock = block.getSingleChannelBlock(0);
	auto rightBlock = block.getSingleChannelBlock(1);

	juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
	juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

	leftChain.process(leftContext);
	rightChain.process(rightContext);
}


bool SimpleEQAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEQAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}


void SimpleEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void SimpleEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}


// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new SimpleEQAudioProcessor();
}
#endif