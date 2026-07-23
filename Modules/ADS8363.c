#include "ADS8363.h"

#define ADS8363_CFG_C_SHIFT              14U
#define ADS8363_CFG_R_SHIFT              12U
#define ADS8363_CFG_PD_SHIFT             10U
#define ADS8363_CFG_R_UPDATE_CHANNEL     (0U << ADS8363_CFG_R_SHIFT)
#define ADS8363_CFG_R_UPDATE_ALL         (1U << ADS8363_CFG_R_SHIFT)
#define ADS8363_CFG_NORMAL_POWER         (0U << ADS8363_CFG_PD_SHIFT)
#define ADS8363_CFG_CID                  (1U << 5)
#define ADS8363_ACCESS_CONFIG_ONLY       0x0U
#define ADS8363_ACCESS_WRITE_REFDAC1     0x2U
#define ADS8363_ACCESS_WRITE_REFDAC2     0x5U
#define ADS8363_ACCESS_RESET             0x4U
#define ADS8363_REFDAC_2V5_ENABLE        0x03FFU
#define ADS8363_REFDAC_2V5_DISABLE       0x07FFU
#define ADS8363_FRAME_TRAILING_BITS      4U

static ADS8363_StatusTypeDef ADS8363_CheckConfig(ADS8363_HandleTypeDef *dev);
static ADS8363_StatusTypeDef ADS8363_CheckSpiConfig(SPI_HandleTypeDef *hspi);
static ADS8363_StatusTypeDef ADS8363_WriteConfig(ADS8363_HandleTypeDef *dev,
                                                 ADS8363_DiffChannelTypeDef channel,
                                                 uint16_t access);
static ADS8363_StatusTypeDef ADS8363_WriteRegister(ADS8363_HandleTypeDef *dev,
                                                   uint16_t access,
                                                   uint16_t value);
static ADS8363_StatusTypeDef ADS8363_TransferFrame(ADS8363_HandleTypeDef *dev,
                                                   uint16_t tx_word,
                                                   uint16_t *rx_word);
static ADS8363_StatusTypeDef ADS8363_WaitBusyState(ADS8363_HandleTypeDef *dev,
                                                   GPIO_PinState state);
static uint32_t ADS8363_WordToFrame(uint16_t word);
static uint16_t ADS8363_FrameToWord(uint32_t frame);
static uint16_t ADS8363_BuildConfigWord(ADS8363_DiffChannelTypeDef channel,
                                        uint16_t r_bits,
                                        uint16_t access);
static void ADS8363_DelayCycles(uint16_t cycles);
static void ADS8363_EnableCycleCounter(void);

void ADS8363_GetDefaultConfig(ADS8363_ConfigTypeDef *cfg)
{
  if (cfg == NULL)
  {
    return;
  }

  cfg->hspi = NULL;
  cfg->cs_port = NULL;
  cfg->cs_pin = 0U;
  cfg->rd_port = NULL;
  cfg->rd_pin = 0U;
  cfg->busy_port = NULL;
  cfg->busy_pin = 0U;
  cfg->spi_timeout_ms = ADS8363_SPI_TIMEOUT_MS;
  cfg->busy_timeout_us = ADS8363_BUSY_TIMEOUT_US;
  cfg->pulse_cycles = ADS8363_CONVST_PULSE_CYCLES;
  cfg->vref_mv = ADS8363_DEFAULT_VREF_MV;
}

