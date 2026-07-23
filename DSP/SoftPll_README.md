# SoftPll ADC—AD9910 锁相模块

`SoftPll_AdcTracker_t` 是不依赖 HAL、无动态内存的双通道数字锁相环。ADC DMA 在一块缓冲区采集时，CPU 可以处理另一块；处理结果是 AD9910 的频率字 `FTW` 和相位字 `POW`。原有 `SoftPll_Init()`、`SoftPll_UpdatePhase()` 等接口保持不变。

## 必须先明确的硬件条件

- 物理闭环需要**同时采样参考输入和 DDS 实际反馈**。只采参考输入、用软件推算 DDS 相位属于开环同步，无法消除 AD9910、IO_UPDATE、模拟通道和时钟间的实际相位误差。
- 直接采样 40 MHz 时必须满足 ADC 实际采样率大于 80 MSPS，并留出抗混叠滤波过渡带。STM32H7 片上 ADC 达不到这个条件；它只能锁到自身奈奎斯特范围内，或配合模拟下变频。
- `SOFTPLL_SAMPLING_BANDPASS_ALIAS` 可以用外部带通滤波器做欠采样，代码会自动修正频谱倒置，但硬件必须先唯一选出目标奈奎斯特区，否则混叠不可判别。
- 0 Hz 没有可定义的周期相位，因而“0 Hz 锁相”无物理意义。本模块有效范围是配置的混叠保护频率以上至 40 MHz；AD9910 仍可单独输出 DC/近 DC 设置。
- ADC 参考通道与反馈通道必须共用采样时钟并同步启动。若 ADC、FPGA 和 AD9910 使用独立晶振，长期相位精度最终由时钟稳定度决定。

## 最小配置

以下例子使用双 ADC 交织 DMA。两个通道共用一块 `uint16_t` 帧，因此通道首地址相差 2 字节、步长为 4 字节。

```c
#include "DSP_Header.h"
#include "ad9910.h"

#define PLL_POINTS       4096U
#define PLL_ADC_FS_HZ    100000000.0f
#define PLL_SIGNAL_HZ     40000000.0f

static uint16_t adc_frame[2][PLL_POINTS * 2U]
    __attribute__((aligned(32)));
static SoftPll_AdcTracker_t pll;

void Pll_Start(void)
{
    SoftPll_AdcConfig_t cfg;
    SoftPll_AdcDefaultConfig(&cfg, PLL_SIGNAL_HZ, PLL_ADC_FS_HZ,
                             (double)AD9910_SYSCLK_HZ, PLL_POINTS);

    cfg.sample_format = SOFTPLL_SAMPLE_UINT16;
    cfg.sample_stride_bytes = 2U * sizeof(uint16_t);
    cfg.reference_scale = ADC_VOLTS_PER_CODE;
    cfg.reference_offset = ADC_REFERENCE_OFFSET_VOLTS;
    cfg.feedback_scale = ADC_VOLTS_PER_CODE;
    cfg.feedback_offset = ADC_FEEDBACK_OFFSET_VOLTS;

    /* 首次上板建议先保持 D=0、直接相位跳变=0，仅用 type-II PI。 */
    cfg.loop.kd_hz_s_per_deg = 0.0f;
    cfg.loop.phase_kp = 0.0f;

    (void)SoftPll_AdcTrackerInit(
        &pll, &cfg,
        &adc_frame[0][0], &adc_frame[1][0],
        &adc_frame[0][1], &adc_frame[1][1]);

    /* 一次性进入单音模式；后续每帧使用 FTW/POW 快路径。 */
    (void)AD9910_SetFrequencyFineHz((float)PLL_SIGNAL_HZ);
}
```

启动每次 DMA 前先取得空闲帧；完整帧完成回调只移交所有权并立即启动另一帧，不在 ISR 中做 IQ、三角函数或 SPI。

