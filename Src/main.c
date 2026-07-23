#include "ti_msp_dl_config.h"
#include "Modules_Header.h"
#include "stm32h7xx_hal.h"

#define UART_TEST_REPORT_LOOPS       (2000000U) /* 周期存活提示的软件轮询次数。 */
#define AD9708_TEST_SINE_HZ          (100000.0f) /* 高速DAC正弦波测试频率。 */
#define AD9708_TEST_AMPLITUDE_CODE   (100U) /* 高速DAC正弦波峰值原始码。 */
#define AD9708_TEST_OFFSET_CODE      (128U) /* 高速DAC正弦波中心原始码。 */
#define AD9280_TEST_SAMPLES          (1024U) /* 高速ADC单次测试采样点数。 */
#define AD9280_TEST_DECIMATION       (1U) /* 高速ADC测试抽取倍数。 */
#define AD9280_TEST_TIMEOUT_MS       (200U) /* 高速ADC采集超时时间。 */
#define AD9280_TEST_PRINT_SAMPLES    (32U) /* 串口显示的前部采样点数。 */
#define PROMAX_TEST_TIMEOUT_MS       (200U) /* ProMax结果等待超时时间。 */
#define PROMAX_TEST_SAMPLE_RATE_HZ   (32000000U) /* ProMax输入采样率。 */
#define PROMAX_TEST_DDC_STEP_HZ      (100000U) /* 相邻DDC测试通道频率间隔。 */
#define FPGA_CS_REAL_PORT            ((GPIO_Regs *)GPIOA_BASE) /* PA15真实GPIO端口。 */

static FpgaPromax s_promax; /* FPGA ProMax测试句柄。 */
static uint8_t s_promax_ready; /* FPGA ProMax初始化成功标志。 */
static const int32_t s_promax_impulse[FPGA_PROMAX_MF_TAPS] = {
    65536
}; /* ProMax匹配滤波测试使用的单位冲激模板。 */

/* 通过UART0阻塞发送以零结尾的字符串。 */
static void UART_TestWriteString(const char *text)
{
    while (*text != '\0')
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t)*text);
        text++;
    }
}

/* 通过UART0发送一个无符号十进制整数。 */
static void UART_TestWriteUint32(uint32_t value)
{
    char digits[10]; /* 十进制数字逆序缓存。 */
    uint8_t count = 0U; /* 当前缓存的数字数量。 */

    do
    {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count++;
    } while ((value != 0U) && (count < sizeof(digits)));

    while (count > 0U)
    {
        count--;
        DL_UART_Main_transmitDataBlocking(
            UART_0_INST,
            (uint8_t)digits[count]);
    }
}

/* 通过UART0发送一个64位无符号十进制整数。 */
static void UART_TestWriteUint64(uint64_t value)
{
    char digits[20]; /* 64位十进制数字逆序缓存。 */
    uint8_t count = 0U; /* 当前缓存的数字数量。 */

    do
    {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count++;
    } while ((value != 0U) && (count < sizeof(digits)));

    while (count > 0U)
    {
        count--;
        DL_UART_Main_transmitDataBlocking(
            UART_0_INST,
            (uint8_t)digits[count]);
    }
}

/* 通过UART0发送一个有符号十进制整数。 */
static void UART_TestWriteInt32(int32_t value)
{
    if (value < 0)
    {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t)'-');
        UART_TestWriteUint32((uint32_t)(-value));
    }
    else
    {
        UART_TestWriteUint32((uint32_t)value);
    }
}

/* 将四位数值转换为大写十六进制字符。 */
static char UART_TestHexDigit(uint8_t value)
{
    value &= 0x0FU;
    return (value < 10U) ?
           (char)('0' + value) :
           (char)('A' + value - 10U);
}

/* 通过UART0发送一个两位十六进制数。 */
static void UART_TestWriteHex8(uint8_t value)
{
    DL_UART_Main_transmitDataBlocking(
        UART_0_INST,
        (uint8_t)UART_TestHexDigit((uint8_t)(value >> 4)));
    DL_UART_Main_transmitDataBlocking(
        UART_0_INST,
        (uint8_t)UART_TestHexDigit(value));
}