ADS8363_StatusTypeDef ADS8363_Init(ADS8363_HandleTypeDef *dev,
                                   const ADS8363_ConfigTypeDef *cfg)
{
  ADS8363_StatusTypeDef status;
  uint16_t dummy;

  if ((dev == NULL) || (cfg == NULL))
  {
    return ADS8363_ERROR_PARAM;
  }

  dev->cfg = *cfg;
  dev->channel = ADS8363_DIFF_CH0;
  dev->pipeline_valid_frames = 0U;
  ADS8363_EnableCycleCounter();

  status = ADS8363_CheckConfig(dev);
  if (status != ADS8363_OK)
  {
    return status;
  }

  HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(dev->cfg.rd_port, dev->cfg.rd_pin, GPIO_PIN_RESET);

  status = ADS8363_Reset(dev);
  if (status != ADS8363_OK)
  {
    return status;
  }

  status = ADS8363_WriteConfig(dev, ADS8363_DIFF_CH0, ADS8363_ACCESS_CONFIG_ONLY);
  if (status != ADS8363_OK)
  {
    return status;
  }

  for (uint32_t i = 0U; i < 2U; i++)
  {
    status = ADS8363_TransferFrame(dev,
                                   ADS8363_BuildConfigWord(ADS8363_DIFF_CH0,
                                                           ADS8363_CFG_R_UPDATE_CHANNEL,
                                                           ADS8363_ACCESS_CONFIG_ONLY),
                                   &dummy);
    if (status != ADS8363_OK)
    {
      return status;
    }
  }

  dev->pipeline_valid_frames = 2U;
  return ADS8363_OK;
}

ADS8363_StatusTypeDef ADS8363_Reset(ADS8363_HandleTypeDef *dev)
{
  ADS8363_StatusTypeDef status;
  uint16_t dummy;

  if (dev == NULL)
  {
    return ADS8363_ERROR_PARAM;
  }

  status = ADS8363_TransferFrame(dev,
                                 ADS8363_BuildConfigWord(ADS8363_DIFF_CH0,
                                                         ADS8363_CFG_R_UPDATE_ALL,
                                                         ADS8363_ACCESS_RESET),
                                 &dummy);
  if (status != ADS8363_OK)
  {
    return status;
  }

  HAL_Delay(1U);
  dev->channel = ADS8363_DIFF_CH0;
  dev->pipeline_valid_frames = 0U;
  return ADS8363_OK;
}

ADS8363_StatusTypeDef ADS8363_SetDiffChannel(ADS8363_HandleTypeDef *dev,
                                             ADS8363_DiffChannelTypeDef channel)
{
  uint16_t dummy;

  if ((dev == NULL) ||
      ((channel != ADS8363_DIFF_CH0) && (channel != ADS8363_DIFF_CH1)))
  {
    return ADS8363_ERROR_PARAM;
  }

  dev->channel = channel;
  dev->pipeline_valid_frames = 0U;

  return ADS8363_TransferFrame(dev,
                               ADS8363_BuildConfigWord(channel,
                                                       ADS8363_CFG_R_UPDATE_CHANNEL,
                                                       ADS8363_ACCESS_CONFIG_ONLY),
                               &dummy);
}

ADS8363_StatusTypeDef ADS8363_EnableInternalReference(ADS8363_HandleTypeDef *dev,
                                                      uint8_t enable)
{
  ADS8363_StatusTypeDef status;
  uint16_t refdac;

  if (dev == NULL)
  {
    return ADS8363_ERROR_PARAM;
  }

  refdac = (enable != 0U) ? ADS8363_REFDAC_2V5_ENABLE : ADS8363_REFDAC_2V5_DISABLE;

  status = ADS8363_WriteRegister(dev, ADS8363_ACCESS_WRITE_REFDAC1, refdac);
  if (status != ADS8363_OK)
  {
    return status;
  }

  status = ADS8363_WriteRegister(dev, ADS8363_ACCESS_WRITE_REFDAC2, refdac);
  if (status != ADS8363_OK)
  {
    return status;
  }

  if (enable != 0U)
  {
    HAL_Delay(8U);
  }

  return ADS8363_OK;
}

