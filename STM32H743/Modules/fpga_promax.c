#include "fpga_promax.h"
#include <string.h>

/* 将32位整数按大端序写入字节数组。 */
static void FpgaPromax_StoreBe32(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value >> 24);
  dst[1] = (uint8_t)(value >> 16);
  dst[2] = (uint8_t)(value >> 8);
  dst[3] = (uint8_t)value;
}

/* 从大端字节数组读取32位整数。 */
static uint32_t FpgaPromax_LoadBe32(const uint8_t *src)
{
  return ((uint32_t)src[0] << 24) |
         ((uint32_t)src[1] << 16) |
         ((uint32_t)src[2] << 8) |
         (uint32_t)src[3];
}

/* 计算CRC-8/ATM，poly=0x07、init=0、不反射且无最终异或。 */
uint8_t FpgaPromax_Crc8(const uint8_t *data, size_t length)
{
  uint8_t crc;
  uint8_t bit;
  size_t index;

  if (data == NULL)
  {
    return 0U;
  }

  crc = 0U;
  for (index = 0U; index < length; index++)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; bit++)
    {
      crc = ((crc & 0x80U) != 0U) ?
            (uint8_t)(((uint32_t)crc << 1) ^ 0x07U) :
            (uint8_t)((uint32_t)crc << 1);
    }
  }
  return crc;
}

/* 组装逻辑请求，调用传输层并完整校验响应。 */
static FpgaPromax_Result FpgaPromax_Transact(FpgaPromax *dev,
                                             uint8_t command,
                                             uint8_t address,
                                             uint32_t tx_data,
                                             uint32_t *rx_data)
{
  uint8_t request[FPGA_PROMAX_FRAME_SIZE];
  uint8_t response[FPGA_PROMAX_FRAME_SIZE];
  uint8_t sequence;

  if ((dev == NULL) || (dev->transport == NULL))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }

  sequence = dev->next_sequence++;
  request[0] = FPGA_PROMAX_REQUEST_SOF;
  request[1] = sequence;
  request[2] = command;
  request[3] = address;
  FpgaPromax_StoreBe32(&request[4], tx_data);
  request[8] = FpgaPromax_Crc8(request, 8U);
  memset(response, 0, sizeof(response));

  dev->last_protocol_status = 0xFFU;
  dev->last_transport_status = dev->transport(dev->transport_user,
                                               request,
                                               response,
                                               dev->timeout_ms);
  if (dev->last_transport_status != 0)
  {
    return FPGA_PROMAX_E_TRANSPORT;
  }
  if (response[0] != FPGA_PROMAX_RESPONSE_SOF)
  {
    return FPGA_PROMAX_E_BAD_FRAME;
  }
  if (FpgaPromax_Crc8(response, 8U) != response[8])
  {
    return FPGA_PROMAX_E_CRC;
  }
  if (response[1] != sequence)
  {
    return FPGA_PROMAX_E_SEQUENCE;
  }

  dev->last_protocol_status = response[2];
  if (response[2] != FPGA_PROMAX_STATUS_OK)
  {
    return FPGA_PROMAX_E_FPGA_STATUS;
  }
  if (response[3] != address)
  {
    return FPGA_PROMAX_E_BAD_FRAME;
  }
  if (rx_data != NULL)
  {
    *rx_data = FpgaPromax_LoadBe32(&response[4]);
  }
  return FPGA_PROMAX_OK;
}

/* 设置结果轮询使用的时基，不改变已初始化的传输配置。 */
void FpgaPromax_SetTimebase(FpgaPromax *dev,
                            FpgaPromax_TimeMsFn time_ms,
                            FpgaPromax_DelayMsFn delay_ms,
                            void *time_user)
{
  if (dev != NULL)
  {
    dev->time_ms = time_ms;
    dev->delay_ms = delay_ms;
    dev->time_user = time_user;
  }
}

