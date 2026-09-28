#pragma once
#include <cmath>
#include <algorithm>
#include <JuceHeader.h>

// ============================================================================
// TS808Model - White-box Tube Screamer (TL072 based)
//
// Signal path (per channel, per sample at oversampled rate):
//
//   in ──► HPF(16Hz) ──► U1a gain + symmetrical diode soft clip
//                      ──► Tone bell EQ ──► LPF(6kHz)
//                      ──► Gain(Level) ──► HPF(10Hz) ──► out
//
// Key difference from DS-1/DS-2:
//   TS-808: anti-parallel diodes IN THE FEEDBACK LOOP → symmetric clipping,
//           harder knee, even and odd harmonics
//   DS-1/2: back-to-back LEDs TO GROUND at op-amp output → asymmetric soft
//           clipping, even harmonics dominance, warmer knee
//
// Normalized digital units throughout: ±1.0 = full scale.
// Drive maps log-pot 1k→1MΩ ratio (same as DS-1), so G = -1000^drive.
// Diode soft clip uses a smooth piecewise curve calibrated so that
// digital-level guitar inputs clip naturally.
// ============================================================================
class TS808Model
{
public:
    TS808Model() = default;

    void prepare (double sampleRate, int osFactor)
    {
        fs = static_cast<float> (sampleRate * osFactor);
        reset();
    }

    void reset()
    {
        hpf1X1 = hpf1X2 = hpf1Y1 = hpf1Y2 = 0.0f;
        hpf2X1 = hpf2X2 = hpf2Y1 = hpf2Y2 = 0.0f;
        toneX1 = toneX2 = toneY1 = toneY2 = 0.0f;
        lpfX1  = lpfX2  = lpfY1  = lpfY2  = 0.0f;
    }

    void setParams (float drive_, float tone_, float level_)
    {
        drive = juce::jlimit (0.0f, 1.0f, drive_);
        tone  = juce::jlimit (0.0f, 1.0f, tone_);
        level = juce::jlimit (0.0f, 1.0f, level_);
        levelGain = juce::jmax (level * level, 0.0f) * 4.0f;
    }

    float processSample (float vin)
    {
        float x = inputHPF (vin);
        x = u1aGainDiodeClip (x);
        x = toneStage (x);
        x = lowpassStage (x);
        x *= levelGain;
        x = outputHPF (x);
        return x;
    }

private:
    float fs = 44100.0f;
    float drive = 0.0f, tone = 0.5f, level = 0.5f;
    float levelGain = 1.0f;

    float hpf1X1 = 0.0f, hpf1X2 = 0.0f, hpf1Y1 = 0.0f, hpf1Y2 = 0.0f;
    float hpf2X1 = 0.0f, hpf2X2 = 0.0f, hpf2Y1 = 0.0f, hpf2Y2 = 0.0f;
    float toneX1 = 0.0f, toneX2 = 0.0f, toneY1 = 0.0f, toneY2 = 0.0f;
    float lpfX1  = 0.0f, lpfX2 = 0.0f, lpfY1  = 0.0f, lpfY2  = 0.0f;

    // ========================================================================
    // Input HPF - C1 coupling cap with input impedance (~16Hz)
    // ========================================================================
    float inputHPF (float x)
    {
        float w0 = 2.0f * juce::float_Pi * 16.0f / fs;
        float cosw = std::cos (w0), alpha = std::sin (w0) / 1.4142135f;

        float b0 = (1.0f + cosw) * 0.5f;
        float b1 = -(1.0f + cosw);
        float b2 = (1.0f + cosw) * 0.5f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw;
        float a2 = 1.0f - alpha;

        float y = (b0 * x + b1 * hpf1X1 + b2 * hpf1X2 - a1 * hpf1Y1 - a2 * hpf1Y2) / a0;
        hpf1X2 = hpf1X1; hpf1X1 = x;
        hpf1Y2 = hpf1Y1; hpf1Y1 = y;
        return y;
    }