ADS8363_StatusTypeDef ADS8363_ReadPair(ADS8363_HandleTypeDef *dev,
                                       ADS8363_DiffChannelTypeDef channel,
                                       ADS8363_SamplePairTypeDef *sample)
{
  ADS8363_StatusTypeDef status;
  uint16_t dummy;

  if ((dev == NULL) || (sample == NULL))
  {
    return ADS8363_ERROR_PARAM;
  }

  if (dev->channel != channel)
  {
    status = ADS8363_SetDiffChannel(dev, channel);
    if (status != ADS8363_OK)
    {
      return status;
    }
  }

  while (dev->pipeline_valid_frames < 2U)
  {
    status = ADS8363_TransferFrame(dev,
                                   ADS8363_BuildConfigWord(channel,
                                                           ADS8363_CFG_R_UPDATE_CHANNEL,
                                                           ADS8363_ACCESS_CONFIG_ONLY),
                                   &dummy);
    if (status != ADS8363_OK)
    {
      return status;
    }
    dev->pipeline_valid_frames++;
  }

  return ADS8363_ReadPairContinuous(dev, sample);
}

ADS8363_StatusTypeDef ADS8363_ReadPairContinuous(ADS8363_HandleTypeDef *dev,
                                                 ADS8363_SamplePairTypeDef *sample)
{
  ADS8363_StatusTypeDef status;
  uint16_t raw_a;

  if ((dev == NULL) || (sample == NULL))
  {
    return ADS8363_ERROR_PARAM;
  }

  status = ADS8363_TransferFrame(dev,
                                 ADS8363_BuildConfigWord(dev->channel,
                                                         ADS8363_CFG_R_UPDATE_CHANNEL,
                                                         ADS8363_ACCESS_CONFIG_ONLY),
                                 &raw_a);
  if (status != ADS8363_OK)
  {
    return status;
  }

  sample->a = (int16_t)raw_a;
  sample->b = 0;
  sample->b_valid = 0U;
  dev->pipeline_valid_frames = 2U;

  return ADS8363_OK;
}

float ADS8363_CodeToVoltageMv(int16_t code, float vref_mv)
{
  return ((float)code * vref_mv) / 32768.0f;
}

static ADS8363_StatusTypeDef ADS8363_CheckConfig(ADS8363_HandleTypeDef *dev)
{
  if ((dev->cfg.hspi == NULL) ||
      (dev->cfg.cs_port == NULL) ||
      (dev->cfg.cs_pin == 0U) ||
      (dev->cfg.rd_port == NULL) ||
      (dev->cfg.rd_pin == 0U) ||
      (dev->cfg.busy_port == NULL) ||
      (dev->cfg.busy_pin == 0U))
  {
    return ADS8363_ERROR_PARAM;
  }

  return ADS8363_CheckSpiConfig(dev->cfg.hspi);
}

static ADS8363_StatusTypeDef ADS8363_CheckSpiConfig(SPI_HandleTypeDef *hspi)
{
  if (hspi == NULL)
  {
    return ADS8363_ERROR_PARAM;
  }

  if ((hspi->Init.Mode != SPI_MODE_MASTER) ||
      (hspi->Init.Direction != SPI_DIRECTION_2LINES) ||
      (hspi->Init.DataSize != SPI_DATASIZE_20BIT) ||
      (hspi->Init.CLKPolarity != SPI_POLARITY_LOW) ||
      (hspi->Init.CLKPhase != SPI_PHASE_2EDGE) ||
      (hspi->Init.FirstBit != SPI_FIRSTBIT_MSB))
  {
    return ADS8363_ERROR_SPI_CONFIG;
  }

  return ADS8363_OK;
}

static ADS8363_StatusTypeDef ADS8363_WriteConfig(ADS8363_HandleTypeDef *dev,
                                                 ADS8363_DiffChannelTypeDef channel,
                                                 uint16_t access)
{
  ADS8363_StatusTypeDef status;
  uint16_t dummy;

  status = ADS8363_TransferFrame(dev,
                                 ADS8363_BuildConfigWord(channel,
                                                         ADS8363_CFG_R_UPDATE_ALL,
                                                         access),
                                 &dummy);
  if (status == ADS8363_OK)
  {
    dev->channel = channel;
    dev->pipeline_valid_frames = 0U;
  }

  return status;
}