/* 初始化设备并校验ID、版本和能力寄存器。 */
FpgaPromax_Result FpgaPromax_Init(FpgaPromax *dev,
                                  FpgaPromax_TransportFn transport,
                                  void *transport_user,
                                  uint32_t timeout_ms)
{
  FpgaPromax_Result result;
  uint32_t device_id;

  if ((dev == NULL) || (transport == NULL) || (timeout_ms == 0U))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }

  memset(dev, 0, sizeof(*dev));
  dev->transport = transport;
  dev->transport_user = transport_user;
  dev->timeout_ms = timeout_ms;
  dev->next_sequence = 1U;
  dev->last_protocol_status = 0xFFU;

  device_id = 0U;
  result = FpgaPromax_Ping(dev, &device_id);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }
  if (device_id != FPGA_PROMAX_EXPECTED_ID)
  {
    return FPGA_PROMAX_E_WRONG_DEVICE;
  }

  result = FpgaPromax_ReadRegister(dev,
                                   FPGA_PROMAX_REG_VERSION,
                                   &dev->version);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }
  return FpgaPromax_ReadRegister(dev,
                                FPGA_PROMAX_REG_CAPABILITY,
                                &dev->capability);
}

/* 查询ProMax设备标识。 */
FpgaPromax_Result FpgaPromax_Ping(FpgaPromax *dev, uint32_t *device_id)
{
  if (device_id == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  return FpgaPromax_Transact(dev,
                            FPGA_PROMAX_CMD_PING,
                            FPGA_PROMAX_REG_ID,
                            0U,
                            device_id);
}

/* 读取一个32位逻辑寄存器。 */
FpgaPromax_Result FpgaPromax_ReadRegister(FpgaPromax *dev,
                                         uint8_t address,
                                         uint32_t *value)
{
  if (value == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  return FpgaPromax_Transact(dev,
                            FPGA_PROMAX_CMD_READ,
                            address,
                            0U,
                            value);
}

/* 写入一个32位逻辑寄存器。 */
FpgaPromax_Result FpgaPromax_WriteRegister(FpgaPromax *dev,
                                          uint8_t address,
                                          uint32_t value)
{
  return FpgaPromax_Transact(dev,
                            FPGA_PROMAX_CMD_WRITE,
                            address,
                            value,
                            NULL);
}

/* 启停实时数据面。 */
FpgaPromax_Result FpgaPromax_SetRun(FpgaPromax *dev, uint8_t enable)
{
  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasRealtime(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }
  return FpgaPromax_WriteRegister(dev,
                                 FPGA_PROMAX_REG_CONTROL,
                                 (enable != 0U) ? 1U : 0U);
}

/* 同步清除运行状态，并保留当前run位和已装载模板。 */
FpgaPromax_Result FpgaPromax_ClearState(FpgaPromax *dev)
{
  FpgaPromax_Result result;
  uint32_t control;

  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  result = FpgaPromax_ReadRegister(dev,
                                   FPGA_PROMAX_REG_CONTROL,
                                   &control);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }
  result = FpgaPromax_WriteRegister(dev,
                                    FPGA_PROMAX_REG_CONTROL,
                                    (control & 1U) | 2U);
  if (result == FPGA_PROMAX_OK)
  {
    dev->snapshot_held = 0U;
  }
  return result;
}

/* 读取实时数据面状态。 */
FpgaPromax_Result FpgaPromax_GetStatus(FpgaPromax *dev, uint32_t *status)
{
  return FpgaPromax_ReadRegister(dev, FPGA_PROMAX_REG_STATUS, status);
}

/* 有界轮询指定的实时结果有效位。 */
FpgaPromax_Result FpgaPromax_WaitResults(FpgaPromax *dev,
                                        uint32_t required_status_mask,
                                        uint32_t timeout_ms)
{
  FpgaPromax_Result result;
  uint32_t fallback_polls;
  uint32_t start_ms;
  uint32_t status;

  if ((dev == NULL) || (timeout_ms == 0U) ||
      ((required_status_mask & ~FPGA_PROMAX_STATUS_ALL_RESULTS) != 0U) ||
      (required_status_mask == 0U))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }

  start_ms = (dev->time_ms != NULL) ? dev->time_ms(dev->time_user) : 0U;
  fallback_polls = timeout_ms;
  for (;;)
  {
    result = FpgaPromax_GetStatus(dev, &status);
    if (result != FPGA_PROMAX_OK)
    {
      return result;
    }
    if ((status & required_status_mask) == required_status_mask)
    {
      return FPGA_PROMAX_OK;
    }

    if (dev->time_ms != NULL)
    {
      if ((dev->time_ms(dev->time_user) - start_ms) >= timeout_ms)
      {
        return FPGA_PROMAX_E_TIMEOUT;
      }
      if (dev->delay_ms != NULL)
      {
        dev->delay_ms(dev->time_user, 1U);
      }
    }
    else
    {
      fallback_polls--;
      if (fallback_polls == 0U)
      {
        return FPGA_PROMAX_E_TIMEOUT;
      }
    }
  }
}

