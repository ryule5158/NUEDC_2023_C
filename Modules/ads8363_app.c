#include "ads8363_app.h"
#include "spi.h"
#include <stdio.h>

static ADS8363_HandleTypeDef s_ads8363;
static uint32_t s_ads8363_frame_count;
static uint32_t s_ads8363_print_tick;
static uint32_t s_ads8363_print_interval_ms;
static uint32_t s_ads8363_sample_period_cycles;
static uint32_t s_ads8363_next_sample_cycle;
static float s_ads8363_vref_mv;
static uint8_t s_ads8363_app_channel;
static ADS8363_DiffChannelTypeDef s_ads8363_diff_channel;
static ADS8363_StatusTypeDef s_ads8363_status;

static ADS8363_StatusTypeDef ADS8363_AppSelectChannel(uint8_t channel);
static ADS8363_StatusTypeDef ADS8363_AppSetSampleRate(float sample_rate_hz);

/************************************************************
 * Function :       ADS8363_AppInit
 * Comment  :       初始化ADS8363应用层, 绑定主工程SPI2和控制引脚
 * Parameter:       channel: 0为ADC-A1-ADC-A0, 1为A3仅偏置-ADC-A2
 *                  vref_mv: 参考电压, 单位mV, 测试板REF5025/VREF2V5_1常用2500.0f
 *                  sample_rate_hz: APP软件采样率, 单位Hz, 受SPI/printf/主循环占用限制
 *                  print_interval_ms: 打印间隔, 单位ms, 0表示每次采样都打印
 * Return   :       ADS8363状态
 * Date     :       2026-06-13 V1
************************************************************/
ADS8363_StatusTypeDef ADS8363_AppInit(uint8_t channel,
                                      float vref_mv,
                                      float sample_rate_hz,
                                      uint32_t print_interval_ms)
{
  ADS8363_ConfigTypeDef cfg;

  s_ads8363_status = ADS8363_AppSelectChannel(channel);
  if ((s_ads8363_status != ADS8363_OK) ||
      (vref_mv <= 0.0f) ||
      (ADS8363_AppSetSampleRate(sample_rate_hz) != ADS8363_OK))
  {
    printf("ADS8363 Init Param Error\r\n");
    s_ads8363_status = ADS8363_ERROR_PARAM;
    return s_ads8363_status;
  }

  ADS8363_GetDefaultConfig(&cfg);
  cfg.hspi = &hspi2;
  cfg.cs_port = ADS8363_CS_PORT;
  cfg.cs_pin = ADS8363_CS_PIN;
  cfg.rd_port = ADS8363_RD_CONVST_PORT;
  cfg.rd_pin = ADS8363_RD_CONVST_PIN;
  cfg.busy_port = ADS8363_BUSY_PORT;
  cfg.busy_pin = ADS8363_BUSY_PIN;
  cfg.vref_mv = vref_mv;

  s_ads8363_frame_count = 0U;
  s_ads8363_print_tick = HAL_GetTick();
  s_ads8363_print_interval_ms = print_interval_ms;
  s_ads8363_vref_mv = vref_mv;
  s_ads8363_status = ADS8363_Init(&s_ads8363, &cfg);

  if (s_ads8363_status == ADS8363_OK)
  {
    s_ads8363_status = ADS8363_SetDiffChannel(&s_ads8363, s_ads8363_diff_channel);
    s_ads8363_next_sample_cycle = DWT->CYCCNT;
    printf("ADS8363 Init OK, CH%u, vref=%.1fmV, Fs=%.1fHz\r\n",
           (unsigned int)s_ads8363_app_channel,
           s_ads8363_vref_mv,
           sample_rate_hz);
  }
  else
  {
    printf("ADS8363 Init Error, status=%d (", (int)s_ads8363_status);
    switch (s_ads8363_status)
    {
      case ADS8363_ERROR_PARAM:        printf("Param"); break;
      case ADS8363_ERROR_BUSY_TIMEOUT: printf("BUSY Timeout: check RD/CONVST and BUSY wiring"); break;
      case ADS8363_ERROR_SPI_CONFIG:   printf("SPI Config: need 20-bit, CPOL=0, CPHA=1"); break;
      case ADS8363_ERROR_SPI:          printf("SPI Transfer Failed"); break;
      default:                         printf("Unknown"); break;
    }
    printf(")\r\n");
  }

  return s_ads8363_status;
}

