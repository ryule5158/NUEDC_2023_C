#include "adc_app.h"
#undef GPIOA
#undef GPIOB
#undef GPIOC
#undef GPIOD
#undef GPIOE
#undef GPIOH
#include "ti_msp_dl_config.h"
#include <stdio.h>

#define ADC_APP_DEFAULT_VREF_V 3.3f                  /* 片上ADC默认参考电压。 */
#define ADC_APP_MAX_CODE       4095.0f               /* 12位ADC满量程码。 */
#define ADC_APP_TIMEOUT_CYCLES (CPUCLK_FREQ / 1000U) /* 单次转换轮询超时周期。 */
#define ADC_APP_MAX_SAMPLE_RATE_HZ 20000.0f /* 软件轮询单通道最高允许采样率。 */

static uint16_t s_raw_buffer[ADC_APP_SAMPLE_LENGTH]; /* 片上ADC原始码缓存。 */
static float s_voltage_buffer[ADC_APP_CHANNEL_COUNT][ADC_APP_SAMPLE_LENGTH]; /* 电压换算缓存。 */
static float s_vref_v = ADC_APP_DEFAULT_VREF_V; /* 当前参考电压。 */
static float s_sample_rate_hz = 20000.0f;       /* 当前目标采样率。 */
static float s_max_harmonic_hz = 8000.0f;       /* 当前允许的最高谐波。 */
static uint8_t s_print_raw;                     /* 原始码打印开关。 */
static ADC_SampleMode_t s_mode = ADC_MODE_ONESHOT; /* 当前采样模式。 */
static uint8_t s_initialized;                   /* 初始化完成标志。 */
static uint8_t s_started;                       /* 采样任务运行标志。 */
static uint8_t s_data_ready;                    /* 完整数据帧就绪标志。 */
static ADC_HalfReady_t s_half_ready = ADC_HALF_NONE; /* 兼容半缓冲状态。 */
static uint8_t s_sync_enable;                   /* 缓冲同步保护开关。 */
static uint8_t s_buffer_locked;                 /* 缓冲区锁定标志。 */
static uint8_t s_overrun_count;                 /* 锁定期间的处理超限计数。 */
static uint8_t s_active_channel;                /* 当前读取通道。 */
static uint32_t s_sample_period_cycles;         /* 相邻样点的CPU周期数。 */

