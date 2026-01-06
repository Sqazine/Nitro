#pragma once
#include "PluginParameter.h"
class PluginParameterSlider : public PluginParameter
{
public:
    PluginParameterSlider(juce::AudioProcessorValueTreeState &apvts,
                          const juce::String &paramName,
                          const juce::String &labelText,
                          float minValue,
                          float maxValue,
                          float defaultValue,
                          std::function<float(float)> callback = nullptr,
                          bool logarithmic = false)
        : PluginParameter(apvts, callback), paramName(paramName), labelText(labelText), minValue(minValue), maxValue(maxValue), defaultValue(defaultValue)

    {
        auto paramID = paramName.removeCharacters(" ").toLowerCase();

        juce::NormalisableRange<float> range(minValue, maxValue);
        if (logarithmic)
            range.setSkewForCentre(sqrt(minValue * maxValue));

        apvts.createAndAddParameter(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(paramID),
            paramName,
            range,
            defaultValue,
            labelText,
            juce::AudioProcessorParameter::genericParameter,
            [](float value, int)
            { return juce::String(value, 2); },
            [](const juce::String &text)
            { return text.getFloatValue(); }));

        apvts.addParameterListener(paramID, this);
        UpdateValue(defaultValue);
    }

    const juce::String &paramName;
    const juce::String &labelText;
    const float minValue;
    const float maxValue;
    const float defaultValue;
};