/**
 * @file    Measure.h
 * @brief   信号测量指标 — 时域(RMS/DC/Vpp) + 频域精测(插值测频) + 质量指标(THD/SNR/SINAD/ENOB)
 * @note    本模块是工程已有 FFT 模块的"测量层"补充，不重复造 FFT：
 *            - 时域：真有效值、直流、交流有效值、峰峰值；
 *            - 频域：在已算好的幅度谱上做抛物线插值，把测频/测幅精度从"bin 分辨率"
 *              提升到 bin 的几十分之一（无需加大 FFT 点数）；
 *            - 质量：基于幅度谱计算 THD / SNR / SINAD / ENOB（ADC 性能、失真测量常用）。
 *          所有函数为纯计算、无静态状态、无动态内存。幅度谱沿用 FFT 单边幅度
 *          （线性幅度，DSP/FFT.c 的 FFT_output[0..N/2] 即可直接传入）。
 * @date    2026-06-18
 */

#ifndef __MEASURE_H
#define __MEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "arm_math.h"
#include <stdint.h>

/* ====================== 时域测量 ====================== */

/** @brief 算术平均(直流分量 DC)。 */
float Measure_Mean(const float *data, uint32_t len);

/** @brief 真有效值 RMS = sqrt(mean(x²))（含直流）。 */
float Measure_RMS(const float *data, uint32_t len);

/** @brief 交流有效值 AC-RMS = sqrt(RMS² - DC²)（去掉直流后的有效值）。 */
float Measure_ACRMS(const float *data, uint32_t len);

/** @brief 峰峰值 Vpp = max - min。 */
float Measure_Vpp(const float *data, uint32_t len);

/**
 * @brief  一次遍历同时给出 DC / RMS / AC-RMS / Vpp（省 CPU）。
 * @param  dc,rms,acrms,vpp  [out] 可任意为 NULL 表示不需要该项
 */
void Measure_TimeStats(const float *data, uint32_t len,
                       float *dc, float *rms, float *acrms, float *vpp);

/* ====================== 频域精测（谱插值） ====================== */

/* 谱峰插值结果 */
typedef struct {
    float freq;       /* 精确频率 (Hz) */
    float amplitude;  /* 精确幅度（与输入幅度谱同量纲） */
    float bin;        /* 插值后的小数 bin 位置 */
} Measure_Peak_t;

/**
 * @brief  在单边幅度谱上，对给定峰值 bin 做抛物线(二次)插值，得到亚 bin 精度的频率与幅度。
 * @param  mag        单边幅度谱（线性幅度），长度至少 n/2+1
 * @param  n          FFT 点数
 * @param  k_peak     峰值所在的整数 bin（可由 arm_max_f32 求得，需 1<=k_peak<=n/2-1）
 * @param  fs         采样率 (Hz)
 * @param  res        [out] 插值结果
 * @note   抛物线插值对加窗(Hann/汉宁)后的主瓣拟合很好，是最常用的高精度测频法之一。
 */
void Measure_PeakInterp(const float *mag, uint32_t n, uint32_t k_peak,
                        float fs, Measure_Peak_t *res);

/**
 * @brief  自动找最大峰(跳过直流 bin0)并做插值测频测幅，一步到位。
 * @param  mag   单边幅度谱（线性幅度）
 * @param  n     FFT 点数
 * @param  fs    采样率 (Hz)
 * @param  res   [out] 结果
 * @return 峰值整数 bin；找不到有效峰返回 0
 */
uint32_t Measure_FindPeak(const float *mag, uint32_t n, float fs,
                          Measure_Peak_t *res);

/* ====================== 信号质量指标 ====================== */

/**
 * @brief  由谐波幅度数组计算总谐波失真 THD。
 * @param  amp    谐波幅度数组：amp[0]=基波，amp[1..]=2,3,4...次谐波
 *                （可直接用工程 FFT_GetHarmonics() 的 amp_out）
 * @param  count  数组长度（含基波），count>=2 才有意义
 * @param  thd_db [out,可空] THD 的 dB 值 = 20log10(THD)
 * @return THD 比值 = sqrt(ΣAh²)/A1 (h>=2)；以百分比显示时再 *100。
 */
float Measure_THD(const float *amp, uint32_t count, float *thd_db);

/* 频谱质量指标结果 */
typedef struct {
    float thd;      /* 总谐波失真（比值） */
    float thd_db;   /* THD (dB) */
    float snr_db;   /* 信噪比 (dB)，仅噪声、不含谐波 */
    float sinad_db; /* 信纳比 SINAD (dB)，噪声+谐波 */
    float enob;     /* 有效位数 = (SINAD-1.76)/6.02 */
    float fund_freq;/* 基波频率 (Hz) */
} Measure_Quality_t;

