#pragma once
#include <JuceHeader.h>

class TunerAudioProcessor  : public juce::AudioProcessor
{
public:
    TunerAudioProcessor();
    ~TunerAudioProcessor() override;

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

    // Tuner-specific methods
    float getDetectedFrequency() const { return smoothedFrequency; }
    float getCentsOff() const { return smoothedCents; }
    int getDetectedNote() const { return smoothedNote; }
    bool isNoteValid() const { return holdCounter > 0; }
    float getSignalAmplitude() const { return signalAmplitude; }

private:
    float detectPitch(const float* buffer, int bufferSize, double sampleRate);
    float amplitudeRMS(const float* buffer, int bufferSize);
    
    // Pitch detection using autocorrelation
    static constexpr int fftSize = 4096;
    juce::dsp::FFT tunnerFFT;
    float fftBuffer[fftSize];
    int bufferIndex = 0;
    
    float detectedFrequency = 0.0f;
    float centsOff = 0.0f;
    int detectedNote = -1;
    float signalAmplitude = 0.0f;
    
    // Smoothed/held values for stable display
    float smoothedFrequency = 0.0f;
    float smoothedCents = 0.0f;
    int smoothedNote = -1;
    int holdCounter = 0;
    static constexpr int holdFrames = 30; // ~1 second at 30fps
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerAudioProcessor)
};