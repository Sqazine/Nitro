// ============================================================================
//  Cabinet.h  -  guitar cabinet simulation (convolution)
//
//  Two IR sources are supported:
//    * Synthesised Greenback-style IR (generateIR): a handful of damped
//      resonant modes, self-contained, always available as a fallback.
//    * Measured IR loaded from a .wav file (loadWav): a real 4x12 Greenback
//      impulse response. Drop a mono/stereo 16/24/32-bit (or 32-bit float)
//      PCM WAV where the plugin looks for it (see PluginProcessor::loadCabinetIR
//      for the search paths, e.g. %APPDATA%/JCM800/Greenback.wav) and it
//      replaces the synth IR automatically on load.
//
//  Both paths end in normalizeIR(), which removes DC and applies a
//  *max-gain* normalisation (convolve reference sines across the passband,
//  scale so no frequency exceeds unity steady-state gain). This keeps the
//  cabinet's output level consistent and clip-safe whether the IR is synth
//  or measured. The convolution itself runs at the base sample rate.
// ============================================================================
#pragma once
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <complex>
#include <vector>

class Cabinet
{
public:
    static constexpr int kDefaultLen = 8192; // synth IR length (~93 ms @44.1k)
    static constexpr int kMaxLen = 16384;    // hard cap for loaded IRs (~371 ms)

    int mLen = kDefaultLen;
    float *mIR = nullptr;
    float *mBuf = nullptr;        // circular input history
    int mIdx = 0;                 // write pointer
    bool mLoadedFromFile = false; // true once a measured IR is active
    float mSr = 44100.0f;         // sample rate (public for inspection/tests)

    ~Cabinet()
    {
        delete[] mIR;
        delete[] mBuf;
    }

    void setSampleRate(double sr)
    {
        mSr = static_cast<float>(sr);
        mLen = kDefaultLen;
        allocBuffers();
        generateIR();
    }

    // Replace the IR with a user-supplied one (e.g. from memory). DC removed
    // and max-gain normalised so the cabinet level stays consistent.
    void loadIR(const float *data, int len) { setIR(data, len); }

