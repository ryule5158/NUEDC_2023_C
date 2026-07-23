#ifndef ARM_MATH_H
#define ARM_MATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef float float32_t; /* CMSIS-DSP兼容的单精度类型。 */

/* 复数FFT长度描述符。 */
typedef struct {
    uint16_t fftLen;
} arm_cfft_instance_f32;

/* 单精度FIR滤波器实例。 */
typedef struct {
    uint16_t numTaps;
    float32_t *pState;
    const float32_t *pCoeffs;
    uint32_t blockSize;
} arm_fir_instance_f32;

/* 计算单精度正弦值，兼容公共高速DAC驱动使用的CMSIS-DSP接口。 */
float32_t arm_sin_f32(float32_t x);

/* 执行512点以内的兼容复数FFT。 */
void arm_cfft_f32(const arm_cfft_instance_f32 *S,
                  float32_t *p1,
                  uint8_t ifftFlag,
                  uint8_t bitReverseFlag);
/* 计算交错复数数组的幅值。 */
void arm_cmplx_mag_f32(const float32_t *pSrc,
                       float32_t *pDst,
                       uint32_t numSamples);
/* 查找数组最大值及其索引。 */
void arm_max_f32(const float32_t *pSrc,
                 uint32_t blockSize,
                 float32_t *pResult,
                 uint32_t *pIndex);
/* 初始化单精度FIR滤波器。 */
void arm_fir_init_f32(arm_fir_instance_f32 *S,
                      uint16_t numTaps,
                      const float32_t *pCoeffs,
                      float32_t *pState,
                      uint32_t blockSize);
/* 执行单精度FIR滤波。 */
void arm_fir_f32(const arm_fir_instance_f32 *S,
                 const float32_t *pSrc,
                 float32_t *pDst,
                 uint32_t blockSize);

#ifdef __cplusplus
}
#endif

#endif /* ARM_MATH_H */
