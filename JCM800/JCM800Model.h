// ============================================================================
//  JCM800Model.h  -  complete white-box Marshall JCM800 (2204/2203) chain
//
//  Signal path (modelled from the real schematic):
//     guitar -> V1a gain stage -> V1b cathode follower
//           -> [470pF bright cap + GAIN pot] -> V2a second gain stage
//           -> Marshall TMB tone stack (Bass/Mid/Treble)
//           -> V3 long-tailed-pair phase inverter
//           -> EL34 push-pull power amp (sag + presence)
//           -> 4x12 cabinet convolution
//
//  The nonlinear stages (triodes) are solved every sample with Newton-Raphson;
//  the tone stack is a real passive R/C network; the cabinet is convolution.
//  processAmp() runs at the oversampled rate, processCab() at the base rate.
// ============================================================================
#pragma once
#include "TubeStage.h"
#include "PhaseInverter.h"
#include "ToneStack.h"
#include "PowerAmp.h"
#include "Cabinet.h"

class JCM800Model
{
public:
    TubeStage mStage1, mStage2;
    TubeStage mCF; // cathode follower (mode set in prepare)
    ToneStack mTone;
    PhaseInverter mLTP;
    PowerAmp mPower;
    Cabinet mCab;

    float mGain = 0.5f, mBass = 0.5f, mMid = 0.5f, mTreble = 0.5f;
    float mPresence = 0.5f, mVolume = 0.8f;

    // level staging (tunable)
    float mInputTrim = 0.8f;
    float mInterstageScale = 0.40f; // drive from CF into V2a (restored headroom: clean at low gain, crunch when cranked)
    float mOutGain = 1.0f;

    float mBrightA = 0.0f, mBrightLp = 0.0f;

    // DEBUG probes (last sample magnitudes)
    float dbg_s1 = 0, dbg_cf = 0, dbg_in2 = 0, dbg_s2 = 0, dbg_ts = 0, dbg_a = 0, dbg_pa = 0;
    float dbg_Vgk2 = 0, dbg_Vsnode2 = 0; // grid/cathode of 2nd gain stage (grid conduction probe)
    float dbg_maxVgk = -1e9f;            // max Vgk2 seen (proves grid conduction engaged)
    float dbg_stage1Kp = 0, dbg_Kp = 0;
    float dbg_paSag = 1.0f; // power-amp HT sag factor (dynamic compression probe)

    void prepare(double sampleRate, int oversample)
    {
        double srAmp = sampleRate * oversample;

        // V1a  (CommonCathode gain stage, driven by the guitar pickup)
        mStage1.mode = TubeStage::Mode::CommonCathode;
        mStage1.Rload = 100'000.0f;
        mStage1.Rk = 2'700.0f;
        mStage1.Ck = 0.68e-6f;
        mStage1.Rg = 1'000'000.0f;
        mStage1.Cc = 0.022e-6f;
        mStage1.Vht = 300.0f;
        mStage1.Rsrc = 10'000.0f; // pickup + cable output impedance
        mStage1.setSampleRate(srAmp);
        mStage1.prepare();

        // V1b cathode follower (driven by V1a plate load = 100k)
        mCF.mode = TubeStage::Mode::CathodeFollower;
        mCF.Rk = 100'000.0f;
        mCF.Ck = 0.0f; // cathode load = resistor only
        mCF.Rg = 1'000'000.0f;
        mCF.Cc = 0.022e-6f;
        mCF.Vht = 300.0f;
        mCF.Rsrc = 100'000.0f; // previous stage plate load
        mCF.setSampleRate(srAmp);
        mCF.prepare();

        // V2a second gain stage (driven by cathode follower through tone stack)
        mStage2.mode = TubeStage::Mode::CommonCathode;
        mStage2.Rload = 100'000.0f;
        mStage2.Rk = 2'700.0f;
        mStage2.Ck = 0.68e-6f;
        mStage2.Rg = 1'000'000.0f;
        mStage2.Cc = 0.022e-6f;
        mStage2.Vht = 300.0f;
        mStage2.Rsrc = 10'000.0f; // CF (low Z) + tone-stack Thevenin Z
        mStage2.setSampleRate(srAmp);
        mStage2.prepare();

        mTone.setSampleRate(srAmp);
        mTone.setPositions(mBass, mMid, mTreble);

        mLTP.setSampleRate(srAmp);
        mLTP.prepare();
        mPower.setSampleRate(srAmp);
        mPower.drive = 0.015f;
        mPower.presence = mPresence;

        mCab.setSampleRate(sampleRate); // cabinet at base rate

        // bright cap (470pF) one-pole, bright at low gain
        float fc = 339.0f;
        mBrightA = 1.0f - std::exp(-2.0f * 3.14159265359f * fc / static_cast<float>(srAmp));
        mBrightLp = 0.0f;

        reset();
    }

