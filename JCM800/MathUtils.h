// ============================================================================
//  math_utils.h  -  small fixed-size linear algebra + helpers (no JUCE deps)
//  White-box JCM800 physical model. C++17, header-only, portable.
// ============================================================================
#pragma once
#include <cmath>
#include <algorithm>

    // ---- scalar helpers -----------------------------------------------------
    inline float clampf(float x, float lo, float hi) noexcept
    {
        return (x < lo) ? lo : (x > hi) ? hi : x;
    }

    // ---- 3x3 linear solve (Gaussian elimination w/ partial pivoting) --------
    // Solves  A * x = b   for a 3x3 system. A is row-major [3][3].
    // Returns false if singular.
    inline bool solve3(const float A[3][3], const float b[3], float x[3]) noexcept
    {
        float M[3][3] = { {A[0][0],A[0][1],A[0][2]},
                          {A[1][0],A[1][1],A[1][2]},
                          {A[2][0],A[2][1],A[2][2]} };
        float c[3]   = { b[0], b[1], b[2] };

        for (int col = 0; col < 3; ++col)
        {
            // partial pivot
            int piv = col;
            float best = std::fabs(M[col][col]);
            for (int r = col + 1; r < 3; ++r)
            {
                float v = std::fabs(M[r][col]);
                if (v > best) { best = v; piv = r; }
            }
            if (best < 1e-12f) return false; // singular
            if (piv != col)
            {
                for (int k = 0; k < 3; ++k) std::swap(M[col][k], M[piv][k]);
                std::swap(c[col], c[piv]);
            }
            // eliminate
            float diag = M[col][col];
            for (int r = col + 1; r < 3; ++r)
            {
                float f = M[r][col] / diag;
                if (f == 0.0f) continue;
                for (int k = col; k < 3; ++k) M[r][k] -= f * M[col][k];
                c[r] -= f * c[col];
            }
        }
        // back-substitution
        for (int i = 2; i >= 0; --i)
        {
            float s = c[i];
            for (int k = i + 1; k < 3; ++k) s -= M[i][k] * x[k];
            x[i] = s / M[i][i];
        }
        return true;
    }

    // ---- 2x2 linear solve ---------------------------------------------------
    inline bool solve2(const float A[2][2], const float b[2], float x[2]) noexcept
    {
        float det = A[0][0]*A[1][1] - A[0][1]*A[1][0];
        if (std::fabs(det) < 1e-12f) return false;
        x[0] = ( b[0]*A[1][1] - A[0][1]*b[1]) / det;
        x[1] = ( A[0][0]*b[1] - b[0]*A[1][0]) / det;
        return true;
    }

    // ---- 4x4 linear solve (Gaussian elimination w/ partial pivoting) -------
    inline bool solve4(const float A[4][4], const float b[4], float x[4]) noexcept
    {
        float M[4][4] = { {A[0][0],A[0][1],A[0][2],A[0][3]},
                          {A[1][0],A[1][1],A[1][2],A[1][3]},
                          {A[2][0],A[2][1],A[2][2],A[2][3]},
                          {A[3][0],A[3][1],A[3][2],A[3][3]} };
        float c[4]   = { b[0], b[1], b[2], b[3] };
        for (int col = 0; col < 4; ++col)
        {
            int piv = col;
            float best = std::fabs(M[col][col]);
            for (int r = col + 1; r < 4; ++r)
            {
                float v = std::fabs(M[r][col]);
                if (v > best) { best = v; piv = r; }
            }
            if (best < 1e-12f) return false;
            if (piv != col)
            {
                for (int k = 0; k < 4; ++k) std::swap(M[col][k], M[piv][k]);
                std::swap(c[col], c[piv]);
            }
            float diag = M[col][col];
            for (int r = col + 1; r < 4; ++r)
            {
                float f = M[r][col] / diag;
                if (f == 0.0f) continue;
                for (int k = col; k < 4; ++k) M[r][k] -= f * M[col][k];
                c[r] -= f * c[col];
            }
        }
        for (int i = 3; i >= 0; --i)
        {
            float s = c[i];
            for (int k = i + 1; k < 4; ++k) s -= M[i][k] * x[k];
            x[i] = s / M[i][i];
        }
        return true;
    }
