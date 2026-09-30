#pragma once

#include <JuceHeader.h>
#include "Common/Common.h"
#include "OD3/OD3Model.h"

class OD3AudioProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
    ,
                          public juce::AudioProcessorARAExtension
#endif
{
public:
    OD3AudioProcessor();
    ~OD3AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout &layouts) const override;
#endif

    void processBlock (juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String &newName) override;

    void getStateInformation (juce::MemoryBlock &destData) override;
    void setStateInformation (const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState mApvts;
    PluginParameterSlider mDrive;
    PluginParameterSlider mTone;
    PluginParameterSlider mMid;
    PluginParameterSlider mLevel;

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampling;
    OD3Model mModel[2];

    float mSampleRate = 44100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OD3AudioProcessor)
};