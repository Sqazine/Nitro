#pragma once
#include <cmath>
#include <algorithm>
#include <JuceHeader.h>

// ============================================================================
// DS2Model - White-box Boss DS-2 Distortion (4558 based + Turbo modes)
//
// DS-1 vs DS-2 circuit architecture:
//   DS-1:  U1a (gain+LED clip to ground) → U1b (filter+Tone) → Level → out
//   DS-2:  U1a (gain+LED clip to ground) → [U2 Turbo path] → U1b → Level → out
//
// Turbo switch (S2) inserts an additional U2 op-amp stage between U1a and U1b
// that provides mode-dependent EQ shaping + extra gain:
//
//   Turbo I:  U2 warm low-mid boost + extra soft clip
//   Turbo II: U2 resonant mid-peak (Q~3 @ 1.5kHz) + harder clip
//
// All values here use NORMALIZED DIGITAL UNITS (±1.0 = full scale).
// LED clipping threshold, rail saturation, op-amp gains are all derived
// from the physical circuit but scaled to the digital domain so that
// typical guitar input levels trigger the distortion naturally.
//
// Key difference from TS-808:
//   TS-808: diodes IN the feedback loop (harder, more symmetric clip)
//   DS-1/2: LEDs TO GROUND at op-amp output (softer knee, even harmonics)
// ============================================================================

enum class TurboMode
{
    TurboI=0,
    TurboII
};

class DS2Model
{
public:
    DS2Model() = default;

    void prepare (double sampleRate, int osFactor)
    {
        fs = static_cast<float> (sampleRate * osFactor);
        reset();
    }

    void reset()
    {
        hpf1X1 = hpf1X2 = hpf1Y1 = hpf1Y2 = 0.0f;
        hpf2X1 = hpf2X2 = hpf2Y1 = hpf2Y2 = 0.0f;
        lpfX1  = lpfX2  = lpfY1  = lpfY2  = 0.0f;
        toneX1 = toneX2 = toneY1 = toneY2 = 0.0f;
        u2X1   = u2X2   = u2Y1   = u2Y2   = 0.0f;
    }

    void setParams (float drive_, float tone_, float level_, TurboMode mode_)
    {
        drive = juce::jlimit (0.0f, 1.0f, drive_);
        tone  = juce::jlimit (0.0f, 1.0f, tone_);
        level = juce::jlimit (0.0f, 1.0f, level_);
        mode  = mode_;
        levelGain = juce::jmax (level * level, 0.0f) * 4.0f;
    }

    float processSample (float vin)
    {
        float x = inputHPF (vin);
        x = u1aGainLEDClip (x);
        x = u2TurboStage (x);
        x = u1bFilterStage (x);
        x = toneStage (x);
        x *= levelGain;
        x = outputHPF (x);
        return x;
    }

private:
    float fs = 44100.0f;
    float drive = 0.0f, tone = 0.5f, level = 0.5f;
    float levelGain = 1.0f;
    TurboMode mode = TurboMode::TurboI;

    float hpf1X1 = 0.0f, hpf1X2 = 0.0f, hpf1Y1 = 0.0f, hpf1Y2 = 0.0f;
    float hpf2X1 = 0.0f, hpf2X2 = 0.0f, hpf2Y1 = 0.0f, hpf2Y2 = 0.0f;
    float lpfX1  = 0.0f, lpfX2  = 0.0f, lpfY1  = 0.0f, lpfY2  = 0.0f;
    float toneX1 = 0.0f, toneX2 = 0.0f, toneY1 = 0.0f, toneY2 = 0.0f;
    float u2X1   = 0.0f, u2X2   = 0.0f, u2Y1   = 0.0f, u2Y2   = 0.0f;