/* 冻结FPGA结果快照。 */
FpgaPromax_Result FpgaPromax_BeginResultSnapshot(FpgaPromax *dev)
{
  FpgaPromax_Result result;

  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  result = FpgaPromax_WriteRegister(dev, FPGA_PROMAX_REG_SNAPSHOT, 1U);
  if (result == FPGA_PROMAX_OK)
  {
    dev->snapshot_held = 1U;
  }
  return result;
}

/* 解冻FPGA结果快照。 */
FpgaPromax_Result FpgaPromax_EndResultSnapshot(FpgaPromax *dev)
{
  FpgaPromax_Result result;

  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  result = FpgaPromax_WriteRegister(dev, FPGA_PROMAX_REG_SNAPSHOT, 0U);
  if (result == FPGA_PROMAX_OK)
  {
    dev->snapshot_held = 0U;
  }
  return result;
}

/* 读取结果代次和实际冻结状态。 */
FpgaPromax_Result FpgaPromax_GetSnapshotInfo(FpgaPromax *dev,
                                            uint8_t *power_generation,
                                            uint8_t *score_generation,
                                            uint8_t *is_held)
{
  FpgaPromax_Result result;
  uint32_t raw;

  if ((dev == NULL) || (power_generation == NULL) ||
      (score_generation == NULL) || (is_held == NULL))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  result = FpgaPromax_ReadRegister(dev, FPGA_PROMAX_REG_SNAPSHOT, &raw);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }

  *is_held = (uint8_t)(raw & 1U);
  *power_generation = (uint8_t)(raw >> 8);
  *score_generation = (uint8_t)(raw >> 16);
  dev->snapshot_held = *is_held;
  return FPGA_PROMAX_OK;
}

/* 设置一路DDC的相位字。 */
FpgaPromax_Result FpgaPromax_SetDdcPhaseWord(FpgaPromax *dev,
                                            uint8_t channel,
                                            uint32_t phase_increment,
                                            uint32_t phase_offset)
{
  FpgaPromax_Result result;

  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasRealtime(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }
  if (channel >= FPGA_PROMAX_DDC_CHANNELS)
  {
    return FPGA_PROMAX_E_RANGE;
  }

  result = FpgaPromax_WriteRegister(
    dev,
    (uint8_t)(FPGA_PROMAX_REG_PHASE_INC0 + channel),
    phase_increment);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }
  return FpgaPromax_WriteRegister(
    dev,
    (uint8_t)(FPGA_PROMAX_REG_PHASE_OFFSET0 + channel),
    phase_offset);
}

/* 根据有效采样率计算并写入32位NCO相位步进字。 */
FpgaPromax_Result FpgaPromax_SetDdcFrequency(FpgaPromax *dev,
                                            uint8_t channel,
                                            uint32_t frequency_hz,
                                            uint32_t sample_rate_hz)
{
  uint64_t numerator;
  uint32_t phase_increment;

  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if ((sample_rate_hz == 0U) || (frequency_hz > (sample_rate_hz / 2U)))
  {
    return FPGA_PROMAX_E_RANGE;
  }

  numerator = ((uint64_t)frequency_hz << 32) + (sample_rate_hz / 2U);
  phase_increment = (uint32_t)(numerator / sample_rate_hz);
  return FpgaPromax_SetDdcPhaseWord(dev,
                                   channel,
                                   phase_increment,
                                   0U);
}

