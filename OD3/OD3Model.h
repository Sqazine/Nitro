#pragma once
#include <cmath>
#include <algorithm>
#include <JuceHeader.h>

// ============================================================================
// OD3Model - White-box Boss OD-3 Overdrive (JRC4558 based)
//
// Signal path (per channel, per sample at oversampled rate):
//
//   in ──► HPF(16Hz) ──► U1a gain + asymmetrical silicon diode soft clip
//                      ──► U1b mid-boost/bell EQ + tone filter
//                      ──► Low-pass(6kHz)
//                      ──► Gain(Level) ──► HPF(10Hz) ──► out
//
// How OD-3 differs from TS-808 and DS-2:
//   TS-808:  anti-parallel diodes IN FEEDBACK LOOP → symmetric clip, harder knee
//   DS-1/2:  back-to-back LEDs TO GROUND → asymmetric, even harmonics, warm
//   OD-3:    silicon diodes TO GROUND (asymmetric like DS) + MID BOOST
//            The Mid control is OD-3's signature: a peaking EQ around 500Hz–2kHz
//            that lets you sculpt the midrange bite independently of Tone
//
// OD-3 front panel controls:
//   Drive  - sets pre-gain before clipping (log pot)
//   Tone   - high-cut / tone balance
//   Mid    - midrange peak gain (unique to OD-3 vs TS/DS series)
//   Level  - output level
//
// Normalized digital units throughout: ±1.0 = full scale.
// ============================================================================
class OD3Model
{
public:
    OD3Model() = default;

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
        midX1  = midX2  = midY1  = midY2  = 0.0f;
    }

    void setParams (float drive_, float tone_, float mid_, float level_)
    {
        drive  = juce::jlimit (0.0f, 1.0f, drive_);
        tone   = juce::jlimit (0.0f, 1.0f, tone_);
        mid    = juce::jlimit (0.0f, 1.0f, mid_);
        level  = juce::jlimit (0.0f, 1.0f, level_);
        levelGain = juce::jmax (level * level, 0.0f) * 4.0f;
    }

    float processSample (float vin)
    {
        float x = inputHPF (vin);
        x = u1aGainDiodeClip (x);
        x = midStage (x);
        x = toneStage (x);
        x = lowpassStage (x);
        x *= levelGain;
        x = outputHPF (x);
        return x;
    }

