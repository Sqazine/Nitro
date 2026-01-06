#pragma once

#include <JuceHeader.h>
namespace IDs {

	const juce::String inputVolume("inputVolume");
	const juce::String HPFreq("HPFreq");
	const juce::String LPFreq("LPFreq");
	const juce::String outputVolume("outputVolume");
	const juce::String wetDry("wetDry");

}

class DistortionAudioProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
	, public juce::AudioProcessorARAExtension
#endif
{
public:
	
	DistortionAudioProcessor();
	~DistortionAudioProcessor() override;

	
	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
	bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

	void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	
	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override;

	
	const juce::String getName() const override;

	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool isMidiEffect() const override;
	double getTailLengthSeconds() const override;

	
	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram(int index) override;
	const juce::String getProgramName(int index) override;
	void changeProgramName(int index, const juce::String& newName) override;

	
	void getStateInformation(juce::MemoryBlock& destData) override;
	void setStateInformation(const void* data, int sizeInBytes) override;

	juce::AudioProcessorValueTreeState parameters;
	juce::dsp::WaveShaper<float> waveShapers{std::tanh} ;
	juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowPassFilter{ juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(44100.f, 20000.f) }, highPassFilter{ juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(44100.f, 20.f) };
	std::unique_ptr<juce::dsp::Oversampling<float>> oversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 3, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
	juce::dsp::Gain<float> inputVolume, outputVolume;

	float sampleRate = 44100.0f;
	uint32_t maxBlockSize = 512;
	uint32_t numChannels = 2;
private:

	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionAudioProcessor)
};
