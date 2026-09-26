// ============================================================================
//  PowerAmp.h  -  white-box EL34 push-pull power amp (JCM800 2204/2203)
//
//  A real Marshall power amp is defined by three physical behaviours, all of
//  which we model here (no black-box waveshaper):
//
//    1. Push-pull even-order cancellation. The two EL34s are driven in
//       anti-phase by the phase inverter; output = g(a) - g(b). Because the
//       transfer g() is odd-symmetric, the even harmonics cancel. This is the
//       real reason a Marshall is "odd-harmonic crunch", not "Fender bloom".
//
//    2. Power-tube soft saturation. Each EL34 follows a smooth, odd-symmetric
//       saturating law (rational knee). The knee sits at a realistic grid
//       drive so the stage stays clean at bedroom levels and grinds when the
//       LTP shoves it hard (cranked) -- exactly like the real amp.
//
//    3. Supply (HT) sag. The rectified load current drags the B+ down under
//       transients. We track an envelope of the (anti-phase) grid drives with
//       a FAST attack / SLOW release and reduce the effective drive -> the
//       characteristic dynamic compression and low-end "bloom". The envelope
//       coefficients are derived from dt, so sag is SAMPLE-RATE INDEPENDENT
//       (it was not before: at 4x oversampling the old fixed coefficient made
//       the sag ~4x too fast and pinned it to a static gain).
//
//  Presence is a high-shelf in the (real) negative-feedback path.
// ============================================================================
#pragma once
#include <cmath>

class PowerAmp
{
public:
    float drive = 0.015f;  // power-amp grid sensitivity (scales LTP volts)
    float sag = 0.35f;     // HT sag / compression amount (0..1)
    float presence = 0.5f; // 0..1 high-shelf in NFB
    float master = 0.5f;   // output level
    float dbg_sag = 1.0f;  // last effective HT factor (debug probe)

    void setSampleRate(double sr)
    {
        mSr = static_cast<float>(sr);

        // presence high-shelf cutoff ~ 2.5 kHz
        float fc = 2500.0f;
        float x = std::exp(-2.0f * 3.14159265359f * fc / mSr);
        mA = x;
        mB = 1.0f - x;

        // sag envelope: fast attack (~2 ms), slow release (~60 ms). Derived
        // from dt so the time constant is identical at 44.1k, 176.4k, ...
        float fa = std::exp(-1.0f / (0.002f * mSr)); // ~2 ms attack
        float fr = std::exp(-1.0f / (0.060f * mSr)); // ~60 ms release (bloom)
        mAtk = 1.0f - fa;
        mRel = 1.0f - fr;
    }

    float process(float a, float b)
    {
        // envelope of the rectified push-pull load (anti-phase grids)
        float inst = std::fabs(a) + std::fabs(b);
        float env = (inst > mEnv)
                        ? (mEnv + mAtk * (inst - mEnv))  // attack
                        : (mEnv + mRel * (inst - mEnv)); // release
        mEnv = env;

        // map the (tens..~150 V) LTP swing to ~0..1 so sag engages
        // progressively: barely compresses clean tones, digs in when cranked.
        float sagF = 1.0f - sag * clamp01(env * mEnvNorm);
        dbg_sag = sagF;

        float d = drive * sagF;

        // anti-phase odd-symmetric soft clip -> even harmonics cancel
        float ga = tube(d * a);
        float gb = tube(d * b);
        float out = ga - gb;

        // presence: high shelf boost in the NFB path
        float hp = mB * (out - mHpPrev) + mA * mHpPrev; // 1-pole high-pass
        mHpPrev = hp;
        out = out + presence * 1.4f * hp;

        return out * master;
    }

    void reset()
    {
        mEnv = 0.0f;
        mHpPrev = 0.0f;
        dbg_sag = 1.0f;
    }

private:
    float mSr = 44100.0f;
    float mA = 0.0f, mB = 0.0f;
    float mAtk = 0.0f, mRel = 0.0f;
    float mEnv = 0.0f, mHpPrev = 0.0f;
    float mEnvNorm = 0.005f; // envelope-volts -> 0..1 sag range

    static float clamp01(float x) { return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f
                                                                          : x; }

    // smooth, odd-symmetric, tube-like knee: asymptotes to +/-1
    static float tube(float z) { return z / std::sqrt(1.0f + z * z); }
};