    void setParams(float gain, float bass, float mid, float treble, float presence, float volume)
    {
        mGain = gain;
        mBass = bass;
        mMid = mid;
        mTreble = treble;
        mPresence = presence;
        mVolume = volume;
        mTone.setPositions(bass, mid, treble);
        mPower.presence = presence;
    }

    // called at the oversampled rate
    float processAmp(float x)
    {
        float s1 = mStage1.process(x * mInputTrim); // V1a AC plate
        float cf = mCF.process(s1);                 // V1b cathode follower

        // 470pF bright cap across the GAIN pot + gain pot division.
        // The GAIN control does two things, like the real JCM800 preamp
        // volume: it blends the bright (high-passed) path vs the full-CF
        // path for tone, AND it scales the drive level into V2a so cranking
        // it pushes the second gain stage into grid-conduction distortion.
        mBrightLp += mBrightA * (cf - mBrightLp);
        float hp = cf - mBrightLp;
        float driveFactor = 0.05f + 0.95f * mGain;
        // 0.05 floor (not 0) so a dimed-down gain still has a little life,
        // but the 1.95*gain term restores real headroom: low gain stays
        // clean (V2a does NOT grid-conduct), high gain grinds. This gives
        // the JCM800 its signature clean->crunch dynamic range instead of
        // one uniform "always crunching" (plastic) character.
        float stage2Drive = mInterstageScale * (0.05f + 1.95f * mGain);
        float into2 = (driveFactor * cf + (1.0f - driveFactor) * hp) * stage2Drive;

        float s2 = mStage2.process(into2); // V2a AC plate
        float ts = mTone.process(s2);      // tone stack
        float a, b;
        mLTP.process(ts, a, b);          // phase inverter
        float pa = mPower.process(a, b); // power amp

        dbg_s1 = s1;
        dbg_cf = cf;
        dbg_in2 = into2;
        dbg_s2 = s2;
        dbg_ts = ts;
        dbg_a = a;
        dbg_pa = pa;
        dbg_stage1Kp = mStage1.triode.Kp;
        dbg_Kp = mStage2.triode.Kp;
        dbg_Vgk2 = mStage2.Vg - mStage2.Vk; // >0 => grid is conducting (soft clip)
        dbg_Vsnode2 = mStage2.Vsnode;
        if (dbg_Vgk2 > dbg_maxVgk)
            dbg_maxVgk = dbg_Vgk2;
        dbg_paSag = mPower.dbg_sag;

        return pa * mOutGain;
    }

    // called at the base rate
    float processCab(float x)
    {
        return mCab.process(x) * mVolume;
    }

    void reset()
    {
        mStage1.Vg = mStage1.Vg_dc;
        mStage1.Vk = mStage1.Vk_dc;
        mStage1.Vp = mStage1.Vp_dc;
        mCF.Vg = mCF.Vg_dc;
        mCF.Vk = mCF.Vk_dc;
        mCF.Vp = mCF.Vp_dc;
        mStage2.Vg = mStage2.Vg_dc;
        mStage2.Vk = mStage2.Vk_dc;
        mStage2.Vp = mStage2.Vp_dc;
        mLTP.Vk = mLTP.Vk_dc;
        mLTP.Vp1 = mLTP.Vp_dc;
        mLTP.Vp2 = mLTP.Vp_dc;
        mLTP.Vg1 = 0.0f;
        mTone.reset();
        mPower.reset();
        mCab.reset();
        mBrightLp = 0.0f;
        dbg_maxVgk = -1e9f;
    }
};
