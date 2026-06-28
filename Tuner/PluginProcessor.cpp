
#include "PluginProcessor.h"
#include "PluginEditor.h"

TunerAudioProcessor::TunerAudioProcessor()
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
      , tunnerFFT(12) // 2^12 = 4096
{
    jassert(juce::isPowerOfTwo(fftSize));
    bufferIndex = 0;
}

TunerAudioProcessor::~TunerAudioProcessor()
{
}

const juce::String TunerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TunerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TunerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TunerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TunerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TunerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TunerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TunerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TunerAudioProcessor::getProgramName (int index)
{
    return {};
}

void TunerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void TunerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    bufferIndex = 0;
    juce::zeromem(fftBuffer, sizeof(fftBuffer));
    holdCounter = 0;
    smoothedFrequency = 0.0f;
    smoothedCents = 0.0f;
    smoothedNote = -1;
}

void TunerAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TunerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void TunerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const auto* inputData = buffer.getReadPointer(0);
    auto numSamples = buffer.getNumSamples();
    
    signalAmplitude = amplitudeRMS(inputData, numSamples);
    
    if (signalAmplitude > 0.01f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            fftBuffer[bufferIndex] = inputData[i];
            bufferIndex = (bufferIndex + 1) % fftSize;
        }
        
        auto sampleRate = getSampleRate();
        detectedFrequency = detectPitch(fftBuffer, fftSize, sampleRate);
        
        if (detectedFrequency > 20.0f && detectedFrequency < 5000.0f)
        {
            const double a4Frequency = 440.0;
            const int a4MidiNote = 69;
            
            double midiNote = 12.0 * std::log2(detectedFrequency / a4Frequency) + a4MidiNote;
            detectedNote = static_cast<int>(juce::roundToInt(midiNote));
            
            double exactFrequency = a4Frequency * std::pow(2.0, (detectedNote - a4MidiNote) / 12.0);
            centsOff = 1200.0 * std::log2(detectedFrequency / exactFrequency);
            
            // Update smoothed values and reset hold counter
            smoothedFrequency = detectedFrequency;
            smoothedCents = centsOff;
            smoothedNote = detectedNote;
            holdCounter = holdFrames;
        }
        else
        {
            // No valid pitch detected, decrement hold counter
            if (holdCounter > 0)
                --holdCounter;
        }
    }
    else
    {
        // Low amplitude, decrement hold counter
        if (holdCounter > 0)
            --holdCounter;
    }
}

bool TunerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TunerAudioProcessor::createEditor()
{
    return new TunerAudioProcessorEditor (*this);
}

void TunerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void TunerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TunerAudioProcessor();
}
#endif

float TunerAudioProcessor::detectPitch(const float* buffer, int bufferSize, double sampleRate)
{
    float maxCorrelation = 0.0f;
    int bestLag = 0;
    
    int minLag = int(sampleRate / 1000.0);  // Max frequency ~1000Hz
    int maxLag = int(sampleRate / 30.0);     // Min frequency ~30Hz
    
    maxLag = juce::jmin(maxLag, bufferSize / 2);
    
    for (int lag = minLag; lag < maxLag; ++lag)
    {
        float correlation = 0.0f;
        float energy1 = 0.0f;
        float energy2 = 0.0f;
        
        for (int i = 0; i < bufferSize - lag; ++i)
        {
            correlation += buffer[i] * buffer[i + lag];
            energy1 += buffer[i] * buffer[i];
            energy2 += buffer[i + lag] * buffer[i + lag];
        }
        
        float normalizedCorrelation = correlation / (std::sqrt(energy1 * energy2) + 1e-10f);
        
        if (normalizedCorrelation > maxCorrelation)
        {
            maxCorrelation = normalizedCorrelation;
            bestLag = lag;
        }
    }
    
    if (maxCorrelation > 0.8f)
    {
        float frequency = static_cast<float>(sampleRate) / bestLag;
        return frequency;
    }
    
    return 0.0f;
}

float TunerAudioProcessor::amplitudeRMS(const float* buffer, int bufferSize)
{
    float sum = 0.0f;
    for (int i = 0; i < bufferSize; ++i)
        sum += buffer[i] * buffer[i];
    return std::sqrt(sum / bufferSize);
}
