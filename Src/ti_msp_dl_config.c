/*
 *  ============ ti_msp_dl_config.c =============
 *  MSPM0G3507 DriverLib configuration for the NUEDC 2023 D port.
 */

#include "ti_msp_dl_config.h"

/* UART0时钟配置。 */
static const DL_UART_Main_ClockConfig gUART_0ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

/* UART0数据格式配置。 */
static const DL_UART_Main_Config gUART_0Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

/* 片上ADC时钟配置。 */
static const DL_ADC12_ClockConfig gADC12_0ClockConfig = {
    .clockSel    = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_1,
    .freqRange   = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32
};

/* TI板与FPGA通信使用的SPI0固定工作参数。 */
static const DL_SPI_Config gFpgaSpiConfig = {
    .mode          = DL_SPI_MODE_CONTROLLER,
    .frameFormat   = DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA0,
    .parity        = DL_SPI_PARITY_NONE,
    .dataSize      = DL_SPI_DATA_SIZE_8,
    .bitOrder      = DL_SPI_BIT_ORDER_MSB_FIRST,
    .chipSelectPin = DL_SPI_CHIP_SELECT_NONE
};

/* SPI0直接使用32 MHz总线时钟。 */
static const DL_SPI_ClockConfig gFpgaSpiClockConfig = {
    .clockSel    = DL_SPI_CLOCK_BUSCLK,
    .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1
};

/* 按固定顺序初始化TI板全部基础资源。 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_GPIO_init();
    SYSCFG_DL_UART_0_init();
    SYSCFG_DL_ADC12_0_init();
    SYSCFG_DL_FPGA_SPI_init();
}

/* 复位并开启所用外设电源域。 */
SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_UART_Main_reset(UART_0_INST);
    DL_ADC12_reset(ADC12_0_INST);
    DL_SPI_reset(FPGA_SPI_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_UART_Main_enablePower(UART_0_INST);
    DL_ADC12_enablePower(ADC12_0_INST);
    DL_SPI_enablePower(FPGA_SPI_INST);

    delay_cycles(POWER_STARTUP_DELAY);
}

/* 配置32 MHz系统时钟和欠压复位。 */
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
}

/* 配置集成工程全部数字引脚及默认电平。 */
SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{
    /* 虚拟GPIOA：PA7输出，PB0/PB4/PB6输出，PB1输入。 */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM12);
    DL_GPIO_initDigitalInput(IOMUX_PINCM13);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM14);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM17);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM23);

    /* 虚拟GPIOB：PB5/PB7/PB8/PB9/PB12输出。 */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM18);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM24);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM25);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM26);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM29);

    /* 虚拟GPIOC映射到PA8、PA16～PA18和PB13，避开SWD引脚。 */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM19);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM38);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM39);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM40);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM30);

    /* 虚拟GPIOE映射到PB20、PB2和PB22输出，避开板载按键PB21。 */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM15);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM48);
    DL_GPIO_initDigitalOutput(IOMUX_PINCM50);

    /* 虚拟GPIOH映射到PB23输出和PB24输入。 */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM51);
    DL_GPIO_initDigitalInput(IOMUX_PINCM52);

    /* ADS8363软件SPI：PB17时钟、PB18输出、PB19输入。 */
    DL_GPIO_initDigitalOutput(NUEDC_ADS8363_SPI_SCLK_IOMUX);
    DL_GPIO_initDigitalOutput(NUEDC_ADS8363_SPI_MOSI_IOMUX);
    DL_GPIO_initDigitalInput(NUEDC_ADS8363_SPI_MISO_IOMUX);

    /* FPGA专用SPI0：PA12 SCLK、PA14 MOSI、PA13 MISO、PA15软件片选。 */
    DL_GPIO_initPeripheralOutputFunction(GPIO_FPGA_SPI_SCLK_IOMUX,
                                         GPIO_FPGA_SPI_SCLK_FUNC);
    DL_GPIO_initPeripheralOutputFunction(GPIO_FPGA_SPI_PICO_IOMUX,
                                         GPIO_FPGA_SPI_PICO_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_FPGA_SPI_POCI_IOMUX,
                                        GPIO_FPGA_SPI_POCI_FUNC);
    DL_GPIO_initDigitalOutput(GPIO_FPGA_CS_IOMUX);

    /* 配置板载状态引脚。 */
    DL_GPIO_initDigitalOutput(GPIO_LEDS_USER_LED_1_IOMUX);
    DL_GPIO_initDigitalOutput(GPIO_LEDS_USER_TEST_IOMUX);

    DL_GPIO_clearPins(GPIOA,
        NUEDC_GPIO_OUT_A_ON_A_MASK | NUEDC_GPIO_OUT_C_ON_A_MASK);
    DL_GPIO_clearPins(GPIOB,
        NUEDC_GPIO_OUT_A_ON_B_MASK |
        NUEDC_GPIO_OUT_B_MASK |
        NUEDC_GPIO_OUT_C_ON_B_MASK |
        NUEDC_GPIO_OUT_E_ON_B_MASK |
        NUEDC_GPIO_OUT_H_ON_B_MASK |
        NUEDC_ADS8363_SPI_SCLK_PIN |
        NUEDC_ADS8363_SPI_MOSI_PIN |
        GPIO_LEDS_USER_LED_1_PIN |
        GPIO_LEDS_USER_TEST_PIN);
    DL_GPIO_setPins(GPIOB, NUEDC_IDLE_HIGH_CS_B_MASK);
    DL_GPIO_setPins(GPIO_FPGA_CS_PORT, GPIO_FPGA_CS_PIN);

    DL_GPIO_enableOutput(GPIOA,
        NUEDC_GPIO_OUT_A_ON_A_MASK | NUEDC_GPIO_OUT_C_ON_A_MASK);
    DL_GPIO_enableOutput(GPIOB,
        NUEDC_GPIO_OUT_A_ON_B_MASK |
        NUEDC_GPIO_OUT_B_MASK |
        NUEDC_GPIO_OUT_C_ON_B_MASK |
        NUEDC_GPIO_OUT_E_ON_B_MASK |
        NUEDC_GPIO_OUT_H_ON_B_MASK |
        NUEDC_ADS8363_SPI_SCLK_PIN |
        NUEDC_ADS8363_SPI_MOSI_PIN |
        GPIO_LEDS_USER_LED_1_PIN |
        GPIO_LEDS_USER_TEST_PIN);
    DL_GPIO_enableOutput(GPIO_FPGA_CS_PORT, GPIO_FPGA_CS_PIN);
}