/* 通过UART0发送一个八位十六进制数。 */
static void UART_TestWriteHex32(uint32_t value)
{
    uint8_t shift; /* 当前输出的半字节位移。 */

    for (shift = 32U; shift > 0U; shift -= 4U)
    {
        DL_UART_Main_transmitDataBlocking(
            UART_0_INST,
            (uint8_t)UART_TestHexDigit((uint8_t)(value >> (shift - 4U))));
    }
}

/* 返回高速DAC状态的简洁调试字符串。 */
static const char *AD9708_TestStatusName(AD9708_StatusTypeDef status)
{
    switch (status)
    {
        case AD9708_OK:
            return "OK";
        case AD9708_ERROR_PARAM:
            return "PARAM";
        case AD9708_ERROR_NOT_INIT:
            return "NOT_INIT";
        case AD9708_ERROR_LINK:
            return "LINK";
        case AD9708_ERROR_DEVICE:
            return "DEVICE";
        case AD9708_ERROR_PLL:
            return "PLL";
        case AD9708_ERROR_VERIFY:
            return "VERIFY";
        case AD9708_ERROR_CALIBRATION:
            return "CALIBRATION";
        default:
            return "UNKNOWN";
    }
}

/* 返回高速ADC状态的简洁调试字符串。 */
static const char *AD9280_TestStatusName(AD9280_StatusTypeDef status)
{
    switch (status)
    {
        case AD9280_OK:
            return "OK";
        case AD9280_ERROR_PARAM:
            return "PARAM";
        case AD9280_ERROR_NOT_INIT:
            return "NOT_INIT";
        case AD9280_ERROR_LINK:
            return "LINK";
        case AD9280_ERROR_DEVICE:
            return "DEVICE";
        case AD9280_ERROR_NOT_READY:
            return "NOT_READY";
        case AD9280_ERROR_BUSY:
            return "BUSY";
        case AD9280_ERROR_TIMEOUT:
            return "TIMEOUT";
        case AD9280_ERROR_NO_DATA:
            return "NO_DATA";
        default:
            return "UNKNOWN";
    }
}

/* 初始化UART、独占SPI0和工程所需基础资源。 */
static void Board_TestInit(void)
{
    SYSCFG_DL_init();
    NUEDC_HAL_Init();
}

/* 打印高速DAC初始化结果及FPGA标识。 */
static AD9708_StatusTypeDef AD9708_TestInitAndReport(void)
{
    AD9708_StatusTypeDef status; /* 高速DAC初始化状态。 */
    const AD9708_DataTypeDef *data; /* 高速DAC底层状态。 */

    status = AD9708_AppInit();
    data = AD9708_AppGetData();
    UART_TestWriteString("[AD9708] ");
    UART_TestWriteString(AD9708_TestStatusName(status));
    UART_TestWriteString(" ID=0x");
    UART_TestWriteHex32(data->device_id);
    UART_TestWriteString(" FW=0x");
    UART_TestWriteHex32(data->firmware_version);
    UART_TestWriteString(" PLL=");
    UART_TestWriteUint32(data->pll_locked);
    UART_TestWriteString(" READY=");
    UART_TestWriteUint32(data->dac_ready);
    UART_TestWriteString("\r\n");
    return status;
}

/* 打印高速ADC初始化结果及FPGA标识。 */
static AD9280_StatusTypeDef AD9280_TestInitAndReport(void)
{
    AD9280_StatusTypeDef status; /* 高速ADC初始化状态。 */
    const AD9280_DataTypeDef *data; /* 高速ADC底层状态。 */

    status = AD9280_AppInit();
    data = AD9280_GetData();
    UART_TestWriteString("[AD9280] ");
    UART_TestWriteString(AD9280_TestStatusName(status));
    UART_TestWriteString(" ID=0x");
    UART_TestWriteHex32(data->device_id);
    UART_TestWriteString(" FW=0x");
    UART_TestWriteHex32(data->firmware_version);
    UART_TestWriteString(" CLK=");
    UART_TestWriteUint32(data->sample_clock_hz);
    UART_TestWriteString("Hz READY=");
    UART_TestWriteUint32(data->capture.adc_ready);
    UART_TestWriteString("\r\n");
    return status;
}

