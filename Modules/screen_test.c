#include "screen_test.h"
#include "ad9910_app.h"
#include "Serial_Buffer.h"
#include "usart.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 基本要求(2)频率设置范围和步进 */
#define SCREEN_REQ2_FREQ_MIN_HZ      100u
#define SCREEN_REQ2_FREQ_MAX_HZ      10000000u
#define SCREEN_REQ2_FREQ_STEP_100HZ  100u
#define SCREEN_REQ2_FREQ_STEP_10KHZ  10000u

/* 串口屏使用USART2 */
#define SCREEN_UART                  (&huart2)

/* AD9910满幅度时探究装置输出端实测峰峰值，单位mV */
#define SCREEN_DDS_FULLSCALE_VPP_MV  3500u

/* 串口屏命令字 */
#define SCREEN_CMD_REFRESH           0x01
#define SCREEN_CMD_REQ2_DEC_100      0x10
#define SCREEN_CMD_REQ2_INC_100      0x11
#define SCREEN_CMD_REQ2_DEC_10K      0x12
#define SCREEN_CMD_REQ2_INC_10K      0x13
#define SCREEN_CMD_REQ2_START        0x14
#define SCREEN_CMD_REQ3_START        0x20
#define SCREEN_CMD_REQ4_DEC_100      0x30
#define SCREEN_CMD_REQ4_INC_100      0x31
#define SCREEN_CMD_REQ4_VPP_DEC      0x32
#define SCREEN_CMD_REQ4_VPP_INC      0x33
#define SCREEN_CMD_REQ4_START        0x34
#define SCREEN_CMD_ADV1_LEARN        0x40
#define SCREEN_CMD_ADV2_INFER        0x50
#define SCREEN_CMD_STOP              0x7E

static Screen_ContextTypeDef s_screen_ctx;       /* 串口屏与主程序共享参数上下文 */
static uint8_t  s_screen_rx_byte;                /* USART2中断接收的单字节缓存 */
static uint32_t s_screen_last_refresh_ms;        /* 串口屏周期刷新计时，单位ms */

static void Screen_SendCmd(const char *cmd);
static void Screen_SetTextf(const char *obj, const char *fmt, ...);
static uint32_t Screen_AddClampU32(uint32_t value, int32_t step,
                                   uint32_t min, uint32_t max);
static float Screen_KnownModelGain(uint32_t freq_hz);
static const char *Screen_FilterName(uint8_t type);
static uint8_t Screen_ContextReady(void);

void Screen_Init(const Screen_ContextTypeDef *context)
{
  if (context != NULL)
  {
    s_screen_ctx = *context;
  }

  HAL_UART_Receive_IT(SCREEN_UART, &s_screen_rx_byte, 1U);

  HAL_Delay(200);
  Screen_SendCmd("bkcmd=0");
  Screen_SetStatus("READY");
  Screen_Refresh();
}

void Screen_Process(void)
{
  uint8_t packet[16];
  uint8_t len;

  if (Screen_ContextReady() == 0U)
  {
    return;
  }

  while ((len = Command_GetCommand(packet)) != 0U)
  {
    if (len < 4U) continue;

    switch (packet[2])
    {
      case SCREEN_CMD_REFRESH:
        Screen_Refresh();
        Screen_SetStatus("READY");
        break;

      case SCREEN_CMD_REQ2_DEC_100:
        *s_screen_ctx.req2_freq_hz =
          Screen_AddClampU32(*s_screen_ctx.req2_freq_hz,
                             -(int32_t)SCREEN_REQ2_FREQ_STEP_100HZ,
                             SCREEN_REQ2_FREQ_MIN_HZ,
                             SCREEN_REQ2_FREQ_MAX_HZ);
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ2_INC_100:
        *s_screen_ctx.req2_freq_hz =
          Screen_AddClampU32(*s_screen_ctx.req2_freq_hz,
                             SCREEN_REQ2_FREQ_STEP_100HZ,
                             SCREEN_REQ2_FREQ_MIN_HZ,
                             SCREEN_REQ2_FREQ_MAX_HZ);
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ2_DEC_10K:
        *s_screen_ctx.req2_freq_hz =
          Screen_AddClampU32(*s_screen_ctx.req2_freq_hz,
                             -(int32_t)SCREEN_REQ2_FREQ_STEP_10KHZ,
                             SCREEN_REQ2_FREQ_MIN_HZ,
                             SCREEN_REQ2_FREQ_MAX_HZ);
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ2_INC_10K:
        *s_screen_ctx.req2_freq_hz =
          Screen_AddClampU32(*s_screen_ctx.req2_freq_hz,
                             SCREEN_REQ2_FREQ_STEP_10KHZ,
                             SCREEN_REQ2_FREQ_MIN_HZ,
                             SCREEN_REQ2_FREQ_MAX_HZ);
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ2_START:
        *s_screen_ctx.state = STATE_REQ2_FIXED;
        break;

      case SCREEN_CMD_REQ3_START:
        *s_screen_ctx.state = STATE_REQ3_1KHZ;
        break;

      case SCREEN_CMD_REQ4_DEC_100:
        *s_screen_ctx.req4_freq_hz =
          Screen_AddClampU32(*s_screen_ctx.req4_freq_hz, -100, 100, 3000);
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ4_INC_100:
        *s_screen_ctx.req4_freq_hz =
          Screen_AddClampU32(*s_screen_ctx.req4_freq_hz, 100, 100, 3000);
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ4_VPP_DEC:
        if (*s_screen_ctx.req4_target_vpp_x10 > 10U)
        {
          (*s_screen_ctx.req4_target_vpp_x10)--;
        }
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ4_VPP_INC:
        if (*s_screen_ctx.req4_target_vpp_x10 < 20U)
        {
          (*s_screen_ctx.req4_target_vpp_x10)++;
        }
        Screen_Refresh();
        break;

      case SCREEN_CMD_REQ4_START:
        *s_screen_ctx.state = STATE_REQ4_SWEEP_INIT;
        break;

      case SCREEN_CMD_ADV1_LEARN:
        *s_screen_ctx.state = STATE_ADV1_LEARN;
        break;

      case SCREEN_CMD_ADV2_INFER:
        *s_screen_ctx.state = STATE_ADV2_INFER;
        break;

      case SCREEN_CMD_STOP:
        AD9910_AppStop();
        *s_screen_ctx.amplitude = 0u;
        AD9910_AppOutputSine(1000u, *s_screen_ctx.amplitude);
        *s_screen_ctx.state = STATE_IDLE;
        Screen_SetStatus("IDLE");
        break;

      default:
        break;
    }
  }
}