```c
static void StartNextAdcDma(void)
{
    uint8_t index;
    void *reference;
    void *feedback;
    if (SoftPll_AdcBeginDma(&pll, &index, &reference, &feedback) ==
        SOFTPLL_TRACK_OK) {
        active_pll_index = index;
        /* 交织 DMA 从 reference 开始，共 PLL_POINTS*2 个 halfword。 */
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)reference, PLL_POINTS * 2U);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc1) {
        /* H7 开启 D-Cache 时，先按 32 字节对齐范围 invalidate。 */
        InvalidatePllFrameCache(adc_frame[active_pll_index],
                                sizeof(adc_frame[active_pll_index]));
        (void)SoftPll_AdcDmaComplete(&pll, active_pll_index);
        StartNextAdcDma();
    }
}
```

主循环或高优先级任务处理已完成帧。`OK` 表示已经达到锁定判据，`NOT_LOCKED` 表示测量和命令均有效但尚未连续满足锁定帧数。

```c
void Pll_Task(void)
{
    SoftPll_Ad9910Command_t command;
    SoftPll_TrackStatus_t status =
        SoftPll_AdcProcessReady(&pll, &command);

    if ((status == SOFTPLL_TRACK_OK ||
         status == SOFTPLL_TRACK_NOT_LOCKED) && command.valid) {
        (void)AD9910_UpdateSingleToneWordsFast(command.ftw, command.pow);
    }
}
```

`AD9910_UpdateSingleToneWordsFast()` 直接使用模块经双精度舍入得到的 `command.ftw` / `command.pow`，只写 Profile 0，并用一次 IO_UPDATE 让频率和相位同时生效。相比每帧分别调用原频率/相位接口，它避免重写 CFR、8 个 Profile、两次更新和 40 MHz 附近约 4 Hz 的 `float` 步距；1 GHz AD9910 的理论 FTW 分辨率约为 0.233 Hz。

## 参数整定

`SoftPll_TrackTunePI()` 按二阶 type-II 环计算 PI 参数。默认宏位于 `SoftPll.h`，可在工程编译选项或包含头文件前覆盖：

```c
#define SOFTPLL_TRACK_DEFAULT_BANDWIDTH_HZ  500.0f
#define SOFTPLL_TRACK_DEFAULT_DAMPING       0.70710678f
#define SOFTPLL_TRACK_DEFAULT_KD_HZ_S_PER_DEG 0.0f
```

如需完全使用上板经验值，可同时定义 `SOFTPLL_TRACK_USE_EXPLICIT_PI_GAINS=1`、`SOFTPLL_TRACK_DEFAULT_KP_HZ_PER_DEG` 和 `SOFTPLL_TRACK_DEFAULT_KI_HZ_PER_DEG_S`；也可在 `SoftPll_AdcDefaultConfig()` 之后直接改 `cfg.loop`，无需重新编译库。

- 先用 `Kp/Ki`，令 `Kd=0`、`phase_kp=0`。增加带宽可加快捕获，但会增加 ADC 噪声、杂散和 IO_UPDATE 抖动传到 DDS 的程度。
- 帧更新率至少取环路带宽的 10～20 倍；默认配置会把带宽自动压到帧更新率的 5% 以下。
- `min_output_hz/max_output_hz` 是捕获范围；实际可捕获频差还必须小于约半个帧更新率，否则相邻帧相位变化会发生周跳。
- `fixed_phase_cal_deg` 补偿 ADC 通道、滤波器、走线和 DDS 模拟输出的固定相差，应使用同频双通道实测标定；宽频段时通常需要做随频率变化的标定表。
- `alias_guard_hz` 限制混叠频率离 DC/奈奎斯特点的最小距离。默认至少半个 FFT 帧频率间隔，防止相位不可观测。
- `lock_error_deg`、`unlock_error_deg` 和 `lock_frames` 构成带滞回锁定判据，避免噪声导致状态抖动。

## 实时预算

双缓冲保证的是所有权和流水线正确，不等于 CPU 必然赶得上数据率。每帧处理时间必须小于一帧采集时间；若 `overrun_count` 增长，就要增大帧间隔、降低处理更新率，或在外部 ADC/FPGA 中先做 DDC 和抽取。100 MSPS 双通道原始样本不适合由 STM32H7 持续逐样本全处理，推荐 FPGA 输出低速 I/Q 或抽取后的窄带数据。

H7 DMA 缓冲区应放在 DMA 可访问 RAM；开启 D-Cache 时必须按芯片手册做 32 字节对齐和 cache clean/invalidate。内存屏障不能替代 cache 一致性操作。
