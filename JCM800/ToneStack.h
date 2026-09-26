// ============================================================================
//  ToneStack.h  -  Marshall TMB passive tone stack (white-box)
//
//  Real JCM800 tone stack component values are dropped into an actual coupled
//  passive R/C network and integrated with the trapezoidal (Tustin) rule. This
//  is NOT three independent peaking filters - it is the real Bass/Mid/Treble
//  network (33k input, Bass 1M, Mid 25k, Treble 250k, three 0.022uF caps), so
//  the familiar "scooped mids" interaction between the bands emerges naturally.
//
//  Node map (5 internal nodes, ground = reference):
//    0 = T   : top of the stack, driven by the 33k from the previous stage
//    1 = M   : mid node (treble-pot bottom, bass-pot top, mid-cap top)
//    2 = OUT : bass-pot WIPER  -> the tone-stack output
//    3 = Bb  : bass-pot bottom (bass cap C_B to ground lives here)
//    4 = Mj  : mid-pot junction (mid cap C_M bottom -> mid pot Rm -> ground)
//
//  Why 5 nodes: the mid control is a 22nF cap in SERIES with the 25k mid pot
//  to ground, which needs its own internal junction node, and the bass pot is
//  modelled as a real 3-terminal pot (top / wiper / bottom) so the output is
//  taken from the wiper and is NOT shorted to ground by the bass cap at HF.
//
//  Resulting behaviour (verified by the self-test frequency sweep):
//    * Treble cap C_T bypasses the stack from input to output at HF  -> treble
//      pot sets how much the resistive path loads that bypass (treble up = air).
//    * Mid pot Rm dips the mids via C_M to ground at mid frequencies.
//    * Bass pot sets the low-frequency shelf level (bass up = more low end).
// ============================================================================
#pragma once
#include <cmath>
#include <algorithm>

// --- generic fixed-size passive R/C network solver (Tustin / trapezoidal) -
template <int N>
struct LinearNet
{
    float G[N][N] = {}; // conductance matrix (nodal)
    float C[N][N] = {}; // capacitance matrix
    float Bsrc[N] = {}; // input injection (Norton current per unit u)
    float A[N][N] = {}; // discrete state matrix  v[n] = A v[n-1] + b*(u[n]+u[n-1])
    float bvec[N] = {};
    float Minv[N][N] = {};
    float v[N] = {}; // state (node voltages)
    int mOutNode = N - 1;

    void reset()
    {
        for (int i = 0; i < N; ++i)
            v[i] = 0.0f;
    }

    void invert(const float m[N][N], float out[N][N]) const
    {
        float a[N][N];
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                a[i][j] = m[i][j];
                out[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        for (int col = 0; col < N; ++col)
        {
            int piv = col;
            float best = std::fabs(a[col][col]);
            for (int r = col + 1; r < N; ++r)
            {
                float val = std::fabs(a[r][col]);
                if (val > best)
                {
                    best = val;
                    piv = r;
                }
            }
            if (piv != col)
            {
                for (int k = 0; k < N; ++k)
                {
                    std::swap(a[col][k], a[piv][k]);
                    std::swap(out[col][k], out[piv][k]);
                }
            }
            float d = a[col][col];
            if (std::fabs(d) < 1e-12f)
                d = (d < 0 ? -1e-12f : 1e-12f);
            float inv = 1.0f / d;
            for (int k = 0; k < N; ++k)
            {
                a[col][k] *= inv;
                out[col][k] *= inv;
            }
            for (int r = 0; r < N; ++r)
            {
                if (r == col)
                    continue;
                float f = a[r][col];
                if (f == 0.0f)
                    continue;
                for (int k = 0; k < N; ++k)
                {
                    a[r][k] -= f * a[col][k];
                    out[r][k] -= f * out[col][k];
                }
            }
        }
    }

    void discretize(float dt)
    {
        float M[N][N], K[N][N];
        const float h = 0.5f * dt;
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                M[i][j] = C[i][j] + h * G[i][j];
                K[i][j] = C[i][j] - h * G[i][j];
            }
        float Minv[N][N];
        invert(M, Minv);
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                this->Minv[i][j] = Minv[i][j];
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                A[i][j] = 0.0f;
                for (int k = 0; k < N; ++k)
                    A[i][j] += Minv[i][k] * K[k][j];
            }
        for (int i = 0; i < N; ++i)
        {
            bvec[i] = 0.0f;
            for (int k = 0; k < N; ++k)
                bvec[i] += Minv[i][k] * (h * Bsrc[k]);
        }
    }

    float process(float u)
    {
        float vnew[N];
        for (int i = 0; i < N; ++i)
        {
            vnew[i] = bvec[i] * (u + mUprev);
            for (int j = 0; j < N; ++j)
                vnew[i] += A[i][j] * v[j];
        }
        for (int i = 0; i < N; ++i)
            v[i] = vnew[i];
        mUprev = u;
        return v[mOutNode];
    }