/* 装载并补零一组匹配模板，首次硬件写入前完成全部范围检查。 */
FpgaPromax_Result FpgaPromax_LoadMatchedTemplate(FpgaPromax *dev,
                                                uint8_t bank,
                                                const int32_t *coeff_q16,
                                                size_t count,
                                                uint8_t reverse)
{
  FpgaPromax_Result result;
  size_t tap;

  if ((dev == NULL) || (coeff_q16 == NULL) ||
      (bank >= FPGA_PROMAX_MF_BANKS) ||
      (count == 0U) || (count > FPGA_PROMAX_MF_TAPS))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasRealtime(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }

  for (tap = 0U; tap < count; tap++)
  {
    if ((coeff_q16[tap] < -131072) || (coeff_q16[tap] > 131071))
    {
      return FPGA_PROMAX_E_RANGE;
    }
  }

  for (tap = 0U; tap < FPGA_PROMAX_MF_TAPS; tap++)
  {
    int32_t coefficient;
    uint32_t selector;

    coefficient = 0;
    selector = ((uint32_t)bank << 8) | (uint32_t)tap;
    if (tap < count)
    {
      size_t source_index;

      source_index = (reverse != 0U) ? (count - 1U - tap) : tap;
      coefficient = coeff_q16[source_index];
    }

    result = FpgaPromax_WriteRegister(dev,
                                      FPGA_PROMAX_REG_MF_SELECTOR,
                                      selector);
    if (result != FPGA_PROMAX_OK)
    {
      return result;
    }
    result = FpgaPromax_WriteRegister(dev,
                                      FPGA_PROMAX_REG_MF_COEFF,
                                      (uint32_t)coefficient & 0x3FFFFU);
    if (result != FPGA_PROMAX_OK)
    {
      return result;
    }
  }
  return FPGA_PROMAX_OK;
}

/* 解冻快照，并在清理成功时保留更早的传输和协议错误。 */
static FpgaPromax_Result FpgaPromax_EndSnapshotPreserveFailure(
  FpgaPromax *dev,
  FpgaPromax_Result primary_result)
{
  FpgaPromax_Result end_result;
  int32_t primary_transport;
  uint8_t primary_protocol;

  primary_transport = dev->last_transport_status;
  primary_protocol = dev->last_protocol_status;
  end_result = FpgaPromax_EndResultSnapshot(dev);
  if (primary_result != FPGA_PROMAX_OK)
  {
    dev->last_transport_status = primary_transport;
    dev->last_protocol_status = primary_protocol;
    return primary_result;
  }
  return end_result;
}

/* 读取一路37位频带功率，并自动管理局部快照。 */
FpgaPromax_Result FpgaPromax_GetBandPower(FpgaPromax *dev,
                                         uint8_t channel,
                                         uint64_t *power)
{
  FpgaPromax_Result result;
  uint32_t high;
  uint32_t low;
  uint8_t opened_snapshot;

  if ((dev == NULL) || (power == NULL) ||
      (channel >= FPGA_PROMAX_DDC_CHANNELS))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasRealtime(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }

  result = FpgaPromax_WaitResults(dev,
                                  FPGA_PROMAX_STATUS_POWER_VALID,
                                  dev->timeout_ms);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }

  opened_snapshot = (dev->snapshot_held == 0U) ? 1U : 0U;
  if (opened_snapshot != 0U)
  {
    result = FpgaPromax_BeginResultSnapshot(dev);
    if (result != FPGA_PROMAX_OK)
    {
      return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
    }
  }

  result = FpgaPromax_ReadRegister(
    dev,
    (uint8_t)(FPGA_PROMAX_REG_POWER_LO0 + channel),
    &low);
  if (result == FPGA_PROMAX_OK)
  {
    result = FpgaPromax_ReadRegister(
      dev,
      (uint8_t)(FPGA_PROMAX_REG_POWER_HI0 + channel),
      &high);
  }
  if (result == FPGA_PROMAX_OK)
  {
    *power = ((uint64_t)(high & 0x1FU) << 32) | low;
  }

  if (opened_snapshot != 0U)
  {
    return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
  }
  return result;
}

/* 读取一组24位有符号匹配峰值，并自动管理局部快照。 */
FpgaPromax_Result FpgaPromax_GetMatchedScore(FpgaPromax *dev,
                                            uint8_t bank,
                                            int32_t *score)
{
  FpgaPromax_Result result;
  uint32_t raw;
  uint8_t opened_snapshot;

  if ((dev == NULL) || (score == NULL) ||
      (bank >= FPGA_PROMAX_MF_BANKS))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasRealtime(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }

  result = FpgaPromax_WaitResults(dev,
                                  FPGA_PROMAX_STATUS_SCORE_VALID,
                                  dev->timeout_ms);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }

  opened_snapshot = (dev->snapshot_held == 0U) ? 1U : 0U;
  if (opened_snapshot != 0U)
  {
    result = FpgaPromax_BeginResultSnapshot(dev);
    if (result != FPGA_PROMAX_OK)
    {
      return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
    }
  }

  result = FpgaPromax_ReadRegister(
    dev,
    (uint8_t)(FPGA_PROMAX_REG_MF_SCORE0 + bank),
    &raw);
  if (result == FPGA_PROMAX_OK)
  {
    *score = ((raw & 0x00800000U) != 0U) ?
             (int32_t)(raw | 0xFF000000U) :
             (int32_t)(raw & 0x00FFFFFFU);
  }

  if (opened_snapshot != 0U)
  {
    return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
  }
  return result;
}