/* 打印ProMax设备版本和能力字。 */
static FpgaPromax_Result ProMax_TestInitAndReport(void)
{
    FpgaPromax_Result result; /* ProMax初始化结果。 */

    result = FPGA_ProMax_Init(&s_promax);
    s_promax_ready = (result == FPGA_PROMAX_OK) ? 1U : 0U;
    UART_TestWriteString("[PROMAX] ");
    UART_TestWriteString(FpgaPromax_ResultString(result));
    if (result == FPGA_PROMAX_OK)
    {
        UART_TestWriteString(" ID=0x");
        UART_TestWriteHex32(FPGA_PROMAX_EXPECTED_ID);
        UART_TestWriteString(" FW=0x");
        UART_TestWriteHex32(s_promax.version);
        UART_TestWriteString(" CAP=0x");
        UART_TestWriteHex32(s_promax.capability);
        UART_TestWriteString(" REALTIME=");
        UART_TestWriteUint32(FpgaPromax_HasRealtime(&s_promax));
    }
    UART_TestWriteString("\r\n");
    return result;
}

/* 依次读取FPGA内三个功能块的设备信息。 */
static void FPGA_TestScan(void)
{
    UART_TestWriteString("\r\n[FPGA] LINK SCAN START\r\n");
    (void)AD9708_TestInitAndReport();
    (void)AD9280_TestInitAndReport();
    (void)ProMax_TestInitAndReport();
    UART_TestWriteString("[FPGA] LINK SCAN END\r\n");
}

/* 确保高速DAC已经通过初始化核验。 */
static AD9708_StatusTypeDef AD9708_TestEnsureReady(void)
{
    const AD9708_DataTypeDef *data = AD9708_AppGetData(); /* 高速DAC状态。 */

    if (data->initialized != 0U)
    {
        return AD9708_OK;
    }
    return AD9708_TestInitAndReport();
}

/* 确保高速ADC已经通过初始化核验。 */
static AD9280_StatusTypeDef AD9280_TestEnsureReady(void)
{
    const AD9280_AppDataTypeDef *data = AD9280_AppGetData(); /* 高速ADC状态。 */

    if (data->initialized != 0U)
    {
        return AD9280_OK;
    }
    return AD9280_TestInitAndReport();
}

/* 确保ProMax功能块已经通过初始化核验。 */
static FpgaPromax_Result ProMax_TestEnsureReady(void)
{
    if (s_promax_ready != 0U)
    {
        return FPGA_PROMAX_OK;
    }
    return ProMax_TestInitAndReport();
}

/* 输出一个高速DAC恒定原始码，不修改电压校准参数。 */
static void AD9708_TestConstant(uint8_t code)
{
    AD9708_StatusTypeDef status; /* 高速DAC输出状态。 */

    status = AD9708_TestEnsureReady();
    if (status == AD9708_OK)
    {
        status = AD9708_AppOutputCalibrationCode(code);
    }

    UART_TestWriteString("[AD9708] CONSTANT CODE=");
    UART_TestWriteUint32(code);
    UART_TestWriteString(" ");
    UART_TestWriteString(AD9708_TestStatusName(status));
    UART_TestWriteString("\r\n");
}

/* 输出100kHz高速DAC原始码正弦波，不修改电压校准参数。 */
static AD9708_StatusTypeDef AD9708_TestSine(void)
{
    AD9708_StatusTypeDef status; /* 高速DAC输出状态。 */

    status = AD9708_TestEnsureReady();
    if (status == AD9708_OK)
    {
        status = AD9708_AppOutputSineCode(
            AD9708_TEST_SINE_HZ,
            AD9708_TEST_AMPLITUDE_CODE,
            AD9708_TEST_OFFSET_CODE);
    }

    UART_TestWriteString("[AD9708] SINE 100kHz AMP_CODE=100 OFFSET=128 ");
    UART_TestWriteString(AD9708_TestStatusName(status));
    UART_TestWriteString("\r\n");
    return status;
}

