// ============================================================================
//  TubeStage.h  -  white-box 12AX7 stages solved every sample
//
//  Two real JCM800 preamp building blocks are modelled here from the actual
//  schematic, not as a black-box waveshaper:
//
//    * GainStage     : common-cathode triode (V1a / V2a)
//                       plate load Rload, cathode Rk + bypass Ck (gives the
//                       stage its gain & tight low end), grid leak Rg, and the
//                       0.022uF interstage coupling cap Cc.
//    * CathodeFollower : low-impedance buffer (V1b) that drives the tone stack
//                       without loading it down.
//
//  Each sample we build the Modified Nodal Analysis (MNA) equations for the
//  stage, linearise the triode with its analytic Jacobian, and solve the
//  resulting small linear system with Newton-Raphson. Capacitors use a
//  trapezoidal (Tustin) companion model so they are unconditionally stable.
//
//  The triode's Kp (perveance) is auto-calibrated in prepare() so the stage
//  idles at a sensible bias current regardless of the chosen resistor values.
// ============================================================================
#pragma once
#include <cmath>
#include "Triode.h"
#include "MathUtils.h"

class TubeStage
{
public:
    enum class Mode
    {
        CommonCathode,
        CathodeFollower
    };

    Mode mode = Mode::CommonCathode;
    Triode triode;

    // ---- schematic component values (ohms, farads, volts) --------------
    float Rload = 100'000.0f; // plate load (CommonCathode) or unused
    float Rk = 2'700.0f;      // cathode resistor
    float Ck = 0.68e-6f;      // cathode bypass cap
    float Rg = 1'000'000.0f;  // grid leak
    float Cc = 0.022e-6f;     // interstage coupling cap
    float Vht = 300.0f;       // B+ high voltage

    // ---- runtime state --------------------------------------------------
    float dt = 1.0f / 176'400.0f;
    float Gc = 0.0f, Gck = 0.0f; // capacitive companion conductances
    float Gg = 0.0f, Gk = 0.0f, Gload = 0.0f;
    float Rsrc = 0.0f; // source (previous stage) output impedance
    float Gsrc = 0.0f; // 1/Rsrc (0 => ideal voltage source)

    float Vg = 0.0f, Vk = 0.0f, Vp = 0.0f; // node voltages
    float Vsnode = 0.0f;                   // source-node voltage (seen through Rsrc)
    float mVcPrev = 0.0f, mIcPrev = 0.0f;  // coupling cap companion
    float mVkPrev = 0.0f, mIckPrev = 0.0f; // cathode cap companion

    float Vg_dc = 0.0f, Vk_dc = 0.0f, Vp_dc = 0.0f; // DC operating point
    float mIsh = 0.0f, mIshk = 0.0f;                // history sources

    // ---- setup ----------------------------------------------------------
    void setSampleRate(double sampleRate)
    {
        dt = static_cast<float>(1.0 / sampleRate);
        Gc = 2.0f * Cc / dt; // Tustin companion conductance
        Gck = 2.0f * Ck / dt;
        Gg = 1.0f / Rg;
        Gk = 1.0f / Rk;
        Gload = (mode == Mode::CommonCathode) ? (1.0f / Rload) : 0.0f;
        // Default source (previous stage) output impedance. This is what
        // LIMITS grid current: when the grid is driven positive the grid
        // diode conducts and draws current back through Rsrc, which pulls
        // the source node down and soft-clamps Vgk -> the real JCM800 grind.
        // JCM800Model overrides these per stage with physically-motivated
        // values (pickup ~10k, prev plate load 100k, cathode-follower+stack
        // ~12-27k). If Rsrc is left at 0 we pick a sensible default.
        if (Rsrc < 1.0f)
            Rsrc = (mode == Mode::CommonCathode) ? 33'000.0f : 100'000.0f;
        Gsrc = 1.0f / Rsrc;
    }