static ADS8363_StatusTypeDef ADS8363_WriteRegister(ADS8363_HandleTypeDef *dev,
                                                   uint16_t access,
                                                   uint16_t value)
{
  ADS8363_StatusTypeDef status;
  uint16_t dummy;

  status = ADS8363_WriteConfig(dev, dev->channel, access);
  if (status != ADS8363_OK)
  {
    return status;
  }

  status = ADS8363_TransferFrame(dev, value, &dummy);
  if (status != ADS8363_OK)
  {
    return status;
  }

  dev->pipeline_valid_frames = 0U;
  return ADS8363_OK;
}

static ADS8363_StatusTypeDef ADS8363_TransferFrame(ADS8363_HandleTypeDef *dev,
                                                   uint16_t tx_word,
                                                   uint16_t *rx_word)
{
  uint32_t tx_frame;
  uint32_t rx_frame;
  HAL_StatusTypeDef hal_status;

  if ((dev == NULL) || (rx_word == NULL))
  {
    return ADS8363_ERROR_PARAM;
  }

  tx_frame = ADS8363_WordToFrame(tx_word);
  rx_frame = 0U;
  *rx_word = 0U;

  HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(dev->cfg.rd_port, dev->cfg.rd_pin, GPIO_PIN_SET);
  ADS8363_DelayCycles(dev->cfg.pulse_cycles);
  HAL_GPIO_WritePin(dev->cfg.rd_port, dev->cfg.rd_pin, GPIO_PIN_RESET);

  if (ADS8363_WaitBusyState(dev, GPIO_PIN_SET) != ADS8363_OK)
  {
    HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);
    return ADS8363_ERROR_BUSY_TIMEOUT;
  }

  hal_status = HAL_SPI_TransmitReceive(dev->cfg.hspi,
                                       (uint8_t *)&tx_frame,
                                       (uint8_t *)&rx_frame,
                                       1U,
                                       dev->cfg.spi_timeout_ms);
  HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);

  if (hal_status != HAL_OK)
  {
    return ADS8363_ERROR_SPI;
  }

  if (ADS8363_WaitBusyState(dev, GPIO_PIN_RESET) != ADS8363_OK)
  {
    return ADS8363_ERROR_BUSY_TIMEOUT;
  }

  *rx_word = ADS8363_FrameToWord(rx_frame);
  return ADS8363_OK;
}

static ADS8363_StatusTypeDef ADS8363_WaitBusyState(ADS8363_HandleTypeDef *dev,
                                                   GPIO_PinState state)
{
  uint32_t timeout_cycles;

  if (dev == NULL)
  {
    return ADS8363_ERROR_PARAM;
  }

  timeout_cycles = (HAL_RCC_GetSysClockFreq() / 1000000U) * dev->cfg.busy_timeout_us;

  while (HAL_GPIO_ReadPin(dev->cfg.busy_port, dev->cfg.busy_pin) != state)
  {
    if (timeout_cycles == 0U)
    {
      return ADS8363_ERROR_BUSY_TIMEOUT;
    }
    timeout_cycles--;
    __NOP();
  }

  return ADS8363_OK;
}

static uint32_t ADS8363_WordToFrame(uint16_t word)
{
  return ((uint32_t)word << ADS8363_FRAME_TRAILING_BITS) & 0x000FFFF0U;
}

static uint16_t ADS8363_FrameToWord(uint32_t frame)
{
  return (uint16_t)((frame >> ADS8363_FRAME_TRAILING_BITS) & 0xFFFFU);
}

static uint16_t ADS8363_BuildConfigWord(ADS8363_DiffChannelTypeDef channel,
                                        uint16_t r_bits,
                                        uint16_t access)
{
  uint16_t c_bits;

  c_bits = ((uint16_t)channel & 0x3U) << ADS8363_CFG_C_SHIFT;

  return (uint16_t)(c_bits |
                    r_bits |
                    ADS8363_CFG_NORMAL_POWER |
                    ADS8363_CFG_CID |
                    (access & 0xFU));
}

static void ADS8363_DelayCycles(uint16_t cycles)
{
  while (cycles > 0U)
  {
    __NOP();
    cycles--;
  }
}

static void ADS8363_EnableCycleCounter(void)
{
  /* Cortex-M0+ has no DWT cycle counter. Keep the API as a no-op. */
}
