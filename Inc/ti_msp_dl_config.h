/*
 *  ============ ti_msp_dl_config.h =============
 *  MSPM0G3507 DriverLib configuration for the NUEDC 2023 D port.
 */
#ifndef TI_MSP_DL_CONFIG_H
#define TI_MSP_DL_CONFIG_H

#define CONFIG_MSP_LITO_G3507 /* 目标板为MSP-LITO-G3507。 */
#define CONFIG_MSPM0G3507     /* 目标器件为MSPM0G3507。 */

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak)) /* TI编译器弱符号。 */
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak /* IAR编译器弱符号。 */
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak)) /* ARMClang/GCC弱符号。 */
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_STARTUP_DELAY (16U)       /* 外设电源稳定等待周期。 */
#define CPUCLK_FREQ         (32000000U) /* 系统主频，单位Hz。 */

/* 板载状态GPIO定义。 */
#define GPIO_LEDS_PORT             ((GPIO_Regs *)GPIOB_BASE) /* 状态灯真实TI端口。 */
#define GPIO_LEDS_USER_LED_1_PIN   (DL_GPIO_PIN_14) /* LITO板载D1引脚。 */
#define GPIO_LEDS_USER_LED_1_IOMUX (IOMUX_PINCM31) /* LITO板载D1复用号。 */
#define GPIO_LEDS_USER_TEST_PIN    (DL_GPIO_PIN_27) /* 用户测试引脚。 */
#define GPIO_LEDS_USER_TEST_IOMUX  (IOMUX_PINCM58) /* 测试引脚复用号。 */

/* UART0用于串口屏和标准输出：PA10发送、PA11接收。 */
#define UART_0_INST               UART0            /* 串口实例。 */
#define UART_0_INST_FREQUENCY     CPUCLK_FREQ      /* 串口功能时钟。 */
#define UART_0_INST_IRQHandler    UART0_IRQHandler /* 串口中断函数名。 */
#define UART_0_INST_INT_IRQN      UART0_INT_IRQn   /* 串口中断号。 */
#define UART_0_BAUD_RATE          (115200U)        /* 串口波特率。 */
#define GPIO_UART_0_TX_PORT       GPIOA            /* 串口发送端口。 */
#define GPIO_UART_0_TX_PIN        DL_GPIO_PIN_10   /* 串口发送引脚。 */
#define GPIO_UART_0_IOMUX_TX      IOMUX_PINCM21    /* 串口发送复用号。 */
#define GPIO_UART_0_IOMUX_TX_FUNC IOMUX_PINCM21_PF_UART0_TX /* 串口发送功能。 */
#define GPIO_UART_0_RX_PORT       GPIOA            /* 串口接收端口。 */
#define GPIO_UART_0_RX_PIN        DL_GPIO_PIN_11   /* 串口接收引脚。 */
#define GPIO_UART_0_IOMUX_RX      IOMUX_PINCM22    /* 串口接收复用号。 */
#define GPIO_UART_0_IOMUX_RX_FUNC IOMUX_PINCM22_PF_UART0_RX /* 串口接收功能。 */

/* 原片上ADC采样接口使用PB25上的ADC0通道4。 */
#define ADC12_0_INST                   ADC0        /* 片上ADC实例。 */
#define ADC12_0_INST_IRQHandler        ADC0_IRQHandler /* ADC中断函数名。 */
#define ADC12_0_INST_INT_IRQN          ADC0_INT_IRQn /* ADC中断号。 */
#define ADC12_0_ADCMEM_0               DL_ADC12_MEM_IDX_0 /* 转换存储器索引。 */
#define ADC12_0_ADCMEM_0_REF           DL_ADC12_REFERENCE_VOLTAGE_VDDA /* ADC参考源。 */
#define ADC12_0_ADCMEM_0_REF_VOLTAGE_V (3.3f)     /* ADC参考电压。 */
#define GPIO_ADC12_0_C4_PORT           GPIOB      /* ADC输入端口。 */
#define GPIO_ADC12_0_C4_PIN            DL_GPIO_PIN_25 /* ADC输入引脚。 */
#define GPIO_ADC12_0_IOMUX_C4          IOMUX_PINCM56 /* ADC输入复用号。 */
#define GPIO_ADC12_0_IOMUX_C4_FUNC     IOMUX_PINCM56_PF_UNCONNECTED /* 模拟输入功能。 */