/* 读取一组有符号匹配峰值及其有效样本位置。 */
FpgaPromax_Result FpgaPromax_GetMatchedPeak(FpgaPromax *dev,
                                           uint8_t bank,
                                           int32_t *score,
                                           uint32_t *sample_index)
{
  FpgaPromax_Result result;
  uint32_t index_raw;
  uint32_t raw;
  uint8_t opened_snapshot;

  if ((dev == NULL) || (score == NULL) || (sample_index == NULL) ||
      (bank >= FPGA_PROMAX_MF_BANKS))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasRealtime(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }

  result = FpgaPromax_WaitResults(dev,
                                  FPGA_PROMAX_STATUS_SCORE_VALID,
                                  dev->timeout_ms);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }

  opened_snapshot = (dev->snapshot_held == 0U) ? 1U : 0U;
  if (opened_snapshot != 0U)
  {
    result = FpgaPromax_BeginResultSnapshot(dev);
    if (result != FPGA_PROMAX_OK)
    {
      return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
    }
  }

  result = FpgaPromax_ReadRegister(
    dev,
    (uint8_t)(FPGA_PROMAX_REG_MF_SCORE0 + bank),
    &raw);
  if (result == FPGA_PROMAX_OK)
  {
    result = FpgaPromax_ReadRegister(
      dev,
      (uint8_t)(FPGA_PROMAX_REG_MF_PEAK_INDEX0 + bank),
      &index_raw);
  }
  if (result == FPGA_PROMAX_OK)
  {
    *score = ((raw & 0x00800000U) != 0U) ?
             (int32_t)(raw | 0xFF000000U) :
             (int32_t)(raw & 0x00FFFFFFU);
    *sample_index = index_raw;
  }

  if (opened_snapshot != 0U)
  {
    return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
  }
  return result;
}

/* 在一个冻结窗口内读取全部实时压缩结果。 */
FpgaPromax_Result FpgaPromax_ReadAllResults(FpgaPromax *dev,
                                           FpgaPromax_AllResults *results,
                                           uint32_t timeout_ms)
{
  FpgaPromax_Result result;
  uint32_t high;
  uint32_t low;
  uint32_t raw;
  uint32_t snapshot;
  uint8_t bank;
  uint8_t channel;
  uint8_t opened_snapshot;

  if ((dev == NULL) || (results == NULL) || (timeout_ms == 0U))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasRealtime(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }

  result = FpgaPromax_WaitResults(dev,
                                  FPGA_PROMAX_STATUS_ALL_RESULTS,
                                  timeout_ms);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }

  opened_snapshot = (dev->snapshot_held == 0U) ? 1U : 0U;
  if (opened_snapshot != 0U)
  {
    result = FpgaPromax_BeginResultSnapshot(dev);
    if (result != FPGA_PROMAX_OK)
    {
      return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
    }
  }

  result = FpgaPromax_ReadRegister(dev,
                                   FPGA_PROMAX_REG_SNAPSHOT,
                                   &snapshot);
  if (result == FPGA_PROMAX_OK)
  {
    results->power_generation = (uint8_t)(snapshot >> 8);
    results->score_generation = (uint8_t)(snapshot >> 16);
  }

  for (channel = 0U;
       (channel < FPGA_PROMAX_DDC_CHANNELS) &&
       (result == FPGA_PROMAX_OK);
       channel++)
  {
    result = FpgaPromax_ReadRegister(
      dev,
      (uint8_t)(FPGA_PROMAX_REG_POWER_LO0 + channel),
      &low);
    if (result == FPGA_PROMAX_OK)
    {
      result = FpgaPromax_ReadRegister(
        dev,
        (uint8_t)(FPGA_PROMAX_REG_POWER_HI0 + channel),
        &high);
    }
    if (result == FPGA_PROMAX_OK)
    {
      results->band_power[channel] =
        ((uint64_t)(high & 0x1FU) << 32) | low;
    }
  }

  for (bank = 0U;
       (bank < FPGA_PROMAX_MF_BANKS) &&
       (result == FPGA_PROMAX_OK);
       bank++)
  {
    result = FpgaPromax_ReadRegister(
      dev,
      (uint8_t)(FPGA_PROMAX_REG_MF_SCORE0 + bank),
      &raw);
    if (result == FPGA_PROMAX_OK)
    {
      results->matched_score[bank] =
        ((raw & 0x00800000U) != 0U) ?
        (int32_t)(raw | 0xFF000000U) :
        (int32_t)(raw & 0x00FFFFFFU);
      result = FpgaPromax_ReadRegister(
        dev,
        (uint8_t)(FPGA_PROMAX_REG_MF_PEAK_INDEX0 + bank),
        &results->matched_peak_index[bank]);
    }
  }

  if (opened_snapshot != 0U)
  {
    return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
  }
  return result;
}