/************************************************************
 * Function :       ADS8363_AppProcess
 * Comment  :       读取ADS8363采样, 换算电压并通过printf周期打印
 * Parameter:       null
 * Return   :       null
 * Date     :       2026-06-13 V1
************************************************************/
void ADS8363_AppProcess(void)
{
  ADS8363_SamplePairTypeDef sample;
  uint32_t now_cycles;
  float voltage_mv;

  if (s_ads8363_status != ADS8363_OK)
  {
    return;
  }

  now_cycles = DWT->CYCCNT;
  if ((int32_t)(now_cycles - s_ads8363_next_sample_cycle) < 0)
  {
    return;
  }

  s_ads8363_next_sample_cycle += s_ads8363_sample_period_cycles;
  if ((int32_t)(now_cycles - s_ads8363_next_sample_cycle) > 0)
  {
    s_ads8363_next_sample_cycle = now_cycles + s_ads8363_sample_period_cycles;
  }

  s_ads8363_status = ADS8363_ReadPair(&s_ads8363,
                                      s_ads8363_diff_channel,
                                      &sample);
  if (s_ads8363_status != ADS8363_OK)
  {
    printf("ADS8363 Read Error, status=%d\r\n", (int)s_ads8363_status);
    return;
  }

  s_ads8363_frame_count++;
  voltage_mv = ADS8363_CodeToVoltageMv(sample.a, s_ads8363_vref_mv);

  if ((s_ads8363_print_interval_ms == 0U) ||
      ((HAL_GetTick() - s_ads8363_print_tick) >= s_ads8363_print_interval_ms))
  {
    s_ads8363_print_tick = HAL_GetTick();
    printf("ADS8363 CH%u: raw=%d, voltage=%.2fmV, frame=%lu\r\n",
           (unsigned int)s_ads8363_app_channel,
           (int)sample.a,
           voltage_mv,
           (unsigned long)s_ads8363_frame_count);
  }
}

static ADS8363_StatusTypeDef ADS8363_AppSelectChannel(uint8_t channel)
{
  if (channel == ADS8363_APP_CHANNEL_0)
  {
    s_ads8363_app_channel = ADS8363_APP_CHANNEL_0;
    s_ads8363_diff_channel = ADS8363_DIFF_CH0;
    return ADS8363_OK;
  }

  if (channel == ADS8363_APP_CHANNEL_1)
  {
    s_ads8363_app_channel = ADS8363_APP_CHANNEL_1;
    s_ads8363_diff_channel = ADS8363_DIFF_CH1;
    return ADS8363_OK;
  }

  return ADS8363_ERROR_PARAM;
}

static ADS8363_StatusTypeDef ADS8363_AppSetSampleRate(float sample_rate_hz)
{
  float period_cycles;

  if (sample_rate_hz <= 0.0f)
  {
    return ADS8363_ERROR_PARAM;
  }

  period_cycles = (float)HAL_RCC_GetSysClockFreq() / sample_rate_hz;
  if ((period_cycles < 1.0f) || (period_cycles > 2147483647.0f))
  {
    return ADS8363_ERROR_PARAM;
  }

  s_ads8363_sample_period_cycles = (uint32_t)(period_cycles + 0.5f);
  if (s_ads8363_sample_period_cycles == 0U)
  {
    return ADS8363_ERROR_PARAM;
  }

  return ADS8363_OK;
}
