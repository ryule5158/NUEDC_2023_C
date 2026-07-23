# FilterEx 使用指南

`FilterEx` 是 `Filter.h/.c` 的纯新增扩展层。旧文件、旧结构体和旧函数没有被替换，已有模块可以继续按原方式工作；新代码只需包含 `FilterEx.h`。

## 电赛场景怎么选

| 信号问题 | 建议接口 | 特点 |
|---|---|---|
| ADC 直流偏置、慢漂移 | `FilterEx_DCBlock*` | 每点 2 加 1 乘，保留交流分量 |
| 显示值/控制量轻度抖动 | `FilterEx_OnePole*` | 状态极小、延迟低 |
| 白噪声、测量值平滑 | `FilterEx_MovingAverage*` | O(1) 更新，窗口启动无零填充偏差 |
| 实时有效值 | `FilterEx_RMS*` | O(1) 滑窗功率更新 |
| 毛刺、脉冲干扰 | `FilterEx_Median*` | 有序窗口 O(W)，适合 3/5/7 点 |
| 已知单频干扰 | `FilterEx_BiquadDesign(...NOTCH...)` | 低开销陷波，Q 控制带宽 |
| 平坦通带的低通/高通 | `FilterEx_ButterworthDesign` | 正确的 1–16 阶 Butterworth SOS |
| 有上下截止频率的选频 | `FilterEx_WideBandPassDesign` | 高通与低通级联，中心增益自动归一 |
| 线性相位、严格波形保真 | `FilterEx_FIRDesign` + `FilterEx_FIR*` | 窗函数 FIR，状态可连续跨块 |
| 抗混叠降采样 | `FilterEx_FIRDesign` + `FilterEx_Decimator*` | 只在输出相位计算卷积，减少 MAC |
| MATLAB/Scipy 已设计系数 | `FilterEx_SOSLoad` | 直接导入六列 SOS |

`Adaptive.h` 中已有 LMS 与自适应陷波，FilterEx 不重复这些功能。

## 三个常用例子

### 1. 四阶 2 kHz Butterworth 低通

```c
#include "FilterEx.h"

static FilterEx_SOS_t lowpass;

void App_FilterInit(void)
{
    FilterEx_Status_t status = FilterEx_ButterworthDesign(
        &lowpass, FILTEREX_BUTTERWORTH_LOWPASS,
        100000.0f, 2000.0f, 4u);
    if (status != FILTEREX_OK) {
        /* 参数错误：在这里进入工程自己的故障处理。 */
    }
}

float App_FilterSample(float adc_value)
{
    return FilterEx_SOSProcess(&lowpass, adc_value);
}
```

### 2. 去直流后做 5 点抗毛刺中值

```c
static FilterEx_DCBlock_t dc;
static FilterEx_Median_t median5;

void App_PreprocessInit(void)
{
    (void)FilterEx_DCBlockInit(&dc, 50000.0f, 5.0f);
    (void)FilterEx_MedianInit(&median5, 5u);
}

float App_Preprocess(float x)
{
    return FilterEx_MedianProcess(&median5,
                                  FilterEx_DCBlockProcess(&dc, x));
}
```

### 3. 63 阶抗混叠 FIR 后 4 倍降采样

```c
#define AA_TAPS 63u

static float aa_coeff[AA_TAPS];
static float aa_state[AA_TAPS];
static FilterEx_Decimator_t decimator;

void App_DecimatorInit(void)
{
    /* fs=100 kHz，降采样后 fs=25 kHz；10 kHz 留出过渡带。 */
    (void)FilterEx_FIRDesign(aa_coeff, AA_TAPS,
                             FILTEREX_FIR_LOWPASS,
                             100000.0f, 10000.0f, 0.0f,
                             FILTEREX_WINDOW_BLACKMAN);
    (void)FilterEx_DecimatorInit(&decimator, 4u,
                                  aa_coeff, aa_state, AA_TAPS, AA_TAPS);
}

uint32_t App_Decimate(const float *input, uint32_t input_length,
                      float *output, uint32_t output_capacity)
{
    uint32_t output_length = 0u;
    FilterEx_Status_t status = FilterEx_DecimatorProcess(
        &decimator, input, input_length,
        output, output_capacity, &output_length);
    return (status == FILTEREX_OK) ? output_length : 0u;
}
```

## 使用约定

- 所有对象和缓冲区由调用者持有，不使用 `malloc`，适合裸机和中断环境。
- `Init/Design` 只在配置变化时调用；`Process` 热路径不计算 `sin/cos/exp/pow`。
- 所有 `ProcessBlock` 都允许 `input == output` 原地处理。降采样也允许原地输出。
- 切换信号源、频率或系数后调用相应 `Reset`，避免旧状态形成瞬态。
- FIR 系数和状态数组必须在滤波器整个生命周期内有效；FIR 设计要求奇数抽头数。
- 热路径假定输入为有限浮点数。出现 `NaN/Inf` 时应先在采集边界处理，避免状态被污染。
- `FilterEx_SOSLoad` 的每行是 `[b0,b1,b2,a0,a1,a2]`；模块内部会除以 `a0`，反馈公式为 `1+a1*z^-1+a2*z^-2`，与 MATLAB/Scipy 的 SOS 符号一致。
- `FilterEx_BiquadMagnitude`、`FilterEx_SOSMagnitude` 和 `FilterEx_FIRMagnitude` 用于初始化/调试，不应放在采样 ISR 中。

## 性能说明

- 一阶低通、去直流和包络均为常数复杂度。
- 滑动均值与 RMS 每点 O(1)，窗口大小不增加运行时间。
- 中值滤波维护有序窗口，每点 O(W)，比每点完整排序更适合 3–31 点小窗口。
- FIR 使用环形状态，不搬移历史样本；抽取器只在输出相位进行 FIR 点积，理论 MAC 数约降为普通“先滤后抽取”的 `1/factor`。
- 高阶 IIR 使用 DF2T 二阶节，低 Q 节排在高 Q 节前，避免把高阶分母直接展开造成数值敏感。