void Screen_PeriodicRefresh(void)
{
  if (HAL_GetTick() - s_screen_last_refresh_ms >= 1000u)
  {
    s_screen_last_refresh_ms = HAL_GetTick();
    Screen_Refresh();
  }
}

void Screen_Refresh(void)
{
  if (Screen_ContextReady() == 0U)
  {
    return;
  }

  Screen_SetTextf("tf2", "%lu Hz", (unsigned long)*s_screen_ctx.req2_freq_hz);
  Screen_SetTextf("tf4", "%lu Hz", (unsigned long)*s_screen_ctx.req4_freq_hz);
  Screen_SetTextf("tv4", "%u.%u V",
                  *s_screen_ctx.req4_target_vpp_x10 / 10U,
                  *s_screen_ctx.req4_target_vpp_x10 % 10U);
  Screen_SetTextf("tamp", "%u", (unsigned int)*s_screen_ctx.amplitude);

  if (*s_screen_ctx.adv1_learning_done != 0U)
  {
    Screen_SetTextf("tflt", "%s", Screen_FilterName(*s_screen_ctx.adv1_filter_type));
  }
  else
  {
    Screen_SetTextf("tflt", "UNKNOWN");
  }
}

void Screen_SetStatus(const char *status)
{
  Screen_SetTextf("tst", "%s", status);
}

uint16_t Screen_CalcAmpForKnownModel(uint32_t freq_hz, uint8_t target_vpp_x10)
{
  float target_mv = (float)target_vpp_x10 * 100.0f;
  float input_mv = target_mv / Screen_KnownModelGain(freq_hz);
  float amp = input_mv * (float)AD9910_MAX_AMPLITUDE /
              (float)SCREEN_DDS_FULLSCALE_VPP_MV;

  if (amp < 1.0f) return 1u;
  if (amp > (float)AD9910_MAX_AMPLITUDE) return AD9910_MAX_AMPLITUDE;
  return (uint16_t)(amp + 0.5f);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    Command_Write(&s_screen_rx_byte, 1U);
    HAL_UART_Receive_IT(SCREEN_UART, &s_screen_rx_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    HAL_UART_Receive_IT(SCREEN_UART, &s_screen_rx_byte, 1U);
  }
}

static void Screen_SendCmd(const char *cmd)
{
  static const uint8_t end[3] = {0xFF, 0xFF, 0xFF};

  HAL_UART_Transmit(SCREEN_UART, (uint8_t *)cmd, strlen(cmd), 100U);
  HAL_UART_Transmit(SCREEN_UART, (uint8_t *)end, 3U, 100U);
}

static void Screen_SetTextf(const char *obj, const char *fmt, ...)
{
  char text[48];
  char cmd[80];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);

  snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", obj, text);
  Screen_SendCmd(cmd);
}

static uint32_t Screen_AddClampU32(uint32_t value, int32_t step,
                                   uint32_t min, uint32_t max)
{
  if (step < 0)
  {
    uint32_t dec = (uint32_t)(-step);
    return (value > min + dec) ? (value - dec) : min;
  }

  return (value < max - (uint32_t)step) ? (value + (uint32_t)step) : max;
}

static float Screen_KnownModelGain(uint32_t freq_hz)
{
  float w = 6.2831853f * (float)freq_hz;
  float real = 1.0f - 1.0e-8f * w * w;
  float imag = 3.0e-4f * w;

  return 5.0f / sqrtf(real * real + imag * imag);
}

static const char *Screen_FilterName(uint8_t type)
{
  switch (type)
  {
    case 0: return "LOWPASS";
    case 1: return "HIGHPASS";
    case 2: return "BANDPASS";
    case 3: return "BANDSTOP";
    default: return "UNKNOWN";
  }
}

static uint8_t Screen_ContextReady(void)
{
  return ((s_screen_ctx.state != NULL) &&
          (s_screen_ctx.req2_freq_hz != NULL) &&
          (s_screen_ctx.req4_freq_hz != NULL) &&
          (s_screen_ctx.req4_target_vpp_x10 != NULL) &&
          (s_screen_ctx.amplitude != NULL) &&
          (s_screen_ctx.adv1_learning_done != NULL) &&
          (s_screen_ctx.adv1_filter_type != NULL)) ? 1U : 0U;
}
