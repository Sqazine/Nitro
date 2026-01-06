

#include "PluginProcessor.h"


ReverbAudioProcessor::ReverbAudioProcessor()
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
	addParameter(roomSize = new juce::AudioParameterFloat("Room Size", "Room Size", 0.0f, 1.0f, 0.5f));
	addParameter(damping = new juce::AudioParameterFloat("Damping", "Damping", 0.0f, 1.0f, 0.5f));
	addParameter(width = new juce::AudioParameterFloat("Width", "Width", 0.0f, 1.0f, 0.5f));
	addParameter(dry_Wet = new juce::AudioParameterFloat("Dry/Wet", "Dry/Wet", 0.0f, 1.0f, 0.5f));
	addParameter(freeze = new juce::AudioParameterFloat("Freeze", "Freeze",0.0f,1.0f,0.5f));
}

ReverbAudioProcessor::~ReverbAudioProcessor()
{
}


const juce::String ReverbAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool ReverbAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool ReverbAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool ReverbAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double ReverbAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int ReverbAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
				// so this should be at least 1, even if you're not really implementing programs.
}

int ReverbAudioProcessor::getCurrentProgram()
{
	return 0;
}

void ReverbAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String ReverbAudioProcessor::getProgramName(int index)
{
	return {};
}

void ReverbAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}


void ReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	juce::dsp::ProcessSpec spec;

	spec.sampleRate = sampleRate;
	spec.maximumBlockSize = samplesPerBlock;
	spec.numChannels = 1;

	leftReverb.prepare(spec);
	rightReverb.prepare(spec);
}

void ReverbAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void ReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	// In case we have more outputs than inputs, this code clears any output
	// channels that didn't contain input data, (because these aren't
	// guaranteed to be empty - they may contain garbage).
	// This is here to avoid people getting screaming feedback
	// when they first compile a plugin, but obviously you don't need to keep
	// this code if your algorithm always overwrites all the output channels.
	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
		buffer.clear(i, 0, buffer.getNumSamples());

	params.roomSize = *roomSize;
	params.damping = *damping;
	params.width = *width;
	params.wetLevel = *dry_Wet;
	params.dryLevel = 1.0f-params.wetLevel;
	params.freezeMode = *freeze;

	leftReverb.setParameters(params);
	rightReverb.setParameters(params);

	juce::dsp::AudioBlock<float> block(buffer);

	if (block.getNumChannels() > 2)
		block = block.getSubsetChannelBlock(0, 2);

	juce::dsp::ProcessContextReplacing<float> leftContext(block);
	juce::dsp::ProcessContextReplacing<float> rightContext(block);
	leftReverb.process(leftContext);
	rightReverb.process(rightContext);
}


bool ReverbAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ReverbAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}


void ReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void ReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}


// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new ReverbAudioProcessor();
}
#endif