    // Load a measured impulse response from a RIFF/WAVE file. Supports
    // 16/24/32-bit integer PCM and 32-bit float, any channel count (mixed
    // down to mono), and resamples to the current sample rate. Returns true
    // on success; keeps the synthesised IR if the file cannot be read.
    bool loadWav(const char *path)
    {
        FILE *f = std::fopen(path, "rb");
        if (!f)
            return false;

        uint8_t hdr[12];
        if (std::fread(hdr, 1, 12, f) != 12)
        {
            std::fclose(f);
            return false;
        }
        if (std::memcmp(hdr, "RIFF", 4) != 0 || std::memcmp(hdr + 8, "WAVE", 4) != 0)
        {
            std::fclose(f);
            return false;
        }

        int sr = 44100, ch = 1, bits = 16, audioFmt = 1;
        bool haveFmt = false;
        long dataOff = -1;
        uint32_t dataSize = 0;

        while (true)
        {
            uint8_t id[4];
            uint8_t szb[4];
            if (std::fread(id, 1, 4, f) != 4)
                break;
            if (std::fread(szb, 1, 4, f) != 4)
                break;
            uint32_t sz = rd32(szb);
            if (std::memcmp(id, "fmt ", 4) == 0)
            {
                uint8_t fd[40];
                int n = sz < 40 ? (int)sz : 40;
                if (std::fread(fd, 1, n, f) != (size_t)n)
                {
                    std::fclose(f);
                    return false;
                }
                audioFmt = rd16(fd);
                ch = rd16(fd + 2);
                sr = (int)rd32(fd + 4);
                bits = rd16(fd + 14);
                haveFmt = true;
                if (sz > 40)
                    std::fseek(f, (long)(sz - 40), SEEK_CUR);
            }
            else if (std::memcmp(id, "data", 4) == 0)
            {
                dataOff = std::ftell(f);
                dataSize = sz;
                break;
            }
            else
            {
                std::fseek(f, (long)sz, SEEK_CUR);
            }
        }
        if (!haveFmt || dataOff < 0 || dataSize == 0)
        {
            std::fclose(f);
            return false;
        }

        int blockAlign = (bits + 7) / 8 * ch;
        if (blockAlign <= 0)
        {
            std::fclose(f);
            return false;
        }
        int frames = (int)(dataSize / (uint32_t)blockAlign);
        if (frames <= 0)
        {
            std::fclose(f);
            return false;
        }

        float *raw = new float[frames];
        std::fseek(f, dataOff, SEEK_SET);
        for (int i = 0; i < frames; ++i)
        {
            double acc = 0.0;
            for (int c = 0; c < ch; ++c)
            {
                if (bits == 32 && audioFmt == 3) // 32-bit float
                {
                    uint8_t b[4];
                    std::fread(b, 1, 4, f);
                    acc += (double)rd32f(b);
                }
                else if (bits == 32)
                {
                    uint8_t b[4];
                    std::fread(b, 1, 4, f);
                    acc += (double)((int32_t)rd32(b)) / 2147483648.0;
                }
                else if (bits == 24)
                {
                    uint8_t b[3];
                    std::fread(b, 1, 3, f);
                    int32_t v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16));
                    if (v & 0x800000)
                        v |= (int32_t)0xFF000000; // sign extend
                    acc += (double)v / 8388608.0;
                }
                else // 16-bit (default)
                {
                    uint8_t b[2];
                    std::fread(b, 1, 2, f);
                    acc += (double)((int16_t)rd16(b)) / 32768.0;
                }
            }
            raw[i] = (float)(acc / (double)ch);
        }
        std::fclose(f);

        // resample to the current cabinet sample rate (linear interpolation)
        int outLen = frames;
        if (sr > 0 && std::abs(sr - (int)mSr) > 1)
            outLen = (int)((double)frames * (double)mSr / (double)sr + 0.5);
        if (outLen < 1)
            outLen = 1;
        float *res = new float[outLen];
        if (outLen == frames)
        {
            for (int i = 0; i < outLen; ++i)
                res[i] = raw[i];
        }
        else
        {
            double ratio = (double)frames / (double)outLen;
            for (int i = 0; i < outLen; ++i)
            {
                double pos = i * ratio;
                int i0 = (int)pos;
                double frac = pos - i0;
                int i1 = i0 + 1 < frames ? i0 + 1 : frames - 1;
                if (i0 >= frames)
                    i0 = frames - 1;
                res[i] = raw[i0] * (1.0f - (float)frac) + raw[i1] * (float)frac;
            }
        }
        delete[] raw;

        bool ok = setIR(res, outLen);
        delete[] res;
        mLoadedFromFile = ok;
        return ok;
    }

    void reset()
    {
        if (mBuf)
            std::memset(mBuf, 0, sizeof(float) * mLen);
        mIdx = 0;
    }

    float process(float x)
    {
        mBuf[mIdx] = x;
        float y = 0.0f;
        for (int k = 0; k < mLen; ++k)
        {
            int idx = mIdx - k;
            if (idx < 0)
                idx += mLen;
            y += mIR[k] * mBuf[idx];
        }
        mIdx++;
        if (mIdx >= mLen)
            mIdx = 0;
        return y;
    }

