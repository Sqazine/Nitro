#pragma once

#include <JuceHeader.h>
#include <memory>
#include "PluginGraph.h"
#include "GraphEditorPanel.h"

//==============================================================================
/**

    When the Host is compiled as a plugin, the very same PluginGraph that the
    standalone application edits is rendered inside processBlock(), so all the
    internal effects (and any external plugin that has been added to the graph)
    stay available from inside another host.

*/
class HostAudioProcessor  : public juce::AudioProcessor
{
public:
    HostAudioProcessor();
    ~HostAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
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
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** The graph that the editor panels work on. */
    PluginGraph& getGraph() const noexcept        { return *graph; }

private:
    //==============================================================================
    void initialiseGraph();

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    std::unique_ptr<PluginGraph> graph;
    juce::CriticalSection graphLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostAudioProcessor)
};

//==============================================================================
/** The plugin version of the Host simply embeds a GraphEditorPanel, i.e. the
    same graph view that the standalone application shows.
*/
class HostAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HostAudioProcessorEditor (HostAudioProcessor&);
    ~HostAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    HostAudioProcessor& audioProcessor;
    std::unique_ptr<GraphEditorPanel> graphPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostAudioProcessorEditor)
};