    // ========================================================================
    // Input HPF - C1 coupling cap forms ~40Hz high-pass with input impedance
    // ========================================================================
    float inputHPF (float x)
    {
        float w0 = 2.0f * juce::float_Pi * 40.0f / fs;
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
    // U1a Inverting gain stage + LED soft clipping to ground
    //
    // Physical circuit (normalized to digital units):
    //   R_in     = 1kΩ  (input resistor at inverting node)
    //   R_fb     = Drive log pot 1kΩ → 1MΩ  (feedback resistor)
    //   C_fb     = 22pF (frequency compensation, prevents oscillation)
    //   LED pair = back-to-back LEDs from output to ground (Vf ~1.8V physical)
    //   Rails    = ±4.5V physical (9V supply → normalized to ±1.0)
    //
    // Digital domain scaling:
    //   Physical 4.5V rail → digital 1.0
    //   LED Vf=1.8V physical → digital 0.4  (soft start of conduction)
    //   Open-loop LED clamp → digital ~0.5  (strong conduction region)
    //
    // Drive mapping (log pot 1k→1M, ratio 1000):
    //   R_fb_norm = 1.0 * 1000^drive  (normalized so R_in=1.0)
    //   Closed-loop gain G = -R_fb_norm / R_in  = -1000^drive
    //   drive=0 → G=-1   (unity buffer, no distortion)
    //   drive=0.5 → G=-31.6
    //   drive=1 → G=-1000 (massive gain, immediately saturates LEDs)
    // ========================================================================
    float u1aGainLEDClip (float vin)
    {
        float rFbNorm = std::pow (1000.0f, drive);
        float idealOut = -vin * rFbNorm;

        constexpr float VF_START = 0.35f;
        constexpr float VF_FULL  = 0.80f;
        constexpr float RAIL     = 1.0f;

        float v = idealOut;

        {
            float absV = std::abs (v);
            if (absV > VF_START)
            {
                float factor = 1.0f - (1.0f - VF_FULL / absV) * (2.0f / juce::float_Pi) * std::atan ((absV - VF_START) * 3.0f);
                factor = juce::jlimit (VF_FULL / juce::jmax (absV, VF_FULL), 1.0f, factor);
                v *= factor;
            }
        }

        v = RAIL * std::tanh (v / RAIL);

        v *= 1.0f - drive * 0.3f;

        return v;
    }

    // ========================================================================
    // U2 Turbo stage - additional op-amp path inserted by Turbo switch (S2)
    //
    // Turbo I:  U2 is a warm shelving boost (low-mid +3dB @ 400Hz)
    //           + +3dB broadband gain + gentle soft clip for warmth
    // Turbo II: U2 is a resonant parametric peak (+10dB @ 1.5kHz, Q=3)
    //           + +5dB broadband gain + harder clip for aggression
    // ========================================================================
    float u2TurboStage (float x)
    {
        if (mode == TurboMode::TurboI)
        {
            // Low shelf boost: +3dB at 400Hz
            float f0 = 400.0f;
            float Q = 0.707f;
            float A = std::pow (10.0f, 3.0f / 40.0f);
            float w0 = 2.0f * juce::float_Pi * f0 / fs;
            float cosw = std::cos (w0);
            float alpha = std::sin (w0) / (2.0f * Q);
            float sqrtA = std::sqrt (A);
            float twoSqrtAAlpha = 2.0f * sqrtA * alpha;

            float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha);
            float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw);
            float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha);
            float a0 = (A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha;
            float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw);
            float a2 = (A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha;

            float y = (b0 * x + b1 * u2X1 + b2 * u2X2 - a1 * u2Y1 - a2 * u2Y2) / a0;
            u2X2 = u2X1; u2X1 = x;
            u2Y2 = u2Y1; u2Y1 = y;

            constexpr float TI_GAIN = 1.5f;
            constexpr float TI_SOFT = 0.7f;
            float clipped = TI_SOFT * std::tanh (y * TI_GAIN / TI_SOFT);
            return clipped;
        }

        else // TurboII
        {
            // Parametric peak EQ: +10dB at 1.5kHz, Q=3 (very resonant)
            float f0 = 1500.0f;
            float Q = 3.0f;
            float A = std::pow (10.0f, 10.0f / 40.0f);
            float w0 = 2.0f * juce::float_Pi * f0 / fs;
            float cosw = std::cos (w0);
            float alpha = std::sin (w0) / (2.0f * Q);

            float b0 = 1.0f + alpha * A;
            float b1 = -2.0f * cosw;
            float b2 = 1.0f - alpha * A;
            float a0 = 1.0f + alpha / A;
            float a1 = -2.0f * cosw;
            float a2 = 1.0f - alpha / A;

            float y = (b0 * x + b1 * u2X1 + b2 * u2X2 - a1 * u2Y1 - a2 * u2Y2) / a0;
            u2X2 = u2X1; u2X1 = x;
            u2Y2 = u2Y1; u2Y1 = y;

            constexpr float TII_GAIN = 1.8f;
            constexpr float TII_SOFT = 0.6f;
            float clipped = TII_SOFT * std::tanh (y * TII_GAIN / TII_SOFT);
            return clipped;
        }
    }

    // ========================================================================
    // U1b second-order low-pass filter (7.5kHz Butterworth)
    // Rolls out-of-band harmonics from the clipping stage
    // ========================================================================
    float u1bFilterStage (float x)
    {
        float freq = 7500.0f;
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
    // Tone stage - parametric EQ
    // Tone=0 → bright (f0=3kHz, +dB boost)
    // Tone=1 → dark  (f0=1kHz, -dB cut)
    // ========================================================================
    float toneStage (float x)
    {
        float f0 = 3000.0f - tone * 2000.0f;
        float Q = 0.707f + tone * 1.5f;
        float boost = (1.0f - tone) * 2.4f - tone * 1.0f;
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
    // Output HPF - removes DC offset (~20Hz)
    // ========================================================================
    float outputHPF (float x)
    {
        float w0 = 2.0f * juce::float_Pi * 20.0f / fs;
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