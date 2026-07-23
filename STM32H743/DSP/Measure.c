/**
 * @file    Measure.c
 * @brief   信号测量指标实现
 */
#include "Measure.h"
#include <math.h>
#include <string.h>

#define MEASURE_MAX_HALF 4097u /* 支持的最大单边频谱长度。 */
#define MEASURE_USED_WORDS ((MEASURE_MAX_HALF + 31u) / 32u) /* 频点标记位图字数。 */

/* 判断浮点测量值是否有效。 */
static int Measure_IsFinite(float x)
{
    return isfinite(x) ? 1 : 0;
}

/* ====================== 时域测量 ====================== */

/* 计算有限样本的均值。 */
float Measure_Mean(const float *data, uint32_t len)
{
    if (data == NULL || len == 0u) return 0.0f;
    float s = 0.0f;
    uint32_t count = 0u;
    for (uint32_t i = 0; i < len; i++) {
        if (Measure_IsFinite(data[i])) { s += data[i]; count++; }
    }
    return (count > 0u) ? (s / (float)count) : 0.0f;
}

/* 计算有限样本的均方根值。 */
float Measure_RMS(const float *data, uint32_t len)
{
    if (data == NULL || len == 0u) return 0.0f;
    float s = 0.0f;
    uint32_t count = 0u;
    for (uint32_t i = 0; i < len; i++) {
        if (Measure_IsFinite(data[i])) { s += data[i] * data[i]; count++; }
    }
    return (count > 0u) ? sqrtf(s / (float)count) : 0.0f;
}

/* 计算去除直流后的交流均方根值。 */
float Measure_ACRMS(const float *data, uint32_t len)
{
    float dc = Measure_Mean(data, len);
    if (data == NULL || len == 0u) return 0.0f;
    float s = 0.0f;
    uint32_t count = 0u;
    for (uint32_t i = 0; i < len; i++) {
        if (!Measure_IsFinite(data[i])) continue;
        float ac = data[i] - dc;
        s += ac * ac;
        count++;
    }
    return (count > 0u) ? sqrtf(s / (float)count) : 0.0f;
}

/* 计算有限样本的峰峰值。 */
float Measure_Vpp(const float *data, uint32_t len)
{
    if (data == NULL || len == 0u) return 0.0f;
    uint32_t first = 0u;
    while (first < len && !Measure_IsFinite(data[first])) first++;
    if (first == len) return 0.0f;
    float vmin = data[first], vmax = data[first];
    for (uint32_t i = first + 1u; i < len; i++) {
        if (!Measure_IsFinite(data[i])) continue;
        if (data[i] > vmax) vmax = data[i];
        if (data[i] < vmin) vmin = data[i];
    }
    return vmax - vmin;
}

/* 一次计算直流、总RMS、交流RMS和峰峰值。 */
void Measure_TimeStats(const float *data, uint32_t len,
                       float *dc, float *rms, float *acrms, float *vpp)
{
    if (data == NULL || len == 0u) {
        if (dc)    *dc    = 0.0f;
        if (rms)   *rms   = 0.0f;
        if (acrms) *acrms = 0.0f;
        if (vpp)   *vpp   = 0.0f;
        return;
    }

    float sum = 0.0f, sumsq = 0.0f;
    float vmin = 0.0f, vmax = 0.0f;
    uint32_t count = 0u;
    for (uint32_t i = 0; i < len; i++) {
        float v = data[i];
        if (!Measure_IsFinite(v)) continue;
        if (count == 0u) vmin = vmax = v;
        sum   += v;
        sumsq += v * v;
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
        count++;
    }
    if (count == 0u) {
        if (dc) *dc = 0.0f;
        if (rms) *rms = 0.0f;
        if (acrms) *acrms = 0.0f;
        if (vpp) *vpp = 0.0f;
        return;
    }

    float mean    = sum / (float)count;
    float meansq  = sumsq / (float)count;
    float ac_var  = meansq - mean * mean;       /* 方差 = E[x²]-E[x]² */
    if (ac_var < 0.0f) ac_var = 0.0f;           /* 数值误差兜底 */

    if (dc)    *dc    = mean;
    if (rms)   *rms   = sqrtf(meansq);
    if (acrms) *acrms = sqrtf(ac_var);
    if (vpp)   *vpp   = vmax - vmin;
}

