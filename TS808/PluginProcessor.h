#pragma once

#include <JuceHeader.h>
#include "Common/Common.h"
#include "TS808/TS808Model.h"

class TS808AudioProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
	,
								 public juce::AudioProcessorARAExtension
#endif
{
public:
	TS808AudioProcessor();
	~TS808AudioProcessor() override;

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

	juce::AudioProcessorValueTreeState mApvts;
	PluginParameterSlider mDrive;
	PluginParameterSlider mTone;
	PluginParameterSlider mLevel;

private:
	std::unique_ptr<juce::dsp::Oversampling<float>> mOversampling;
	TS808Model mModel[2];

	float mSampleRate = 44100.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TS808AudioProcessor)
};