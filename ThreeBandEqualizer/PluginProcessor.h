#pragma once

#include <JuceHeader.h>

enum ChainIndex
{
	LowCut,
	Peak,
	HighCut
};

enum Slope
{
	SLOPE_12,
	SLOPE_24,
	SLOPE_36,
	SLOPE_48 
};

struct ChainSettings
{
	float peakFreq = 0.0f;
	float peakChainInDecibels = 0.0f;
	float peakQuality = 1.0f;
	float lowCutFreq = 0.0f;
	int32_t lowCutSlope = SLOPE_12;
	float highCutFreq = 0.0f;
	int32_t highCutSlope = SLOPE_12;
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

class ThreeBandEqualizerAudioProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
	, public juce::AudioProcessorARAExtension
#endif
{
public:
	
	ThreeBandEqualizerAudioProcessor();
	~ThreeBandEqualizerAudioProcessor() override;

	
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

private:

	using Filter = juce::dsp::IIR::Filter<float>;
	using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
	using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

	juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
	void updatePeakFilter(const ChainSettings& chainSettings);

	template<typename ChainType, typename CoefficientType>
	void updateCutFilter(ChainType& leftLowCut,const CoefficientType& cutCoefficients,const Slope& lowCutSlope);

	MonoChain leftChain, rightChain;

	juce::AudioProcessorValueTreeState apvts;

	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreeBandEqualizerAudioProcessor)
};

template<typename ChainType, typename CoefficientType>
inline void ThreeBandEqualizerAudioProcessor::updateCutFilter(ChainType& leftLowCut, const CoefficientType& cutCoefficients, const Slope& lowCutSlope)
{
	leftLowCut.template setBypassed<0>(true);
	leftLowCut.template setBypassed<1>(true);
	leftLowCut.template setBypassed<2>(true);
	leftLowCut.template setBypassed<3>(true);

	switch (lowCutSlope)
	{
	case SLOPE_12:
	{
		*leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
		leftLowCut.template setBypassed<0>(false);
		break;
	}
	case SLOPE_24:
	{
		*leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
		leftLowCut.template setBypassed<0>(false);
		*leftLowCut.template get<1>().coefficients = *cutCoefficients[1];
		leftLowCut.template setBypassed<1>(false);
		break;
	}
	case SLOPE_36:
	{
		*leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
		leftLowCut.template setBypassed<0>(false);
		*leftLowCut.template get<1>().coefficients = *cutCoefficients[1];
		leftLowCut.template setBypassed<1>(false);
		*leftLowCut.template get<2>().coefficients = *cutCoefficients[2];
		leftLowCut.template setBypassed<2>(false);
		break;
	}
	case SLOPE_48:
	{
		*leftLowCut.template get<0>().coefficients = *cutCoefficients[0];
		leftLowCut.template setBypassed<0>(false);
		*leftLowCut.template get<1>().coefficients = *cutCoefficients[1];
		leftLowCut.template setBypassed<1>(false);
		*leftLowCut.template get<2>().coefficients = *cutCoefficients[2];
		leftLowCut.template setBypassed<2>(false);
		*leftLowCut.template get<3>().coefficients = *cutCoefficients[3];
		leftLowCut.template setBypassed<3>(false);
		break;
	}
	}
}
