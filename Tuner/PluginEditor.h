#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TunerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    TunerAudioProcessorEditor (TunerAudioProcessor&);
    ~TunerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String getNoteName(int midiNote) const;
    
    TunerAudioProcessor& audioProcessor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerAudioProcessorEditor)
};
