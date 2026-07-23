#ifndef STM32H7XX_HAL_H
#define STM32H7XX_HAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __weak
/* 统一不同编译器的弱符号写法。 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#define __weak __attribute__((weak)) /* ARMClang弱符号。 */
#else
#define __weak __attribute__((weak)) /* GCC兼容弱符号。 */
#endif
#endif

#ifndef __NOP
/* 提供兼容的空操作指令。 */
#if defined(__ARMCC_VERSION)
#define __NOP() __asm volatile ("nop") /* ARMClang空操作。 */
#else
#define __NOP() do { } while (0) /* 其他编译器空操作占位。 */
#endif
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x)) /* 显式标记未使用参数。 */
#endif

#define HAL_SPI_MODULE_ENABLED 1 /* 启用公共驱动所需SPI声明。 */

/* HAL兼容状态码。 */
typedef enum {
    HAL_OK      = 0x00U,
    HAL_ERROR   = 0x01U,
    HAL_BUSY    = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

/* HAL兼容GPIO电平。 */
typedef enum {
    GPIO_PIN_RESET = 0U,
    GPIO_PIN_SET
} GPIO_PinState;

/* 虚拟STM32 GPIO端口描述符。 */
typedef struct {
    uint8_t port_index;
    const char *name;
} GPIO_TypeDef;

/* 公共驱动引用的虚拟GPIO端口。 */
extern GPIO_TypeDef NUEDC_STM32_GPIOA;
extern GPIO_TypeDef NUEDC_STM32_GPIOB;
extern GPIO_TypeDef NUEDC_STM32_GPIOC;
extern GPIO_TypeDef NUEDC_STM32_GPIOD;
extern GPIO_TypeDef NUEDC_STM32_GPIOE;
extern GPIO_TypeDef NUEDC_STM32_GPIOH;

/* 虚拟GPIO端口别名。 */
#define GPIOA (&NUEDC_STM32_GPIOA) /* 虚拟GPIOA。 */
#define GPIOB (&NUEDC_STM32_GPIOB) /* 虚拟GPIOB。 */
#define GPIOC (&NUEDC_STM32_GPIOC) /* 虚拟GPIOC。 */
#define GPIOD (&NUEDC_STM32_GPIOD) /* 虚拟GPIOD。 */
#define GPIOE (&NUEDC_STM32_GPIOE) /* 虚拟GPIOE。 */
#define GPIOH (&NUEDC_STM32_GPIOH) /* 虚拟GPIOH。 */

/* STM32兼容的GPIO位掩码。 */
#define GPIO_PIN_0                 (0x00000001UL) /* 虚拟引脚0。 */
#define GPIO_PIN_1                 (0x00000002UL) /* 虚拟引脚1。 */
#define GPIO_PIN_2                 (0x00000004UL) /* 虚拟引脚2。 */
#define GPIO_PIN_3                 (0x00000008UL) /* 虚拟引脚3。 */
#define GPIO_PIN_4                 (0x00000010UL) /* 虚拟引脚4。 */
#define GPIO_PIN_5                 (0x00000020UL) /* 虚拟引脚5。 */
#define GPIO_PIN_6                 (0x00000040UL) /* 虚拟引脚6。 */
#define GPIO_PIN_7                 (0x00000080UL) /* 虚拟引脚7。 */
#define GPIO_PIN_8                 (0x00000100UL) /* 虚拟引脚8。 */
#define GPIO_PIN_9                 (0x00000200UL) /* 虚拟引脚9。 */
#define GPIO_PIN_10                (0x00000400UL) /* 虚拟引脚10。 */
#define GPIO_PIN_11                (0x00000800UL) /* 虚拟引脚11。 */
#define GPIO_PIN_12                (0x00001000UL) /* 虚拟引脚12。 */
#define GPIO_PIN_13                (0x00002000UL) /* 虚拟引脚13。 */
#define GPIO_PIN_14                (0x00004000UL) /* 虚拟引脚14。 */
#define GPIO_PIN_15                (0x00008000UL) /* 虚拟引脚15。 */
#define GPIO_PIN_All               (0x0000FFFFUL) /* 全部虚拟引脚。 */

/* SPI初始化参数的兼容子集。 */
typedef struct {
    uint32_t Mode;
    uint32_t Direction;
    uint32_t DataSize;
    uint32_t CLKPolarity;
    uint32_t CLKPhase;
    uint32_t NSS;
    uint32_t BaudRatePrescaler;
    uint32_t FirstBit;
} SPI_InitTypeDef;

/* SPI句柄的兼容子集。 */
typedef struct {
    void *Instance;
    SPI_InitTypeDef Init;
} SPI_HandleTypeDef;

/* UART句柄的兼容子集。 */
typedef struct {
    void *Instance;
} UART_HandleTypeDef;

/* 仅保留实例地址的其他外设兼容句柄。 */
typedef struct { void *Instance; } ADC_HandleTypeDef;
typedef struct { void *Instance; } TIM_HandleTypeDef;
typedef struct { void *Instance; } DMA_HandleTypeDef;

/* Cortex-M0+软件模拟的DWT寄存器子集。 */
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
} DWT_Type;