/* 判断当前FPGA是否包含实时并行数据面。 */
uint8_t FpgaPromax_HasRealtime(const FpgaPromax *dev)
{
  return ((dev != NULL) &&
          ((dev->capability & FPGA_PROMAX_CAPABILITY_REALTIME) != 0U)) ?
         1U : 0U;
}

/* 判断当前FPGA是否包含流式FFT数据面。 */
uint8_t FpgaPromax_HasFft(const FpgaPromax *dev)
{
  return ((dev != NULL) &&
          ((dev->capability & FPGA_PROMAX_CAPABILITY_FFT) != 0U)) ?
         1U : 0U;
}

/* 配置FFT启停、方向和流水线重启。 */
FpgaPromax_Result FpgaPromax_SetFft(FpgaPromax *dev,
                                   uint8_t enable,
                                   uint8_t inverse,
                                   uint8_t restart)
{
  uint32_t control;

  if (dev == NULL)
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasFft(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }

  control = ((enable != 0U) ? FPGA_PROMAX_FFT_CONTROL_ENABLE : 0U) |
            ((inverse != 0U) ? FPGA_PROMAX_FFT_CONTROL_INVERSE : 0U) |
            ((restart != 0U) ? FPGA_PROMAX_FFT_CONTROL_RESTART : 0U);
  return FpgaPromax_WriteRegister(dev,
                                 FPGA_PROMAX_REG_FFT_CONTROL,
                                 control);
}

/* 有界轮询FFT峰值有效位，并优先报告FFT粘滞错误。 */
FpgaPromax_Result FpgaPromax_WaitFftPeak(FpgaPromax *dev,
                                        uint32_t timeout_ms)
{
  FpgaPromax_Result result;
  uint32_t fallback_polls;
  uint32_t start_ms;
  uint32_t status;

  if ((dev == NULL) || (timeout_ms == 0U))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }
  if (FpgaPromax_HasFft(dev) == 0U)
  {
    return FPGA_PROMAX_E_UNSUPPORTED;
  }

  start_ms = (dev->time_ms != NULL) ? dev->time_ms(dev->time_user) : 0U;
  fallback_polls = timeout_ms;
  for (;;)
  {
    result = FpgaPromax_ReadRegister(dev,
                                     FPGA_PROMAX_REG_FFT_STATUS,
                                     &status);
    if (result != FPGA_PROMAX_OK)
    {
      return result;
    }
    if ((status & FPGA_PROMAX_FFT_STATUS_ERROR) != 0U)
    {
      return FPGA_PROMAX_E_FPGA_STATUS;
    }
    if ((status & FPGA_PROMAX_FFT_STATUS_PEAK_VALID) != 0U)
    {
      return FPGA_PROMAX_OK;
    }

    if (dev->time_ms != NULL)
    {
      if ((dev->time_ms(dev->time_user) - start_ms) >= timeout_ms)
      {
        return FPGA_PROMAX_E_TIMEOUT;
      }
      if (dev->delay_ms != NULL)
      {
        dev->delay_ms(dev->time_user, 1U);
      }
    }
    else
    {
      fallback_polls--;
      if (fallback_polls == 0U)
      {
        return FPGA_PROMAX_E_TIMEOUT;
      }
    }
  }
}

