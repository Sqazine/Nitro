// ============================================================================
//  PhaseInverter.h  -  white-box long-tailed-pair (LTP) phase inverter (V3)
//
//  The JCM800 (like every classic Marshall) uses a differential pair of
//  triodes sharing a tail resistor as the phase inverter that drives the two
//  power tubes in anti-phase. We solve it every sample with Newton-Raphson
//  (3 nodes: shared cathode Vk, plate 1, plate 2). The input is AC-coupled to
//  grid 1; grid 2 sits at fixed bias. The differential output is what feeds
//  the push-pull power amp.
// ============================================================================
#pragma once
#include <cmath>
#include "Triode.h"
#include "MathUtils.h"


    class PhaseInverter
    {
    public:
        Triode triode;

        float Rload = 100'000.0f;   // each plate load
        float Rtail = 4'700.0f;     // shared tail resistor
        float Ctail = 100.0e-6f;    // tail bypass cap (large -> near current source)
        float Rg1   = 1'000'000.0f; // grid1 leak
        float Cc    = 0.022e-6f;    // input coupling cap (grid1)
        float Vht   = 300.0f;

        float dt = 1.0f / 176'400.0f;
        float Gc = 0.0f, Gg1 = 0.0f, Gtail = 0.0f, Gctail = 0.0f;

        float Vk = 0.0f, Vp1 = 0.0f, Vp2 = 0.0f, Vg1 = 0.0f;
        float mVcPrev1 = 0.0f, mIcPrev1 = 0.0f;     // grid1 coupling companion
        float mVkPrevT = 0.0f, mIckPrevT = 0.0f;     // tail cap companion
        float Vp_dc = 0.0f, Vk_dc = 0.0f;
        float mIsh1 = 0.0f, mIshT = 0.0f;

        void setSampleRate(double sampleRate)
        {
            dt = static_cast<float>(1.0 / sampleRate);
            Gc = 2.0f * Cc / dt;
            Gg1 = 1.0f / Rg1;
            Gtail = 1.0f / Rtail;
            Gctail = 2.0f * Ctail / dt;
        }

        float solveDcIdle(float kp) const
        {
            Triode t = triode; t.Kp = kp;
            float Ip = 0.0003f;
            for (int i = 0; i < 40; ++i)
            {
                float Vk_ = 2.0f * Ip * Rtail;
                float Vp_ = Vht - Ip * Rload;
                float Vpk = Vp_ - Vk_;
                float Vgk = -Vk_;          // grids at 0 (leak to ground)
                float Ip_est = t.plateCurrent(Vpk, Vgk);
                float dVpk_dIp = -(Rload + 2.0f * Rtail);
                float dVgk_dIp = -2.0f * Rtail;
                float dIp_dVpk, dIp_dVgk;
                t.plateJacobian(Vpk, Vgk, dIp_dVpk, dIp_dVgk);
                float dIp_dIp = dIp_dVpk * dVpk_dIp + dIp_dVgk * dVgk_dIp;
                float f = Ip_est - Ip;
                float df = dIp_dIp - 1.0f;
                if (std::fabs(df) < 1e-9f) break;
                Ip -= f / df;
                Ip = clampf(Ip, 1e-6f, 0.02f);
            }
            return Ip;
        }

        void calibrateKp(float targetIdle = 0.0003f)
        {
            float lo = 1e-5f, hi = 0.05f;
            for (int i = 0; i < 36; ++i)
            {
                float mid = 0.5f * (lo + hi);
                float Ip = solveDcIdle(mid);
                if (Ip < targetIdle) lo = mid; else hi = mid;
            }
            triode.Kp = 0.5f * (lo + hi);
        }

        void prepare()
        {
            calibrateKp();
            // grid conduction clamp (see TubeStage::prepare for rationale)
            triode.Ggrid = 0.001f;
            triode.Vth  = 0.0f;
            float Ip = solveDcIdle(triode.Kp);
            Vk_dc = 2.0f * Ip * Rtail;
            Vp_dc = Vht - Ip * Rload;
            Vk = Vk_dc; Vp1 = Vp_dc; Vp2 = Vp_dc; Vg1 = 0.0f;
            mVcPrev1 = 0.0f; mIcPrev1 = 0.0f;
            mVkPrevT = Vk_dc; mIckPrevT = 0.0f;
            mIsh1 = 0.0f; mIshT = 0.0f;
        }

        // Vs = AC input (tone stack output). Returns differential pair (a, b).
        void process(float Vs, float& outA, float& outB)
        {
            // grid1 AC coupling (closed form 1-pole high-pass)
            mIsh1 = -(Gc * mVcPrev1 + mIcPrev1);
            Vg1 = (Gc * Vs - mIsh1) / (Gg1 + Gc);
            mVcPrev1 = Vg1 - Vs;
            mIcPrev1 = Gc * (Vg1 - Vs) + mIsh1;

            // grid conduction clamps positive grid excursions (Vgk ~ 0): without
            // this the tone-stack output (tens of volts) would drive grid1 to
            // tens of volts and slam the phase-inverter plates into hard rail
            // clamping, freezing the output regardless of the gain control.
            Vg1 = clampf(Vg1, -100.0f, Vk_dc + 0.7f);

            // tail cap history
            mIshT = -(Gctail * mVkPrevT + mIckPrevT);

            for (int iter = 0; iter < 10; ++iter)
            {
                float Vpk1 = Vp1 - Vk, Vgk1 = Vg1 - Vk;
                float Vpk2 = Vp2 - Vk, Vgk2 = 0.0f - Vk;

                float Ip1 = triode.plateCurrent(Vpk1, Vgk1);
                float Ip2 = triode.plateCurrent(Vpk2, Vgk2);
                float dIp1_dVpk, dIp1_dVgk, dIp2_dVpk, dIp2_dVgk;
                triode.plateJacobian(Vpk1, Vgk1, dIp1_dVpk, dIp1_dVgk);
                triode.plateJacobian(Vpk2, Vgk2, dIp2_dVpk, dIp2_dVgk);

                float dIp1_dVk = -dIp1_dVpk - dIp1_dVgk;
                float dIp2_dVk = -dIp2_dVpk - dIp2_dVgk;

                float Fk  = (Gtail + Gctail) * Vk - Ip1 - Ip2 + mIshT;
                float Fp1 = GloadSafe() * Vp1 + Ip1 - GloadSafe() * Vht;
                float Fp2 = GloadSafe() * Vp2 + Ip2 - GloadSafe() * Vht;

                float J[3][3];
                J[0][0] = (Gtail + Gctail) - dIp1_dVk - dIp2_dVk;
                J[0][1] = -dIp1_dVpk;
                J[0][2] = -dIp2_dVpk;
                J[1][0] = dIp1_dVk;
                J[1][1] = GloadSafe() + dIp1_dVpk;
                J[1][2] = 0.0f;
                J[2][0] = dIp2_dVk;
                J[2][1] = 0.0f;
                J[2][2] = GloadSafe() + dIp2_dVpk;

                float b[3] = { -Fk, -Fp1, -Fp2 };
                float x[3];
                if (!solve3(J, b, x)) break;
                Vk  += x[0];
                Vp1 += x[1];
                Vp2 += x[2];

                Vk  = clampf(Vk,  -5.0f, 60.0f);
                Vp1 = clampf(Vp1, -20.0f, Vht + 50.0f);
                Vp2 = clampf(Vp2, -20.0f, Vht + 50.0f);

                if (std::fabs(x[0]) < 1e-5f && std::fabs(x[1]) < 1e-5f && std::fabs(x[2]) < 1e-5f)
                    break;
            }

            mVkPrevT = Vk;
            mIckPrevT = Gctail * Vk + mIshT;

            outA = Vp1 - Vp_dc;
            outB = Vp2 - Vp_dc;
        }

    private:
        inline float GloadSafe() const { return 1.0f / Rload; }
    };