private:
    float mUprev = 0.0f;
};

class ToneStack
{
public:
    // pot positions 0..1
    float bassPos = 0.5f, midPos = 0.5f, treblePos = 0.5f;

    // schematic values (ohms, farads)
    static constexpr float Rin = 33'000.0f;       // input feed resistor
    static constexpr float Rt_max = 250'000.0f;   // treble pot
    static constexpr float Rm_max = 25'000.0f;    // mid pot
    static constexpr float Rb_max = 1'000'000.0f; // bass pot
    static constexpr float C_T = 0.022e-6f;       // treble cap (node0<->node2)
    static constexpr float C_M = 0.022e-6f;       // mid cap    (node1<->node4)
    static constexpr float C_B = 0.022e-6f;       // bass cap   (node3->gnd)

    LinearNet<5> net;

    void setSampleRate(double sr)
    {
        mDt = static_cast<float>(1.0 / sr);
        update();
    }

    void setPositions(float bass, float mid, float treble)
    {
        bassPos = bass;
        midPos = mid;
        treblePos = treble;
        update();
    }

    // nodes: 0=T(input), 1=M(mid), 2=OUT(wiper), 3=Bb(bass bottom), 4=Mj(mid junction)
    void update()
    {
        auto &n = net;
        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < 5; ++j)
            {
                n.G[i][j] = 0.0f;
                n.C[i][j] = 0.0f;
            }
            n.Bsrc[i] = 0.0f;
        }
        n.mOutNode = 2;

        // input: 33k Norton source holds node0 near the source voltage u
        n.G[0][0] += 1.0f / Rin;
        n.Bsrc[0] += 1.0f / Rin;

        // treble cap C_T: node0 (T) <-> node2 (OUT)  [HF bypass to output]
        {
            float c = C_T;
            n.C[0][0] += c;
            n.C[2][2] += c;
            n.C[0][2] -= c;
            n.C[2][0] -= c;
        }

        // treble pot Rt: node0 (T) <-> node1 (M)
        {
            float g = 1.0f / (Rt_max * (0.05f + 0.95f * treblePos));
            n.G[0][0] += g;
            n.G[1][1] += g;
            n.G[0][1] -= g;
            n.G[1][0] -= g;
        }

        // mid cap C_M: node1 (M) <-> node4 (Mj)
        {
            float c = C_M;
            n.C[1][1] += c;
            n.C[4][4] += c;
            n.C[1][4] -= c;
            n.C[4][1] -= c;
        }

        // mid pot Rm: node4 (Mj) -> ground
        n.G[4][4] += 1.0f / (Rm_max * (0.05f + 0.95f * midPos));

        // bass pot: real 3-terminal pot, top=node1, wiper=node2(OUT), bottom=node3.
        // wiper sits at fraction (1-bassPos) from the top:
        //   ra = node1->node2 = (1-bassPos)*Rb_max
        //   rb = node2->node3 = bassPos*Rb_max
        {
            float ra = 1.0f / (Rb_max * (0.001f + 0.999f * (1.0f - bassPos)));
            float rb = 1.0f / (Rb_max * (0.001f + 0.999f * bassPos));
            n.G[1][1] += ra;
            n.G[2][2] += ra;
            n.G[1][2] -= ra;
            n.G[2][1] -= ra;
            n.G[2][2] += rb;
            n.G[3][3] += rb;
            n.G[2][3] -= rb;
            n.G[3][2] -= rb;
        }

        // bass cap C_B: node3 (Bb) -> ground
        n.C[3][3] += C_B;

        n.discretize(mDt);
    }

    float process(float u) { return net.process(u); }
    void reset() { net.reset(); }

private:
    float mDt = 1.0f / 44100.0f;
};