private:
    static uint32_t rd32(const uint8_t *p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    static float rd32f(const uint8_t *p)
    {
        uint32_t u = rd32(p);
        float v;
        std::memcpy(&v, &u, 4);
        return v;
    }

    void allocBuffers()
    {
        if (mIR)
            delete[] mIR;
        if (mBuf)
            delete[] mBuf;
        mIR = new float[mLen];
        mBuf = new float[mLen];
        std::memset(mBuf, 0, sizeof(float) * mLen);
    }

    // Reallocate to `len` (capped), copy data, then normalise.
    bool setIR(const float *data, int len)
    {
        if (len <= 0)
            return false;
        int L = len;
        if (L > kMaxLen)
            L = kMaxLen; // truncate very long IRs
        mLen = L;
        allocBuffers();
        for (int i = 0; i < L; ++i)
            mIR[i] = data[i];
        normalizeIR(L);
        return true;
    }

    // DC removal + max-gain normalisation (keeps cabinet output clip-safe
    // and level-consistent between synth and measured IRs). The reference
    // gain at each band is the exact steady-state magnitude response |H(f)|
    // (DTFT), so the worst-case band is scaled to unity and no frequency can
    // exceed the input level.
    void normalizeIR(int L)
    {
        float mean = 0.0f;
        for (int i = 0; i < L; ++i)
            mean += mIR[i];
        mean /= L;
        for (int i = 0; i < L; ++i)
            mIR[i] -= mean;

        const float refs[8] = {70.0f, 150.0f, 250.0f, 400.0f,
                               800.0f, 1500.0f, 3000.0f, 6000.0f};
        float maxGain = 1e-9f;
        for (int r = 0; r < 8; ++r)
        {
            float w = 2.0f * 3.14159265359f * refs[r] / mSr;
            float re = 0.0f, im = 0.0f;
            for (int k = 0; k < L; ++k)
            {
                re += mIR[k] * std::cos(w * k);
                im -= mIR[k] * std::sin(w * k);
            }
            float mag = std::sqrt(re * re + im * im);
            if (mag > maxGain)
                maxGain = mag;
        }
        float gnorm = 1.0f / maxGain;
        for (int i = 0; i < L; ++i)
            mIR[i] *= gnorm;
    }

    // --- Greenback 4x12 closed-back cabinet signature (Celestion G12M) ---
    // The synthesised cabinet is built from a *target magnitude response*
    // (the three signature traits of a Greenback, see gbTarget) turned into a
    // minimum-phase impulse response via the complex cepstrum. Minimum-phase
    // synthesis gives a smooth, gap-free, natural-sounding IR whose steady-
    // state magnitude equals the target curve - with none of the inter-modal
    // nulls a discrete resonant-mode sum would produce:
    //   * low "thump" resonance near 95 Hz  (cabinet / box mode)
    //   * mid "cone-cry" presence bump near 1.8 kHz (cone breakup)
    //   * smooth high-frequency rolloff above ~4.5 kHz
    // plus a gentle sub-bass roll-off below ~70 Hz.
    // Greenback target magnitude response (see header comment above).
    static double gbTarget(double f)
    {
        double sub   = 1.0 / (1.0 + std::pow(70.0 / f, 2.0));            // <70 Hz cut
        double thump = 0.60 * std::exp(-std::pow(std::log(f / 95.0) / 0.30, 2.0));
        double cry   = 0.70 * std::exp(-std::pow(std::log(f / 1800.0) / 0.22, 2.0));
        double hf    = 1.0 / (1.0 + std::pow(f / 4500.0, 1.8));          // >4.5k rolloff
        double g     = (1.0 * sub + thump + cry) * hf;
        if (g < 0.04) g = 0.04; // floor keeps the cepstrum well-conditioned
        return g;
    }

    // in-place iterative radix-2 FFT (inverse when `inverse` is true)
    static void fft(std::vector<std::complex<double>> &a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap(a[i], a[j]);
        }
        const double PI = 3.141592653589793;
        for (size_t len = 2; len <= n; len <<= 1)
        {
            double ang = 2.0 * PI / (double)len * (inverse ? 1.0 : -1.0);
            std::complex<double> wlen(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w(1.0, 0.0);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    std::complex<double> u = a[i + k];
                    std::complex<double> v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
        if (inverse)
        {
            double inv = 1.0 / (double)n;
            for (auto &x : a)
                x *= inv;
        }
    }

    void generateIR()
    {
        const int N = mLen; // must be a power of two (kDefaultLen = 8192)
        if ((N & (N - 1)) != 0)
        {
            // fall back to a flat impulse if length is not FFT-friendly
            for (int n = 0; n < N; ++n)
                mIR[n] = (n == 0) ? 1.0f : 0.0f;
            normalizeIR(N);
            return;
        }
        const double sr = mSr;
        std::vector<std::complex<double>> A(N);
        // 1) target magnitude spectrum, symmetric about the Nyquist
        for (int k = 0; k <= N / 2; ++k)
        {
            double f = (double)k * sr / (double)N;
            double g = gbTarget(f);
            A[k] = std::complex<double>(std::log(g), 0.0);
            if (k > 0 && k < N / 2)
                A[N - k] = A[k];
        }
        // 2) complex cepstrum (inverse FFT of log-magnitude)
        fft(A, true);
        // 3) force causality -> minimum phase: keep n = 0..N/2, zero the rest
        for (int n = N / 2 + 1; n < N; ++n)
            A[n] = std::complex<double>(0.0, 0.0);
        // 4) minimum-phase log spectrum (forward FFT)
        fft(A, false);
        // 5) exponentiate -> complex minimum-phase spectrum
        for (int k = 0; k < N; ++k)
            A[k] = std::exp(A[k]);
        // 6) back to time
        fft(A, true);
        for (int n = 0; n < N; ++n)
            mIR[n] = (float)std::real(A[n]);
        normalizeIR(N); // DC removal + max-gain normalisation
    }
};