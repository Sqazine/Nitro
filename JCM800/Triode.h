// ============================================================================
//  Triode.h  -  physically-based triode (12AX7 / ECC83) model
//
//  Uses the classic "three-halves power law" (Child-Langmuir) with the
//  amplification factor mu, which is the physical foundation every real tube
//  equation (Koren, Dempwolf, Yeh-Smith) builds on:
//
//        Ip = Kp * (Vgk + Vpk/mu)^1.5          (for the argument > 0)
//
//  An analytic Jacobian is supplied (only one sqrt per evaluation) so the
//  per-sample Newton-Raphson circuit solver stays cheap. Grid conduction is
//  modelled as a piecewise-linear "diode" that loads the grid and produces the
//  characteristic JCM800 grind when the stage is driven hard.
// ============================================================================
#pragma once
#include <cmath>

struct Triode
{
    // ---- tunable physical parameters ------------------------------------
    float mu = 100.0f;   // amplification factor (12AX7 ~ 100)
    float Kp = 0.00060f; // perveance-like scale (calibrated for ~1-2mA idle)
    float Vth = 0.0f;    // grid conduction threshold (volts, grid vs cathode)
    float Ggrid = 0.0f;  // grid "diode" conductance when conducting (1/ohm)

    // ---- plate current (amperes, but we work in consistent volt/amp) ----
    inline float plateCurrent(float Vpk, float Vgk) const noexcept
    {
        const float E1 = Vgk + Vpk / mu;
        if (E1 <= 0.0f)
            return 0.0f;
        const float s = std::sqrt(E1);
        return Kp * E1 * s; // Kp * E1^1.5
    }

    // ---- analytic Jacobian of plate current -----------------------------
    // dIp/dVpk and dIp/dVgk (the two independent variables).
    inline void plateJacobian(float Vpk, float Vgk,
                              float &dIp_dVpk, float &dIp_dVgk) const noexcept
    {
        const float E1 = Vgk + Vpk / mu;
        if (E1 <= 0.0f)
        {
            dIp_dVpk = 0.0f;
            dIp_dVgk = 0.0f;
            return;
        }
        const float s = std::sqrt(E1);
        dIp_dVgk = 1.5f * Kp * s; // d/dE1  (dE1/dVgk = 1)
        dIp_dVpk = dIp_dVgk / mu; // dE1/dVpk = 1/mu
    }

    // ---- grid current (conduction when grid goes positive vs cathode) ----
    inline float gridCurrent(float Vgk) const noexcept
    {
        if (Vgk <= Vth)
            return 0.0f;
        return Ggrid * (Vgk - Vth);
    }
    inline float gridJacobian(float Vgk) const noexcept
    {
        if (Vgk <= Vth)
            return 0.0f;
        return Ggrid;
    }
};
