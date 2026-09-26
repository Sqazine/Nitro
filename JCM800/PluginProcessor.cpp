#include "PluginProcessor.h"

JCM800AudioProcessor::JCM800AudioProcessor()
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
     , mApvts(*this, nullptr)
     , mGain(mApvts, "Gain", "GAIN", 0.0f, 1.0f, 0.5f)
     , mBass(mApvts, "Bass", "BASS", 0.0f, 1.0f, 0.5f)
     , mMiddle(mApvts, "Middle", "MID", 0.0f, 1.0f, 0.5f)
     , mHigh(mApvts, "High", "HIGH", 0.0f, 1.0f, 0.5f)
     , mVolume(mApvts, "Volume", "VOL", 0.0f, 1.0f, 0.5f)
     , mPresence(mApvts, "Presence", "PRES", 0.0f, 1.0f, 0.5f)
{
}

JCM800AudioProcessor::~JCM800AudioProcessor()
{
}

const juce::String JCM800AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JCM800AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool JCM800AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool JCM800AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double JCM800AudioProcessor::getTailLengthSeconds() const
{
    return kTailSec;
}

int JCM800AudioProcessor::getNumPrograms()
{
    return 1;
}

int JCM800AudioProcessor::getCurrentProgram()
{
    return 0;
}

void JCM800AudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String JCM800AudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void JCM800AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void JCM800AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 4x oversampling for the nonlinear tube/transformer stages.
    // NOTE: the Oversampling "factor" argument is 2^factor, so exponent 2 -> 4x.
    mOversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t) kNumModels,
        (size_t) kOsExponent,
        juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR);

    // This JUCE build's Oversampling has no prepare(ProcessSpec); the half-band
    // filters are sample-rate independent, so just size the internal buffers.
    mOversampling->initProcessing ((size_t) samplesPerBlock);

    // One white-box amp model per channel (kept independent for true stereo).
    for (int i = 0; i < kNumModels; ++i)
        mModel[i].prepare (sampleRate, kOversample);

    // Push initial parameter values into both models.
    auto g = mGain.getTargetValue(), b = mBass.getTargetValue(),
         m = mMiddle.getTargetValue(), h = mHigh.getTargetValue(),
         p = mPresence.getTargetValue(), v = mVolume.getTargetValue();
    for (int i = 0; i < kNumModels; ++i)
        mModel[i].setParams (g, b, m, h, p, v);

    // Load a measured Greenback IR if one is present on disk (else synth IR).
    loadCabinetIR();
}

void JCM800AudioProcessor::loadCabinetIR()
{
    // Only attempt to swap in a measured IR if we are not already using one
    // (so a host that calls prepareToPlay repeatedly does not re-read/re-normalise
    // the file each time). If no file exists we keep retrying cheaply.
    if (mModel[0].mCab.mLoadedFromFile) return;

    juce::StringArray candidates;
    auto addDir = [&](const juce::File& dir)
    {
        candidates.add (dir.getChildFile ("Greenback.wav").getFullPathName());
        candidates.add (dir.getChildFile ("Greenback_IR.wav").getFullPathName());
        candidates.add (dir.getChildFile ("greenback.wav").getFullPathName());
    };
    addDir (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("JCM800"));
    addDir (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("JCM800"));
    addDir (juce::File ("D:/sc/Nitro/JCM800"));   // dev convenience

    for (int i = 0; i < candidates.size(); ++i)
    {
        juce::File f (candidates[i]);
        if (!f.existsAsFile()) continue;
        juce::String path = f.getFullPathName();
        bool ok = true;
        for (int c = 0; c < kNumModels; ++c)
            if (!mModel[c].mCab.loadWav (path.toRawUTF8())) ok = false;
        if (ok) { DBG ("JCM800: loaded measured cabinet IR from " + path); return; }
    }
    DBG ("JCM800: no measured Greenback IR found -> using synthesised IR");
}

void JCM800AudioProcessor::releaseResources()
{
    mOversampling.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool JCM800AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void JCM800AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Shared front-panel parameters are applied to every channel model.
    auto g = mGain.getTargetValue(), b = mBass.getTargetValue(),
         m = mMiddle.getTargetValue(), h = mHigh.getTargetValue(),
         p = mPresence.getTargetValue(), v = mVolume.getTargetValue();
    for (int i = 0; i < kNumModels; ++i)
        mModel[i].setParams (g, b, m, h, p, v);

    const int numSamples = buffer.getNumSamples();
    const int numCh = (int) std::min ((size_t) kNumModels, (size_t) buffer.getNumChannels());

    // ---- oversample the input, run the preamp/power-amp, downsample ----
    juce::dsp::AudioBlock<float> block (buffer);
    auto osBlock = mOversampling->processSamplesUp (block);
    const int osNumSamples = (int) osBlock.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* osData = osBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < osNumSamples; ++i)
            osData[i] = mModel[ch].processAmp (osData[i]);
    }

    // processSamplesDown writes the base-rate (downsampled) amp output back
    // into `block` (== buffer).
    mOversampling->processSamplesDown (block);

    // ---- cabinet convolution at the base rate (per channel) ----
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = mModel[ch].processCab (data[i]);
    }
}

bool JCM800AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* JCM800AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void JCM800AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = mApvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void JCM800AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName (mApvts.state.getType()))
        mApvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// This creates new instances of the plugin..
#ifdef EXPORT_CREATE_FILTER_FUNCTION
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JCM800AudioProcessor();
}
#endif