/* SPI0专用于TI板与BX71 FPGA通信：模式0、8位、MSB优先、1 MHz。 */
#define FPGA_SPI_INST                  SPI0       /* FPGA专用SPI实例。 */
#define FPGA_SPI_BIT_RATE_HZ           1000000U   /* FPGA SPI速率。 */
#define GPIO_FPGA_SPI_SCLK_PORT        GPIOA      /* FPGA时钟端口。 */
#define GPIO_FPGA_SPI_SCLK_PIN         DL_GPIO_PIN_12 /* FPGA时钟引脚。 */
#define GPIO_FPGA_SPI_SCLK_IOMUX       IOMUX_PINCM34 /* FPGA时钟复用号。 */
#define GPIO_FPGA_SPI_SCLK_FUNC        IOMUX_PINCM34_PF_SPI0_SCLK /* FPGA时钟功能。 */
#define GPIO_FPGA_SPI_POCI_PORT        GPIOA      /* FPGA返回数据端口。 */
#define GPIO_FPGA_SPI_POCI_PIN         DL_GPIO_PIN_13 /* FPGA返回数据引脚。 */
#define GPIO_FPGA_SPI_POCI_IOMUX       IOMUX_PINCM35 /* FPGA返回数据复用号。 */
#define GPIO_FPGA_SPI_POCI_FUNC        IOMUX_PINCM35_PF_SPI0_POCI /* FPGA返回数据功能。 */
#define GPIO_FPGA_SPI_PICO_PORT        GPIOA      /* FPGA写入数据端口。 */
#define GPIO_FPGA_SPI_PICO_PIN         DL_GPIO_PIN_14 /* FPGA写入数据引脚。 */
#define GPIO_FPGA_SPI_PICO_IOMUX       IOMUX_PINCM36 /* FPGA写入数据复用号。 */
#define GPIO_FPGA_SPI_PICO_FUNC        IOMUX_PINCM36_PF_SPI0_PICO /* FPGA写入数据功能。 */
#define GPIO_FPGA_CS_PORT              GPIOA      /* FPGA片选端口。 */
#define GPIO_FPGA_CS_PIN               DL_GPIO_PIN_15 /* FPGA片选引脚。 */
#define GPIO_FPGA_CS_IOMUX             IOMUX_PINCM37 /* FPGA片选复用号。 */

/* ADS8363为20位帧，使用PB17～PB19软件SPI，避免占用FPGA硬件SPI。 */
#define NUEDC_ADS8363_SPI_PORT       GPIOB          /* ADS8363软件SPI端口。 */
#define NUEDC_ADS8363_SPI_SCLK_PIN   DL_GPIO_PIN_17 /* ADS8363时钟引脚。 */
#define NUEDC_ADS8363_SPI_MOSI_PIN   DL_GPIO_PIN_18 /* ADS8363写入引脚。 */
#define NUEDC_ADS8363_SPI_MISO_PIN   DL_GPIO_PIN_19 /* ADS8363读取引脚。 */
#define NUEDC_ADS8363_SPI_SCLK_IOMUX IOMUX_PINCM43  /* ADS8363时钟复用号。 */
#define NUEDC_ADS8363_SPI_MOSI_IOMUX IOMUX_PINCM44  /* ADS8363写入复用号。 */
#define NUEDC_ADS8363_SPI_MISO_IOMUX IOMUX_PINCM45  /* ADS8363读取复用号。 */

/* 原模块使用的虚拟STM32端口由兼容层映射到以下真实MSPM0引脚。 */
/* 虚拟GPIOA映射到真实GPIOA的输出掩码。 */
#define NUEDC_GPIO_OUT_A_ON_A_MASK (DL_GPIO_PIN_7)
/* 虚拟GPIOA映射到真实GPIOB的输出掩码。 */
#define NUEDC_GPIO_OUT_A_ON_B_MASK \
    (DL_GPIO_PIN_0 | DL_GPIO_PIN_4 | DL_GPIO_PIN_6)
#define NUEDC_GPIO_IN_A_ON_B_MASK (DL_GPIO_PIN_1) /* 虚拟GPIOA映射到GPIOB的输入掩码。 */
/* 虚拟GPIOB输出掩码。 */
#define NUEDC_GPIO_OUT_B_MASK \
    (DL_GPIO_PIN_5 | DL_GPIO_PIN_7 | DL_GPIO_PIN_8 | DL_GPIO_PIN_9 | DL_GPIO_PIN_12)
/* 映射到GPIOA的虚拟GPIOC输出掩码，PA9替代BSL专用的PA18。 */
#define NUEDC_GPIO_OUT_C_ON_A_MASK \
    (DL_GPIO_PIN_8 | DL_GPIO_PIN_9 | DL_GPIO_PIN_16 | DL_GPIO_PIN_17)
#define NUEDC_GPIO_OUT_C_ON_B_MASK (DL_GPIO_PIN_13) /* 映射到GPIOB的虚拟GPIOC输出。 */
/* 映射到GPIOB的虚拟GPIOE输出掩码。 */
#define NUEDC_GPIO_OUT_E_ON_B_MASK \
    (DL_GPIO_PIN_2 | DL_GPIO_PIN_20 | DL_GPIO_PIN_22)
#define NUEDC_GPIO_OUT_H_ON_B_MASK (DL_GPIO_PIN_23) /* 映射到GPIOB的虚拟GPIOH输出。 */
#define NUEDC_GPIO_IN_H_ON_B_MASK  (DL_GPIO_PIN_24) /* 映射到GPIOB的虚拟GPIOH输入。 */

#define NUEDC_IDLE_HIGH_CS_B_MASK \
    (DL_GPIO_PIN_0 | DL_GPIO_PIN_9 | DL_GPIO_PIN_12 | DL_GPIO_PIN_22) /* DAC8831、AD9910、ADS8363和AD5687低有效片选的空闲高电平。 */

void SYSCFG_DL_init(void);          /* 初始化全部基础资源。 */
void SYSCFG_DL_initPower(void);     /* 初始化外设电源域。 */
void SYSCFG_DL_GPIO_init(void);     /* 初始化数字引脚。 */
void SYSCFG_DL_SYSCTL_init(void);   /* 初始化系统时钟。 */
void SYSCFG_DL_UART_0_init(void);   /* 初始化UART0。 */
void SYSCFG_DL_ADC12_0_init(void);  /* 初始化片上ADC0。 */
void SYSCFG_DL_FPGA_SPI_init(void); /* 初始化FPGA专用SPI0。 */

#ifdef __cplusplus
}
#endif

#endif /* TI_MSP_DL_CONFIG_H */