/* 停止高速DAC输出并回到安全中点码。 */
static void AD9708_TestStop(void)
{
    AD9708_StatusTypeDef status; /* 高速DAC停止状态。 */

    status = AD9708_AppStop();
    UART_TestWriteString("[AD9708] STOP ");
    UART_TestWriteString(AD9708_TestStatusName(status));
    UART_TestWriteString("\r\n");
}

/* 打印最近一次高速ADC采集统计和前部原始样本。 */
static void AD9280_TestPrintCapture(void)
{
    const AD9280_AppDataTypeDef *data = AD9280_AppGetData(); /* 采集统计。 */
    const uint8_t *samples = AD9280_AppGetSamples(); /* 原始采样缓存。 */
    uint32_t mean_x100; /* 放大100倍的平均原始码。 */
    uint16_t print_count; /* 本次串口输出的样本点数。 */
    uint16_t index; /* 当前输出样本索引。 */

    mean_x100 = (data->sample_count == 0U) ?
                0U :
                (data->capture.sample_sum * 100U +
                 data->sample_count / 2U) / data->sample_count;
    UART_TestWriteString("[AD9280] COUNT=");
    UART_TestWriteUint32(data->sample_count);
    UART_TestWriteString(" MIN=");
    UART_TestWriteUint32(data->capture.min_code);
    UART_TestWriteString(" MAX=");
    UART_TestWriteUint32(data->capture.max_code);
    UART_TestWriteString(" MEAN=");
    UART_TestWriteUint32(mean_x100 / 100U);
    UART_TestWriteString(".");
    if ((mean_x100 % 100U) < 10U)
    {
        UART_TestWriteString("0");
    }
    UART_TestWriteUint32(mean_x100 % 100U);
    UART_TestWriteString(" SUM=");
    UART_TestWriteUint32(data->capture.sample_sum);
    UART_TestWriteString(" OTR=");
    UART_TestWriteUint32(data->capture.overrange_count);
    UART_TestWriteString("\r\n[AD9280] FIRST_HEX=");

    print_count = data->sample_count;
    if (print_count > AD9280_TEST_PRINT_SAMPLES)
    {
        print_count = AD9280_TEST_PRINT_SAMPLES;
    }
    for (index = 0U; index < print_count; index++)
    {
        if (index != 0U)
        {
            UART_TestWriteString(",");
        }
        UART_TestWriteHex8(samples[index]);
    }
    UART_TestWriteString("\r\n");
}

/* 立即触发一次高速ADC采集并输出真实统计数据。 */
static AD9280_StatusTypeDef AD9280_TestCapture(void)
{
    AD9280_StatusTypeDef status; /* 高速ADC采集状态。 */

    status = AD9280_TestEnsureReady();
    if (status == AD9280_OK)
    {
        status = AD9280_AppCapture(
            AD9280_TEST_SAMPLES,
            AD9280_TEST_DECIMATION,
            AD9280_TEST_TIMEOUT_MS);
    }

    UART_TestWriteString("[AD9280] CAPTURE ");
    UART_TestWriteString(AD9280_TestStatusName(status));
    UART_TestWriteString("\r\n");
    if (status == AD9280_OK)
    {
        AD9280_TestPrintCapture();
    }
    return status;
}

/* 启动DAC正弦波后立即完成一次DAC到ADC闭环采集。 */
static void FPGA_TestLoopback(void)
{
    UART_TestWriteString("[LOOPBACK] CONNECT S1 TO S2 WITH BNC CABLE\r\n");
    if (AD9708_TestSine() != AD9708_OK)
    {
        UART_TestWriteString("[LOOPBACK] DAC START FAILED\r\n");
        return;
    }
    HAL_Delay(20U);
    (void)AD9280_TestCapture();
}