    // 1-D DC idle current for a given Kp (caps open at DC).
    float solveDcIdle(float kp) const
    {
        Triode t = triode;
        t.Kp = kp;
        float Ip = 0.0008f; // initial guess (amperes)
        for (int i = 0; i < 40; ++i)
        {
            float Vk_, Vp_;
            if (mode == Mode::CommonCathode)
            {
                Vk_ = Ip * Rk;
                Vp_ = Vht - Ip * Rload;
            }
            else // cathode follower: plate pinned at Vht
            {
                Vk_ = Ip * Rk;
                Vp_ = Vht;
            }
            float Vpk = Vp_ - Vk_;
            float Vgk = (mode == Mode::CommonCathode ? 0.0f : 0.0f) - Vk_; // Vg=0 at DC (grid leak to ground)
            float Ip_est = t.plateCurrent(Vpk, Vgk);
            // derivative wrt Ip:
            float dVpk_dIp = (mode == Mode::CommonCathode) ? -(Rload + Rk) : -(Rk);
            float dVgk_dIp = -Rk;
            float dIp_dVpk, dIp_dVgk;
            t.plateJacobian(Vpk, Vgk, dIp_dVpk, dIp_dVgk);
            float dIp_dIp = dIp_dVpk * dVpk_dIp + dIp_dVgk * dVgk_dIp;
            float f = Ip_est - Ip;
            float df = dIp_dIp - 1.0f;
            if (std::fabs(df) < 1e-9f)
                break;
            Ip -= f / df;
            if (Ip < 1e-6f)
                Ip = 1e-6f;
            if (Ip > 0.05f)
                Ip = 0.05f;
        }
        return Ip;
    }

    // Calibrate Kp so the stage idles at a target current (amperes).
    void calibrateKp(float targetIdle = 0.0009f)
    {
        float lo = 1e-5f, hi = 0.05f;
        for (int i = 0; i < 36; ++i)
        {
            float mid = 0.5f * (lo + hi);
            float Ip = solveDcIdle(mid);
            if (Ip < targetIdle)
                lo = mid;
            else
                hi = mid;
        }
        triode.Kp = 0.5f * (lo + hi);
    }

    // Compute DC operating point and reset companion history (no transient).
    // NOTE: target idle current is mode-dependent and MUST stay below the
    // conduction limit of the 3/2 law for the chosen mu / Rk / Rload / Vht.
    //   * CommonCathode (Rk=2.7k, Rload=100k, mu=100, Vht=300):
    //       max idle ~ Vht/mu / (Rk + (Rload+Rk)/mu) ~ 0.805mA  -> use 0.6mA
    //   * CathodeFollower (Rk=100k, mu=100, Vht=300):
    //       max idle ~ Vht/mu / (Rk + Rk/mu) ~ 0.0297mA          -> use 0.02mA
    // Picking an unreachable target pushes Kp to its upper bound and leaves
    // the stage in cutoff (harsh, "plastic" clipping).
    void prepare()
    {
        calibrateKp(mode == Mode::CathodeFollower ? 0.00002f : 0.0006f);
        // Grid conduction: when the grid is driven positive w.r.t. the cathode
        // the tube "grid diode" conducts and clamps Vgk near 0. Without this the
        // stage lets the grid swing to tens of volts, slamming the plate into
        // hard rail clamping -> the harsh, "plastic" distortion we are removing.
        // ~1kOhm effective grid resistance when conducting is typical for a 12AX7.
        triode.Ggrid = 0.001f;
        triode.Vth = 0.0f;
        float Ip = solveDcIdle(triode.Kp);
        Vk_dc = Ip * Rk;
        Vg_dc = 0.0f;
        Vp_dc = (mode == Mode::CommonCathode) ? (Vht - Ip * Rload) : Vht;
        Vg = Vg_dc;
        Vk = Vk_dc;
        Vp = Vp_dc;
        Vsnode = 0.0f;
        mVcPrev = 0.0f;
        mIcPrev = 0.0f; // grid at 0, source at 0 -> 0
        mVkPrev = Vk_dc;
        mIckPrev = 0.0f; // cathode cap holds DC, no AC current
        mIsh = 0.0f;
        mIshk = 0.0f;
    }

