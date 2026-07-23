#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fpga_promax.h"
#include "fpga_link.h"

static uint32_t s_registers[128]; /* 模拟FPGA的七位地址寄存器空间。 */
static uint32_t s_tick_ms;        /* 模拟TI毫秒时基。 */
static uint8_t s_fail_read;       /* 强制下一次物理读取失败。 */
static uint8_t s_fail_write;      /* 强制下一次物理写入失败。 */
static uint8_t s_corrupt_after_write; /* 强制写后读回不一致。 */
static uint8_t s_corrupt_next_read;   /* 标记下一次读取需篡改。 */

/* 检查测试条件并报告失败源码行。 */
#define CHECK(condition)                                                     \
  do                                                                         \
  {                                                                          \
    if (!(condition))                                                        \
    {                                                                        \
      (void)printf("FPGA_PROMAX_TI_TEST_FAIL:%d\n", __LINE__);              \
      return 1;                                                              \
    }                                                                        \
  } while (0)

/* 提供测试使用的FPGA链路默认配置。 */
void FPGA_Link_GetDefaultConfig(FPGA_LinkConfigTypeDef *cfg)
{
  (void)memset(cfg, 0, sizeof(*cfg));
  cfg->timeout_ms = 10U;
}

/* 初始化模拟物理链路。 */
FPGA_LinkStatusTypeDef FPGA_Link_Init(FPGA_LinkHandleTypeDef *dev,
                                      const FPGA_LinkConfigTypeDef *cfg)
{
  dev->cfg = *cfg;
  dev->last_status = FPGA_LINK_OK;
  return FPGA_LINK_OK;
}

/* 模拟连续事务开始，本测试不使用该接口。 */
FPGA_LinkStatusTypeDef FPGA_Link_Begin(FPGA_LinkHandleTypeDef *dev)
{
  (void)dev;
  return FPGA_LINK_OK;
}

/* 模拟连续事务结束，本测试不使用该接口。 */
FPGA_LinkStatusTypeDef FPGA_Link_End(FPGA_LinkHandleTypeDef *dev)
{
  (void)dev;
  return FPGA_LINK_OK;
}

/* 按真实硬件有效位模拟32位寄存器写入。 */
FPGA_LinkStatusTypeDef FPGA_Link_WriteReg(FPGA_LinkHandleTypeDef *dev,
                                          uint8_t addr,
                                          uint32_t value)
{
  uint32_t coefficient;

  (void)dev;
  if (s_fail_write != 0U)
  {
    s_fail_write = 0U;
    return FPGA_LINK_ERROR_SPI;
  }
  if (addr >= 128U)
  {
    return FPGA_LINK_ERROR_PARAM;
  }

  switch (addr)
  {
    case 0x33U:
      s_registers[addr] = value & 0x00000001U;
      break;
    case 0x35U:
      s_registers[addr] =
        (s_registers[addr] & 0xFFFFFFFEU) | (value & 0x00000001U);
      break;
    case 0x48U:
      s_registers[addr] = value & 0x0000031FU;
      break;
    case 0x49U:
      coefficient = value & 0x0003FFFFU;
      s_registers[addr] = ((coefficient & 0x00020000U) != 0U) ?
                          (coefficient | 0xFFFC0000U) : coefficient;
      break;
    default:
      s_registers[addr] = value;
      break;
  }
  if (s_corrupt_after_write != 0U)
  {
    s_corrupt_after_write = 0U;
    s_corrupt_next_read = 1U;
  }
  return FPGA_LINK_OK;
}

/* 模拟32位寄存器读取及可控故障。 */
FPGA_LinkStatusTypeDef FPGA_Link_ReadReg(FPGA_LinkHandleTypeDef *dev,
                                         uint8_t addr,
                                         uint32_t *value)
{
  (void)dev;
  if (s_fail_read != 0U)
  {
    s_fail_read = 0U;
    return FPGA_LINK_ERROR_SPI;
  }
  if ((addr >= 128U) || (value == NULL))
  {
    return FPGA_LINK_ERROR_PARAM;
  }

  *value = s_registers[addr];
  if (s_corrupt_next_read != 0U)
  {
    s_corrupt_next_read = 0U;
    *value ^= 1U;
  }
  return FPGA_LINK_OK;
}

