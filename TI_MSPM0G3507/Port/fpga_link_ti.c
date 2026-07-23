#include "ti_msp_dl_config.h"
#include "fpga_link.h"

#define FPGA_LINK_FRAME_LEN       6U    /* FPGA寄存器协议固定帧长。 */
#define FPGA_LINK_WRITE_FLAG      0x80U /* 地址字节的写操作标志。 */
#define FPGA_LINK_ADDR_MASK       0x7FU /* FPGA寄存器地址有效位。 */
#define FPGA_LINK_ACK0            0xA5U /* FPGA响应帧首字节。 */
#define FPGA_LINK_ACK1            0x5AU /* FPGA响应帧次字节。 */
#define FPGA_LINK_CS_GUARD_CYCLES 128U  /* 片选建立和帧间保护周期。 */
#define FPGA_LINK_READ_ATTEMPTS   3U    /* 只读事务最大尝试次数。 */
#define FPGA_LINK_RX_FIFO_DEPTH   4U    /* MSPM0G3507 SPI接收FIFO深度。 */

/* 在已开始的会话中执行一帧寄存器事务。 */
static FPGA_LinkStatusTypeDef FPGA_Link_TransferFrame(
    FPGA_LinkHandleTypeDef *dev,
    uint8_t addr,
    uint8_t is_write,
    uint32_t tx_value,
    uint32_t *rx_value);

/* 自动管理单帧事务的开始和结束。 */
static FPGA_LinkStatusTypeDef FPGA_Link_Transfer(
    FPGA_LinkHandleTypeDef *dev,
    uint8_t addr,
    uint8_t is_write,
    uint32_t tx_value,
    uint32_t *rx_value);

/* 检查TI板FPGA链路句柄。 */
static FPGA_LinkStatusTypeDef FPGA_Link_CheckConfig(
    const FPGA_LinkHandleTypeDef *dev);

/* 使用MSPM0G3507专用SPI0完成全双工字节传输。 */
static FPGA_LinkStatusTypeDef FPGA_Link_TransferBytes(
    const FPGA_LinkHandleTypeDef *dev,
    const uint8_t *tx,
    uint8_t *rx,
    uint32_t length);

/* 判断一次SPI等待是否超时。 */
static uint8_t FPGA_Link_TimeoutExpired(uint32_t start_tick,
                                        uint32_t timeout_ms);

/* 提供片选边沿保护延时。 */
static void FPGA_Link_DelayCycles(uint32_t cycles);

/************************************************************
 * Function :       FPGA_Link_GetDefaultConfig
 * Comment  :       填充TI板到FPGA的专用SPI0与片选配置
 * Parameter:       cfg: 配置结构体指针
 * Return   :       null
 ************************************************************/
void FPGA_Link_GetDefaultConfig(FPGA_LinkConfigTypeDef *cfg)
{
  if (cfg == NULL)
  {
    return;
  }

  cfg->hspi = &hspi1;
  cfg->cs_port = FPGA_LINK_CS_PORT;
  cfg->cs_pin = FPGA_LINK_CS_PIN;
  cfg->timeout_ms = FPGA_LINK_TIMEOUT_MS;
}

/************************************************************
 * Function :       FPGA_Link_Init
 * Comment  :       初始化TI板到FPGA的寄存器链路
 * Parameter:       dev: 链路句柄; cfg: 链路配置
 * Return   :       FPGA链路状态
 ************************************************************/
FPGA_LinkStatusTypeDef FPGA_Link_Init(FPGA_LinkHandleTypeDef *dev,
                                      const FPGA_LinkConfigTypeDef *cfg)
{
  if ((dev == NULL) || (cfg == NULL))
  {
    return FPGA_LINK_ERROR_PARAM;
  }

  dev->cfg = *cfg;
  dev->last_rx = 0U;
  dev->session_active = 0U;
  dev->spi_changed = 0U;
  dev->last_status = FPGA_Link_CheckConfig(dev);
  if (dev->last_status != FPGA_LINK_OK)
  {
    return dev->last_status;
  }

  HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);
  return FPGA_LINK_OK;
}

/************************************************************
 * Function :       FPGA_Link_Begin
 * Comment  :       开始一个TI板到FPGA的连续事务
 * Parameter:       dev: 链路句柄
 * Return   :       FPGA链路状态
 ************************************************************/