/**
 * @brief  由单边幅度谱综合计算 THD/SNR/SINAD/ENOB（ADC 动态性能评估常用）。
 * @param  mag        单边幅度谱（线性幅度），长度 n/2+1
 * @param  n          FFT 点数
 * @param  fs         采样率 (Hz)
 * @param  num_harm   计入失真的谐波次数（如 5 表示统计 2~5 次谐波）
 * @param  leak_bins  每个谱峰因加窗泄漏占据的"半宽"bin 数（不加窗填 0~1，Hann 窗填 2~3）
 * @param  q          [out] 质量指标结果
 * @note   做法：以最大峰为基波；基波/各次谐波各取 ±leak_bins 个 bin 聚为该分量功率；
 *         其余非直流 bin 计为噪声。请保证窗内为多周期、信噪比合理时结果才可信。
 */
void Measure_Quality(const float *mag, uint32_t n, float fs,
                     uint32_t num_harm, uint32_t leak_bins,
                     Measure_Quality_t *q);

/* ====================== 时域波形参数（示波器/频率计） ====================== */

/**
 * @brief  过零检测测频 —— 低频/非平稳信号比 FFT 更准，且不受频率分辨率限制。
 * @param  data  输入采样(任意波形，内部自动以均值为门限)
 * @param  len   采样点数
 * @param  fs    采样率 (Hz)
 * @return 频率 (Hz)；检测不到完整周期返回 0。
 * @note   只统计"上升过零"，并对过零点做线性插值得到亚采样精度；噪声大时请先滤波。
 */
float Measure_FreqZeroCross(const float *data, uint32_t len, float fs);

/**
 * @brief  占空比测量 —— 适用于方波/脉冲，门限取(顶+底)/2。
 * @return 占空比 0~1（高电平时间占比）。
 */
float Measure_DutyCycle(const float *data, uint32_t len);

/* 完整波形参数 */
typedef struct {
    float freq;         /* 频率 (Hz, 过零法) */
    float period;       /* 周期 (s) */
    float vmax, vmin;   /* 绝对最大/最小 */
    float vpp;          /* 峰峰值 */
    float vtop, vbase;  /* 顶/底电平(直方图法，抗过冲，用于上升时间基准) */
    float amplitude;    /* 幅度 = vtop - vbase */
    float mean, rms;    /* 均值 / 有效值 */
    float duty;         /* 占空比 0~1 */
    float rise_time;    /* 上升时间 10%->90% (s) */
    float fall_time;    /* 下降时间 90%->10% (s) */
    float overshoot;    /* 过冲 % = (vmax-vtop)/amplitude*100 */
    float form_factor;  /* 波形因数 = RMS / |平均(整流)| */
    float crest_factor; /* 峰值因数 = 峰值 / RMS */
} Measure_Wave_t;

/**
 * @brief  一次性提取常用波形参数（数字示波器自动测量那一栏）。
 * @param  data  输入采样
 * @param  len   采样点数
 * @param  fs    采样率 (Hz)
 * @param  res   [out] 结果
 * @note   vtop/vbase 用直方图双峰法估计，使上升时间/过冲在有振铃时也准确。
 */
void Measure_Waveform(const float *data, uint32_t len, float fs, Measure_Wave_t *res);

/**
 * @brief  边沿触发查找 —— 找到第一个穿越 level 的点，用于稳定波形显示/对齐帧。
 * @param  data    输入采样
 * @param  len     采样点数
 * @param  level   触发电平
 * @param  rising  1=上升沿触发，0=下降沿触发
 * @return 触发点索引；未找到返回 -1。
 */
int32_t Measure_Trigger(const float *data, uint32_t len, float level, int rising);

/**
 * @brief  幅度直方图 —— 噪声分布观察、ADC 码密度测试、顶/底电平估计。
 * @param  data      输入采样
 * @param  len       采样点数
 * @param  vmin,vmax 直方图量程(低于/高于者归入首/末 bin)
 * @param  bins      [out] 计数数组，长度 num_bins(调用者清零或由本函数清零)
 * @param  num_bins  bin 个数
 */
void Measure_Histogram(const float *data, uint32_t len, float vmin, float vmax,
                       uint32_t *bins, uint32_t num_bins);

#ifdef __cplusplus
}
#endif

#endif /* __MEASURE_H */
