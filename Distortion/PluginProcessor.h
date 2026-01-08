#pragma once

#include <JuceHeader.h>
#include "Common/Common.h"

class DistortionAudioProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
	,
								 public juce::AudioProcessorARAExtension
#endif
{
public:
	DistortionAudioProcessor();
	~DistortionAudioProcessor() override;

	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
	bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

	void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

	juce::AudioProcessorEditor *createEditor() override;
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
	void changeProgramName(int index, const juce::String &newName) override;

	void getStateInformation(juce::MemoryBlock &destData) override;
	void setStateInformation(const void *data, int sizeInBytes) override;

private:
	juce::AudioProcessorValueTreeState mApvts;
	PluginParameterSlider mDistortion;
	PluginParameterSlider mLevel;
	PluginParameterSlider mHighPassFrequency;
	PluginParameterSlider mLowPassFrequency;
	PluginParameterSlider mWetDry;

	juce::dsp::WaveShaper<float> mWaveShapers{std::tanh};

	juce::dsp::ProcessorDuplicator<DspIIRFilterFloat, juce::dsp::IIR::Coefficients<float>> mLowPassFilter{juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(44100.f, 20000.f)}, mHighPassFilter{juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(44100.f, 20.f)};
	std::unique_ptr<juce::dsp::Oversampling<float>> mOversampling = std::make_unique<juce::dsp::Oversampling<float>>(2, 3, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
	juce::dsp::Gain<float> mInputVolume, mOutputVolume;

	float mSampleRate = 44100.0f;
	uint32_t mMaxBlockSize = 512;
	uint32_t mNumChannels = 2;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionAudioProcessor)
};
