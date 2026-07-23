#ifndef __FPGA_PROMAX_H
#define __FPGA_PROMAX_H /* FPGA ProMax通用控制接口包含保护。 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define FPGA_PROMAX_FRAME_SIZE       9U          /* ProMax逻辑协议固定帧长。 */
#define FPGA_PROMAX_REQUEST_SOF      0xA5U       /* ProMax请求帧头。 */
#define FPGA_PROMAX_RESPONSE_SOF     0x5AU       /* ProMax响应帧头。 */
#define FPGA_PROMAX_EXPECTED_ID      0x4258504DU /* ProMax设备标识，ASCII为BXPM。 */
#define FPGA_PROMAX_DDC_CHANNELS     8U          /* 实时档DDC通道数。 */
#define FPGA_PROMAX_MF_BANKS         4U          /* 实时档匹配滤波模板组数。 */
#define FPGA_PROMAX_MF_TAPS          32U         /* 每组匹配滤波抽头数。 */

#define FPGA_PROMAX_STATUS_RUN             (1U << 0) /* 实时数据面运行标志。 */
#define FPGA_PROMAX_STATUS_POWER_VALID     (1U << 1) /* 频带功率结果有效标志。 */
#define FPGA_PROMAX_STATUS_SCORE_VALID     (1U << 2) /* 匹配峰值结果有效标志。 */
#define FPGA_PROMAX_STATUS_ALL_RESULTS     \
  (FPGA_PROMAX_STATUS_POWER_VALID | FPGA_PROMAX_STATUS_SCORE_VALID) /* 全部实时结果有效掩码。 */
#define FPGA_PROMAX_CAPABILITY_REALTIME    (1U << 27) /* FPGA具有实时DDC和匹配滤波能力。 */
#define FPGA_PROMAX_CAPABILITY_FFT         (1U << 28) /* FPGA具有流式FFT能力。 */
#define FPGA_PROMAX_FFT_CONTROL_ENABLE     (1U << 0)  /* FFT使能控制位。 */
#define FPGA_PROMAX_FFT_CONTROL_INVERSE    (1U << 1)  /* FFT逆变换控制位。 */
#define FPGA_PROMAX_FFT_CONTROL_RESTART    (1U << 2)  /* FFT流水线重启控制位。 */
#define FPGA_PROMAX_FFT_STATUS_CONFIGURED  (1U << 0)  /* FFT配置完成状态位。 */
#define FPGA_PROMAX_FFT_STATUS_ERROR       (1U << 1)  /* FFT粘滞错误状态位。 */
#define FPGA_PROMAX_FFT_STATUS_PEAK_VALID  (1U << 2)  /* FFT峰值结果有效状态位。 */

/* ProMax逻辑命令码。 */
typedef enum
{
  FPGA_PROMAX_CMD_PING = 0x00U,  /* 查询设备标识。 */
  FPGA_PROMAX_CMD_READ = 0x01U,  /* 读取32位逻辑寄存器。 */
  FPGA_PROMAX_CMD_WRITE = 0x02U  /* 写入32位逻辑寄存器。 */
} FpgaPromax_Command;

/* FPGA返回的逻辑协议状态。 */
typedef enum
{
  FPGA_PROMAX_STATUS_OK = 0x00U,        /* 命令执行成功。 */
  FPGA_PROMAX_STATUS_CRC = 0x01U,       /* 请求CRC错误。 */
  FPGA_PROMAX_STATUS_BAD_CMD = 0x02U,   /* 命令码不受支持。 */
  FPGA_PROMAX_STATUS_BAD_ADDR = 0x03U,  /* 逻辑地址不存在。 */
  FPGA_PROMAX_STATUS_READ_ONLY = 0x04U, /* 尝试写只读寄存器。 */
  FPGA_PROMAX_STATUS_BUSY = 0x05U       /* FPGA链路正忙。 */
} FpgaPromax_ProtocolStatus;

