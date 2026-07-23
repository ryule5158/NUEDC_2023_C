#include "nuedc_port.h"
#include "adc_app.h"
#include "ad5687_app.h"
#include "ad9910_app.h"
#include "ad9708_app.h"
#include "ad9280_app.h"
#include "ads8363_app.h"
#include "dac8831_app.h"
#include "screen_test.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>

/* 初始化TI板基础资源及已启用的通用模块。 */
void NUEDC_PortInit(void)
{
    NUEDC_HAL_Init();
    if (ADC_Sample_Init(3.3f, 20000.0f, 8000.0f,
                        ADC_APP_PRINT_RAW_OFF, ADC_MODE_ONESHOT) == HAL_OK)
    {
        (void)ADC_Sample_Start();
    }

#if (NUEDC_ENABLE_AD5687 != 0U)
    (void)AD5687_AppInit();
#endif
#if (NUEDC_ENABLE_DAC8831 != 0U)
    (void)DAC8831_AppInit();
#endif
#if (NUEDC_ENABLE_AD9910 != 0U)
    (void)AD9910_AppInit();
#endif
#if (NUEDC_ENABLE_ADS8363 != 0U)
    (void)ADS8363_AppInit(ADS8363_APP_CHANNEL_0, 2500.0f, 1000.0f, 200U);
#endif

#if (NUEDC_ENABLE_FPGA_MODULE_INIT != 0U)
    {
        AD9708_StatusTypeDef dac_status; /* FPGA高速DAC初始化结果。 */
        AD9280_StatusTypeDef adc_status; /* FPGA高速ADC初始化结果。 */

        dac_status = AD9708_AppInit();
        adc_status = AD9280_AppInit();
        printf("FPGA AD9708=%d, AD9280=%d\r\n",
               (int)dac_status,
               (int)adc_status);
    }
#endif
}

/* 在主循环中调度已启用的非阻塞任务。 */
void NUEDC_PortProcess(void)
{
    NUEDC_HAL_Service();
    ADC_Sample_Process();

#if (NUEDC_ENABLE_AD5687 != 0U)
    AD5687_AppProcess();
#endif
#if (NUEDC_ENABLE_DAC8831 != 0U)
    DAC8831_AppProcess();
#endif
#if (NUEDC_ENABLE_AD9910 != 0U)
    AD9910_AppProcess();
#endif
#if (NUEDC_ENABLE_ADS8363 != 0U)
    ADS8363_AppProcess();
#endif
#if (NUEDC_ENABLE_SCREEN_PROCESS != 0U)
    Screen_Process();
#endif

    NUEDC_HAL_Service();
}

/* 返回系统毫秒节拍。 */
uint32_t NUEDC_PortGetTick(void)
{
    return HAL_GetTick();
}