/* ====================== 频域精测（谱插值） ====================== */

/* 对指定频谱峰执行三点抛物线插值。 */
void Measure_PeakInterp(const float *mag, uint32_t n, uint32_t k_peak,
                        float fs, Measure_Peak_t *res)
{
    if (res == NULL) return;
    res->bin = 0.0f;
    res->freq = 0.0f;
    res->amplitude = 0.0f;
    if (mag == NULL || n == 0u || !(fs > 0.0f)) return;
    if (k_peak == 0u || k_peak >= (n / 2u)) {
        /* 边界无法插值，退化为整数 bin 结果 */
        res->bin = (float)k_peak;
        res->freq = (float)k_peak * fs / (float)n;
        res->amplitude = (k_peak < (n / 2u + 1u)) ? mag[k_peak] : 0.0f;
        return;
    }

    /* 抛物线插值：用峰值及左右相邻 bin 拟合二次曲线，求顶点偏移 delta */
    float a = mag[k_peak - 1u];
    float b = mag[k_peak];
    float c = mag[k_peak + 1u];

    float denom = (a - 2.0f * b + c);
    float delta = 0.0f;
    if (fabsf(denom) > 1e-12f) {
        delta = 0.5f * (a - c) / denom;         /* delta ∈ (-0.5, 0.5) */
    }
    if (delta > 0.5f)  delta = 0.5f;
    if (delta < -0.5f) delta = -0.5f;

    float bin = (float)k_peak + delta;
    res->bin = bin;
    res->freq = bin * fs / (float)n;
    /* 顶点幅度修正（抛物线顶点值），抑制栅栏效应造成的幅度低估 */
    res->amplitude = b - 0.25f * (a - c) * delta;
}

/* 查找最大非直流频谱峰并执行插值。 */
uint32_t Measure_FindPeak(const float *mag, uint32_t n, float fs,
                          Measure_Peak_t *res)
{
    if (mag == NULL || n < 4u || !(fs > 0.0f)) {
        if (res) { res->freq = 0.0f; res->amplitude = 0.0f; res->bin = 0.0f; }
        return 0u;
    }

    /* 从 bin1 起找最大峰，跳过直流 bin0 */
    uint32_t k_max = 1u;
    int found = Measure_IsFinite(mag[1]);
    float v_max = found ? mag[1] : 0.0f;
    uint32_t half = n / 2u;
    for (uint32_t k = 2u; k < half; k++) {
        if (Measure_IsFinite(mag[k]) && (!found || mag[k] > v_max)) {
            v_max = mag[k]; k_max = k; found = 1;
        }
    }
    if (!found) {
        if (res) { res->freq = 0.0f; res->amplitude = 0.0f; res->bin = 0.0f; }
        return 0u;
    }

    Measure_PeakInterp(mag, n, k_max, fs, res);
    return k_max;
}

/* ====================== 信号质量指标 ====================== */

/* 根据基波和谐波幅度计算总谐波失真。 */
float Measure_THD(const float *amp, uint32_t count, float *thd_db)
{
    if (amp == NULL || count < 2u || !Measure_IsFinite(amp[0]) || amp[0] <= 0.0f) {
        if (thd_db) *thd_db = -999.0f;
        return 0.0f;
    }
    float harm_sq = 0.0f;
    for (uint32_t h = 1u; h < count; h++) {     /* 从 2 次谐波(下标1)起 */
        if (Measure_IsFinite(amp[h])) harm_sq += amp[h] * amp[h];
    }
    float thd = sqrtf(harm_sq) / amp[0];
    if (thd_db) *thd_db = 20.0f * log10f(thd > 1e-12f ? thd : 1e-12f);
    return thd;
}

/* 查询频点是否已计入某个功率分量。 */
static int Measure_UsedGet(const uint32_t *used, uint32_t index)
{
    return (used[index / 32u] & (1UL << (index % 32u))) != 0u;
}

/* 将频点标记为已计入功率分量。 */
static void Measure_UsedSet(uint32_t *used, uint32_t index)
{
    used[index / 32u] |= (1UL << (index % 32u));
}