    // ---- per-sample processing (Vs = AC source voltage at coupling cap) --
    // Returns AC plate voltage (CommonCathode) or AC cathode (Follower).
    //
    // The source is NOT an ideal voltage source. It is a Thévenin source
    // (ideal Vs in series with Rsrc) seen at the coupling-cap input. That
    // series resistance is what physically LIMITS grid current: when the
    // grid goes positive the grid diode conducts and pulls current back
    // through Rsrc, which yanks the source node down and soft-clamps Vgk
    // near 0. The classic Marshall grind comes from exactly this, instead
    // of the grid being pinned to Vs by an ideal source (which produces a
    // hard, "plastic" clip). MNA nodes:
    //   CommonCathode : 0=source, 1=grid, 2=cathode, 3=plate
    //   CathodeFollower: 0=source, 1=grid, 2=cathode   (plate pinned at Vht)
    float process(float Vs)
    {
        // history (Tustin) sources computed from previous sample.
        // Coupling cap spans (source <-> grid); Vc = Vg - Vsnode.
        mIsh = -(Gc * mVcPrev + mIcPrev);
        mIshk = -(Gck * mVkPrev + mIckPrev);

        const float GsrcLoc = (Rsrc > 1.0f) ? (1.0f / Rsrc) : 1000.0f;

        for (int iter = 0; iter < 12; ++iter)
        {
            float Vpk = Vp - Vk;
            float Vgk = Vg - Vk;
            float Ip = triode.plateCurrent(Vpk, Vgk);
            float dIp_dVpk = 0.0f, dIp_dVgk = 0.0f;
            triode.plateJacobian(Vpk, Vgk, dIp_dVpk, dIp_dVgk);
            float Ig = triode.gridCurrent(Vgk);
            float c = triode.gridJacobian(Vgk); // dIg/dVgk

            // shorthand derivatives of the plate current
            const float a = dIp_dVpk; // dIp/dVpk
            const float b = dIp_dVgk; // dIp/dVgk
            // dIp/dVk = -a - b ;  dIg/dVk = -c

            // ---- node residuals (set to 0 at the solution) ----
            // F0: source node. Currents leaving = cap(source->grid) + source branch.
            float F0 = -Gc * Vg + (Gc + GsrcLoc) * Vsnode - mIsh - GsrcLoc * Vs;
            // F1: grid node. leak + cap(grid->source) + grid diode current.
            float F1 = (Gg + Gc) * Vg - Gc * Vsnode + Ig + mIsh;
            // F2: cathode node.
            float F2 = (Gk + Gck) * Vk - Ip - Ig + mIshk;

            float x[4];
            if (mode == Mode::CommonCathode)
            {
                float F3 = Gload * Vp + Ip - Gload * Vht;
                float J[4][4] = {{0}};
                J[0][0] = (Gc + GsrcLoc);
                J[0][1] = -Gc;
                J[0][2] = 0.0f;
                J[0][3] = 0.0f;
                J[1][0] = -Gc;
                J[1][1] = (Gg + Gc) + c;
                J[1][2] = -c;
                J[1][3] = 0.0f;
                J[2][0] = 0.0f;
                J[2][1] = -b - c;
                J[2][2] = (Gk + Gck) + a + b + c;
                J[2][3] = -a;
                J[3][0] = 0.0f;
                J[3][1] = b;
                J[3][2] = (-a - b);
                J[3][3] = Gload + a;
                float rr[4] = {-F0, -F1, -F2, -F3};
                if (!solve4(J, rr, x))
                    break;
            }
            else
            {
                // CathodeFollower: Vp fixed at Vht, so 3 unknowns.
                float J[3][3] = {{0}};
                J[0][0] = (Gc + GsrcLoc);
                J[0][1] = -Gc;
                J[0][2] = 0.0f;
                J[1][0] = -Gc;
                J[1][1] = (Gg + Gc) + c;
                J[1][2] = -c;
                J[2][0] = 0.0f;
                J[2][1] = -b - c;
                J[2][2] = (Gk + Gck) + a + b + c;
                float rr[3] = {-F0, -F1, -F2};
                float x3[3];
                if (!solve3(J, rr, x3))
                    break;
                x[0] = x3[0];
                x[1] = x3[1];
                x[2] = x3[2];
                x[3] = 0.0f;
            }

            Vsnode += x[0];
            Vg += x[1];
            Vk += x[2];
            if (mode == Mode::CommonCathode)
                Vp += x[3];

            // safety clamps (physical bounds)
            Vsnode = clampf(Vsnode, -300.0f, Vht + 100.0f);
            Vg = clampf(Vg, -50.0f, 50.0f);
            Vk = clampf(Vk, -5.0f, 80.0f);
            Vp = clampf(Vp, -50.0f, Vht + 80.0f);

            if (std::fabs(x[0]) < 1e-6f && std::fabs(x[1]) < 1e-6f && std::fabs(x[2]) < 1e-6f &&
                (mode == Mode::CathodeFollower || std::fabs(x[3]) < 1e-6f))
                break;
        }

        // update companion history (Vc = Vg - Vsnode)
        mVcPrev = Vg - Vsnode;
        mIcPrev = Gc * (Vg - Vsnode) + mIsh;
        mVkPrev = Vk;
        mIckPrev = Gck * Vk + mIshk;

        if (mode == Mode::CommonCathode)
            return Vp - Vp_dc;
        else
            return Vk - Vk_dc;
    }
};
