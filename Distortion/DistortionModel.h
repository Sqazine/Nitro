#pragma once

#include <cmath>
#include <algorithm>
#include <JuceHeader.h>

// ============================================================================
// DistortionModel - generic waveshaper distortion with HPF / LPF
//
// Signal path (per channel, per sample at oversampled rate):
//
//   in ──► InputGain(distortion dB) ──► HPF(freq) ──► tanh waveshaper (×0.7)
//                                      ──► LPF(freq) ──► OutputGain(level dB) ──► out
//
// This is a clean, flexible DSP-based distortion suitable as a building block
// before adding more specific circuit models.
// ============================================================================
class DistortionModel
{
public:
    DistortionModel() = default;

    void prepare (double sampleRate, int /*osFactor*/)
    {
        fs = static_cast<float> (sampleRate);
        reset();
    }

    void reset()
    {
        hpfX1 = hpfX2 = hpfY1 = hpfY2 = 0.0f;
        lpfX1 = lpfX2 = lpfY1 = lpfY2 = 0.0f;
    }

    void setParams (float distortionDb, float levelDb, float hpfFreq, float lpfFreq)
    {
        inputGain  = juce::Decibels::decibelsToGain (distortionDb);
        outputGain = juce::Decibels::decibelsToGain (levelDb);
        hpfCutoff  = hpfFreq;
        lpfCutoff  = lpfFreq;
    }

    float processSample (float vin)
    {
        float x = vin * inputGain;
        x = hpfStage (x);
        x = waveshapeStage (x);
        x = lpfStage (x);
        x *= outputGain;
        return x;
    }

private:
    float fs = 44100.0f;
    float inputGain  = 1.0f;
    float outputGain = 1.0f;
    float hpfCutoff  = 20.0f;
    float lpfCutoff  = 20000.0f;

    float hpfX1 = 0.0f, hpfX2 = 0.0f, hpfY1 = 0.0f, hpfY2 = 0.0f;
    float lpfX1 = 0.0f, lpfX2 = 0.0f, lpfY1 = 0.0f, lpfY2 = 0.0f;

    // ========================================================================
    // First-order HPF (One-pole, same as original ProcessorDuplicator)
    // ========================================================================
    float hpfStage (float x)
    {
        float w0 = 2.0f * juce::float_Pi * hpfCutoff / fs;
        float cosw = std::cos (w0);
        float alpha = std::sin (w0) / std::sqrt (2.0f);

        float b0 = (1.0f + cosw) * 0.5f;
        float b1 = -(1.0f + cosw);
        float b2 = (1.0f + cosw) * 0.5f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw;
        float a2 = 1.0f - alpha;

        float y = (b0 * x + b1 * hpfX1 + b2 * hpfX2 - a1 * hpfY1 - a2 * hpfY2) / a0;
        hpfX2 = hpfX1; hpfX1 = x;
        hpfY2 = hpfY1; hpfY1 = y;
        return y;
    }

    // ========================================================================
    // First-order LPF (One-pole, same as original ProcessorDuplicator)
    // ========================================================================
    float lpfStage (float x)
    {
        float w0 = 2.0f * juce::float_Pi * lpfCutoff / fs;
        float cosw = std::cos (w0);
        float alpha = std::sin (w0) / std::sqrt (2.0f);

        float b0 = (1.0f - cosw) * 0.5f;
        float b1 = 1.0f - cosw;
        float b2 = (1.0f - cosw) * 0.5f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw;
        float a2 = 1.0f - alpha;

        float y = (b0 * x + b1 * lpfX1 + b2 * lpfX2 - a1 * lpfY1 - a2 * lpfY2) / a0;
        lpfX2 = lpfX1; lpfX1 = x;
        lpfY2 = lpfY1; lpfY1 = y;
        return y;
    }

    // ========================================================================
    // tanh waveshaper with 0.7 output reduction to keep headroom
    // ========================================================================
    float waveshapeStage (float x)
    {
        return std::tanh (x) * 0.7f;
    }
};