/* 返回模拟毫秒节拍。 */
uint32_t HAL_GetTick(void)
{
  return s_tick_ms;
}

/* 推进模拟毫秒节拍。 */
void HAL_Delay(uint32_t delay_ms)
{
  s_tick_ms += delay_ms;
}

/* 直接纳入生产适配层，以覆盖其私有帧校验回调。 */
#include "../Port/fpga_promax_link_ti.c"

/* 构造一帧有效的本地ProMax逻辑请求。 */
static void BuildRequest(uint8_t command,
                         uint8_t address,
                         uint32_t value,
                         uint8_t request[FPGA_PROMAX_FRAME_SIZE])
{
  request[0] = FPGA_PROMAX_REQUEST_SOF;
  request[1] = 0x5AU;
  request[2] = command;
  request[3] = address;
  request[4] = (uint8_t)(value >> 24);
  request[5] = (uint8_t)(value >> 16);
  request[6] = (uint8_t)(value >> 8);
  request[7] = (uint8_t)value;
  request[8] = FpgaPromax_Crc8(request, 8U);
}

/* 验证TI适配层映射、校验、能力边界和写后读回。 */
int main(void)
{
  FpgaPromax dev;
  FpgaPromax_AllResults results;
  int32_t coefficients[FPGA_PROMAX_MF_TAPS];
  uint8_t request[FPGA_PROMAX_FRAME_SIZE];
  uint8_t response[FPGA_PROMAX_FRAME_SIZE];
  uint64_t expected_power;
  uint32_t index;

  (void)memset(s_registers, 0, sizeof(s_registers));
  s_registers[0x30U] = FPGA_PROMAX_EXPECTED_ID;
  s_registers[0x31U] = 0x00010000U;
  s_registers[0x32U] = 0x09200408U |
                       (1UL << 25) | (1UL << 26) | (1UL << 28);
  CHECK(FPGA_ProMax_Init(&dev) == FPGA_PROMAX_OK);
  CHECK(dev.version == 0x00010000U);
  CHECK(dev.capability == 0x09200408U);
  CHECK(FpgaPromax_HasRealtime(&dev) == 1U);
  CHECK(FpgaPromax_HasFft(&dev) == 0U);
  CHECK(FpgaPromax_SetFft(&dev, 1U, 0U, 1U) ==
        FPGA_PROMAX_E_UNSUPPORTED);

  CHECK(FpgaPromax_SetDdcPhaseWord(&dev,
                                   7U,
                                   0x12345678U,
                                   0xABCDEF01U) == FPGA_PROMAX_OK);
  CHECK(s_registers[0x3FU] == 0x12345678U);
  CHECK(s_registers[0x47U] == 0xABCDEF01U);
  CHECK(FpgaPromax_SetDdcPhaseWord(&dev, 8U, 0U, 0U) ==
        FPGA_PROMAX_E_RANGE);
  CHECK(FpgaPromax_SetDdcFrequency(&dev,
                                   0U,
                                   1000000U,
                                   32000000U) == FPGA_PROMAX_OK);
  CHECK(s_registers[0x38U] == 0x08000000U);

  s_registers[0x35U] = 0x005A3C00U;
  CHECK(FpgaPromax_BeginResultSnapshot(&dev) == FPGA_PROMAX_OK);
  CHECK(s_registers[0x35U] == 0x005A3C01U);
  CHECK(FpgaPromax_EndResultSnapshot(&dev) == FPGA_PROMAX_OK);
  CHECK(s_registers[0x35U] == 0x005A3C00U);

  for (index = 0U; index < FPGA_PROMAX_MF_TAPS; index++)
  {
    coefficients[index] = (index == (FPGA_PROMAX_MF_TAPS - 1U)) ?
                          -131072 : (int32_t)index;
  }
  CHECK(FpgaPromax_LoadMatchedTemplate(&dev,
                                       2U,
                                       coefficients,
                                       FPGA_PROMAX_MF_TAPS,
                                       0U) == FPGA_PROMAX_OK);
  CHECK(s_registers[0x48U] == 0x0000021FU);
  CHECK((s_registers[0x49U] & 0x0003FFFFU) == 0x00020000U);

  CHECK(FpgaPromax_SetRun(&dev, 1U) == FPGA_PROMAX_OK);
  CHECK(FpgaPromax_ClearState(&dev) == FPGA_PROMAX_OK);
  CHECK(s_registers[0x33U] == 1U);

  s_registers[0x34U] = FPGA_PROMAX_STATUS_RUN |
                       FPGA_PROMAX_STATUS_ALL_RESULTS;
  s_registers[0x35U] = 0x003C5A00U;
  for (index = 0U; index < FPGA_PROMAX_DDC_CHANNELS; index++)
  {
    s_registers[0x50U + index] = 0x10203040U + index;
    s_registers[0x58U + index] = index + 1U;
  }
  for (index = 0U; index < FPGA_PROMAX_MF_BANKS; index++)
  {
    s_registers[0x60U + index] = (index == 0U) ?
                                 0x00FFFFFEU : index;
    s_registers[0x64U + index] = 100U + index;
  }
  CHECK(FpgaPromax_ReadAllResults(&dev, &results, 10U) == FPGA_PROMAX_OK);
  CHECK(results.power_generation == 0x5AU);
  CHECK(results.score_generation == 0x3CU);
  for (index = 0U; index < FPGA_PROMAX_DDC_CHANNELS; index++)
  {
    expected_power = ((uint64_t)(index + 1U) << 32) |
                     (uint64_t)(0x10203040U + index);
    CHECK(results.band_power[index] == expected_power);
  }
  CHECK(results.matched_score[0] == -2);
  CHECK(results.matched_peak_index[3] == 103U);
  CHECK((s_registers[0x35U] & 1U) == 0U);

  s_registers[0x34U] = 0U;
  s_tick_ms = 0U;
  CHECK(FpgaPromax_WaitResults(&dev,
                               FPGA_PROMAX_STATUS_POWER_VALID,
                               2U) == FPGA_PROMAX_E_TIMEOUT);

  BuildRequest(FPGA_PROMAX_CMD_READ,
               FPGA_PROMAX_REG_CAPABILITY,
               0U,
               request);
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) == 0);
  CHECK(response[0] == FPGA_PROMAX_RESPONSE_SOF);
  CHECK(response[1] == request[1]);
  CHECK(response[2] == FPGA_PROMAX_STATUS_OK);
  CHECK(response[3] == FPGA_PROMAX_REG_CAPABILITY);
  CHECK(FpgaPromax_Crc8(response, 8U) == response[8]);

  request[8] ^= 1U;
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) == 0);
  CHECK(response[2] == FPGA_PROMAX_STATUS_CRC);
  CHECK(FpgaPromax_Crc8(response, 8U) == response[8]);

  BuildRequest(0x7FU, FPGA_PROMAX_REG_ID, 0U, request);
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) == 0);
  CHECK(response[2] == FPGA_PROMAX_STATUS_BAD_CMD);

  BuildRequest(FPGA_PROMAX_CMD_READ, 0x06U, 0U, request);
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) == 0);
  CHECK(response[2] == FPGA_PROMAX_STATUS_BAD_ADDR);

  BuildRequest(FPGA_PROMAX_CMD_WRITE, FPGA_PROMAX_REG_ID, 0U, request);
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) == 0);
  CHECK(response[2] == FPGA_PROMAX_STATUS_READ_ONLY);

  BuildRequest(FPGA_PROMAX_CMD_WRITE,
               FPGA_PROMAX_REG_PHASE_INC0,
               0x13579BDFU,
               request);
  s_corrupt_after_write = 1U;
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) < 0);

  s_fail_read = 1U;
  BuildRequest(FPGA_PROMAX_CMD_READ, FPGA_PROMAX_REG_ID, 0U, request);
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) < 0);

  s_fail_write = 1U;
  BuildRequest(FPGA_PROMAX_CMD_WRITE,
               FPGA_PROMAX_REG_CONTROL,
               1U,
               request);
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) < 0);

  s_promax_link_busy = 1U;
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) == 0);
  CHECK(response[2] == FPGA_PROMAX_STATUS_BUSY);
  s_promax_link_busy = 0U;

  request[0] = 0U;
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 10U) < 0);
  CHECK(FPGA_ProMaxTiTransport(NULL, request, response, 0U) < 0);

  (void)printf("FPGA_PROMAX_TI_HOST_TEST_PASS\n");
  return 0;
}
