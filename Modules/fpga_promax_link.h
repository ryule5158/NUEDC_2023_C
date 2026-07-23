#ifndef __FPGA_PROMAX_LINK_H
#define __FPGA_PROMAX_LINK_H /* 主控板ProMax链路封装接口包含保护。 */

#ifdef __cplusplus
extern "C" {
#endif

#include "fpga_promax.h"

/* 使用当前主控板的FPGA链路初始化一个ProMax设备句柄。 */
FpgaPromax_Result FPGA_ProMax_Init(FpgaPromax *dev);

#ifdef __cplusplus
}
#endif

#endif /* __FPGA_PROMAX_LINK_H */