/* ProMax通用接口返回状态。 */
typedef enum
{
  FPGA_PROMAX_OK = 0,              /* 操作成功。 */
  FPGA_PROMAX_E_ARGUMENT = -1,     /* 输入参数无效。 */
  FPGA_PROMAX_E_TRANSPORT = -2,    /* 底层传输失败或超时。 */
  FPGA_PROMAX_E_BAD_FRAME = -3,    /* 响应帧格式错误。 */
  FPGA_PROMAX_E_CRC = -4,          /* 响应CRC错误。 */
  FPGA_PROMAX_E_SEQUENCE = -5,     /* 响应序号错误。 */
  FPGA_PROMAX_E_FPGA_STATUS = -6,  /* FPGA拒绝命令。 */
  FPGA_PROMAX_E_WRONG_DEVICE = -7, /* 设备标识不匹配。 */
  FPGA_PROMAX_E_RANGE = -8,        /* 数值超出硬件范围。 */
  FPGA_PROMAX_E_TIMEOUT = -9,      /* 等待结果超时。 */
  FPGA_PROMAX_E_UNSUPPORTED = -10  /* 当前FPGA档位不支持该功能。 */
} FpgaPromax_Result;

/* ProMax逻辑寄存器地址。 */
typedef enum
{
  FPGA_PROMAX_REG_ID = 0x00U,             /* 设备标识寄存器。 */
  FPGA_PROMAX_REG_VERSION = 0x01U,        /* 固件版本寄存器。 */
  FPGA_PROMAX_REG_CAPABILITY = 0x02U,     /* 硬件能力寄存器。 */
  FPGA_PROMAX_REG_CONTROL = 0x03U,        /* 实时数据面控制寄存器。 */
  FPGA_PROMAX_REG_STATUS = 0x04U,         /* 实时结果状态寄存器。 */
  FPGA_PROMAX_REG_SNAPSHOT = 0x05U,       /* 结果快照控制寄存器。 */
  FPGA_PROMAX_REG_PHASE_INC0 = 0x10U,     /* DDC0相位步进字首地址。 */
  FPGA_PROMAX_REG_PHASE_OFFSET0 = 0x20U,  /* DDC0相位偏移首地址。 */
  FPGA_PROMAX_REG_MF_SELECTOR = 0x40U,    /* 匹配滤波抽头选择寄存器。 */
  FPGA_PROMAX_REG_MF_COEFF = 0x41U,       /* 匹配滤波系数寄存器。 */
  FPGA_PROMAX_REG_POWER_LO0 = 0x50U,      /* 通道0功率低字首地址。 */
  FPGA_PROMAX_REG_POWER_HI0 = 0x58U,      /* 通道0功率高字首地址。 */
  FPGA_PROMAX_REG_MF_SCORE0 = 0x60U,      /* 模板0匹配峰值首地址。 */
  FPGA_PROMAX_REG_MF_PEAK_INDEX0 = 0x64U, /* 模板0峰值位置首地址。 */
  FPGA_PROMAX_REG_FFT_CONTROL = 0x80U,    /* FFT控制寄存器。 */
  FPGA_PROMAX_REG_FFT_STATUS = 0x81U,     /* FFT状态寄存器。 */
  FPGA_PROMAX_REG_FFT_PEAK_BIN = 0x82U,   /* FFT峰值频点寄存器。 */
  FPGA_PROMAX_REG_FFT_PEAK_LO = 0x83U,    /* FFT峰值平方低字寄存器。 */
  FPGA_PROMAX_REG_FFT_PEAK_HI = 0x84U,    /* FFT峰值平方高字寄存器。 */
  FPGA_PROMAX_REG_FFT_GENERATION = 0x85U  /* FFT结果代次寄存器。 */
} FpgaPromax_Register;

/* 完成一次逻辑请求和响应的底层传输回调。 */
typedef int32_t (*FpgaPromax_TransportFn)(
  void *user,
  const uint8_t request[FPGA_PROMAX_FRAME_SIZE],
  uint8_t response[FPGA_PROMAX_FRAME_SIZE],
  uint32_t timeout_ms);

