#include "MSP_LITO_G3507_RuntimeConfig.h"

#include "ti_msp_dl_config.h"

void MSP_LITO_G3507_ApplyRuntimeConfig(void)
{
    /*
     * SysConfig models an external GPIO chip select as Motorola 3-wire.
     * The BX71 register link was hardware-validated with Motorola 4-wire
     * framing while PA15 remains a separately controlled GPIO. Restore that
     * single CTL0 field after the generated initialization.
     */
    DL_SPI_disable(FPGA_SPI_INST);
    DL_SPI_setFrameFormat(
        FPGA_SPI_INST,
        DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA0);
    DL_SPI_setChipSelect(FPGA_SPI_INST, DL_SPI_CHIP_SELECT_NONE);
    DL_SPI_enable(FPGA_SPI_INST);
}

bool MSP_LITO_G3507_IsFpgaSpiConfigValid(void)
{
    return (DL_SPI_getFrameFormat(FPGA_SPI_INST) ==
            DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA0);
}
