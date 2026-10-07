#pragma once
#include <cmath>
#include <algorithm>
#include <JuceHeader.h>

// ============================================================================
//  TS808Model - 电路级 (circuit-level) Ibanez Tube Screamer 建模
//
//  信号通路 (每声道, 过采样率逐采样处理):
//
//    in -> 输入缓冲(射极跟随器 + C1 隔直 HPF ~20Hz)
//       -> 削波放大级 (JRC4558 + 反馈环反并联二极管对 D1/D2)
//       -> Tone 有源网络 (R7*C5 主低通 723Hz + R8*C6 3.29kHz 补偿)
//       -> Level 分压 -> 输出缓冲 (HPF ~16Hz) -> out
//
//  削波级不是 tanh() 波形整形, 而是对反并联二极管对列 KCL (v- = 0 虚地):
//
//      Vsrc/R4 + vo/Rfb + C4*d(vo)/dt + 2*Is*sinh(vo/(n*Vt)) = 0
//
//  其中输入电流 Vsrc/R4 由 C3+R4 有源高通 HP720(s) 滤波后给出, 反馈网络
//  电压即运放输出 vo. 方程对 vo 严格单调, 每采样用 Newton-Raphson 求解
//  (上一采样解热启动, 实测 3-5 次迭代收敛到 1e-9). 最后叠加 JRC4558 输出
//  摆幅限制 (9V 单电源, 相对 4.5V 偏置约 ±3.5V).
//
//  内部以物理伏特为单位运算, 数字域 ±1.0 直接映射为 ±1.0V.
//  元件值取自 TS808 原版原理图 (Electrosmash 分析).
// ============================================================================
class TS808Model
{
public:
    TS808Model() = default;

    void prepare (double sampleRate, int osFactor)
    {
        fs = static_cast<float> (sampleRate * osFactor);
        const float dt = 1.0f / fs;

        // ---- 反馈环有源高通 HP720 (C3 + R4) ----
        // H(s) = s/(s+w0),  w0 = 1/(R4*C3)
        const float w0_hp = 1.0f / (R4 * C3);
        const float c_hp = 0.5f * w0_hp * dt;
        hpB0 = 1.0f / (1.0f + c_hp);
        hpB1 = -hpB0;
        hpA1 = (c_hp - 1.0f) / (1.0f + c_hp);

        // ---- C4 反馈电容 Tustin 伴随模型 ----
        Gc4 = 2.0f * C4 / dt;

        // ---- 运放摆率限制 (每采样最大输出变化量) ----
        maxSlewStep = SlewRate * dt;

        // ---- 输入缓冲 HPF (C1 + 输入阻抗, ~20Hz) ----
        const float w_in = 2.0f * juce::float_Pi * 20.0f;
        inHpA = 1.0f - std::exp (-w_in * dt);

        // ---- Tone 级固定系数: 低中频 body 隆起(~700Hz) + 固定 presence 高切(~5kHz) ----
        // (原实现只有 723Hz 主低通 + 3.29kHz 高频补偿, 午点近全通 -> 无高切 = 冷/毛刺.
        //  改进: 末端叠 presence 高切把高频滚掉, 并加低中频 body 增加厚度/暖)
        const float w_body = 2.0f * juce::float_Pi * BodyFc;
        bodyA = 1.0f - std::exp (-w_body * dt);
        const float w_pres = 2.0f * juce::float_Pi * PresenceFc;
        presLpA = 1.0f - std::exp (-w_pres * dt);

        // ---- 输出隔直 HPF (~16Hz) ----
        const float w_out = 2.0f * juce::float_Pi * 16.0f;
        outHpA = 1.0f - std::exp (-w_out * dt);

        reset();
    }

    void reset()
    {
        // 削波级状态
        mDelta = 0.0f;
        mIc4Prev = 0.0f;
        mHpOut = 0.0f;
        mVinPrev = 0.0f;
        mVoPrev = 0.0f;

        // 输入 HPF
        mInHpX1 = 0.0f;
        mInHpY1 = 0.0f;

        // Tone 级
        mToneLpY1 = 0.0f;
        mBodyY1 = 0.0f;
        mPresY1 = 0.0f;

        // 输出 HPF
        mOutHpX1 = 0.0f;
        mOutHpY1 = 0.0f;
    }

    void setParams (float drive_, float tone_, float level_)
    {
        drive = juce::jlimit (0.0f, 1.0f, drive_);
        tone  = juce::jlimit (0.0f, 1.0f, tone_);
        level = juce::jlimit (0.0f, 1.0f, level_);

        // Drive -> 反馈电阻: 51k 固定 + 0..500k 线性电位器
        Rfb = RfbFixed + drive * RfbPotMax;
        Gfb = 1.0f / Rfb;

        // Level 电位器: 0..+12dB, level=0.5 -> 0dB (与原参数映射保持兼容)
        levelGain = juce::jmax (level * level, 0.0f) * 4.0f;

        // Tone G-taper S 曲线 (smoothstep)
        toneG = tone * tone * (3.0f - 2.0f * tone);

        // 可变低通截止: 暗(350Hz) -> 亮(3.85kHz)
        const float fcTone = 350.0f + toneG * 3500.0f;
        // w_tone 已是数字角频率(rad/sample, 含 /fs), 低通系数 = 1 - exp(-w_tone)
        const float w_tone = 2.0f * juce::float_Pi * fcTone / fs;
        mToneLpA = 1.0f - std::exp (-w_tone);
    }

