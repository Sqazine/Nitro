#pragma once
#include "PluginParameter.h"
class PluginParameterComboBox : public PluginParameter
{
public:
    PluginParameterComboBox(juce::AudioProcessorValueTreeState &apvts,
                          const juce::String &paramName,
                          const juce::String &label,
                          const juce::StringArray &items,
                          const int defaultChoice = 0,
                          const std::function<float(const float)> callback = nullptr)
        : PluginParameter(apvts, callback),
          paramName(paramName),
          items(items),
          defaultChoice(defaultChoice)
    {
        auto paramID = paramName.removeCharacters(" ").toLowerCase();

        apvts.createAndAddParameter(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(paramID),
            paramName,
            items,
            defaultChoice,
            label,
            [items](int value, int)
            { return items[value]; },
            [items](const juce::String &text)
            { return items.indexOf(text); }));

        apvts.addParameterListener(paramID, this);
        UpdateValue(defaultChoice);
    }

    const juce::String &paramName;
    const juce::StringArray items;
    const int defaultChoice;
};