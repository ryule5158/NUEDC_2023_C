#include "ti_msp_dl_config.h"
#include "nuedc_port.h"

#define NUEDC_LED_TOGGLE_PERIOD_MS 500U /* 板载状态灯翻转周期，单位ms。 */

/* 初始化TI集成工程并持续调度非阻塞任务。 */
int main(void)
{
    uint32_t led_tick; /* 最近一次状态灯翻转时刻。 */

    SYSCFG_DL_init();
    NUEDC_PortInit();

    DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
    led_tick = NUEDC_PortGetTick();

    while (1) {
        NUEDC_PortProcess();
        if ((NUEDC_PortGetTick() - led_tick) >= NUEDC_LED_TOGGLE_PERIOD_MS) {
            led_tick = NUEDC_PortGetTick();
            DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
        }
    }
}
