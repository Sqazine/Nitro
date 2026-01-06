
#include "PluginProcessor.h"


NoiseGateAudioProcessor::NoiseGateAudioProcessor()
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
{
    addParameter(threshold = new juce::AudioParameterFloat("threshold", "Threshold", 0.0f, 1.0f, 0.5f));
    addParameter(alpha = new juce::AudioParameterFloat("alpha","Alpha",0.0f,1.0f,0.8f));
}

NoiseGateAudioProcessor::~NoiseGateAudioProcessor()
{
}


const juce::String NoiseGateAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NoiseGateAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NoiseGateAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NoiseGateAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NoiseGateAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NoiseGateAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NoiseGateAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NoiseGateAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NoiseGateAudioProcessor::getProgramName (int index)
{
    return {};
}

void NoiseGateAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}


void NoiseGateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lowPassCoeff = 0.0f;
    sampleCountDown = 0;
}

void NoiseGateAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NoiseGateAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
	return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()&& !layouts.getMainInputChannelSet().isDisabled();
}
#endif

void NoiseGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	auto mainInputOutput = getBusBuffer(buffer, true, 0);                                 

	auto alphaCopy = alpha->get();                                                         
	auto thresholdCopy = threshold->get();

	for (auto j = 0; j < buffer.getNumSamples(); ++j)                                     
	{
		auto mixedSamples = 0.0f;

		for (auto i = 0; i < mainInputOutput.getNumChannels(); ++i)
			mixedSamples += mainInputOutput.getReadPointer(i)[j];

		mixedSamples /= static_cast<float> (mainInputOutput.getNumChannels());
		lowPassCoeff = (alphaCopy * lowPassCoeff) + ((1.0f - alphaCopy) * mixedSamples); 

		if (lowPassCoeff >= thresholdCopy)                                               
			sampleCountDown = (int)getSampleRate();

		for (auto i = 0; i < mainInputOutput.getNumChannels(); ++i)
			*mainInputOutput.getWritePointer(i, j) = sampleCountDown > 0 ? *mainInputOutput.getReadPointer(i, j)
			: 0.0f;

		if (sampleCountDown > 0)                                                         
			--sampleCountDown;
	}
}


bool NoiseGateAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NoiseGateAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}


void NoiseGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void NoiseGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseGateAudioProcessor();
}
#endif