    // ========================================================================
    // U1a Inverting gain stage + symmetrical diode soft clip in feedback
    //
    // Physical circuit (normalized to digital units):
    //   R_in     = 10kΩ
    //   R_fb     = Drive log pot 1kΩ → 1MΩ  (same 1000:1 ratio as DS-1)
    //   C_fb     = 1nF  (compensation, prevents oscillation)
    //   Diode pair = anti-parallel 1N4148 in series with 10k from output back
    //                to virtual ground (so they clip symmetrically)
    //   Rails    = ±4.5V physical → normalized to ±1.0
    //
    // Digital domain scaling:
    //   Open-loop clipping threshold ~ Vf/2 scaled → digital ~0.30
    //   Strong clipping region → digital ~0.70
    //
    // Drive mapping (log pot 1k→1M, ratio 1000):
    //   G = -1000^drive  (same as DS-1 for consistency)
    //   drive=0 → G=-1, drive=0.5 → G=-31.6, drive=1 → G=-1000
    //
    // TS-808 uses diodes IN THE FEEDBACK, so the clip is:
    //   - Symmetric (odd harmonics present)
    //   - Harder knee than DS-1's LED-to-ground
    //   - More midrange-forward distortion characteristic
    // ========================================================================
    float u1aGainDiodeClip (float vin)
    {
        float rFbNorm = std::pow (1000.0f, drive);
        float idealOut = -vin * rFbNorm;

        constexpr float VD_START = 0.25f;
        constexpr float VD_FULL  = 0.70f;
        constexpr float RAIL     = 1.0f;

        float v = idealOut;
        float absV = std::abs (v);

        if (absV > VD_START)
        {
            float excess = absV - VD_START;
            float factor = 1.0f - (1.0f - VD_FULL / absV) * (2.0f / juce::float_Pi) * std::atan (excess * 4.0f);
            factor = juce::jlimit (VD_FULL / juce::jmax (absV, VD_FULL), 1.0f, factor);
            v *= factor;
        }

        v = RAIL * std::tanh (v / RAIL);

        v *= 1.0f - drive * 0.25f;

        return v;
    }

    // ========================================================================
    // Tone stage - parametric bell EQ
    // Tone=0 → f0=1200Hz, Q=1.5, boost=0.2dB (bright)
    // Tone=1 → f0=4700Hz, Q=0.5, boost=2.0dB (warm thickening)
    // ========================================================================
    float toneStage (float x)
    {
        float f0 = 1200.0f + tone * 3500.0f;
        float Q = 0.5f + (1.0f - tone) * 1.0f;
        float boost = 0.2f + tone * 1.8f;
        float A = std::pow (10.0f, boost / 40.0f);

        float w0 = 2.0f * juce::float_Pi * f0 / fs;
        float cosw = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * Q);

        float b0 = 1.0f + alpha * A;
        float b1 = -2.0f * cosw;
        float b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        float a1 = -2.0f * cosw;
        float a2 = 1.0f - alpha / A;

        float y = (b0 * x + b1 * toneX1 + b2 * toneX2 - a1 * toneY1 - a2 * toneY2) / a0;
        toneX2 = toneX1; toneX1 = x;
        toneY2 = toneY1; toneY1 = y;
        return y;
    }

    // ========================================================================
    // U1b second-order low-pass filter (6kHz Butterworth)
    // Rolls out-of-band harmonics from the diode clipping stage
    // ========================================================================
    float lowpassStage (float x)
    {
        float freq = 6000.0f;
        float w0 = 2.0f * juce::float_Pi * freq / fs;
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
    // Output HPF - removes DC offset (~10Hz)
    // ========================================================================
    float outputHPF (float x)
    {
        float w0 = 2.0f * juce::float_Pi * 10.0f / fs;
        float cosw = std::cos (w0), alpha = std::sin (w0) / 1.4142135f;

        float b0 = (1.0f + cosw) * 0.5f;
        float b1 = -(1.0f + cosw);
        float b2 = (1.0f + cosw) * 0.5f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw;
        float a2 = 1.0f - alpha;

        float y = (b0 * x + b1 * hpf2X1 + b2 * hpf2X2 - a1 * hpf2Y1 - a2 * hpf2Y2) / a0;
        hpf2X2 = hpf2X1; hpf2X1 = x;
        hpf2Y2 = hpf2Y1; hpf2Y1 = y;
        return y;
    }
};