FPGA_LinkStatusTypeDef FPGA_Link_Begin(FPGA_LinkHandleTypeDef *dev)
{
  if (dev == NULL)
  {
    return FPGA_LINK_ERROR_PARAM;
  }
  if (dev->session_active != 0U)
  {
    dev->last_status = FPGA_LINK_ERROR_BUSY;
    return dev->last_status;
  }

  dev->last_status = FPGA_Link_CheckConfig(dev);
  if (dev->last_status != FPGA_LINK_OK)
  {
    return dev->last_status;
  }

  dev->session_active = 1U;
  dev->last_status = FPGA_LINK_OK;
  return dev->last_status;
}

/************************************************************
 * Function :       FPGA_Link_End
 * Comment  :       结束连续事务并释放软件片选
 * Parameter:       dev: 链路句柄
 * Return   :       FPGA链路状态
 ************************************************************/
FPGA_LinkStatusTypeDef FPGA_Link_End(FPGA_LinkHandleTypeDef *dev)
{
  if ((dev == NULL) || (dev->session_active == 0U))
  {
    return FPGA_LINK_ERROR_PARAM;
  }

  HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);
  dev->session_active = 0U;
  dev->last_status = FPGA_LINK_OK;
  return dev->last_status;
}

/************************************************************
 * Function :       FPGA_Link_WriteReg
 * Comment  :       写一个32位FPGA寄存器
 * Parameter:       dev: 链路句柄; addr: 地址; value: 写入值
 * Return   :       FPGA链路状态
 ************************************************************/
FPGA_LinkStatusTypeDef FPGA_Link_WriteReg(FPGA_LinkHandleTypeDef *dev,
                                          uint8_t addr,
                                          uint32_t value)
{
  return FPGA_Link_Transfer(dev, addr, 1U, value, NULL);
}

/************************************************************
 * Function :       FPGA_Link_ReadReg
 * Comment  :       读取一个32位FPGA寄存器并自动重试
 * Parameter:       dev: 链路句柄; addr: 地址; value: 读数指针
 * Return   :       FPGA链路状态
 ************************************************************/
FPGA_LinkStatusTypeDef FPGA_Link_ReadReg(FPGA_LinkHandleTypeDef *dev,
                                         uint8_t addr,
                                         uint32_t *value)
{
  FPGA_LinkStatusTypeDef status;
  uint32_t attempt;

  status = FPGA_LINK_ERROR_ACK;
  for (attempt = 0U; attempt < FPGA_LINK_READ_ATTEMPTS; attempt++)
  {
    status = FPGA_Link_Transfer(dev, addr, 0U, 0U, value);
    if ((status == FPGA_LINK_OK) ||
        ((status != FPGA_LINK_ERROR_ACK) &&
         (status != FPGA_LINK_ERROR_SPI)))
    {
      break;
    }
    FPGA_Link_DelayCycles(FPGA_LINK_CS_GUARD_CYCLES);
  }
  return status;
}

/* 检查链路所需句柄、片选和超时参数。 */
static FPGA_LinkStatusTypeDef FPGA_Link_CheckConfig(
    const FPGA_LinkHandleTypeDef *dev)
{
  if ((dev == NULL) ||
      (dev->cfg.hspi != &hspi1) ||
      (dev->cfg.cs_port == NULL) ||
      (dev->cfg.cs_pin == 0U) ||
      (dev->cfg.timeout_ms == 0U))
  {
    return FPGA_LINK_ERROR_PARAM;
  }
  return FPGA_LINK_OK;
}

/* 自动管理独立事务，连续事务中直接复用当前SPI0配置。 */
static FPGA_LinkStatusTypeDef FPGA_Link_Transfer(
    FPGA_LinkHandleTypeDef *dev,
    uint8_t addr,
    uint8_t is_write,
    uint32_t tx_value,
    uint32_t *rx_value)
{
  FPGA_LinkStatusTypeDef status;
  FPGA_LinkStatusTypeDef end_status;
  uint8_t manage_session;

  if ((dev == NULL) ||
      ((addr & (uint8_t)(~FPGA_LINK_ADDR_MASK)) != 0U) ||
      ((is_write == 0U) && (rx_value == NULL)))
  {
    return FPGA_LINK_ERROR_PARAM;
  }

  manage_session = (dev->session_active == 0U) ? 1U : 0U;
  if (manage_session != 0U)
  {
    status = FPGA_Link_Begin(dev);
    if (status != FPGA_LINK_OK)
    {
      return status;
    }
  }

  status = FPGA_Link_TransferFrame(dev,
                                   addr,
                                   is_write,
                                   tx_value,
                                   rx_value);
  if (manage_session != 0U)
  {
    end_status = FPGA_Link_End(dev);
    if (status == FPGA_LINK_OK)
    {
      status = end_status;
    }
  }

  dev->last_status = status;
  return status;
}

