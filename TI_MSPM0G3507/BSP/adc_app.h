#ifndef ADC_APP_H
#define ADC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define ADC_APP_SAMPLE_LENGTH 512U /* 单帧样点数。 */
#define ADC_APP_CHANNEL_COUNT 1U   /* 真实启用的片上ADC通道数。 */
#define ADC_APP_PRINT_RAW_OFF 0U   /* 关闭原始码打印。 */
#define ADC_APP_PRINT_RAW_ON  1U   /* 开启原始码打印。 */

/* 片上ADC采样模式。 */
typedef enum {
    ADC_MODE_ONESHOT = 0,   /* 软件定时完成一帧512点采样。 */
    ADC_MODE_CONTINUOUS = 1, /* 当前TI端无DMA，初始化时明确拒绝该模式。 */
} ADC_SampleMode_t;

/* 兼容双半缓冲业务的就绪状态。 */
typedef enum {
    ADC_HALF_NONE = 0,
    ADC_HALF_FIRST = 1,
    ADC_HALF_SECOND = 2,
} ADC_HalfReady_t;

HAL_StatusTypeDef ADC_Sample_Init(float vref_v,
                                  float sample_rate_hz,
                                  float max_harmonic_hz,
                                  uint8_t print_raw,
                                  ADC_SampleMode_t mode);
HAL_StatusTypeDef ADC_Sample_Start(void);
void ADC_Sample_Stop(void);
void ADC_Sample_Process(void);

void ADC_Sample_ConfigSync(uint8_t enable);
HAL_StatusTypeDef ADC_Sample_SetActiveChannel(uint8_t channel);
uint8_t ADC_Sample_GetActiveChannel(void);
float ADC_Sample_GetSampleRateHz(void);

uint8_t ADC_Sample_DataReady(void);
ADC_HalfReady_t ADC_Sample_GetReadyHalf(void);
void ADC_Sample_BufferLock(void);
void ADC_Sample_BufferUnlock(void);
uint8_t ADC_Sample_IsOverrun(void);

const uint16_t *ADC_Sample_GetRawData(void);
const float *ADC_Sample_GetVoltageData(void);
const float *ADC_Sample_GetVoltageDataByChannel(uint8_t channel);
void ADC_Sample_PrintRaw(void);

uint16_t NUEDC_ADC_ReadSampleRaw(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* ADC_APP_H */