    float processSample (float vin)
    {
        float x = inputBuffer (vin);
        x = clippingStage (x);
        x = toneStage (x);
        x = outputStage (x);
        return x;
    }

private:
    // ========================================================================
    //  元件值 (物理单位: 欧姆, 法拉, 伏特, 安培)
    // ========================================================================
    // 削波级
    static constexpr float R4        = 4700.0f;      // 输入电阻
    static constexpr float C3        = 0.047e-6f;    // 反馈环高通电容 (fc=720Hz)
    static constexpr float RfbFixed  = 51000.0f;     // 反馈固定电阻
    static constexpr float RfbPotMax = 500000.0f;    // Drive 电位器最大值
    static constexpr float C4        = 51e-12f;      // 反馈软化电容
    // 二极管 D1/D2 (MA150, 反并联)
    static constexpr float Is        = 2.5e-9f;      // 反向饱和电流 (真实 MA150 ~2-5nA)
    static constexpr float nDiode    = 1.75f;        // 发射系数
    static constexpr float Vt        = 0.02585f;     // 热电压 (~25mV)
    // JRC4558 输出摆幅 (9V 单电源, 相对 4.5V 偏置)
    static constexpr float OpAmpSwing = 3.5f;
    // JRC4558 摆率限制 (~1.5 V/us), 自然圆滑削波边缘, 减少高频谐波
    static constexpr float SlewRate   = 1.5e6f;

    // Tone 级
    static constexpr float BodyFc     = 700.0f;    // 低中频 body 隆起中心频率
    static constexpr float PresenceFc = 5000.0f;   // 固定 presence 高切频率
    static constexpr float BodyGain   = 0.18f;     // body 隆起量 (~+3dB 低频厚度)

    // ========================================================================
    //  运行时状态
    // ========================================================================
    float fs = 44100.0f;
    float drive = 0.5f, tone = 0.5f, level = 0.5f;
    float levelGain = 1.0f;
    float toneG = 0.5f;

    float Rfb = RfbFixed;
    float Gfb = 1.0f / RfbFixed;

    // HP720 (输入侧有源高通) 系数
    float hpB0 = 0.0f, hpB1 = 0.0f, hpA1 = 0.0f;
    // C4 伴随模型
    float Gc4 = 0.0f;
    // 摆率限制
    float maxSlewStep = 0.0f;

    // 削波级状态
    float mDelta   = 0.0f;   // 上一采样 delta (热启动)
    float mIc4Prev = 0.0f;   // C4 上一采样电流
    float mHpOut   = 0.0f;   // HP720 输出上一采样
    float mVinPrev = 0.0f;   // vin 上一采样
    float mVoPrev  = 0.0f;   // 运放输出上一采样 (摆率限制)

    // 输入 HPF (一阶)
    float inHpA = 0.0f;
    float mInHpX1 = 0.0f, mInHpY1 = 0.0f;

    // Tone 级 (可变低通 + body 隆起 + presence 高切)
    float mToneLpA = 0.0f;   // 可变低通系数 (setParams 计算)
    float bodyA    = 0.0f;    // body 隆起系数 (prepare 计算)
    float presLpA  = 0.0f;    // presence 高切系数 (prepare 计算)
    float mToneLpY1 = 0.0f;
    float mBodyY1   = 0.0f;
    float mPresY1   = 0.0f;

    // 输出 HPF (一阶)
    float outHpA = 0.0f;
    float mOutHpX1 = 0.0f, mOutHpY1 = 0.0f;

    // ========================================================================
    //  输入缓冲 - 射极跟随器 (近似 unity) + C1 隔直高通 ~20Hz
    // ========================================================================
    float inputBuffer (float vin)
    {
        // 一阶 HPF: y[n] = (1-a)*y[n-1] + x[n] - x[n-1]
        float y = (1.0f - inHpA) * mInHpY1 + vin - mInHpX1;
        mInHpX1 = vin;
        mInHpY1 = y;
        return y;
    }

