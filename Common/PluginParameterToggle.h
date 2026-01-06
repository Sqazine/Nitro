#pragma once
#include "PluginParameter.h"
class PluginParameterToggle : public PluginParameter
{
public:
    PluginParameterToggle(juce::AudioProcessorValueTreeState &apvts,
                          const juce::String &paramName,
                          const juce::String &label="",
                          const bool defaultState = false,
                          const std::function<float(float)> callback = nullptr)
        : PluginParameter(apvts, callback),
          paramName(paramName),
          defaultState(defaultState)
    {
        auto paramID = paramName.removeCharacters(" ").toLowerCase();
        const juce::StringArray toggleStates = {"False", "True"};

        apvts.createAndAddParameter(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(paramID),
            paramName,
            defaultState,
            label,
            [toggleStates](bool value, int)
            { return toggleStates[value]; },
            [toggleStates](const juce::String &text)
            { return toggleStates.indexOf(text); }));

        apvts.addParameterListener(paramID, this);
        UpdateValue(defaultState);
    }

    const juce::String &paramName;
    const bool defaultState;
};