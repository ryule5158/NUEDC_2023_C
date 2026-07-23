#include "arm_const_structs.h"
#include <math.h>
#include <string.h>

#define ARM_MATH_LITE_MAX_CFFT 512U                  /* 轻量复数FFT最大点数。 */
#define ARM_MATH_LITE_PI       3.14159265358979323846f /* 单精度圆周率。 */

/* CMSIS-DSP兼容的固定FFT长度描述符。 */
const arm_cfft_instance_f32 arm_cfft_sR_f32_len64   = {64U};   /* 64点FFT描述符。 */
const arm_cfft_instance_f32 arm_cfft_sR_f32_len128  = {128U};  /* 128点FFT描述符。 */
const arm_cfft_instance_f32 arm_cfft_sR_f32_len256  = {256U};  /* 256点FFT描述符。 */
const arm_cfft_instance_f32 arm_cfft_sR_f32_len512  = {512U};  /* 512点FFT描述符。 */
const arm_cfft_instance_f32 arm_cfft_sR_f32_len1024 = {1024U}; /* 超限兼容描述符。 */
const arm_cfft_instance_f32 arm_cfft_sR_f32_len2048 = {2048U}; /* 超限兼容描述符。 */
const arm_cfft_instance_f32 arm_cfft_sR_f32_len4096 = {4096U}; /* 超限兼容描述符。 */

static float32_t s_cfft_work[ARM_MATH_LITE_MAX_CFFT * 2U]; /* 复数FFT工作缓存。 */

/* 使用C数学库实现公共驱动需要的单精度正弦接口。 */
float32_t arm_sin_f32(float32_t x)
{
    return sinf(x);
}

/* 使用直接DFT实现512点以内的兼容复数FFT。 */
void arm_cfft_f32(const arm_cfft_instance_f32 *S,
                  float32_t *p1,
                  uint8_t ifftFlag,
                  uint8_t bitReverseFlag)
{
    uint32_t n;

    (void)bitReverseFlag;

    if ((S == 0) || (p1 == 0)) {
        return;
    }

    n = S->fftLen;
    if ((n == 0U) || (n > ARM_MATH_LITE_MAX_CFFT)) {
        return;
    }

    for (uint32_t k = 0U; k < n; k++) {
        float32_t sum_re = 0.0f;
        float32_t sum_im = 0.0f;

        for (uint32_t t = 0U; t < n; t++) {
            float32_t angle = (2.0f * ARM_MATH_LITE_PI * (float32_t)k * (float32_t)t) / (float32_t)n;
            float32_t c = cosf(angle);
            float32_t s = sinf(angle);
            float32_t re = p1[2U * t];
            float32_t im = p1[(2U * t) + 1U];

            if (ifftFlag == 0U) {
                s = -s;
            }

            sum_re += (re * c) - (im * s);
            sum_im += (re * s) + (im * c);
        }

        if (ifftFlag != 0U) {
            sum_re /= (float32_t)n;
            sum_im /= (float32_t)n;
        }

        s_cfft_work[2U * k] = sum_re;
        s_cfft_work[(2U * k) + 1U] = sum_im;
    }

    memcpy(p1, s_cfft_work, sizeof(float32_t) * 2U * n);
}

/* 计算交错复数数组的幅值。 */
void arm_cmplx_mag_f32(const float32_t *pSrc,
                       float32_t *pDst,
                       uint32_t numSamples)
{
    if ((pSrc == 0) || (pDst == 0)) {
        return;
    }

    for (uint32_t i = 0U; i < numSamples; i++) {
        float32_t re = pSrc[2U * i];
        float32_t im = pSrc[(2U * i) + 1U];
        pDst[i] = sqrtf((re * re) + (im * im));
    }
}

/* 查找单精度数组的最大值及索引。 */
void arm_max_f32(const float32_t *pSrc,
                 uint32_t blockSize,
                 float32_t *pResult,
                 uint32_t *pIndex)
{
    float32_t max_value;
    uint32_t max_index = 0U;

    if ((pSrc == 0) || (pResult == 0) || (pIndex == 0) || (blockSize == 0U)) {
        return;
    }

    max_value = pSrc[0];
    for (uint32_t i = 1U; i < blockSize; i++) {
        if (pSrc[i] > max_value) {
            max_value = pSrc[i];
            max_index = i;
        }
    }

    *pResult = max_value;
    *pIndex = max_index;
}

/* 初始化轻量FIR实例并清零状态。 */
void arm_fir_init_f32(arm_fir_instance_f32 *S,
                      uint16_t numTaps,
                      const float32_t *pCoeffs,
                      float32_t *pState,
                      uint32_t blockSize)
{
    if (S == 0) {
        return;
    }

    S->numTaps = numTaps;
    S->pCoeffs = pCoeffs;
    S->pState = pState;
    S->blockSize = blockSize;

    if (pState != 0) {
        memset(pState, 0, sizeof(float32_t) * ((uint32_t)numTaps + blockSize - 1U));
    }
}

/* 对一个数据块执行轻量FIR滤波。 */
void arm_fir_f32(const arm_fir_instance_f32 *S,
                 const float32_t *pSrc,
                 float32_t *pDst,
                 uint32_t blockSize)
{
    uint32_t taps;

    if ((S == 0) || (pSrc == 0) || (pDst == 0) ||
        (S->pCoeffs == 0) || (S->pState == 0) || (S->numTaps == 0U)) {
        return;
    }

    taps = S->numTaps;
    for (uint32_t n = 0U; n < blockSize; n++) {
        float32_t acc = 0.0f;

        for (uint32_t i = taps - 1U; i > 0U; i--) {
            S->pState[i] = S->pState[i - 1U];
        }
        S->pState[0] = pSrc[n];

        for (uint32_t i = 0U; i < taps; i++) {
            acc += S->pState[i] * S->pCoeffs[i];
        }
        pDst[n] = acc;
    }
}