/* 聚合中心频点附近的功率并标记已用频点。 */
static float Measure_ClusterPower(const float *mag, uint32_t half_len,
                                  uint32_t center, uint32_t leak_bins,
                                  uint32_t *used)
{
    float p = 0.0f;
    uint32_t lo = (center > leak_bins) ? (center - leak_bins) : 1u; /* 不含直流 */
    uint32_t hi = (leak_bins >= (half_len - 1u - center)) ?
                  (half_len - 1u) : (center + leak_bins);
    for (uint32_t k = lo; k <= hi; k++) {
        if (Measure_UsedGet(used, k)) continue;
        if (Measure_IsFinite(mag[k])) p += mag[k] * mag[k];
        Measure_UsedSet(used, k);
    }
    return p;
}

/* 由单边幅度谱计算THD、SNR、SINAD和ENOB。 */
void Measure_Quality(const float *mag, uint32_t n, float fs,
                     uint32_t num_harm, uint32_t leak_bins,
                     Measure_Quality_t *q)
{
    if (q == NULL) return;
    q->thd = 0.0f; q->thd_db = -999.0f;
    q->snr_db = 0.0f; q->sinad_db = 0.0f; q->enob = 0.0f; q->fund_freq = 0.0f;
    if (mag == NULL || n < 8u || !(fs > 0.0f)) return;

    const uint32_t half_len = n / 2u + 1u;      /* 单边谱长度 */
    if (half_len > MEASURE_MAX_HALF) return;

    /* 局部位图替代静态字节数组：函数保持可重入，同时把栈占用控制在约 516 B。 */
    uint32_t used[MEASURE_USED_WORDS];
    const uint32_t mark_len = half_len;
    memset(used, 0, sizeof(used));

    /* 1) 找基波 bin（跳过直流） */
    uint32_t k0 = 1u;
    int found = Measure_IsFinite(mag[1]);
    float vmax = found ? mag[1] : 0.0f;
    for (uint32_t k = 2u; k < (n / 2u); k++) {
        if (Measure_IsFinite(mag[k]) && (!found || mag[k] > vmax)) {
            vmax = mag[k]; k0 = k; found = 1;
        }
    }
    if (!found) return;
    q->fund_freq = (float)k0 * fs / (float)n;

    /* 2) 总功率（去直流，bin0 不计） */
    float p_total = 0.0f;
    for (uint32_t k = 1u; k < half_len && k < MEASURE_MAX_HALF; k++) {
        if (Measure_IsFinite(mag[k])) p_total += mag[k] * mag[k];
    }

    /* 3) 基波功率（聚合主瓣泄漏） */
    float p_sig = Measure_ClusterPower(mag, mark_len, k0, leak_bins, used);

    /* 4) 各次谐波功率 */
    float p_harm = 0.0f;
    for (uint32_t h = 2u; h <= num_harm; h++) {
        if (k0 > ((n / 2u) - 1u) / h) break;
        uint32_t kh = k0 * h;
        if (kh >= (n / 2u)) break;              /* 超出奈奎斯特则停止 */
        p_harm += Measure_ClusterPower(mag, mark_len, kh, leak_bins, used);
    }

    /* 5) 噪声功率 = 总 - 基波 - 谐波；噪声+失真 = 总 - 基波 */
    float p_noise   = p_total - p_sig - p_harm;
    float p_noisdist = p_total - p_sig;
    if (p_noise   < 1e-20f) p_noise   = 1e-20f;
    if (p_noisdist < 1e-20f) p_noisdist = 1e-20f;
    if (p_sig     < 1e-20f) p_sig     = 1e-20f;

    /* 6) 指标 */
    q->thd      = (p_harm > 0.0f) ? (sqrtf(p_harm) / sqrtf(p_sig)) : 0.0f;
    q->thd_db   = 20.0f * log10f(q->thd > 1e-12f ? q->thd : 1e-12f);
    q->snr_db   = 10.0f * log10f(p_sig / p_noise);
    q->sinad_db = 10.0f * log10f(p_sig / p_noisdist);
    q->enob     = (q->sinad_db - 1.76f) / 6.02f;

}

/* ====================== 时域波形参数 ====================== */

