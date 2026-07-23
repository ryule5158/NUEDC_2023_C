#ifndef __DSP_PROMAX_H
#define __DSP_PROMAX_H /* DSP ProMax聚合分析接口包含保护。 */

#ifdef __cplusplus
extern "C" {
#endif

#include "FFT.h"
#include "Measure.h"
#include <stdint.h>

/* DSP ProMax分析状态。 */
typedef enum
{
  DSP_PROMAX_OK = 0,          /* 时域和频域分析均已完成。 */
  DSP_PROMAX_ERROR_PARAM,     /* 输入指针、长度或采样率无效。 */
  DSP_PROMAX_ERROR_LENGTH     /* 时域分析完成，但长度不足以执行固定点数FFT。 */
} DSP_ProMaxStatusTypeDef;

/* DSP ProMax一站式分析结果。 */
typedef struct
{
  uint8_t time_valid;         /* 时域结果有效标志。 */
  uint8_t fft_valid;          /* 频域结果有效标志。 */
  uint8_t harmonic_count;     /* 已提取的谐波数量。 */

  float dc;                   /* 直流分量。 */
  float rms;                  /* 含直流的真有效值。 */
  float acrms;                /* 去除直流后的交流有效值。 */
  float vpp;                  /* 峰峰值。 */

  Measure_Wave_t wave;        /* 频率、周期、占空比和边沿等时域参数。 */
  Measure_Peak_t peak;        /* FFT主峰频率、幅度和插值后频点。 */
  Measure_Quality_t quality;  /* THD、SNR、SINAD和ENOB。 */

  float harmonic_freq[FFT_MAX_HARMONICS]; /* 各次谐波频率。 */
  float harmonic_amp[FFT_MAX_HARMONICS];  /* 各次谐波幅度。 */
} DSP_ProMaxResultTypeDef;

/*
 * 一次完成时域与频域分析。
 * data为输入采样，len为采样点数，fs_hz为采样率，result返回分析结果。
 * len等于FFT_length时执行完整FFT；其他长度只返回时域结果和长度错误状态。
 */
DSP_ProMaxStatusTypeDef DSP_ProMax_Analyze(const float *data,
                                           uint32_t len,
                                           float fs_hz,
                                           DSP_ProMaxResultTypeDef *result);

/* 获取最近一次分析结果的只读指针。 */
const DSP_ProMaxResultTypeDef *DSP_ProMax_GetLastResult(void);

/* 通过printf输出关键分析结果，result为NULL时输出最近一次结果。 */
void DSP_ProMax_PrintResult(const DSP_ProMaxResultTypeDef *result);

#ifdef __cplusplus
}
#endif

#endif /* __DSP_PROMAX_H */
