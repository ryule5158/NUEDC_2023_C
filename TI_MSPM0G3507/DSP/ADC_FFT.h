/**
 * @file    ADC_FFT.h
 * @brief   Generic FFT analysis for ADC sample buffers.
 */

#ifndef __ADC_FFT_H
#define __ADC_FFT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FFT.h"
#include <stdint.h>

#define ADC_FFT_SAMPLE_LENGTH            FFT_length
#define ADC_FFT_MAX_HARMONICS            FFT_MAX_HARMONICS
#define ADC_FFT_STATUS_OK                0U
#define ADC_FFT_STATUS_ERROR             1U

typedef struct {
    float   fundamental_hz;
    uint8_t harmonic_count;
    uint8_t harmonic_fft_count;
    float   harmonic_freq[ADC_FFT_MAX_HARMONICS];
    float   harmonic_amp[ADC_FFT_MAX_HARMONICS];
    float   harmonic_freq_fft[ADC_FFT_MAX_HARMONICS];
    float   harmonic_amp_fft[ADC_FFT_MAX_HARMONICS];
    uint32_t frame_count;
    uint8_t status;
} ADC_FFT_Result_t;

/* Backward-compatible result type used by the STM32 on-chip ADC wrapper. */
typedef ADC_FFT_Result_t ADC_SampleResult_t;

void ADC_FFT_SetMaxHarmonic(float max_harmonic_hz);
uint8_t ADC_FFT_Run(const float *data, uint32_t len, float sample_rate_hz);
uint8_t ADC_FFT_RunWithMaxHarmonic(const float *data,
                                   uint32_t len,
                                   float sample_rate_hz,
                                   float max_harmonic_hz);
const ADC_FFT_Result_t* ADC_FFT_GetResult(void);
void ADC_FFT_PrintResult(const ADC_FFT_Result_t *result);

/* Compatibility API for BSP/adc_app users. */
void ADC_Sample_RunFft(void);
void ADC_Sample_PrintResult(void);
void ADC_Sample_SetFftMaxHarmonic(float max_harmonic_hz);
const ADC_SampleResult_t* ADC_Sample_GetResult(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_FFT_H */
