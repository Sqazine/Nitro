#include "PluginProcessor.h"
#include "GraphEditorPanel.h"
#include "PluginInstanceFormat.h"

//==============================================================================
HostAudioProcessor::HostAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    // The graph needs to know about the internal effects before it is created,
    // PluginGraph::newDocument() already instantiates the default I/O nodes.
    formatManager.addFormat (std::make_unique<PluginInstanceFormat>());
    addDefaultFormatsToManager (formatManager);

    graph = std::make_unique<PluginGraph> (formatManager, knownPluginList);
}

HostAudioProcessor::~HostAudioProcessor()
{
    const juce::ScopedLock sl (graphLock);
    graph = nullptr;
}

//==============================================================================
void HostAudioProcessor::initialiseGraph()
{
    // PluginGraph::newDocument() adds the audio/midi input and output nodes
    // asynchronously, so this has to be retried until they show up. It only
    // wires the defaults up, as soon as the user connects something by hand
    // (or a saved graph is restored) this becomes a no-op.
    if (! graph->graph.getConnections().empty())
        return;

    const auto audioInput  = graph->getNodeForName ("Audio Input");
    const auto audioOutput = graph->getNodeForName ("Audio Output");

    if (audioInput != nullptr && audioOutput != nullptr)
        for (int channel = 0; channel < 2; ++channel)
            graph->graph.addConnection ({ { audioInput->nodeID, channel },
                                          { audioOutput->nodeID, channel } });

    const auto midiInput  = graph->getNodeForName ("MIDI Input");
    const auto midiOutput = graph->getNodeForName ("MIDI Output");

    if (midiInput != nullptr && midiOutput != nullptr)
        graph->graph.addConnection ({ { midiInput->nodeID,  juce::AudioProcessorGraph::midiChannelIndex },
                                      { midiOutput->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
}

//==============================================================================
const juce::String HostAudioProcessor::getName() const
{
    // Not JucePlugin_Name: this file is linked against every other plugin of
    // the collection, which would otherwise redefine the macro.
    return "Host";
}

bool HostAudioProcessor::acceptsMidi() const
{
    return true;
}

bool HostAudioProcessor::producesMidi() const
{
    return true;
}

bool HostAudioProcessor::isMidiEffect() const
{
    return false;
}

double HostAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HostAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int HostAudioProcessor::getCurrentProgram()
{
    return 0;
}

void HostAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String HostAudioProcessor::getProgramName (int index)
{
    return {};
}

void HostAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void HostAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl (graphLock);

    // The I/O nodes follow the channel count of the graph itself, so the graph
    // has to be told about the layout the surrounding host asked for.
    graph->graph.setPlayConfigDetails (getTotalNumInputChannels(),
                                       getTotalNumOutputChannels(),
                                       sampleRate, samplesPerBlock);
    graph->graph.prepareToPlay (sampleRate, samplesPerBlock);

    initialiseGraph();
}

void HostAudioProcessor::releaseResources()
{
    const juce::ScopedLock sl (graphLock);
    graph->graph.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool HostAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void HostAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const juce::ScopedLock sl (graphLock);

    if (graph != nullptr)
        graph->graph.processBlock (buffer, midiMessages);
}

//==============================================================================
void HostAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl (graphLock);

    if (auto xml = graph->createXml())
        copyXmlToBinary (*xml, destData);
}

void HostAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const juce::ScopedLock sl (graphLock);

    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        graph->restoreFromXml (*xml);
}

//==============================================================================
bool HostAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* HostAudioProcessor::createEditor()
{
    return new HostAudioProcessorEditor (*this);
}

//==============================================================================
HostAudioProcessorEditor::HostAudioProcessorEditor (HostAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    graphPanel = std::make_unique<GraphEditorPanel> (audioProcessor.getGraph());
    addAndMakeVisible (*graphPanel);

    setResizable (true, true);
    setSize (700, 500);
}

HostAudioProcessorEditor::~HostAudioProcessorEditor()
{
}

void HostAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void HostAudioProcessorEditor::resized()
{
    graphPanel->setBounds (getLocalBounds());
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HostAudioProcessor();
}
#endif
