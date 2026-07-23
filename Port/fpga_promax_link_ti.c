#include "fpga_promax_link.h"
#include "fpga_promax_link_map.h"
#include "fpga_link.h"
#include "stm32h7xx_hal.h"

#define FPGA_PROMAX_TI_LINK_ERROR_PARAM   (-1) /* TI适配层参数或请求帧错误。 */
#define FPGA_PROMAX_TI_LINK_ERROR_IO      (-2) /* 六字节物理链路传输失败。 */
#define FPGA_PROMAX_TI_CAPABILITY_MASK    \
  ((1UL << 25) | (1UL << 26) | (1UL << 28)) /* TI链路未实现UART、线CRC和FFT。 */

static FPGA_LinkHandleTypeDef s_promax_link; /* ProMax复用的TI到FPGA六字节链路。 */
static uint8_t s_promax_link_ready;           /* ProMax物理链路初始化标志。 */
static uint8_t s_promax_link_busy;            /* 防止ProMax传输回调重入。 */

/* 从大端字节序读取一个32位值。 */
static uint32_t FPGA_ProMaxLoadBe32(const uint8_t *src)
{
  return ((uint32_t)src[0] << 24) |
         ((uint32_t)src[1] << 16) |
         ((uint32_t)src[2] << 8) |
         (uint32_t)src[3];
}

/* 按大端字节序保存一个32位值。 */
static void FPGA_ProMaxStoreBe32(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value >> 24);
  dst[1] = (uint8_t)(value >> 16);
  dst[2] = (uint8_t)(value >> 8);
  dst[3] = (uint8_t)value;
}

/* 构造通用核心要求的九字节本地响应。 */
static void FPGA_ProMaxBuildResponse(
  const uint8_t request[FPGA_PROMAX_FRAME_SIZE],
  uint8_t status,
  uint32_t value,
  uint8_t response[FPGA_PROMAX_FRAME_SIZE])
{
  response[0] = FPGA_PROMAX_RESPONSE_SOF;
  response[1] = request[1];
  response[2] = status;
  response[3] = request[3];
  FPGA_ProMaxStoreBe32(&response[4], value);
  response[8] = FpgaPromax_Crc8(response, FPGA_PROMAX_FRAME_SIZE - 1U);
}

/* 把一个逻辑读写请求转换为现有六字节FPGA寄存器事务。 */
static int32_t FPGA_ProMaxAccessRegister(uint8_t command,
                                        uint8_t logical_address,
                                        uint32_t write_value,
                                        uint32_t *read_value,
                                        uint8_t *protocol_status)
{
  FPGA_LinkStatusTypeDef link_status;
  uint32_t verify_value;
  uint8_t physical_address;

  if ((read_value == NULL) || (protocol_status == NULL))
  {
    return FPGA_PROMAX_TI_LINK_ERROR_PARAM;
  }

  *read_value = 0U;
  if (FPGA_ProMaxMapAddress(logical_address, &physical_address) == 0U)
  {
    *protocol_status = FPGA_PROMAX_STATUS_BAD_ADDR;
    return 0;
  }

  if (command == FPGA_PROMAX_CMD_WRITE)
  {
    if (FPGA_ProMaxIsWritable(logical_address) == 0U)
    {
      *protocol_status = FPGA_PROMAX_STATUS_READ_ONLY;
      return 0;
    }
    link_status = FPGA_Link_WriteReg(&s_promax_link,
                                    physical_address,
                                    write_value);
    if (link_status == FPGA_LINK_OK)
    {
      link_status = FPGA_Link_ReadReg(&s_promax_link,
                                     physical_address,
                                     &verify_value);
      if (link_status != FPGA_LINK_OK)
      {
        return FPGA_PROMAX_TI_LINK_ERROR_IO;
      }
      if (FPGA_ProMaxVerifyWrite(logical_address,
                                 write_value,
                                 verify_value) == 0U)
      {
        return FPGA_PROMAX_TI_LINK_ERROR_IO;
      }
    }
  }
  else
  {
    link_status = FPGA_Link_ReadReg(&s_promax_link,
                                   physical_address,
                                   read_value);
  }

  if (link_status == FPGA_LINK_ERROR_BUSY)
  {
    *protocol_status = FPGA_PROMAX_STATUS_BUSY;
    return 0;
  }
  if (link_status != FPGA_LINK_OK)
  {
    return FPGA_PROMAX_TI_LINK_ERROR_IO;
  }

  if ((command == FPGA_PROMAX_CMD_READ) &&
      (logical_address == FPGA_PROMAX_REG_CAPABILITY))
  {
    *read_value &= ~FPGA_PROMAX_TI_CAPABILITY_MASK;
  }
  *protocol_status = FPGA_PROMAX_STATUS_OK;
  return 0;
}