private:
    float fs = 44100.0f;
    float drive = 0.0f, tone = 0.5f, mid = 0.5f, level = 0.5f;
    float levelGain = 1.0f;

    float hpf1X1 = 0.0f, hpf1X2 = 0.0f, hpf1Y1 = 0.0f, hpf1Y2 = 0.0f;
    float hpf2X1 = 0.0f, hpf2X2 = 0.0f, hpf2Y1 = 0.0f, hpf2Y2 = 0.0f;
    float lpfX1  = 0.0f, lpfX2  = 0.0f, lpfY1  = 0.0f, lpfY2  = 0.0f;
    float toneX1 = 0.0f, toneX2 = 0.0f, toneY1 = 0.0f, toneY2 = 0.0f;
    float midX1  = 0.0f, midX2  = 0.0f, midY1  = 0.0f, midY2  = 0.0f;

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
    // U1a Inverting gain stage + ASYMMETRICAL silicon diode soft clip to ground
    //
    // Physical circuit (normalized to digital units):
    //   R_in     = 10kΩ
    //   R_fb     = Drive log pot 1kΩ → 1MΩ  (1000:1 ratio, same as TS808/DS)
    //   D1/D2    = back-to-back 1N4148 silicon diodes from output to ground
    //              Asymmetric: forward Vf ~0.55V → digital ~0.28,
    //              reverse   Vf ~0.65V → digital ~0.32  (slight asymmetry
    //              adds even harmonics for warmth)
    //   Rails    = ±4.5V physical → normalized to ±1.0
    //
    // Drive mapping (log pot 1k→1M, ratio 1000):
    //   G = -1000^drive
    //   drive=0 → G=-1, drive=0.5 → G=-31.6, drive=1 → G=-1000
    //
    // Unlike TS-808 (diodes in FEEDBACK = symmetric clip), OD-3 clips diodes
    // TO GROUND at the op-amp output, producing mild asymmetry that makes
    // the overdrive sound warmer and less aggressive than TS808.
    // Unlike DS-2's LEDs (softer Vf ~1.8V physical), silicon diodes clip
    // harder and more abruptly.
    // ========================================================================
    float u1aGainDiodeClip (float vin)
    {
        float rFbNorm = std::pow (1000.0f, drive);
        float idealOut = -vin * rFbNorm;

        constexpr float VF_POS_START = 0.22f;
        constexpr float VF_POS_FULL  = 0.55f;
        constexpr float VF_NEG_START = 0.26f;
        constexpr float VF_NEG_FULL  = 0.62f;
        constexpr float RAIL         = 1.0f;

        float v = idealOut;

        if (v > 0.0f)
        {
            if (v > VF_POS_START)
            {
                float excess = v - VF_POS_START;
                float factor = 1.0f - (1.0f - VF_POS_FULL / v) * (2.0f / juce::float_Pi) * std::atan (excess * 3.5f);
                factor = juce::jlimit (VF_POS_FULL / juce::jmax (v, VF_POS_FULL), 1.0f, factor);
                v *= factor;
            }
        }
        else
        {
            float absV = -v;
            if (absV > VF_NEG_START)
            {
                float excess = absV - VF_NEG_START;
                float factor = 1.0f - (1.0f - VF_NEG_FULL / absV) * (2.0f / juce::float_Pi) * std::atan (excess * 3.5f);
                factor = juce::jlimit (VF_NEG_FULL / juce::jmax (absV, VF_NEG_FULL), 1.0f, factor);
                v *= factor;
            }
        }

        v = RAIL * std::tanh (v / RAIL);

        v *= 1.0f - drive * 0.2f;

        return v;
    }

    // ========================================================================
    // Mid stage - parametric bell EQ (OD-3's signature feature)
    //
    // Mid=0 → f0=2000Hz, Q=1.0, gain=-4dB (mid scoop)
    // Mid=0.5 → f0=1000Hz, Q=1.4, gain=0dB (neutral)
    // Mid=1 → f0=600Hz, Q=1.8, gain=+6dB (mid push)
    //
    // This lets the OD-3 cut through a dense mix without cranking Drive.
    // ========================================================================
    float midStage (float x)
    {
        float f0   = 2000.0f - mid * 1400.0f;
        float Q    = 1.0f + mid * 0.8f;
        float gainDB = -4.0f + mid * 10.0f;
        float A    = std::pow (10.0f, gainDB / 40.0f);

        float w0 = 2.0f * juce::float_Pi * f0 / fs;
        float cosw = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * Q);

        float b0 = 1.0f + alpha * A;
        float b1 = -2.0f * cosw;
        float b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        float a1 = -2.0f * cosw;
        float a2 = 1.0f - alpha / A;

        float y = (b0 * x + b1 * midX1 + b2 * midX2 - a1 * midY1 - a2 * midY2) / a0;
        midX2 = midX1; midX1 = x;
        midY2 = midY1; midY1 = y;
        return y;
    }

    // ========================================================================
    // Tone stage - parametric EQ / bell filter
    // Tone=0 → bright (f0=4kHz, +3dB boost)
    // Tone=1 → dark  (f0=1.2kHz, -3dB cut)
    // ========================================================================
    float toneStage (float x)
    {
        float f0   = 4000.0f - tone * 2800.0f;
        float Q    = 0.707f + tone * 1.0f;
        float gainDB = (1.0f - tone) * 3.0f - tone * 3.0f;
        float A    = std::pow (10.0f, gainDB / 40.0f);

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