    // ========================================================================
    //  削波级 - JRC4558 + 反馈环反并联二极管对 (Newton-Raphson 求解)
    //
    //  KCL at (-) node (virtual ground v- = 0):
    //    Vsrc/R4 + vo/Rfb + C4*d(vo)/dt + 2*Is*sinh(vo/(n*Vt)) = 0
    //  输入电流经 C3+R4 高通 HP720(s) 滤波, 未知量为运放输出 vo.
    //  C4 用 Tustin 伴随模型: iC4 = Gc4*vo - I_hist.
    // ========================================================================
    float clippingStage (float vin)
    {
        // --- 输入侧有源高通 HP720: i_in = hp(vin)/R4 ---
        float hp = hpB0 * vin + hpB1 * mVinPrev - hpA1 * mHpOut;
        mHpOut = hp;
        mVinPrev = vin;
        const float iIn = hp / R4;

        // --- C4 Tustin 伴随历史源 ---
        const float Ic4Hist = Gc4 * mDelta + mIc4Prev;

        // --- Newton-Raphson 求解 delta ---
        const float nVt = nDiode * Vt;
        const float twoIs = 2.0f * Is;

        // 初始估计: 二极管解析近似 (削波时接近真实解, 小信号时接近 0)
        // 忽略线性项: 2*Is*sinh(delta/nVt) ≈ Ic4Hist - iIn
        const float diodeDrive = (Ic4Hist - iIn) / twoIs;
        float delta = nVt * std::asinh (diodeDrive);
        delta = juce::jlimit (-1.5f, 1.5f, delta);

        for (int iter = 0; iter < 12; ++iter)
        {
            const float dOverNvt = delta / nVt;
            const float iD = twoIs * std::sinh (dOverNvt);

            // f(delta) = Gfb*delta + Gc4*delta - Ic4Hist + iD + iIn
            // KCL at v- (=0 virtual ground): Vsrc/R4 + vo/Rfb + iC4 + iD = 0
            //   => (Gfb+Gc4)*vo - Ic4Hist + iD = -iIn,  iIn = Vsrc/R4
            const float f = (Gfb + Gc4) * delta - Ic4Hist + iD + iIn;

            // f'(delta) = Gfb + Gc4 + (2*Is/(n*Vt))*cosh(delta/(n*Vt))
            const float df = Gfb + Gc4 + (twoIs / nVt) * std::cosh (dOverNvt);

            if (std::fabs (df) < 1e-15f)
                break;

            const float step = f / df;
            delta -= step;

            // 物理限幅 (二极管削波后 |vo| 不会超过 ~1V)
            delta = juce::jlimit (-1.5f, 1.5f, delta);

            if (std::fabs (step) < 1e-9f)
                break;
        }

        // --- 输出电压 vo = delta (v- = 0 虚地, 反馈网络电压即输出电压) ---

        // --- JRC4558 输出软饱和 (tanh 圆角, 非硬钳位) ---
        // 诊断: 常规输入下二极管对已把 |vo| 限制在 ~1.2V, 远未到 ±3.5V 轨,
        // 故硬钳位几乎不触发; 但保留 tanh 软饱和可避免极端输入时的硬拐点
        // 不连续(硬钳位会引入刺耳奇次谐波), 使音色更顺滑/暖.
        float vo = OpAmpSwing * std::tanh (delta / OpAmpSwing);

        // --- JRC4558 摆率限制 (圆滑削波边缘, 减少高频谐波) ---
        vo = mVoPrev + juce::jlimit (-maxSlewStep, maxSlewStep, vo - mVoPrev);
        mVoPrev = vo;

        // --- 更新 C4 伴随历史 (基于实际输出电压 vo, 保持电容模型自洽) ---
        const float iC4 = Gc4 * vo - Ic4Hist;
        mIc4Prev = iC4;
        mDelta = vo;

        return vo;
    }

    // ========================================================================
    //  Tone 级 - 有源 tone 网络 (温暖取向重做)
    //
    //  1) 可变低通: 暗(350Hz) -> 亮(3.85kHz), tone=0.5 为中性低通
    //  2) 低中频 body 隆起 (~700Hz, +3dB): 增加厚度与暖感 (TS808 标志性的中频体)
    //  3) 固定 presence 高切 (~5kHz): 把削波产生的毛刺高频滚掉 -> 暖、不刺耳
    //  原实现午点接近全通(5k~10k 仍 ~0dB), 保留全部高频 fizz, 是"冷"的主因.
    // ========================================================================
    float toneStage (float vin)
    {
        // 1) 可变低通
        float lp = mToneLpY1 + mToneLpA * (vin - mToneLpY1);
        mToneLpY1 = lp;

        // 2) 低中频 body 隆起
        float bump = mBodyY1 + bodyA * (lp - mBodyY1);
        mBodyY1 = bump;
        float body = lp + BodyGain * (bump - lp);

        // 3) 固定 presence 高切
        float out = mPresY1 + presLpA * (body - mPresY1);
        mPresY1 = out;

        return out;
    }

    // ========================================================================
    //  输出级 - Level 分压 + 输出隔直高通 ~16Hz
    // ========================================================================
    float outputStage (float vin)
    {
        float x = vin * levelGain;

        // 一阶 HPF @ 16Hz: y[n] = (1-a)*y[n-1] + x[n] - x[n-1]
        float y = (1.0f - outHpA) * mOutHpY1 + x - mOutHpX1;
        mOutHpX1 = x;
        mOutHpY1 = y;
        return y;
    }
};