/* 通过上升过零间隔估计信号频率。 */
float Measure_FreqZeroCross(const float *data, uint32_t len, float fs)
{
    if (data == NULL || len < 2u || fs <= 0.0f) return 0.0f;
    for (uint32_t i = 0u; i < len; i++) {
        if (!Measure_IsFinite(data[i])) return 0.0f;
    }

    float mean = Measure_Mean(data, len);

    float first_cross = 0.0f, last_cross = 0.0f;
    uint32_t count = 0u;

    for (uint32_t i = 1; i < len; i++) {
        float prev = data[i - 1] - mean;
        float cur  = data[i]     - mean;
        /* 上升过零：prev<0 且 cur>=0 */
        if (prev < 0.0f && cur >= 0.0f) {
            float denom = cur - prev;
            float frac  = (denom != 0.0f) ? (-prev / denom) : 0.0f; /* 线性插值 */
            float pos   = (float)(i - 1) + frac;
            if (count == 0u) first_cross = pos;
            last_cross = pos;
            count++;
        }
    }

    if (count < 2u) return 0.0f;
    float period_samples = (last_cross - first_cross) / (float)(count - 1u);
    if (period_samples <= 0.0f) return 0.0f;
    return fs / period_samples;
}

/* 根据上下电平中点计算占空比。 */
float Measure_DutyCycle(const float *data, uint32_t len)
{
    if (data == NULL || len == 0u) return 0.0f;
    for (uint32_t i = 0u; i < len; i++) {
        if (!Measure_IsFinite(data[i])) return 0.0f;
    }
    float vmin = data[0], vmax = data[0];
    for (uint32_t i = 1; i < len; i++) {
        if (data[i] > vmax) vmax = data[i];
        if (data[i] < vmin) vmin = data[i];
    }
    float mid = 0.5f * (vmax + vmin);
    uint32_t high = 0u;
    for (uint32_t i = 0; i < len; i++) {
        if (data[i] >= mid) high++;
    }
    return (float)high / (float)len;
}

/* 查找指定阈值的首个上升沿或下降沿位置。 */
int32_t Measure_Trigger(const float *data, uint32_t len, float level, int rising)
{
    if (data == NULL || len < 2u) return -1;
    for (uint32_t i = 1; i < len; i++) {
        if (rising) {
            if (data[i - 1] < level && data[i] >= level) return (int32_t)i;
        } else {
            if (data[i - 1] > level && data[i] <= level) return (int32_t)i;
        }
    }
    return -1;
}

/* 统计指定电压范围内的直方图。 */
void Measure_Histogram(const float *data, uint32_t len, float vmin, float vmax,
                       uint32_t *bins, uint32_t num_bins)
{
    if (data == NULL || bins == NULL || num_bins == 0u) return;
    for (uint32_t b = 0; b < num_bins; b++) bins[b] = 0u;

    if (!Measure_IsFinite(vmin) || !Measure_IsFinite(vmax)) return;
    float span = vmax - vmin;
    if (span <= 0.0f) return;
    float inv = (float)num_bins / span;

    for (uint32_t i = 0; i < len; i++) {
        if (!Measure_IsFinite(data[i])) continue;
        int32_t idx = (int32_t)((data[i] - vmin) * inv);
        if (idx < 0) idx = 0;
        if (idx >= (int32_t)num_bins) idx = (int32_t)num_bins - 1;
        bins[idx]++;
    }
}

/* 用直方图双峰法估计顶电平和底电平。 */
static void Measure_TopBase(const float *data, uint32_t len,
                            float vmin, float vmax, float *vtop, float *vbase)
{
    #define MEAS_HIST_BINS 64u /* 顶底电平估计的直方图箱数。 */
    uint32_t hist[MEAS_HIST_BINS];
    Measure_Histogram(data, len, vmin, vmax, hist, MEAS_HIST_BINS);

    float span = vmax - vmin;
    float bin_w = span / (float)MEAS_HIST_BINS;

    /* 下半区找众数 -> vbase */
    uint32_t lo_max = 0u, lo_idx = 0u;
    for (uint32_t b = 0; b < MEAS_HIST_BINS / 2u; b++) {
        if (hist[b] > lo_max) { lo_max = hist[b]; lo_idx = b; }
    }
    /* 上半区找众数 -> vtop */
    uint32_t hi_max = 0u, hi_idx = MEAS_HIST_BINS - 1u;
    for (uint32_t b = MEAS_HIST_BINS / 2u; b < MEAS_HIST_BINS; b++) {
        if (hist[b] > hi_max) { hi_max = hist[b]; hi_idx = b; }
    }

    *vbase = vmin + ((float)lo_idx + 0.5f) * bin_w;
    *vtop  = vmin + ((float)hi_idx + 0.5f) * bin_w;
    #undef MEAS_HIST_BINS
}

