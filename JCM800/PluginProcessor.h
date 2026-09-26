#pragma once
#include <JuceHeader.h>
#include <memory>
#include "Common/Common.h"
#include "JCM800/JCM800Model.h"

// ============================================================================
//  JCM800AudioProcessor
//
//  Drives the WHITE-BOX physical JCM800 model (JCM800Model.h) instead of
//  the old static waveshaper + EQ chain. The nonlinear preamp / phase-inverter
//  / power-amp stages run at 4x oversampling (juce::dsp::Oversampling) so the
//  per-sample Newton-Raphson circuit solver stays alias-free; the cabinet
//  convolution runs at the base rate. Parameter IDs (Gain/Bass/Middle/High/
//  Volume/Presence) are unchanged so existing host sessions recall correctly.
// ============================================================================
class JCM800AudioProcessor : public juce::AudioProcessor
{
public:
    JCM800AudioProcessor();
    ~JCM800AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

private:
    static constexpr int kNumModels = 2;                 // stereo: L + R, each with its own amp state
    static constexpr int kOsExponent = 2;                // 2^kOsExponent = 4x oversampling
    static constexpr int kOversample = 1 << kOsExponent; // actual factor (4) for the model
    static constexpr double kTailSec = 0.12;             // cabinet impulse tail (~4096/44100)

    juce::AudioProcessorValueTreeState mApvts;

    // Scans conventional locations for a measured Greenback IR (Greenback.wav /
    // Greenback_IR.wav) and loads it into every channel's cabinet. Falls back to
    // the synthesised IR when none is found. Called from prepareToPlay.
    void loadCabinetIR();
    PluginParameterSlider mGain;
    PluginParameterSlider mBass;
    PluginParameterSlider mMiddle;
    PluginParameterSlider mHigh;
    PluginParameterSlider mVolume;
    PluginParameterSlider mPresence;

    std::unique_ptr<juce::dsp::Oversampling<float>> mOversampling;
    JCM800Model mModel[kNumModels];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JCM800AudioProcessor)
};
