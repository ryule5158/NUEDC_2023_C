#ifndef DAC8831_H
#define DAC8831_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define DAC8831_RESOLUTION_BITS         16U       /* DAC8831 DAC分辨率位数 */
#define DAC8831_MAX_CODE                0xFFFFU   /* DAC8831最大16位DAC码值 */
#define DAC8831_MIDSCALE_CODE           0x8000U   /* DAC8831中点码值 */
#define DAC8831_SOFT_SPI_DELAY_CYCLES   1U       /* DAC8831软件SPI每个时序阶段的延时周期 */
#define DAC8831_DEFAULT_VREF_MV         2500.0f   /* DAC8831默认参考电压, 单位mV */

typedef enum
{
  DAC8831_OK = 0,
  DAC8831_ERROR,
  DAC8831_ERROR_PARAM,
  DAC8831_ERROR_SPI
} DAC8831_StatusTypeDef;

typedef enum
{
  DAC8831_LDAC_TIED_LOW = 0,
  DAC8831_LDAC_GPIO_PULSE
} DAC8831_LdacModeTypeDef;

typedef enum
{
  DAC8831_OUTPUT_UNIPOLAR = 0,
  DAC8831_OUTPUT_BIPOLAR
} DAC8831_OutputModeTypeDef;

typedef struct
{
  GPIO_TypeDef *sck_port;
  uint16_t sck_pin;
  GPIO_TypeDef *mosi_port;
  uint16_t mosi_pin;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  GPIO_TypeDef *ldac_port;
  uint16_t ldac_pin;
  DAC8831_LdacModeTypeDef ldac_mode;
  uint16_t spi_delay_cycles;
  uint16_t ldac_pulse_delay_cycles;
  float vref_mv;
  DAC8831_OutputModeTypeDef output_mode;
} DAC8831_ConfigTypeDef;

typedef struct
{
  DAC8831_ConfigTypeDef cfg;
  uint16_t last_code;
} DAC8831_HandleTypeDef;

void DAC8831_GetDefaultConfig(DAC8831_ConfigTypeDef *cfg);
DAC8831_StatusTypeDef DAC8831_Init(DAC8831_HandleTypeDef *dev,
                                   const DAC8831_ConfigTypeDef *cfg);
DAC8831_StatusTypeDef DAC8831_WriteRaw(DAC8831_HandleTypeDef *dev,
                                       uint16_t code);
DAC8831_StatusTypeDef DAC8831_WriteUnipolarMv(DAC8831_HandleTypeDef *dev,
                                              float millivolts);
DAC8831_StatusTypeDef DAC8831_WriteBipolarMv(DAC8831_HandleTypeDef *dev,
                                             float millivolts);
DAC8831_StatusTypeDef DAC8831_PulseLdac(DAC8831_HandleTypeDef *dev);

uint16_t DAC8831_UnipolarMvToCode(float millivolts, float vref_mv);
uint16_t DAC8831_BipolarMvToCode(float millivolts, float vref_mv);
float DAC8831_CodeToUnipolarMv(uint16_t code, float vref_mv);
float DAC8831_CodeToBipolarMv(uint16_t code, float vref_mv);

#ifdef __cplusplus
}
#endif

#endif /* DAC8831_H */