/* 获取单调递增毫秒时基的回调。 */
typedef uint32_t (*FpgaPromax_TimeMsFn)(void *user);

/* 执行毫秒延时的回调。 */
typedef void (*FpgaPromax_DelayMsFn)(void *user, uint32_t delay_ms);

/* 实时档一次快照的全部压缩结果。 */
typedef struct
{
  uint64_t band_power[FPGA_PROMAX_DDC_CHANNELS];          /* 八路37位频带功率。 */
  int32_t matched_score[FPGA_PROMAX_MF_BANKS];            /* 四路24位有符号匹配峰值。 */
  uint32_t matched_peak_index[FPGA_PROMAX_MF_BANKS];      /* 四路匹配峰值样本位置。 */
  uint8_t power_generation;                               /* 功率结果代次。 */
  uint8_t score_generation;                               /* 匹配结果代次。 */
} FpgaPromax_AllResults;

/* FFT档的帧内峰值摘要。 */
typedef struct
{
  uint16_t bin;               /* 峰值频点编号。 */
  uint64_t magnitude_square;  /* 峰值幅度平方，低61位有效。 */
  uint8_t generation;         /* FFT结果代次。 */
} FpgaPromax_FftPeak;

/* ProMax设备运行句柄。 */
typedef struct
{
  FpgaPromax_TransportFn transport; /* 底层传输回调。 */
  void *transport_user;             /* 底层传输上下文。 */
  uint32_t timeout_ms;              /* 单次逻辑事务超时，单位ms。 */
  uint8_t next_sequence;            /* 下一请求序号。 */
  uint8_t last_protocol_status;     /* 最近一次FPGA协议状态。 */
  int32_t last_transport_status;    /* 最近一次底层传输状态。 */
  uint32_t version;                 /* 已读取的FPGA固件版本。 */
  uint32_t capability;              /* 已读取的FPGA能力位。 */
  uint8_t snapshot_held;            /* 软件记录的快照冻结状态。 */
  FpgaPromax_TimeMsFn time_ms;      /* 可选毫秒时基回调。 */
  FpgaPromax_DelayMsFn delay_ms;    /* 可选毫秒延时回调。 */
  void *time_user;                  /* 时基回调上下文。 */
} FpgaPromax;

/* 初始化设备并读取设备标识、版本和能力。 */
FpgaPromax_Result FpgaPromax_Init(FpgaPromax *dev,
                                  FpgaPromax_TransportFn transport,
                                  void *transport_user,
                                  uint32_t timeout_ms);

/* 设置结果等待所用的可选毫秒时基。 */
void FpgaPromax_SetTimebase(FpgaPromax *dev,
                            FpgaPromax_TimeMsFn time_ms,
                            FpgaPromax_DelayMsFn delay_ms,
                            void *time_user);

/* 查询并返回ProMax设备标识。 */
FpgaPromax_Result FpgaPromax_Ping(FpgaPromax *dev, uint32_t *device_id);

/* 读取一个32位逻辑寄存器。 */
FpgaPromax_Result FpgaPromax_ReadRegister(FpgaPromax *dev,
                                         uint8_t address,
                                         uint32_t *value);

/* 写入一个32位逻辑寄存器。 */
FpgaPromax_Result FpgaPromax_WriteRegister(FpgaPromax *dev,
                                          uint8_t address,
                                          uint32_t value);

/* 启停实时数据面。 */
FpgaPromax_Result FpgaPromax_SetRun(FpgaPromax *dev, uint8_t enable);

/* 同步清除实时状态并保留已装载模板。 */
FpgaPromax_Result FpgaPromax_ClearState(FpgaPromax *dev);

/* 读取实时数据面状态。 */
FpgaPromax_Result FpgaPromax_GetStatus(FpgaPromax *dev, uint32_t *status);

