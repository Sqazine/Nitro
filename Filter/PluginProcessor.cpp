

#include "PluginProcessor.h"


FilterAudioProcessor::FilterAudioProcessor()
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
	addParameter(filterChoice = new juce::AudioParameterChoice("filter choice", "Filter Type", { "LowPass","HighPass" }, 0));
	addParameter(frequency = new juce::AudioParameterFloat("frequency", "Frequency", 20.0f, 20000.0f, 440.0f));
}

FilterAudioProcessor::~FilterAudioProcessor()
{
}


const juce::String FilterAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool FilterAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool FilterAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool FilterAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double FilterAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int FilterAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
				// so this should be at least 1, even if you're not really implementing programs.
}

int FilterAudioProcessor::getCurrentProgram()
{
	return 0;
}

void FilterAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String FilterAudioProcessor::getProgramName(int index)
{
	return {};
}

void FilterAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

void FilterAudioProcessor::reset()
{
	filter.reset();
}


void FilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	this->sampleRate = sampleRate;
	if (filterChoice->getIndex() == 0)
		*filter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, *frequency);
	else if (filterChoice->getIndex() == 1)
		*filter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, *frequency);

	juce::dsp::ProcessSpec spec{ sampleRate,static_cast<juce::uint32>(samplesPerBlock),2 };
	filter.prepare(spec);
}

void FilterAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FilterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void FilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	if (filterChoice->getIndex() == 0)
		*filter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, *frequency);
	else if (filterChoice->getIndex() == 1)
		*filter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, *frequency);

	juce::dsp::AudioBlock<float> block(buffer);
	juce::dsp::ProcessContextReplacing<float> context(block);
	filter.process(context);
}


bool FilterAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FilterAudioProcessor::createEditor()
{
	return new juce::GenericAudioProcessorEditor(*this);
}


void FilterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	// You should use this method to store your parameters in the memory block.
	// You could do that either as raw data, or use the XML or ValueTree classes
	// as intermediaries to make it easy to save and load complex data.
}

void FilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	// You should use this method to restore your parameters from this memory block,
	// whose contents will have been created by the getStateInformation() call.
}


// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new FilterAudioProcessor();
}
#endif