/* 在MSPM0上完成一次ProMax逻辑请求和本地响应。 */
static int32_t FPGA_ProMaxTiTransport(
  void *user,
  const uint8_t request[FPGA_PROMAX_FRAME_SIZE],
  uint8_t response[FPGA_PROMAX_FRAME_SIZE],
  uint32_t timeout_ms)
{
  uint32_t value;
  uint8_t status;
  int32_t transport_status;

  (void)user;
  if ((request == NULL) || (response == NULL) || (timeout_ms == 0U) ||
      (s_promax_link_ready == 0U) ||
      (request[0] != FPGA_PROMAX_REQUEST_SOF))
  {
    return FPGA_PROMAX_TI_LINK_ERROR_PARAM;
  }

  if (s_promax_link_busy != 0U)
  {
    FPGA_ProMaxBuildResponse(request, FPGA_PROMAX_STATUS_BUSY, 0U, response);
    return 0;
  }

  s_promax_link_busy = 1U;
  value = 0U;
  status = FPGA_PROMAX_STATUS_OK;
  transport_status = 0;

  if (FpgaPromax_Crc8(request, FPGA_PROMAX_FRAME_SIZE - 1U) != request[8])
  {
    status = FPGA_PROMAX_STATUS_CRC;
  }
  else if (request[2] == FPGA_PROMAX_CMD_PING)
  {
    transport_status = FPGA_ProMaxAccessRegister(
      FPGA_PROMAX_CMD_READ,
      FPGA_PROMAX_REG_ID,
      0U,
      &value,
      &status);
  }
  else if ((request[2] == FPGA_PROMAX_CMD_READ) ||
           (request[2] == FPGA_PROMAX_CMD_WRITE))
  {
    transport_status = FPGA_ProMaxAccessRegister(
      request[2],
      request[3],
      FPGA_ProMaxLoadBe32(&request[4]),
      &value,
      &status);
  }
  else
  {
    status = FPGA_PROMAX_STATUS_BAD_CMD;
  }

  if (transport_status == 0)
  {
    FPGA_ProMaxBuildResponse(request, status, value, response);
  }
  s_promax_link_busy = 0U;
  return transport_status;
}

/* 返回TI工程的单调递增毫秒节拍。 */
static uint32_t FPGA_ProMaxTiTimeMs(void *user)
{
  (void)user;
  return HAL_GetTick();
}

/* 为通用等待接口提供TI毫秒延时。 */
static void FPGA_ProMaxTiDelayMs(void *user, uint32_t delay_ms)
{
  (void)user;
  HAL_Delay(delay_ms);
}

/* 初始化TI板的ProMax控制接口，但不自动启动FPGA数据面。 */
FpgaPromax_Result FPGA_ProMax_Init(FpgaPromax *dev)
{
  FPGA_LinkConfigTypeDef config;
  FpgaPromax_Result result;

  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }

  FPGA_Link_GetDefaultConfig(&config);
  if (FPGA_Link_Init(&s_promax_link, &config) != FPGA_LINK_OK)
  {
    s_promax_link_ready = 0U;
    return FPGA_PROMAX_E_TRANSPORT;
  }

  s_promax_link_ready = 1U;
  s_promax_link_busy = 0U;
  result = FpgaPromax_Init(dev,
                          FPGA_ProMaxTiTransport,
                          NULL,
                          config.timeout_ms);
  if (result == FPGA_PROMAX_OK)
  {
    FpgaPromax_SetTimebase(dev,
                           FPGA_ProMaxTiTimeMs,
                           FPGA_ProMaxTiDelayMs,
                           NULL);
  }
  return result;
}