/* 初始化115200-8-N-1的UART0。 */
SYSCONFIG_WEAK void SYSCFG_DL_UART_0_init(void)
{
    DL_GPIO_initPeripheralOutputFunction(GPIO_UART_0_IOMUX_TX, GPIO_UART_0_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_UART_0_IOMUX_RX, GPIO_UART_0_IOMUX_RX_FUNC);

    DL_UART_Main_setClockConfig(UART_0_INST, (DL_UART_Main_ClockConfig *) &gUART_0ClockConfig);
    DL_UART_Main_init(UART_0_INST, (DL_UART_Main_Config *) &gUART_0Config);
    DL_UART_Main_configBaudRate(UART_0_INST, UART_0_INST_FREQUENCY, UART_0_BAUD_RATE);
    DL_UART_Main_enableFIFOs(UART_0_INST);
    DL_UART_Main_setRXFIFOThreshold(UART_0_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(UART_0_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);
    DL_UART_Main_enable(UART_0_INST);
}

/* 初始化PB25上的ADC0通道4。 */
SYSCONFIG_WEAK void SYSCFG_DL_ADC12_0_init(void)
{
    DL_ADC12_setClockConfig(ADC12_0_INST, (DL_ADC12_ClockConfig *) &gADC12_0ClockConfig);
    DL_ADC12_initSingleSample(ADC12_0_INST,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC12_0_INST,
        ADC12_0_ADCMEM_0,
        DL_ADC12_INPUT_CHAN_4,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setPowerDownMode(ADC12_0_INST, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(ADC12_0_INST, 8U);
    DL_ADC12_clearInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC12_0_INST);
}

/* 初始化TI板到FPGA的独占SPI0，串行时钟为1 MHz。 */
SYSCONFIG_WEAK void SYSCFG_DL_FPGA_SPI_init(void)
{
    uint32_t divider;

    DL_SPI_setClockConfig(FPGA_SPI_INST,
                          (DL_SPI_ClockConfig *)&gFpgaSpiClockConfig);
    DL_SPI_init(FPGA_SPI_INST, (DL_SPI_Config *)&gFpgaSpiConfig);

    divider = (CPUCLK_FREQ / (2U * FPGA_SPI_BIT_RATE_HZ)) - 1U;
    DL_SPI_setBitRateSerialClockDivider(FPGA_SPI_INST, divider);
    DL_SPI_setFIFOThreshold(FPGA_SPI_INST,
                            DL_SPI_RX_FIFO_LEVEL_ONE_FRAME,
                            DL_SPI_TX_FIFO_LEVEL_ONE_FRAME);
    DL_SPI_enable(FPGA_SPI_INST);
}
