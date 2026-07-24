/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_SPI_backupConfig gFPGA_SPIBackup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_UART_0_init();
    SYSCFG_DL_FPGA_SPI_init();
    SYSCFG_DL_ADC12_0_init();
    /* Ensure backup structures have no valid state */

	gFPGA_SPIBackup.backupRdy 	= false;

}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_SPI_saveConfiguration(FPGA_SPI_INST, &gFPGA_SPIBackup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_SPI_restoreConfiguration(FPGA_SPI_INST, &gFPGA_SPIBackup);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_UART_Main_reset(UART_0_INST);
    DL_SPI_reset(FPGA_SPI_INST);
    DL_ADC12_reset(ADC12_0_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_UART_Main_enablePower(UART_0_INST);
    DL_SPI_enablePower(FPGA_SPI_INST);
    DL_ADC12_enablePower(ADC12_0_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART_0_IOMUX_TX, GPIO_UART_0_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART_0_IOMUX_RX, GPIO_UART_0_IOMUX_RX_FUNC);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_FPGA_SPI_IOMUX_SCLK, GPIO_FPGA_SPI_IOMUX_SCLK_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_FPGA_SPI_IOMUX_PICO, GPIO_FPGA_SPI_IOMUX_PICO_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_FPGA_SPI_IOMUX_POCI, GPIO_FPGA_SPI_IOMUX_POCI_FUNC);

    DL_GPIO_initDigitalOutput(GPIO_FPGA_CONTROL_FPGA_CS_N_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_BOARD_STATUS_LED_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_BOARD_USER_TEST_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_DAC_SHARED_SERIAL_CLOCK_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_DAC_SHARED_SERIAL_DATA_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_DAC_SHARED_DAC8831_CS_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_DAC_SHARED_AD5687_CS_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_SCK_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_SDIO_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_PWR_IOMUX);

    DL_GPIO_initDigitalInputFeatures(GPIO_AD9910_DROVER_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_DRCTL_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_DRHOLD_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_RESET_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_PROFILE_1_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_PROFILE_2_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_PROFILE_0_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_OSK_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_IO_UPDATE_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_AD9910_AD9910_CS_N_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_ADS8363_ADS8363_CS_N_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_ADS8363_RD_CONVST_IOMUX);

    DL_GPIO_initDigitalInputFeatures(GPIO_ADS8363_BUSY_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutput(GPIO_ADS8363_SW_SPI_SCLK_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_ADS8363_SW_SPI_MOSI_IOMUX);

    DL_GPIO_initDigitalInputFeatures(GPIO_ADS8363_SW_SPI_MISO_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_clearPins(GPIOA, GPIO_DAC_SHARED_SERIAL_DATA_PIN |
		GPIO_AD9910_DRCTL_PIN |
		GPIO_AD9910_DRHOLD_PIN |
		GPIO_AD9910_PROFILE_1_PIN |
		GPIO_AD9910_PROFILE_2_PIN);
    DL_GPIO_setPins(GPIOA, GPIO_FPGA_CONTROL_FPGA_CS_N_PIN);
    DL_GPIO_enableOutput(GPIOA, GPIO_FPGA_CONTROL_FPGA_CS_N_PIN |
		GPIO_DAC_SHARED_SERIAL_DATA_PIN |
		GPIO_AD9910_DRCTL_PIN |
		GPIO_AD9910_DRHOLD_PIN |
		GPIO_AD9910_PROFILE_1_PIN |
		GPIO_AD9910_PROFILE_2_PIN);
    DL_GPIO_clearPins(GPIOB, GPIO_BOARD_STATUS_LED_PIN |
		GPIO_BOARD_USER_TEST_PIN |
		GPIO_DAC_SHARED_SERIAL_CLOCK_PIN |
		GPIO_AD9910_SCK_PIN |
		GPIO_AD9910_SDIO_PIN |
		GPIO_AD9910_PWR_PIN |
		GPIO_AD9910_RESET_PIN |
		GPIO_AD9910_PROFILE_0_PIN |
		GPIO_AD9910_OSK_PIN |
		GPIO_AD9910_IO_UPDATE_PIN |
		GPIO_ADS8363_RD_CONVST_PIN |
		GPIO_ADS8363_SW_SPI_SCLK_PIN |
		GPIO_ADS8363_SW_SPI_MOSI_PIN);
    DL_GPIO_setPins(GPIOB, GPIO_DAC_SHARED_DAC8831_CS_PIN |
		GPIO_DAC_SHARED_AD5687_CS_PIN |
		GPIO_AD9910_AD9910_CS_N_PIN |
		GPIO_ADS8363_ADS8363_CS_N_PIN);
    DL_GPIO_enableOutput(GPIOB, GPIO_BOARD_STATUS_LED_PIN |
		GPIO_BOARD_USER_TEST_PIN |
		GPIO_DAC_SHARED_SERIAL_CLOCK_PIN |
		GPIO_DAC_SHARED_DAC8831_CS_PIN |
		GPIO_DAC_SHARED_AD5687_CS_PIN |
		GPIO_AD9910_SCK_PIN |
		GPIO_AD9910_SDIO_PIN |
		GPIO_AD9910_PWR_PIN |
		GPIO_AD9910_RESET_PIN |
		GPIO_AD9910_PROFILE_0_PIN |
		GPIO_AD9910_OSK_PIN |
		GPIO_AD9910_IO_UPDATE_PIN |
		GPIO_AD9910_AD9910_CS_N_PIN |
		GPIO_ADS8363_ADS8363_CS_N_PIN |
		GPIO_ADS8363_RD_CONVST_PIN |
		GPIO_ADS8363_SW_SPI_SCLK_PIN |
		GPIO_ADS8363_SW_SPI_MOSI_PIN);

}



SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
	/* Set default configuration */
	DL_SYSCTL_disableHFXT();
	DL_SYSCTL_disableSYSPLL();

}


static const DL_UART_Main_ClockConfig gUART_0ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART_0Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART_0_init(void)
{
    DL_UART_Main_setClockConfig(UART_0_INST, (DL_UART_Main_ClockConfig *) &gUART_0ClockConfig);

    DL_UART_Main_init(UART_0_INST, (DL_UART_Main_Config *) &gUART_0Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART_0_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_0_INST, UART_0_IBRD_32_MHZ_115200_BAUD, UART_0_FBRD_32_MHZ_115200_BAUD);


    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(UART_0_INST);
    DL_UART_Main_setRXFIFOThreshold(UART_0_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(UART_0_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_enable(UART_0_INST);
}

static const DL_SPI_Config gFPGA_SPI_config = {
    .mode        = DL_SPI_MODE_CONTROLLER,
    .frameFormat = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0,
    .parity      = DL_SPI_PARITY_NONE,
    .dataSize    = DL_SPI_DATA_SIZE_8,
    .bitOrder    = DL_SPI_BIT_ORDER_MSB_FIRST,
};

static const DL_SPI_ClockConfig gFPGA_SPI_clockConfig = {
    .clockSel    = DL_SPI_CLOCK_BUSCLK,
    .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1
};

SYSCONFIG_WEAK void SYSCFG_DL_FPGA_SPI_init(void) {
    DL_SPI_setClockConfig(FPGA_SPI_INST, (DL_SPI_ClockConfig *) &gFPGA_SPI_clockConfig);

    DL_SPI_init(FPGA_SPI_INST, (DL_SPI_Config *) &gFPGA_SPI_config);

    /* Configure Controller mode */
    /*
     * Set the bit rate clock divider to generate the serial output clock
     *     outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
     *     1000000 = (32000000)/((1 + 15) * 2)
     */
    DL_SPI_setBitRateSerialClockDivider(FPGA_SPI_INST, 15);
    /* Set RX and TX FIFO threshold levels */
    DL_SPI_setFIFOThreshold(FPGA_SPI_INST, DL_SPI_RX_FIFO_LEVEL_ONE_FRAME, DL_SPI_TX_FIFO_LEVEL_ONE_FRAME);

    /* Enable module */
    DL_SPI_enable(FPGA_SPI_INST);
}

/* ADC12_0 Initialization */
static const DL_ADC12_ClockConfig gADC12_0ClockConfig = {
    .clockSel       = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio    = DL_ADC12_CLOCK_DIVIDE_1,
    .freqRange      = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};
SYSCONFIG_WEAK void SYSCFG_DL_ADC12_0_init(void)
{
    DL_ADC12_setClockConfig(ADC12_0_INST, (DL_ADC12_ClockConfig *) &gADC12_0ClockConfig);
    DL_ADC12_configConversionMem(ADC12_0_INST, ADC12_0_ADCMEM_0,
        DL_ADC12_INPUT_CHAN_4, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setPowerDownMode(ADC12_0_INST,DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(ADC12_0_INST,8);
    DL_ADC12_enableConversions(ADC12_0_INST);
}