/* 组装并收发FPGA固定6字节寄存器帧。 */
static FPGA_LinkStatusTypeDef FPGA_Link_TransferFrame(
    FPGA_LinkHandleTypeDef *dev,
    uint8_t addr,
    uint8_t is_write,
    uint32_t tx_value,
    uint32_t *rx_value)
{
  uint8_t tx[FPGA_LINK_FRAME_LEN];
  uint8_t rx[FPGA_LINK_FRAME_LEN] = {0U};
  FPGA_LinkStatusTypeDef status;

  tx[0] = (uint8_t)(((is_write != 0U) ? FPGA_LINK_WRITE_FLAG : 0U) |
                    (addr & FPGA_LINK_ADDR_MASK));
  tx[1] = (uint8_t)(tx_value >> 24);
  tx[2] = (uint8_t)(tx_value >> 16);
  tx[3] = (uint8_t)(tx_value >> 8);
  tx[4] = (uint8_t)tx_value;
  tx[5] = 0U;

  FPGA_Link_DelayCycles(FPGA_LINK_CS_GUARD_CYCLES);
  HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_RESET);
  FPGA_Link_DelayCycles(FPGA_LINK_CS_GUARD_CYCLES);
  status = FPGA_Link_TransferBytes(dev, tx, rx, FPGA_LINK_FRAME_LEN);
  HAL_GPIO_WritePin(dev->cfg.cs_port, dev->cfg.cs_pin, GPIO_PIN_SET);
  FPGA_Link_DelayCycles(FPGA_LINK_CS_GUARD_CYCLES);

  if (status != FPGA_LINK_OK)
  {
    return status;
  }
  if ((rx[0] != FPGA_LINK_ACK0) || (rx[1] != FPGA_LINK_ACK1))
  {
    return FPGA_LINK_ERROR_ACK;
  }

  dev->last_rx = ((uint32_t)rx[2] << 24) |
                 ((uint32_t)rx[3] << 16) |
                 ((uint32_t)rx[4] << 8) |
                 (uint32_t)rx[5];
  if (rx_value != NULL)
  {
    *rx_value = dev->last_rx;
  }
  return FPGA_LINK_OK;
}

/* 逐字节收发，避免6字节帧超过MSPM0G3507的4级FIFO。 */
static FPGA_LinkStatusTypeDef FPGA_Link_TransferBytes(
    const FPGA_LinkHandleTypeDef *dev,
    const uint8_t *tx,
    uint8_t *rx,
    uint32_t length)
{
  uint8_t discarded[FPGA_LINK_RX_FIFO_DEPTH];
  uint32_t start_tick;
  uint32_t index;

  if ((dev == NULL) || (tx == NULL) || (rx == NULL) || (length == 0U))
  {
    return FPGA_LINK_ERROR_PARAM;
  }

  (void)DL_SPI_drainRXFIFO8(FPGA_SPI_INST,
                            discarded,
                            FPGA_LINK_RX_FIFO_DEPTH);
  start_tick = HAL_GetTick();

  for (index = 0U; index < length; index++)
  {
    while (DL_SPI_isTXFIFOFull(FPGA_SPI_INST))
    {
      if (FPGA_Link_TimeoutExpired(start_tick,
                                   dev->cfg.timeout_ms) != 0U)
      {
        return FPGA_LINK_ERROR_SPI;
      }
    }
    DL_SPI_transmitData8(FPGA_SPI_INST, tx[index]);

    while (DL_SPI_isRXFIFOEmpty(FPGA_SPI_INST))
    {
      if (FPGA_Link_TimeoutExpired(start_tick,
                                   dev->cfg.timeout_ms) != 0U)
      {
        return FPGA_LINK_ERROR_SPI;
      }
    }
    rx[index] = DL_SPI_receiveData8(FPGA_SPI_INST);
  }

  while (DL_SPI_isBusy(FPGA_SPI_INST))
  {
    if (FPGA_Link_TimeoutExpired(start_tick, dev->cfg.timeout_ms) != 0U)
    {
      return FPGA_LINK_ERROR_SPI;
    }
  }
  return FPGA_LINK_OK;
}

/* 按毫秒节拍判断等待是否超过配置上限。 */
static uint8_t FPGA_Link_TimeoutExpired(uint32_t start_tick,
                                        uint32_t timeout_ms)
{
  return ((HAL_GetTick() - start_tick) >= timeout_ms) ? 1U : 0U;
}

/* 以短循环保证FPGA跨时钟同步器可靠识别片选边沿。 */
static void FPGA_Link_DelayCycles(uint32_t cycles)
{
  while (cycles > 0U)
  {
    __NOP();
    cycles--;
  }
}