/* 有界等待指定实时结果有效。 */
FpgaPromax_Result FpgaPromax_WaitResults(FpgaPromax *dev,
                                        uint32_t required_status_mask,
                                        uint32_t timeout_ms);

/* 冻结结果快照。 */
FpgaPromax_Result FpgaPromax_BeginResultSnapshot(FpgaPromax *dev);

/* 解冻结果快照。 */
FpgaPromax_Result FpgaPromax_EndResultSnapshot(FpgaPromax *dev);

/* 读取结果代次和快照冻结状态。 */
FpgaPromax_Result FpgaPromax_GetSnapshotInfo(FpgaPromax *dev,
                                            uint8_t *power_generation,
                                            uint8_t *score_generation,
                                            uint8_t *is_held);

/* 设置一路DDC的相位步进和偏移字。 */
FpgaPromax_Result FpgaPromax_SetDdcPhaseWord(FpgaPromax *dev,
                                            uint8_t channel,
                                            uint32_t phase_increment,
                                            uint32_t phase_offset);

/* 按频率和有效采样率设置一路DDC。 */
FpgaPromax_Result FpgaPromax_SetDdcFrequency(FpgaPromax *dev,
                                            uint8_t channel,
                                            uint32_t frequency_hz,
                                            uint32_t sample_rate_hz);

/* 装载一组Q1.16匹配模板，可选时间反转。 */
FpgaPromax_Result FpgaPromax_LoadMatchedTemplate(FpgaPromax *dev,
                                                uint8_t bank,
                                                const int32_t *coeff_q16,
                                                size_t count,
                                                uint8_t reverse);

/* 读取一路37位频带功率。 */
FpgaPromax_Result FpgaPromax_GetBandPower(FpgaPromax *dev,
                                         uint8_t channel,
                                         uint64_t *power);

/* 读取一组24位有符号匹配峰值。 */
FpgaPromax_Result FpgaPromax_GetMatchedScore(FpgaPromax *dev,
                                            uint8_t bank,
                                            int32_t *score);

/* 读取一组匹配峰值及其样本位置。 */
FpgaPromax_Result FpgaPromax_GetMatchedPeak(FpgaPromax *dev,
                                           uint8_t bank,
                                           int32_t *score,
                                           uint32_t *sample_index);

/* 在同一快照内读取全部实时结果。 */
FpgaPromax_Result FpgaPromax_ReadAllResults(FpgaPromax *dev,
                                           FpgaPromax_AllResults *results,
                                           uint32_t timeout_ms);

/* 判断当前FPGA是否具有实时数据面。 */
uint8_t FpgaPromax_HasRealtime(const FpgaPromax *dev);

/* 判断当前FPGA是否具有FFT数据面。 */
uint8_t FpgaPromax_HasFft(const FpgaPromax *dev);

/* 配置FFT启停、方向和重启。 */
FpgaPromax_Result FpgaPromax_SetFft(FpgaPromax *dev,
                                   uint8_t enable,
                                   uint8_t inverse,
                                   uint8_t restart);

/* 有界等待FFT峰值结果有效。 */
FpgaPromax_Result FpgaPromax_WaitFftPeak(FpgaPromax *dev,
                                        uint32_t timeout_ms);

/* 读取一帧FFT峰值摘要。 */
FpgaPromax_Result FpgaPromax_GetFftPeak(FpgaPromax *dev,
                                       FpgaPromax_FftPeak *peak,
                                       uint32_t timeout_ms);

/* 计算CRC-8/ATM校验值。 */
uint8_t FpgaPromax_Crc8(const uint8_t *data, size_t length);

/* 返回通用结果状态的调试字符串。 */
const char *FpgaPromax_ResultString(FpgaPromax_Result result);

/* 返回FPGA协议状态的调试字符串。 */
const char *FpgaPromax_ProtocolStatusString(uint8_t status);

#ifdef __cplusplus
}
#endif

#endif /* __FPGA_PROMAX_H */
