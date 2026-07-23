#ifndef __SCREEN_TEST_H__
#define __SCREEN_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* 主状态机状态：串口屏按键会把状态机切到对应状态 */
typedef enum
{
  STATE_REQ2_FIXED = 0,       /* 基本要求(2)：可设置频率正弦输出 */
  STATE_REQ3_1KHZ,            /* 基本要求(3)：1kHz已知模型控制 */
  STATE_REQ4_SWEEP_INIT,      /* 基本要求(4)：已知模型单频输出初始化 */
  STATE_ADV1_LEARN,           /* 发挥(1)：未知模型学习 */
  STATE_ADV2_INFER,           /* 发挥(2)：未知模型推理 */
  STATE_IDLE                  /* 空闲状态 */
} G_StateTypeDef;

/* 串口屏模块需要读写的主程序参数指针 */
typedef struct
{
  G_StateTypeDef *state;              /* 主状态机当前状态 */
  uint32_t       *req2_freq_hz;       /* 基本(2)频率，单位Hz */
  uint32_t       *req4_freq_hz;       /* 基本(4)频率，单位Hz */
  uint8_t        *req4_target_vpp_x10;/* 基本(4)目标Vpp，单位0.1V */
  uint16_t       *amplitude;          /* 当前AD9910幅度字 */
  uint8_t        *adv1_learning_done; /* 发挥(1)学习完成标志 */
  uint8_t        *adv1_filter_type;   /* 发挥(1)滤波器类型 */
} Screen_ContextTypeDef;

void Screen_Init(const Screen_ContextTypeDef *context);        /* 初始化串口屏通信和首屏显示 */
void Screen_Process(void);                                     /* 处理串口屏按键命令 */
void Screen_PeriodicRefresh(void);                             /* 周期刷新串口屏显示 */
void Screen_Refresh(void);                                     /* 立即刷新串口屏显示 */
void Screen_SetStatus(const char *status);                     /* 设置串口屏状态栏文本 */
uint16_t Screen_CalcAmpForKnownModel(uint32_t freq_hz,
                                     uint8_t target_vpp_x10);  /* 已知模型反推AD9910幅度字 */

#ifdef __cplusplus
}
#endif

#endif /* __SCREEN_TEST_H__ */
