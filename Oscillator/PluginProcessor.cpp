#include "PluginProcessor.h"

OscillatorAudioProcessor::OscillatorAudioProcessor()
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
	addParameter(frequency = new juce::AudioParameterFloat("frequency", "Frequency", 0.0f, 20000.0f, 440.0f));

	oscillator.initialise([](float x) {
		return std::sin(x);
		});
}

OscillatorAudioProcessor::~OscillatorAudioProcessor()
{
}

const juce::String OscillatorAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool OscillatorAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool OscillatorAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool OscillatorAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double OscillatorAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int OscillatorAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
				// so this should be at least 1, even if you're not really implementing programs.
}

int OscillatorAudioProcessor::getCurrentProgram()
{
	return 0;
}

void OscillatorAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String OscillatorAudioProcessor::getProgramName(int index)
{
	return {};
}

void OscillatorAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}


void OscillatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	previousFrequency = *frequency;
	juce::dsp::ProcessSpec spec{ sampleRate,static_cast<juce::uint32>(samplesPerBlock) };
	oscillator.setFrequency(previousFrequency);
	oscillator.prepare(spec);
}

void OscillatorAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OscillatorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void OscillatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	if (*frequency != previousFrequency)
	{
		previousFrequency = *frequency;
		oscillator.setFrequency(previousFrequency);
	}

	juce::dsp::AudioBlock<float> block(buffer);
	juce::dsp::ProcessContextReplacing<float> context(block);
	oscillator.process(context);
}


bool OscillatorAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* OscillatorAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}


void OscillatorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void OscillatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}

void OscillatorAudioProcessor::reset()
{
	oscillator.reset();
}


// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new OscillatorAudioProcessor();
}
#endif