/* 从指定位置起查找首个阈值穿越的插值位置。 */
static float Measure_FindCross(const float *data, uint32_t len, uint32_t start,
                               float level, int dir)
{
    for (uint32_t i = (start > 0u ? start : 1u); i < len; i++) {
        float a = data[i - 1], b = data[i];
        int hit = (dir > 0) ? (a < level && b >= level)
                            : (a > level && b <= level);
        if (hit) {
            float denom = b - a;
            float frac = (denom != 0.0f) ? ((level - a) / denom) : 0.0f;
            return (float)(i - 1) + frac;
        }
    }
    return -1.0f;
}

/* 计算一帧波形的幅度、频率、占空比和边沿参数。 */
void Measure_Waveform(const float *data, uint32_t len, float fs, Measure_Wave_t *res)
{
    if (res == NULL) return;
    /* 所有字段都清零，避免参数错误时留下调用者栈上的旧值。 */
    memset(res, 0, sizeof(*res));
    if (data == NULL || len < 2u || fs <= 0.0f) return;
    for (uint32_t i = 0u; i < len; i++) {
        if (!Measure_IsFinite(data[i])) return;
    }

    /* 基础统计：min/max/mean/rms + 交流整流均值/交流峰值 */
    float vmin = data[0], vmax = data[0], sum = 0.0f, sumsq = 0.0f;
    for (uint32_t i = 0; i < len; i++) {
        float v = data[i];
        sum += v; sumsq += v * v;
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
    }
    float mean = sum / (float)len;
    res->vmax = vmax;
    res->vmin = vmin;
    res->vpp  = vmax - vmin;
    res->mean = mean;
    res->rms  = sqrtf(sumsq / (float)len);

    float arv = 0.0f, peak_ac = 0.0f;     /* 交流整流均值 / 交流峰值 */
    float acsq = 0.0f;
    for (uint32_t i = 0; i < len; i++) {
        float ac = data[i] - mean;
        float aac = fabsf(ac);
        arv += aac;
        acsq += ac * ac;
        if (aac > peak_ac) peak_ac = aac;
    }
    arv /= (float)len;
    float rms_ac = sqrtf(acsq / (float)len);
    res->form_factor  = (arv > 1e-9f) ? (rms_ac / arv) : 0.0f;       /* 正弦≈1.111 */
    res->crest_factor = (rms_ac > 1e-9f) ? (peak_ac / rms_ac) : 0.0f; /* 正弦≈1.414 */

    /* 频率/周期（过零法） */
    res->freq = Measure_FreqZeroCross(data, len, fs);
    res->period = (res->freq > 0.0f) ? (1.0f / res->freq) : 0.0f;

    /* 顶/底电平（直方图法） */
    Measure_TopBase(data, len, vmin, vmax, &res->vtop, &res->vbase);
    res->amplitude = res->vtop - res->vbase;

    /* 占空比 */
    res->duty = Measure_DutyCycle(data, len);

    /* 过冲：相对顶电平 */
    if (res->amplitude > 1e-9f) {
        res->overshoot = (vmax - res->vtop) / res->amplitude * 100.0f;
        if (res->overshoot < 0.0f) res->overshoot = 0.0f;
    }

    /* 上升/下降时间：10%~90% */
    if (res->amplitude > 1e-9f) {
        float lo = res->vbase + 0.1f * res->amplitude;
        float hi = res->vbase + 0.9f * res->amplitude;

        /* 上升沿：先过 10% 再过 90% */
        float t_lo = Measure_FindCross(data, len, 1u, lo, +1);
        if (t_lo >= 0.0f) {
            float t_hi = Measure_FindCross(data, len, (uint32_t)t_lo + 1u, hi, +1);
            if (t_hi > t_lo) res->rise_time = (t_hi - t_lo) / fs;
        }
        /* 下降沿：先过 90% 再过 10% */
        float t_hi2 = Measure_FindCross(data, len, 1u, hi, -1);
        if (t_hi2 >= 0.0f) {
            float t_lo2 = Measure_FindCross(data, len, (uint32_t)t_hi2 + 1u, lo, -1);
            if (t_lo2 > t_hi2) res->fall_time = (t_lo2 - t_hi2) / fs;
        }
    }
}
