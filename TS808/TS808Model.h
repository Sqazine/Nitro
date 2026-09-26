#pragma once
#include <cmath>
#include <algorithm>
#include <JuceHeader.h>

// ============================================================================
// TS808Model - White-box Tube Screamer (TL072 based)
//
// Signal path (per channel, per sample at oversampled rate):
//
//   in ──► HPF(16Hz) ──► U1a diode clipper ──► Gain(Drive)
//                      ──► U1b Tone EQ ──► LPF(6kHz)
//                      ──► Gain(Level) ──► HPF(10Hz) ──► out
//
// Diode clipper:  Newton-Raphson solves the op-amp feedback loop with
//                 anti-parallel 1N4148 diode pair + 22k feedback resistor
//                 + 1nF compensation cap + 10k series resistor.
//
// All nonlinear stages run at the oversampled rate; the linear IIRs
// run at base rate.
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
        capX  = 0.0f;
        toneX1 = toneX2 = toneY1 = toneY2 = 0.0f;
        lpfX1  = lpfX2 = lpfY1 = lpfY2 = 0.0f;
        hpf2X1 = hpf2X2 = hpf2Y1 = hpf2Y2 = 0.0f;
    }

    void setParams (float drive_, float tone_, float level_)
    {
        drive = juce::jlimit (0.0f, 1.0f, drive_);
        tone  = juce::jlimit (0.0f, 1.0f, tone_);
        level = juce::jlimit (0.0f, 1.0f, level_);
        levelGain = juce::jmax (level * level, 0.0f);
    }

    float processSample (float vin)
    {
        float x = inputHPF (vin);
        x = clipperStage (x);
        x *= drive + 0.05f;
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
    float capX  = 0.0f;

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

    // ========================================================================
    // Diode clipper stage (the heart of TS808)
    //
    // Circuit: inverting op-amp with R1=10k input, Rf=22k feedback,
    //          Cf=1nF parallel across Rf for frequency compensation.
    //          Anti-parallel 1N4148 diode pair in series with 10k resistor
    //          from output back to virtual ground.
    //
    // Simplified KCL at inverting input (virtual ground V- = 0 for AC):
    //     Vin / R1  =  -Vout / Rf  +  Cf * d(-Vout)/dt  +  Id(Vd)
    //
    // where Vd = -Vout  (voltage across diode pair / 10k series)
    // and Id(Vd) = Is * (exp(Vd/Vt) - exp(-Vd/Vt))
    //              (two 1N4148s anti-parallel; one conducts each polarity)
    //
    // Newton-Raphson solves for Vd per sample.
    // ========================================================================
    float clipperStage (float vin)
    {
        constexpr float R1 = 10000.0f;
        constexpr float Rf = 22000.0f;
        constexpr float Cf = 1e-9f;
        constexpr float Is = 2.52e-9f;
        constexpr float Vt = 0.02585f;

        float h = 1.0f / fs;
        float Cf_over_h = Cf / h;

        // Vd is voltage across diode pair (equals -Vout approximately)
        // We solve f(Vd) = 0 for Vd

        float Vd = -vin; // initial guess

        for (int iter = 0; iter < 10; ++iter)
        {
            // Diode current + conductance (soft-clamp to avoid overflow)
            float Vd_safe = juce::jlimit (-1.5f, 1.5f, Vd);
            float ep = std::exp (Vd_safe / Vt);
            float en = std::exp (-Vd_safe / Vt);
            float Id = Is * (ep - en);
            float dId_dVd = Is / Vt * (ep + en);

            float i_Rf = Vd / Rf;
            float i_cap = Cf_over_h * (Vd - capX);
            float i_in  = -vin / R1;

            float f = i_in + i_Rf + i_cap + Id;
            float df = 1.0f / Rf + Cf_over_h + dId_dVd;

            float dx = f / df;
            Vd -= dx;

            if (std::abs (dx) < 1e-7f) break;
        }

        capX = Vd;

        return -Vd;
    }

    // ========================================================================
    // Tone stage - bell EQ around 1-4 kHz, gain varies with Tone pot
    // ========================================================================
    float toneStage (float x)
    {
        float f0 = 1200.0f + tone * 3500.0f;
        float boost = 0.2f + tone * 1.8f;
        float Q = 0.5f + (1.0f - tone) * 1.0f;

        float w0 = 2.0f * juce::float_Pi * f0 / fs;
        float cosw = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * Q);

        float A = std::pow (10.0f, boost / 40.0f);

        // Parametric peaking EQ biquad
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
    // 6 kHz second-order Butterworth low-pass (U1b output stage roll-off)
    // ========================================================================
    float lowpassStage (float x)
    {
        float freq = 6000.0f;
        float w0 = 2.0f * juce::float_Pi * freq / fs;
        float cosw = std::cos (w0);
        float alpha = std::sin (w0) / std::sqrt (2.0f);

        float a0 = (1.0f + cosw) / 2.0f + alpha;
        float a1 = -(1.0f + cosw);
        float a2 = (1.0f + cosw) / 2.0f - alpha;
        float b0 = (1.0f + cosw) / 2.0f;
        float b1 = (1.0f + cosw) / 2.0f;
        float b2 = (1.0f + cosw) / 2.0f;

        float y = (b0 * x + b1 * lpfX1 + b2 * lpfX2 - a1 * lpfY1 - a2 * lpfY2) / a0;
        lpfX2 = lpfX1; lpfX1 = x;
        lpfY2 = lpfY1; lpfY1 = y;
        return y;
    }
};