/* 读取ProMax当前运行状态。 */
static void ProMax_TestStatus(void)
{
    FpgaPromax_Result result; /* ProMax读状态结果。 */
    uint32_t status = 0U; /* ProMax实时状态字。 */

    result = ProMax_TestEnsureReady();
    if (result == FPGA_PROMAX_OK)
    {
        result = FpgaPromax_GetStatus(&s_promax, &status);
    }
    UART_TestWriteString("[PROMAX] STATUS ");
    UART_TestWriteString(FpgaPromax_ResultString(result));
    UART_TestWriteString(" VALUE=0x");
    UART_TestWriteHex32(status);
    UART_TestWriteString("\r\n");
}

/* 配置八路DDC和四组冲激模板，并读取一帧实时压缩结果。 */
static void ProMax_TestRealtime(void)
{
    FpgaPromax_AllResults results; /* ProMax本次完整结果。 */
    FpgaPromax_Result result; /* ProMax实时测试状态。 */
    uint8_t channel; /* 当前配置或输出的DDC通道。 */
    uint8_t bank; /* 当前配置或输出的匹配模板组。 */
    uint8_t running = 0U; /* ProMax数据面已经启动标志。 */

    result = ProMax_TestEnsureReady();
    if ((result == FPGA_PROMAX_OK) &&
        (FpgaPromax_HasRealtime(&s_promax) == 0U))
    {
        result = FPGA_PROMAX_E_UNSUPPORTED;
    }
    if (result == FPGA_PROMAX_OK)
    {
        result = FpgaPromax_SetRun(&s_promax, 0U);
    }
    for (channel = 0U;
         (channel < FPGA_PROMAX_DDC_CHANNELS) &&
         (result == FPGA_PROMAX_OK);
         channel++)
    {
        result = FpgaPromax_SetDdcFrequency(
            &s_promax,
            channel,
            (uint32_t)(channel + 1U) * PROMAX_TEST_DDC_STEP_HZ,
            PROMAX_TEST_SAMPLE_RATE_HZ);
    }
    for (bank = 0U;
         (bank < FPGA_PROMAX_MF_BANKS) &&
         (result == FPGA_PROMAX_OK);
         bank++)
    {
        result = FpgaPromax_LoadMatchedTemplate(
            &s_promax,
            bank,
            s_promax_impulse,
            FPGA_PROMAX_MF_TAPS,
            0U);
    }
    if (result == FPGA_PROMAX_OK)
    {
        result = FpgaPromax_ClearState(&s_promax);
    }
    if (result == FPGA_PROMAX_OK)
    {
        result = FpgaPromax_SetRun(&s_promax, 1U);
        running = (result == FPGA_PROMAX_OK) ? 1U : 0U;
    }
    if (result == FPGA_PROMAX_OK)
    {
        result = FpgaPromax_ReadAllResults(
            &s_promax,
            &results,
            PROMAX_TEST_TIMEOUT_MS);
    }
    if (running != 0U)
    {
        FpgaPromax_Result stop_result; /* ProMax停止状态。 */

        stop_result = FpgaPromax_SetRun(&s_promax, 0U);
        if (result == FPGA_PROMAX_OK)
        {
            result = stop_result;
        }
    }

    UART_TestWriteString("[PROMAX] REALTIME ");
    UART_TestWriteString(FpgaPromax_ResultString(result));
    UART_TestWriteString("\r\n");
    if (result != FPGA_PROMAX_OK)
    {
        return;
    }

    UART_TestWriteString("[PROMAX] POWER_GEN=");
    UART_TestWriteUint32(results.power_generation);
    UART_TestWriteString(" SCORE_GEN=");
    UART_TestWriteUint32(results.score_generation);
    UART_TestWriteString("\r\n");
    for (channel = 0U; channel < FPGA_PROMAX_DDC_CHANNELS; channel++)
    {
        UART_TestWriteString("[PROMAX] DDC");
        UART_TestWriteUint32(channel);
        UART_TestWriteString("_POWER=");
        UART_TestWriteUint64(results.band_power[channel]);
        UART_TestWriteString("\r\n");
    }
    for (bank = 0U; bank < FPGA_PROMAX_MF_BANKS; bank++)
    {
        UART_TestWriteString("[PROMAX] MF");
        UART_TestWriteUint32(bank);
        UART_TestWriteString("_SCORE=");
        UART_TestWriteInt32(results.matched_score[bank]);
        UART_TestWriteString(" INDEX=");
        UART_TestWriteUint32(results.matched_peak_index[bank]);
        UART_TestWriteString("\r\n");
    }
}

