#ifndef NUEDC_PORT_H
#define NUEDC_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void NUEDC_PortInit(void);           /* 初始化TI板集成端口。 */
void NUEDC_PortProcess(void);        /* 调度TI板集成端口任务。 */
uint32_t NUEDC_PortGetTick(void);    /* 返回系统毫秒节拍。 */

#ifndef NUEDC_ENABLE_EXTERNAL_MODULE_INIT
#define NUEDC_ENABLE_EXTERNAL_MODULE_INIT 0U /* 兼容用总开关，默认不初始化未接模块。 */
#endif

#ifndef NUEDC_ENABLE_AD5687
#define NUEDC_ENABLE_AD5687 NUEDC_ENABLE_EXTERNAL_MODULE_INIT /* AD5687任务开关。 */
#endif

#ifndef NUEDC_ENABLE_DAC8831
#define NUEDC_ENABLE_DAC8831 NUEDC_ENABLE_EXTERNAL_MODULE_INIT /* DAC8831任务开关。 */
#endif

#ifndef NUEDC_ENABLE_AD9910
#define NUEDC_ENABLE_AD9910 NUEDC_ENABLE_EXTERNAL_MODULE_INIT /* AD9910任务开关。 */
#endif

#ifndef NUEDC_ENABLE_ADS8363
#define NUEDC_ENABLE_ADS8363 NUEDC_ENABLE_EXTERNAL_MODULE_INIT /* ADS8363任务开关。 */
#endif

#ifndef NUEDC_ENABLE_SCREEN_PROCESS
#define NUEDC_ENABLE_SCREEN_PROCESS NUEDC_ENABLE_EXTERNAL_MODULE_INIT /* 串口屏任务开关。 */
#endif

#ifndef NUEDC_ENABLE_FPGA_MODULE_INIT
#define NUEDC_ENABLE_FPGA_MODULE_INIT 1U /* 上电时核验高速AD/DA的FPGA链路。 */
#endif

#ifdef __cplusplus
}
#endif

#endif /* NUEDC_PORT_H */
