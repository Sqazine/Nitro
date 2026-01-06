#pragma once
#include <JuceHeader.h>
class PluginParameter
    : public juce::LinearSmoothedValue<float>,
      public juce::AudioProcessorValueTreeState::Listener
{
public:
    PluginParameter(juce::AudioProcessorValueTreeState &apvts, const std::function<float(float)> callback = nullptr)
        : apvts(apvts), callback(callback)
    {
    }

    void UpdateValue(float value)
    {
        if (callback != nullptr)
            setCurrentAndTargetValue(callback(value));
        else
            setCurrentAndTargetValue(value);
    }

    void parameterChanged(const juce::String &parameterID, float newValue) override
    {
        UpdateValue(newValue);
    }

    juce::AudioProcessorValueTreeState &apvts;

    std::function<float(float)> callback;
};