/* 打印实板测试串口命令。 */
static void UART_TestPrintHelp(void)
{
    UART_TestWriteString(
        "\r\ni=FPGA ID/FW/CAP SCAN\r\n"
        "0/1/2=AD9708 CONSTANT CODE 0/128/255\r\n"
        "s=AD9708 100kHz SINE, x=AD9708 STOP\r\n"
        "a=AD9280 IMMEDIATE CAPTURE 1024 POINTS\r\n"
        "c=DAC S1 TO ADC S2 LOOPBACK CAPTURE\r\n"
        "p=PROMAX STATUS, v=PROMAX REALTIME RESULTS\r\n"
        "h/?=HELP\r\n");
}

/* 处理UART0单字符实板测试命令。 */
static void UART_TestProcess(void)
{
    uint8_t data; /* 本次接收到的串口数据。 */

    while (DL_UART_Main_receiveDataCheck(UART_0_INST, &data))
    {
        if ((data == (uint8_t)'h') || (data == (uint8_t)'?'))
        {
            UART_TestPrintHelp();
        }
        else if (data == (uint8_t)'i')
        {
            FPGA_TestScan();
        }
        else if (data == (uint8_t)'0')
        {
            AD9708_TestConstant(0U);
        }
        else if (data == (uint8_t)'1')
        {
            AD9708_TestConstant(128U);
        }
        else if (data == (uint8_t)'2')
        {
            AD9708_TestConstant(255U);
        }
        else if (data == (uint8_t)'s')
        {
            (void)AD9708_TestSine();
        }
        else if (data == (uint8_t)'x')
        {
            AD9708_TestStop();
        }
        else if (data == (uint8_t)'a')
        {
            (void)AD9280_TestCapture();
        }
        else if (data == (uint8_t)'c')
        {
            FPGA_TestLoopback();
        }
        else if (data == (uint8_t)'p')
        {
            ProMax_TestStatus();
        }
        else if (data == (uint8_t)'v')
        {
            ProMax_TestRealtime();
        }
        else if ((data != (uint8_t)'\r') && (data != (uint8_t)'\n'))
        {
            UART_TestWriteString("[UART] UNKNOWN, USE h\r\n");
        }
    }
}

/* 运行TI板到BX71及高速AD/DA的实板测试程序。 */
int main(void)
{
    uint32_t report_loops = 0U; /* 距离下次存活提示的轮询计数。 */

    Board_TestInit();
    UART_TestWriteString("\r\nMSP-LITO-G3507 FPGA AD/DA TEST READY\r\n");
    UART_TestWriteString("UART0 PA10/PA11 115200-8-N-1\r\n");
    UART_TestWriteString("SPI0 PA12/PA14/PA13, CS PA15 ACTIVE LOW\r\n");
    UART_TestWriteString("[SPI] CS POWER-UP=");
    UART_TestWriteString(
        (DL_GPIO_readPins(FPGA_CS_REAL_PORT, GPIO_FPGA_CS_PIN) != 0U) ?
        "HIGH OK\r\n" :
        "LOW ERROR\r\n");
    UART_TestWriteString("AD9910 BACKGROUND DISABLED\r\n");
    UART_TestPrintHelp();

    while (1)
    {
        UART_TestProcess();
        NUEDC_HAL_Service();
        report_loops++;
        if (report_loops >= UART_TEST_REPORT_LOOPS)
        {
            report_loops = 0U;
            UART_TestWriteString("[ALIVE] MAIN LOOP OK, USE i FOR FPGA LINK\r\n");
        }
    }
}