/* 触发一次ADC转换并返回12位原始码。 */
uint16_t NUEDC_ADC_ReadSampleRaw(uint32_t index)
{
    uint32_t timeout = ADC_APP_TIMEOUT_CYCLES;

    UNUSED(index);

    DL_ADC12_clearInterruptStatus(ADC12_0_INST,
                                  DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_startConversion(ADC12_0_INST);

    while ((DL_ADC12_getRawInterruptStatus(
                ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) &
            DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) == 0U) {
        if (timeout == 0U) {
            DL_ADC12_stopConversion(ADC12_0_INST);
            return 0U;
        }
        timeout--;
    }

    uint16_t sample = DL_ADC12_getMemResult(ADC12_0_INST, ADC12_0_ADCMEM_0);

    DL_ADC12_stopConversion(ADC12_0_INST);
    return sample;
}

/* 按目标采样率填充一帧原始码。 */
static void ADC_Sample_FillRaw(void)
{
    uint32_t next_sample_cycle;

    next_sample_cycle = NUEDC_HAL_GetCycleCount();
    for (uint32_t i = 0U; i < ADC_APP_SAMPLE_LENGTH; i++) {
        while ((int32_t)(NUEDC_HAL_GetCycleCount() - next_sample_cycle) < 0) {
            __NOP();
        }
        s_raw_buffer[i] = NUEDC_ADC_ReadSampleRaw(i);
        next_sample_cycle += s_sample_period_cycles;
    }
}

/* 把一帧原始码换算为电压。 */
static void ADC_Sample_Convert(void)
{
    for (uint32_t i = 0U; i < ADC_APP_SAMPLE_LENGTH; i++) {
        s_voltage_buffer[0][i] = ((float)s_raw_buffer[i] * s_vref_v) / ADC_APP_MAX_CODE;
    }
}

/* 配置片上ADC单次采样参数。 */
HAL_StatusTypeDef ADC_Sample_Init(float vref_v,
                                  float sample_rate_hz,
                                  float max_harmonic_hz,
                                  uint8_t print_raw,
                                  ADC_SampleMode_t mode)
{
    if ((vref_v <= 0.0f) ||
        (sample_rate_hz <= 0.0f) ||
        (sample_rate_hz > ADC_APP_MAX_SAMPLE_RATE_HZ) ||
        (max_harmonic_hz < 0.0f) ||
        (max_harmonic_hz > (sample_rate_hz * 0.5f)) ||
        (mode != ADC_MODE_ONESHOT)) {
        return HAL_ERROR;
    }

    s_vref_v = vref_v;
    s_sample_rate_hz = sample_rate_hz;
    s_max_harmonic_hz = max_harmonic_hz;
    s_print_raw = print_raw;
    s_mode = mode;
    s_initialized = 1U;
    s_started = 0U;
    s_data_ready = 0U;
    s_half_ready = ADC_HALF_NONE;
    s_sync_enable = 0U;
    s_buffer_locked = 0U;
    s_overrun_count = 0U;
    s_active_channel = 0U;
    s_sample_period_cycles =
        (uint32_t)(((float)CPUCLK_FREQ / sample_rate_hz) + 0.5f);

    return HAL_OK;
}

/* 启动并完成一帧软件定时采样。 */
HAL_StatusTypeDef ADC_Sample_Start(void)
{
    if (s_initialized == 0U) {
        return HAL_ERROR;
    }

    s_started = 1U;
    ADC_Sample_FillRaw();
    s_data_ready = 1U;
    s_half_ready = (s_mode == ADC_MODE_CONTINUOUS) ? ADC_HALF_FIRST : ADC_HALF_NONE;
    return HAL_OK;
}

/* 停止片上ADC采样任务。 */
void ADC_Sample_Stop(void)
{
    s_started = 0U;
}

/* 在主循环中换算并发布已完成的数据帧。 */
void ADC_Sample_Process(void)
{
    if (s_started == 0U) {
        return;
    }

    if ((s_sync_enable != 0U) && (s_buffer_locked != 0U)) {
        if (s_overrun_count < 255U) {
            s_overrun_count++;
        }
        return;
    }

    if ((s_data_ready != 0U) || (s_half_ready != ADC_HALF_NONE)) {
        ADC_Sample_Convert();
        if (s_print_raw == ADC_APP_PRINT_RAW_ON) {
            ADC_Sample_PrintRaw();
        }
        s_data_ready = 0U;
        if (s_mode == ADC_MODE_CONTINUOUS) {
            s_half_ready = (s_half_ready == ADC_HALF_FIRST) ? ADC_HALF_SECOND : ADC_HALF_FIRST;
        } else {
            s_half_ready = ADC_HALF_NONE;
            s_started = 0U;
        }
    }
}

/* 配置缓冲区同步保护。 */
void ADC_Sample_ConfigSync(uint8_t enable)
{
    s_sync_enable = (enable != 0U) ? 1U : 0U;
}

/* 选择业务层读取的有效通道。 */
HAL_StatusTypeDef ADC_Sample_SetActiveChannel(uint8_t channel)
{
    if (channel >= ADC_APP_CHANNEL_COUNT) {
        return HAL_ERROR;
    }
    s_active_channel = channel;
    return HAL_OK;
}

/* 返回当前有效通道。 */
uint8_t ADC_Sample_GetActiveChannel(void)
{
    return s_active_channel;
}

/* 返回当前目标采样率。 */
float ADC_Sample_GetSampleRateHz(void)
{
    return s_sample_rate_hz;
}

/* 查询完整数据帧是否就绪。 */
uint8_t ADC_Sample_DataReady(void)
{
    return s_data_ready;
}

/* 返回兼容接口的半缓冲状态。 */
ADC_HalfReady_t ADC_Sample_GetReadyHalf(void)
{
    ADC_HalfReady_t ready = s_half_ready;

    if (s_mode == ADC_MODE_CONTINUOUS) {
        s_half_ready = ADC_HALF_NONE;
    }
    return ready;
}

/* 锁定电压缓存以供同步读取。 */
void ADC_Sample_BufferLock(void)
{
    if (s_sync_enable != 0U) {
        s_buffer_locked = 1U;
    }
}

/* 解除电压缓存锁定。 */
void ADC_Sample_BufferUnlock(void)
{
    s_buffer_locked = 0U;
}

/* 返回并清除缓冲锁定期间的超限计数。 */
uint8_t ADC_Sample_IsOverrun(void)
{
    uint8_t count = s_overrun_count;
    s_overrun_count = 0U;
    return (s_sync_enable != 0U) ? count : 0U;
}

/* 返回原始码缓存只读指针。 */
const uint16_t *ADC_Sample_GetRawData(void)
{
    return s_raw_buffer;
}

/* 返回当前通道电压缓存只读指针。 */
const float *ADC_Sample_GetVoltageData(void)
{
    return s_voltage_buffer[s_active_channel];
}

/* 返回指定通道电压缓存只读指针。 */
const float *ADC_Sample_GetVoltageDataByChannel(uint8_t channel)
{
    if (channel >= ADC_APP_CHANNEL_COUNT) {
        return s_voltage_buffer[0];
    }
    return s_voltage_buffer[channel];
}

/* 通过重定向串口输出当前帧原始码。 */
void ADC_Sample_PrintRaw(void)
{
    for (uint32_t i = 0U; i < ADC_APP_SAMPLE_LENGTH; i++) {
        printf("%u\r\n", (unsigned int)s_raw_buffer[i]);
    }
}