/* 调试寄存器兼容占位结构。 */
typedef struct {
    volatile uint32_t DEMCR;
} CoreDebug_Type;

extern DWT_Type NUEDC_DWT;
extern CoreDebug_Type NUEDC_CoreDebug;
extern uint32_t SystemCoreClock;

/* CMSIS兼容的调试寄存器别名和位掩码。 */
#define DWT (&NUEDC_DWT)                         /* 软件DWT别名。 */
#define CoreDebug (&NUEDC_CoreDebug)             /* 调试占位别名。 */
#define CoreDebug_DEMCR_TRCENA_Msk (0x01000000UL) /* 跟踪使能兼容位。 */
#define DWT_CTRL_CYCCNTENA_Msk     (0x00000001UL) /* 周期计数使能兼容位。 */

void SystemCoreClockUpdate(void);

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim3;

/* 兼容外设实例标识，仅用于区分句柄。 */
#define USART1 ((void *)0x40011000UL) /* 兼容串口实例1。 */
#define USART2 ((void *)0x40004400UL) /* 兼容串口实例2。 */
#define USART3 ((void *)0x40004800UL) /* 兼容串口实例3。 */

/* 公共驱动使用的SPI初始化兼容常量。 */
#define SPI_MODE_MASTER            0x00000001UL /* 主机模式。 */
#define SPI_DIRECTION_2LINES       0x00000002UL /* 双线全双工。 */
#define SPI_DATASIZE_8BIT          0x00000008UL /* 8位帧。 */
#define SPI_DATASIZE_16BIT         0x00000010UL /* 16位帧。 */
#define SPI_DATASIZE_20BIT         0x00000014UL /* 20位帧。 */
#define SPI_POLARITY_LOW           0x00000000UL /* 空闲低电平。 */
#define SPI_POLARITY_HIGH          0x00000001UL /* 空闲高电平。 */
#define SPI_PHASE_1EDGE            0x00000000UL /* 第一边沿采样。 */
#define SPI_PHASE_2EDGE            0x00000001UL /* 第二边沿采样。 */
#define SPI_NSS_SOFT               0x00000001UL /* 软件片选。 */
#define SPI_BAUDRATEPRESCALER_8    0x00000008UL /* 兼容八分频标志。 */
#define SPI_FIRSTBIT_MSB           0x00000000UL /* 高位先行。 */
#define SPI_FIRSTBIT_LSB           0x00000001UL /* 低位先行。 */

/* TI兼容层基础服务接口。 */
void NUEDC_HAL_Init(void);
void NUEDC_HAL_Service(void);
uint32_t NUEDC_HAL_GetCycleCount(void);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t delay_ms);
uint32_t HAL_RCC_GetSysClockFreq(void);

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi,
                                   const uint8_t *pData,
                                   uint16_t Size,
                                   uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi,
                                          uint8_t *pTxData,
                                          uint8_t *pRxData,
                                          uint16_t Size,
                                          uint32_t Timeout);

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart,
                                    uint8_t *pData,
                                    uint16_t Size,
                                    uint32_t Timeout);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart,
                                      uint8_t *pData,
                                      uint16_t Size);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* STM32H7XX_HAL_H */
