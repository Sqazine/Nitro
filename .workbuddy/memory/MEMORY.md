# Nitro 项目长期记忆 (JCM800 白盒建模)

## 项目定位
- 目标：Marshall JCM800 (2204/2203) 箱体模拟电吉他效果器 VST3，白盒物理建模（非黑盒 waveshaper），追求"和真箱头一样"的软削波、无塑料味。
- 信号链（白盒）：guitar → V1a 共阴极增益级 → V1b 阴极跟随 → [470pF bright + GAIN] → V2a 增益级 → Marshall TMB 音色栈(Bass/Mid/Treble) → V3 长尾倒相(LTP) → EL34 推挽功放(偶次抵消+电源塌陷+Presence) → 4x12 箱体卷积。
- 技术栈：JUCE 框架 VST3，项目根 `D:/.sc/Nitro/`。白盒 DSP 全部在 `JCM800/`（纯 C++17 头文件，独立于 JUCE，便于无 JUCE 自检；源码已从 `JCM800/dsp/` 扁平化到此）。

## 关键设计决策（已验证）
- 三极管用 3/2 幂律 `Ip=Kp*(Vgk+Vpk/μ)^1.5`，解析雅可比给 Newton-Raphson；MNA + Tustin 电容伴随模型。
- **栅流软削波**：栅极源是 Thévenin 源（串 `Rsrc`），`Rsrc` 限制栅流把 `Vgk` 箱位近 0 → 真实软削波（去塑料味关键）。
- `Kp` 按模式二分校准到可达空闲电流（共阴极 0.6mA / 阴极跟随 0.02mA）。
- 箱体：合成 IR 用**最小相位（complex cepstrum）合成**——以 Greenback 目标频响（95Hz thump / 1.8kHz cone-cry / 4.5kHz 以上滚降 / 70Hz 以下 sub-cut）做目标幅度谱，经 FFT→复倒谱→因果化→exp→IFFT 得最小相位 IR。比旧"离散阻尼模态求和"更平滑、无模态间零点（旧版中频塌到 −37dB、6k 落在模态间隙）。`gbTarget(f)` 定义目标曲线，`fft()` 是自实现的 radix-2。
  - **归一化改用精确稳态幅度 |H(f)|（DTFT）**，不再用卷积峰值（`n>L` 在 `mLen==RN==8192` 时永不触发 → `maxGain=1e-9` → 增益爆炸 1e9 的 bug，已修）。修后箱体输出峰稳定在 ~0.49（宿主安全）。
  - `kDefaultLen=8192`（~186ms@44.1k），`kMaxLen=16384`。`mSr` 已提到 public 供自检读取。
- **仍支持接入测量版 IR**：`Cabinet::loadWav(path)` 解码 RIFF/WAVE（16/24/32-bit int + 32-bit float，多声道混单），按插件采样率线性重采样，DC 去除 + 最大增益归一化（与合成 IR 一致）。插件 `PluginProcessor::loadCabinetIR()` 在 `prepareToPlay` 扫描 `%APPDATA%/JCM800/Greenback.wav`、`%USERPROFILE%/Documents/JCM800/Greenback.wav`、`D:/sc/Nitro/JCM800/Greenback.wav`（及 `*_IR.wav`/`greenback.wav`）自动加载，找不到则回退合成 IR。自检 `runIRTest()` 用 delta WAV 往返验证（maxErr=0，RESULT OK）。
- **无 JUCE 自检基建（已删）**：本次合成箱体改写时曾建 `JCM800/self_test.cpp` + `build_selftest.bat` 做验证（显式 INCLUDE/LIB 绕过 vcvarsall 的 `reg.exe` 拦截；MSVC v18 / SDK `10.0.26100.0`），验证通过后**用户要求删除**，目录已无测试文件。如需再验证，重建方式：该 bat 设 `INCLUDE=%MSVC%\include;%SDK%\Include\10.0.26100.0\{um,shared,ucrt}`、`LIB=%MSVC%\lib\x64;%SDK%\Lib\10.0.26100.0\{um,ucrt}\x64`、`PATH` 加 `Hostx64\x64`，再 `cl /std:c++17 /O2 /MT /EHsc /I"JCM800" self_test.cpp`。
- 过采样：插件里 `juce::dsp::Oversampling` 4x，非线性级在过采样率跑，箱体在基率跑。
- **功率放大（白盒 EL34 推挽）**：`g(a)-g(b)` 反相奇对称软削波 → 偶次抵消；`PowerAmp` 的 **HT 塌陷包络用 `dt` 推导 attack(~2ms)/release(~60ms)，与采样率无关**（早期固定系数在 4x 下塌成常数，无动态压缩）；`sagF=1-sag*clamp01(env*0.005)` 渐进压缩；功率管拐点 `drive=0.015`、master=0.5。
- **前级清音余量**：`stage2Drive = 0.40*(0.05+1.95*gain)` → 低增益 V2a 不栅流（真清音），高增益栅流 grind，形成 clean→crunch 动态（旧 `0.5*(0.4+1.6*gain)` 全程一样 crunch = 塑料感主因之一）。

## 自检 / 构建（详见每日日志）
- 无 JUCE 自检基建已删除（见上方箱体节）。如需重建见该节说明。
- 插件：`build/` 已有 VS2026 cmake 缓存，清掉代理环境变量后 `cmake --build . --target JCM800_VST3 --config Release`。**约定：构建产物 `.vst3` 只留在 `build/Bin/Release/`，不要拷回 `JCM800/` 源码目录**（用户自行编译，源码树保持只有源码）。

## 待办 / 风险
- ~~输出电平偏保守（mOutGain=1.0，cranked peak≈0.37）~~ → 已通过 PowerAmp `master=0.5` + `drive=0.015` 把峰控制在 0.23–0.49（宿主安全），无需再调 mOutGain。
- 目前是 mono 内部模型 ×2 通道（L/R 独立），非真立体声运算；对吉他箱头足够。
- 尚未做：真正的立体声箱体 IR、参数平滑（setParams 每块整体设，已够用）、GUI 美化（当前 GenericAudioProcessorEditor）。
- 残留：倒相级(LTP) 被音色栈 ~25–53V 驱动过 3.5V 栅箱位 → 全档位参与奇次削波（真实高增益下倒相也失真，可接受）；要更真需给 TMB 音色栈加 ~−20dB 插入损耗（现拓扑实测 ~−2.4dB）。`ToneStack.h` 局部 `Minv` 遮蔽成员(C4458)纯 warning，未动。