/* 在同一快照内读取FFT峰值频点、幅度平方和代次。 */
FpgaPromax_Result FpgaPromax_GetFftPeak(FpgaPromax *dev,
                                       FpgaPromax_FftPeak *peak,
                                       uint32_t timeout_ms)
{
  FpgaPromax_Result result;
  uint32_t bin;
  uint32_t generation;
  uint32_t high;
  uint32_t low;
  uint8_t opened_snapshot;

  if ((dev == NULL) || (peak == NULL))
  {
    return FPGA_PROMAX_E_ARGUMENT;
  }

  result = FpgaPromax_WaitFftPeak(dev, timeout_ms);
  if (result != FPGA_PROMAX_OK)
  {
    return result;
  }

  opened_snapshot = (dev->snapshot_held == 0U) ? 1U : 0U;
  if (opened_snapshot != 0U)
  {
    result = FpgaPromax_BeginResultSnapshot(dev);
    if (result != FPGA_PROMAX_OK)
    {
      return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
    }
  }

  result = FpgaPromax_ReadRegister(dev,
                                   FPGA_PROMAX_REG_FFT_PEAK_BIN,
                                   &bin);
  if (result == FPGA_PROMAX_OK)
  {
    result = FpgaPromax_ReadRegister(dev,
                                     FPGA_PROMAX_REG_FFT_PEAK_LO,
                                     &low);
  }
  if (result == FPGA_PROMAX_OK)
  {
    result = FpgaPromax_ReadRegister(dev,
                                     FPGA_PROMAX_REG_FFT_PEAK_HI,
                                     &high);
  }
  if (result == FPGA_PROMAX_OK)
  {
    result = FpgaPromax_ReadRegister(dev,
                                     FPGA_PROMAX_REG_FFT_GENERATION,
                                     &generation);
  }
  if (result == FPGA_PROMAX_OK)
  {
    peak->bin = (uint16_t)(bin & 0x0FFFU);
    peak->magnitude_square =
      ((uint64_t)(high & 0x1FFFFFFFU) << 32) | low;
    peak->generation = (uint8_t)generation;
  }

  if (opened_snapshot != 0U)
  {
    return FpgaPromax_EndSnapshotPreserveFailure(dev, result);
  }
  return result;
}

/* 返回通用结果状态的简短调试字符串。 */
const char *FpgaPromax_ResultString(FpgaPromax_Result result)
{
  switch (result)
  {
    case FPGA_PROMAX_OK:             return "ok";
    case FPGA_PROMAX_E_ARGUMENT:     return "bad argument";
    case FPGA_PROMAX_E_TRANSPORT:    return "transport error/timeout";
    case FPGA_PROMAX_E_BAD_FRAME:    return "malformed FPGA response";
    case FPGA_PROMAX_E_CRC:          return "response CRC mismatch";
    case FPGA_PROMAX_E_SEQUENCE:     return "response sequence mismatch";
    case FPGA_PROMAX_E_FPGA_STATUS:  return "FPGA rejected command";
    case FPGA_PROMAX_E_WRONG_DEVICE: return "wrong FPGA ID/bitstream";
    case FPGA_PROMAX_E_RANGE:        return "value outside supported range";
    case FPGA_PROMAX_E_TIMEOUT:      return "result-valid wait timed out";
    case FPGA_PROMAX_E_UNSUPPORTED:  return "feature not present in profile";
    default:                         return "unknown error";
  }
}

/* 返回FPGA逻辑协议状态的简短调试字符串。 */
const char *FpgaPromax_ProtocolStatusString(uint8_t status)
{
  switch (status)
  {
    case FPGA_PROMAX_STATUS_OK:        return "ok";
    case FPGA_PROMAX_STATUS_CRC:       return "request CRC mismatch";
    case FPGA_PROMAX_STATUS_BAD_CMD:   return "unknown command";
    case FPGA_PROMAX_STATUS_BAD_ADDR:  return "unknown register address";
    case FPGA_PROMAX_STATUS_READ_ONLY: return "register is read-only";
    case FPGA_PROMAX_STATUS_BUSY:      return "FPGA endpoint busy";
    default:                           return "no valid protocol status";
  }
}
