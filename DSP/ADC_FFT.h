/**
 * @file    ADC_FFT.h
 * @brief   ADC采样缓冲区通用FFT分析接口。
 */

#ifndef __ADC_FFT_H
#define __ADC_FFT_H /* ADC频谱分析头文件包含保护。 */

#ifdef __cplusplus
extern "C" {
#endif

#include "FFT.h"
#include <stdint.h>

#define ADC_FFT_SAMPLE_LENGTH FFT_length       /* FFT固定采样点数。 */
#define ADC_FFT_MAX_HARMONICS FFT_MAX_HARMONICS /* 最大谐波数量。 */
#define ADC_FFT_STATUS_OK 0U                   /* FFT分析成功状态。 */
#define ADC_FFT_STATUS_ERROR 1U                /* FFT分析失败状态。 */

/* 一帧ADC频谱分析结果。 */
typedef struct {
    float fundamental_hz; /* 基波频率，单位Hz。 */
    uint8_t harmonic_count; /* 时域提取的谐波数量。 */
    uint8_t harmonic_fft_count; /* 频谱提取的谐波数量。 */
    float harmonic_freq[ADC_FFT_MAX_HARMONICS]; /* 时域谐波频率。 */
    float harmonic_amp[ADC_FFT_MAX_HARMONICS]; /* 时域谐波幅度。 */
    float harmonic_freq_fft[ADC_FFT_MAX_HARMONICS]; /* 频谱谐波频率。 */
    float harmonic_amp_fft[ADC_FFT_MAX_HARMONICS]; /* 频谱谐波幅度。 */
    uint32_t frame_count; /* 已处理帧数。 */
    uint8_t status; /* 最近一次分析状态。 */
} ADC_FFT_Result_t;

/* STM32片内ADC兼容结果类型。 */
typedef ADC_FFT_Result_t ADC_SampleResult_t;

/* 设置默认最高谐波频率。 */
void ADC_FFT_SetMaxHarmonic(float max_harmonic_hz);
/* 使用默认最高谐波频率分析一帧数据。 */
uint8_t ADC_FFT_Run(const float *data, uint32_t len, float sample_rate_hz);
/* 使用指定最高谐波频率分析一帧数据。 */
uint8_t ADC_FFT_RunWithMaxHarmonic(const float *data,
                                   uint32_t len,
                                   float sample_rate_hz,
                                   float max_harmonic_hz);
/* 获取最近一次FFT分析结果。 */
const ADC_FFT_Result_t* ADC_FFT_GetResult(void);
/* 打印FFT分析结果。 */
void ADC_FFT_PrintResult(const ADC_FFT_Result_t *result);

/* 分析片内ADC最近一帧数据。 */
void ADC_Sample_RunFft(void);
/* 打印片内ADC最近一次FFT结果。 */
void ADC_Sample_PrintResult(void);
/* 设置片内ADC包装层的最高谐波频率。 */
void ADC_Sample_SetFftMaxHarmonic(float max_harmonic_hz);
/* 获取片内ADC最近一次FFT结果。 */
const ADC_SampleResult_t* ADC_Sample_GetResult(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_FFT_H */
