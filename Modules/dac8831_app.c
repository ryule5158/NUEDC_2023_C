#include "dac8831_app.h"

#define DAC8831_APP_SCK_GPIO_PORT   GPIOA       /* 软件SPI时钟虚拟端口，TI映射到PB6。 */
#define DAC8831_APP_SCK_PIN         GPIO_PIN_5  /* 软件SPI时钟虚拟引脚。 */
#define DAC8831_APP_MOSI_GPIO_PORT  GPIOA       /* 软件SPI数据虚拟端口，TI映射到PA7。 */
#define DAC8831_APP_MOSI_PIN        GPIO_PIN_7  /* 软件SPI数据虚拟引脚。 */
#define DAC8831_APP_CS_GPIO_PORT    GPIOA       /* 片选虚拟端口，TI映射到PB0。 */
#define DAC8831_APP_CS_PIN          GPIO_PIN_4  /* 低有效片选虚拟引脚。 */

/*
 * DAC8831波形由主循环软件送点产生, 必须持续调用DAC8831_AppProcess()。
 * output_hz越高需要越高送点速率，当前默认1000点/秒，16点表最高约62Hz。
 */

typedef struct
{
  const uint16_t *table;
  uint16_t size;
  uint16_t amplitude;
  uint16_t index;
  uint32_t frequency_hz;
  uint32_t last_sample_cycle;   /* 上一次送点时的CPU周期计数，用于提高时间分辨率 */
  uint64_t phase_accumulator;
  uint32_t low_hz;
  uint32_t high_hz;
  uint32_t frequency_step_hz;
  uint32_t dwell_ms;
  uint32_t last_sweep_tick;
  int8_t sweep_direction;
  uint8_t running;
  uint8_t sweep_running;
} DAC8831_AppWaveTaskTypeDef;

static DAC8831_HandleTypeDef s_dac8831;
static DAC8831_AppDataTypeDef s_dac8831_app_data;
static DAC8831_AppWaveTaskTypeDef s_wave_task;
static uint8_t s_dac8831_initialized;
static void DAC8831_AppTimebaseInit(void);  /* 初始化CPU周期计数器 */
static uint32_t DAC8831_AppGetCycle(void);  /* 读取当前CPU周期计数 */

const uint16_t DAC8831_AppSineTable[DAC8831_APP_WAVE_TABLE_SIZE] =
{
  32768U, 45307U, 55938U, 63041U,
  65535U, 63041U, 55938U, 45307U,
  32768U, 20228U,  9597U,  2494U,
      0U,  2494U,  9597U, 20228U
};

const uint16_t DAC8831_AppTriangleTable[DAC8831_APP_WAVE_TABLE_SIZE] =
{
      0U,  8192U, 16384U, 24576U,
  32768U, 40960U, 49152U, 57344U,
  65535U, 57344U, 49152U, 40960U,
  32768U, 24576U, 16384U,  8192U
};

const uint16_t DAC8831_AppSquareTable[DAC8831_APP_WAVE_TABLE_SIZE] =
{
      0U,     0U,     0U,     0U,
      0U,     0U,     0U,     0U,
  65535U, 65535U, 65535U, 65535U,
  65535U, 65535U, 65535U, 65535U
};

const uint16_t DAC8831_AppSawtoothTable[DAC8831_APP_WAVE_TABLE_SIZE] =
{
      0U,  4369U,  8738U, 13107U,
  17476U, 21845U, 26214U, 30583U,
  34952U, 39321U, 43690U, 48059U,
  52428U, 56797U, 61166U, 65535U
};

uint16_t DAC8831_AppArbitraryTable[DAC8831_APP_WAVE_TABLE_SIZE] =
{
      0U,  8192U, 16384U, 24576U,
  32768U, 40960U, 49152U, 57344U,
  65535U, 57344U, 49152U, 40960U,
  32768U, 24576U, 16384U,  8192U
};

static DAC8831_StatusTypeDef EnsureInit(void);
static DAC8831_StatusTypeDef StartWave(const uint16_t *wave_table,
                                       uint16_t table_size,
                                       uint32_t output_hz,
                                       uint16_t amplitude);
static DAC8831_StatusTypeDef StartSweep(const uint16_t *wave_table,
                                        uint16_t table_size,
                                        uint32_t low_hz,
                                        uint32_t high_hz,
                                        uint32_t frequency_step_hz,
                                        uint16_t amplitude,
                                        uint32_t dwell_ms);
static DAC8831_StatusTypeDef StepWave(uint8_t force);
static void UpdateSweep(void);
static uint16_t ScaleCode(uint16_t code, uint16_t amplitude);

/************************************************************
 * Function :       DAC8831_AppInit
 * Comment  :       初始化DAC8831应用层实例
 * Parameter:       null
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V2
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppInit(void)
{
  DAC8831_AppTimebaseInit();  /* 启用DWT周期计数，用于微秒级送点 */
  DAC8831_ConfigTypeDef cfg;

  DAC8831_GetDefaultConfig(&cfg);
  cfg.sck_port = DAC8831_APP_SCK_GPIO_PORT;
  cfg.sck_pin = DAC8831_APP_SCK_PIN;
  cfg.mosi_port = DAC8831_APP_MOSI_GPIO_PORT;
  cfg.mosi_pin = DAC8831_APP_MOSI_PIN;
  cfg.cs_port = DAC8831_APP_CS_GPIO_PORT;
  cfg.cs_pin = DAC8831_APP_CS_PIN;
  cfg.spi_delay_cycles = DAC8831_SOFT_SPI_DELAY_CYCLES;
  cfg.ldac_mode = DAC8831_LDAC_TIED_LOW;
  cfg.vref_mv = DAC8831_APP_DEFAULT_VREF_MV;
  cfg.output_mode = DAC8831_OUTPUT_UNIPOLAR;

  DAC8831_AppStop();
  s_dac8831_app_data.last_code = 0U;
  s_dac8831_app_data.last_target_mv = 0.0f;
  s_dac8831_app_data.status = DAC8831_Init(&s_dac8831, &cfg);
  s_dac8831_initialized =
    (s_dac8831_app_data.status == DAC8831_OK) ? 1U : 0U;

  return s_dac8831_app_data.status;
}

/************************************************************
 * Function :       DAC8831_AppGetData
 * Comment  :       获取DAC8831应用层状态和最近一次输出值
 * Parameter:       null
 * Return   :       DAC8831应用层数据指针
 * Date     :       2026-06-12 V2
************************************************************/
const DAC8831_AppDataTypeDef *DAC8831_AppGetData(void)
{
  return &s_dac8831_app_data;
}

/************************************************************
 * Function :       DAC8831_AppOutputRaw
 * Comment  :       按16位原始码值输出DAC8831
 * Parameter:       code: 16位DAC码值, 范围0~65535
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputRaw(uint16_t code)
{
  DAC8831_StatusTypeDef status;

  status = EnsureInit();
  if (status != DAC8831_OK)
  {
    return status;
  }

  DAC8831_AppStop();
  s_dac8831_app_data.status = DAC8831_WriteRaw(&s_dac8831, code);
  if (s_dac8831_app_data.status == DAC8831_OK)
  {
    s_dac8831_app_data.last_code = code;
    if (s_dac8831.cfg.output_mode == DAC8831_OUTPUT_BIPOLAR)
    {
      s_dac8831_app_data.last_target_mv =
        DAC8831_CodeToBipolarMv(code, s_dac8831.cfg.vref_mv);
    }
    else
    {
      s_dac8831_app_data.last_target_mv =
        DAC8831_CodeToUnipolarMv(code, s_dac8831.cfg.vref_mv);
    }
  }

  return s_dac8831_app_data.status;
}

/************************************************************
 * Function :       DAC8831_AppOutputUnipolarMv
 * Comment  :       按单极性毫伏值输出DAC8831
 * Parameter:       millivolts: 输出电压, 单位mV
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputUnipolarMv(float millivolts)
{
  return DAC8831_AppOutputRaw(DAC8831_UnipolarMvToCode(millivolts,
                                                       DAC8831_APP_DEFAULT_VREF_MV));
}

/************************************************************
 * Function :       DAC8831_AppOutputBipolarMv
 * Comment  :       按双极性毫伏值输出DAC8831
 * Parameter:       millivolts: 输出电压, 单位mV
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputBipolarMv(float millivolts)
{
  return DAC8831_AppOutputRaw(DAC8831_BipolarMvToCode(millivolts,
                                                      DAC8831_APP_DEFAULT_VREF_MV));
}

/************************************************************
 * Function :       DAC8831_AppOutputSine
 * Comment  :       启动DAC8831正弦波输出任务
 * Parameter:       output_hz: 1~62Hz; amplitude: 0~65535
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputSine(uint32_t output_hz,
                                            uint16_t amplitude)
{
  return StartWave(DAC8831_AppSineTable,
                   DAC8831_APP_WAVE_TABLE_SIZE,
                   output_hz,
                   amplitude);
}

/************************************************************
 * Function :       DAC8831_AppOutputTriangle
 * Comment  :       启动DAC8831三角波输出任务
 * Parameter:       output_hz: 1~62Hz; amplitude: 0~65535
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputTriangle(uint32_t output_hz,
                                                uint16_t amplitude)
{
  return StartWave(DAC8831_AppTriangleTable,
                   DAC8831_APP_WAVE_TABLE_SIZE,
                   output_hz,
                   amplitude);
}

/************************************************************
 * Function :       DAC8831_AppOutputSquare
 * Comment  :       启动DAC8831方波输出任务
 * Parameter:       output_hz: 1~62Hz; amplitude: 0~65535
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputSquare(uint32_t output_hz,
                                              uint16_t amplitude)
{
  return StartWave(DAC8831_AppSquareTable,
                   DAC8831_APP_WAVE_TABLE_SIZE,
                   output_hz,
                   amplitude);
}

/************************************************************
 * Function :       DAC8831_AppOutputSawtooth
 * Comment  :       启动DAC8831锯齿波输出任务
 * Parameter:       output_hz: 1~62Hz; amplitude: 0~65535
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputSawtooth(uint32_t output_hz,
                                                uint16_t amplitude)
{
  return StartWave(DAC8831_AppSawtoothTable,
                   DAC8831_APP_WAVE_TABLE_SIZE,
                   output_hz,
                   amplitude);
}

/************************************************************
 * Function :       DAC8831_AppOutputArbitraryHz
 * Comment  :       启动DAC8831任意波输出任务
 * Parameter:       wave_table: 波形表; table_size: 点数; output_hz: 波形频率; amplitude: 16位幅度
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppOutputArbitraryHz(const uint16_t *wave_table,
                                                   uint16_t table_size,
                                                   uint32_t output_hz,
                                                   uint16_t amplitude)
{
  return StartWave(wave_table,
                   table_size,
                   output_hz,
                   amplitude);
}

/************************************************************
 * Function :       DAC8831_AppSweepSine
 * Comment  :       启动DAC8831正弦波扫频任务
 * Parameter:       low_hz/high_hz: 扫频范围Hz; frequency_step_hz: 频率步进Hz; amplitude: 16位幅度; dwell_ms: 每步停留时间
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppSweepSine(uint32_t low_hz,
                                           uint32_t high_hz,
                                           uint32_t frequency_step_hz,
                                           uint16_t amplitude,
                                           uint32_t dwell_ms)
{
  return StartSweep(DAC8831_AppSineTable,
                    DAC8831_APP_WAVE_TABLE_SIZE,
                    low_hz,
                    high_hz,
                    frequency_step_hz,
                    amplitude,
                    dwell_ms);
}

/************************************************************
 * Function :       DAC8831_AppSweepTriangle
 * Comment  :       启动DAC8831三角波扫频任务
 * Parameter:       low_hz/high_hz: 扫频范围Hz; frequency_step_hz: 频率步进Hz; amplitude: 16位幅度; dwell_ms: 每步停留时间
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppSweepTriangle(uint32_t low_hz,
                                               uint32_t high_hz,
                                               uint32_t frequency_step_hz,
                                               uint16_t amplitude,
                                               uint32_t dwell_ms)
{
  return StartSweep(DAC8831_AppTriangleTable,
                    DAC8831_APP_WAVE_TABLE_SIZE,
                    low_hz,
                    high_hz,
                    frequency_step_hz,
                    amplitude,
                    dwell_ms);
}

/************************************************************
 * Function :       DAC8831_AppSweepSquare
 * Comment  :       启动DAC8831方波扫频任务
 * Parameter:       low_hz/high_hz: 扫频范围Hz; frequency_step_hz: 频率步进Hz; amplitude: 16位幅度; dwell_ms: 每步停留时间
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppSweepSquare(uint32_t low_hz,
                                             uint32_t high_hz,
                                             uint32_t frequency_step_hz,
                                             uint16_t amplitude,
                                             uint32_t dwell_ms)
{
  return StartSweep(DAC8831_AppSquareTable,
                    DAC8831_APP_WAVE_TABLE_SIZE,
                    low_hz,
                    high_hz,
                    frequency_step_hz,
                    amplitude,
                    dwell_ms);
}

/************************************************************
 * Function :       DAC8831_AppSweepSawtooth
 * Comment  :       启动DAC8831锯齿波扫频任务
 * Parameter:       low_hz/high_hz: 扫频范围Hz; frequency_step_hz: 频率步进Hz; amplitude: 16位幅度; dwell_ms: 每步停留时间
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppSweepSawtooth(uint32_t low_hz,
                                               uint32_t high_hz,
                                               uint32_t frequency_step_hz,
                                               uint16_t amplitude,
                                               uint32_t dwell_ms)
{
  return StartSweep(DAC8831_AppSawtoothTable,
                    DAC8831_APP_WAVE_TABLE_SIZE,
                    low_hz,
                    high_hz,
                    frequency_step_hz,
                    amplitude,
                    dwell_ms);
}

/************************************************************
 * Function :       DAC8831_AppSweepArbitrary
 * Comment  :       启动DAC8831任意波扫频任务
 * Parameter:       wave_table: 波形表; table_size: 点数; low_hz/high_hz: 扫频范围Hz; frequency_step_hz: 频率步进Hz; amplitude: 16位幅度; dwell_ms: 每步停留时间
 * Return   :       DAC8831_OK表示成功, 其他值表示失败
 * Date     :       2026-06-12 V1
************************************************************/
DAC8831_StatusTypeDef DAC8831_AppSweepArbitrary(const uint16_t *wave_table,
                                                uint16_t table_size,
                                                uint32_t low_hz,
                                                uint32_t high_hz,
                                                uint32_t frequency_step_hz,
                                                uint16_t amplitude,
                                                uint32_t dwell_ms)
{
  return StartSweep(wave_table,
                    table_size,
                    low_hz,
                    high_hz,
                    frequency_step_hz,
                    amplitude,
                    dwell_ms);
}

/************************************************************
 * Function :       DAC8831_AppStop
 * Comment  :       停止DAC8831应用层后台波形任务
 * Parameter:       null
 * Return   :       null
 * Date     :       2026-06-12 V1
************************************************************/
void DAC8831_AppStop(void)
{
  s_wave_task.table = NULL;
  s_wave_task.size = 0U;
  s_wave_task.amplitude = 0U;
  s_wave_task.index = 0U;
  s_wave_task.frequency_hz = 0U;
  s_wave_task.last_sample_cycle = 0U;
  s_wave_task.phase_accumulator = 0ULL;
  s_wave_task.low_hz = 0U;
  s_wave_task.high_hz = 0U;
  s_wave_task.frequency_step_hz = 0U;
  s_wave_task.dwell_ms = 0U;
  s_wave_task.last_sweep_tick = 0U;
  s_wave_task.sweep_direction = -1;
  s_wave_task.running = 0U;
  s_wave_task.sweep_running = 0U;
}

/************************************************************
 * Function :       DAC8831_AppProcess
 * Comment  :       DAC8831应用层后台处理函数, 放在while(1)中周期调用
 * Parameter:       null
 * Return   :       null
 * Date     :       2026-06-12 V1
************************************************************/
void DAC8831_AppProcess(void)
{
  if (s_wave_task.running != 0U)
  {
    UpdateSweep();
    (void)StepWave(0U);
  }
}

static DAC8831_StatusTypeDef EnsureInit(void)
{
  if (s_dac8831_initialized != 0U)
  {
    return DAC8831_OK;
  }

  return DAC8831_AppInit();
}

static DAC8831_StatusTypeDef StartWave(const uint16_t *wave_table,
                                       uint16_t table_size,
                                       uint32_t output_hz,
                                       uint16_t amplitude)
{
  DAC8831_StatusTypeDef status;

  if ((wave_table == NULL) ||
      (table_size == 0U) ||
      (output_hz == 0U) ||
      (((uint64_t)table_size * (uint64_t)output_hz) >
       DAC8831_APP_SAMPLE_RATE_HZ))
  {
    return DAC8831_ERROR_PARAM;
  }

  status = EnsureInit();
  if (status != DAC8831_OK)
  {
    return status;
  }

  s_wave_task.table = wave_table;
  s_wave_task.size = table_size;
  s_wave_task.amplitude = amplitude;
  s_wave_task.index = 0U;
  s_wave_task.frequency_hz = output_hz;
  s_wave_task.last_sample_cycle = HAL_GetTick();
  s_wave_task.phase_accumulator = 0ULL;
  s_wave_task.low_hz = 0U;
  s_wave_task.high_hz = 0U;
  s_wave_task.frequency_step_hz = 0U;
  s_wave_task.dwell_ms = 0U;
  s_wave_task.last_sweep_tick = 0U;
  s_wave_task.sweep_direction = -1;
  s_wave_task.running = 1U;
  s_wave_task.sweep_running = 0U;

  return StepWave(1U);
}

static DAC8831_StatusTypeDef StartSweep(const uint16_t *wave_table,
                                        uint16_t table_size,
                                        uint32_t low_hz,
                                        uint32_t high_hz,
                                        uint32_t frequency_step_hz,
                                        uint16_t amplitude,
                                        uint32_t dwell_ms)
{
  DAC8831_StatusTypeDef status;

  if ((low_hz == 0U) ||
      (high_hz <= low_hz) ||
      (frequency_step_hz == 0U) ||
      (((uint64_t)table_size * (uint64_t)high_hz) >
       DAC8831_APP_SAMPLE_RATE_HZ) ||
      (dwell_ms == 0U))
  {
    return DAC8831_ERROR_PARAM;
  }

  status = StartWave(wave_table,
                     table_size,
                     low_hz,
                     amplitude);
  if (status != DAC8831_OK)
  {
    return status;
  }

  s_wave_task.low_hz = low_hz;
  s_wave_task.high_hz = high_hz;
  s_wave_task.frequency_step_hz = frequency_step_hz;
  s_wave_task.dwell_ms = dwell_ms;
  s_wave_task.last_sweep_tick = HAL_GetTick();
  s_wave_task.sweep_direction = 1;
  s_wave_task.sweep_running = 1U;

  return DAC8831_OK;
}

static DAC8831_StatusTypeDef StepWave(uint8_t force)
{
  uint32_t now_cycle;
  uint32_t elapsed_cycles;
  uint32_t samples_due;
  uint16_t output_code;
  DAC8831_StatusTypeDef status;

  if (s_wave_task.running == 0U)
  {
    return DAC8831_OK;
  }

  now_cycle = DAC8831_AppGetCycle();

  if (force != 0U)
  {
    samples_due = 1U;
    s_wave_task.last_sample_cycle = now_cycle;
  }
  else
  {
    elapsed_cycles = now_cycle - s_wave_task.last_sample_cycle;
    if (elapsed_cycles == 0U)
    {
      return s_dac8831_app_data.status;
    }

    s_wave_task.last_sample_cycle = now_cycle;

    /* 按 CPU 周期计算本次应该送出的波形点数 */
    s_wave_task.phase_accumulator +=
      (uint64_t)elapsed_cycles *
      (uint64_t)s_wave_task.size *
      (uint64_t)s_wave_task.frequency_hz;

    samples_due =
      (uint32_t)(s_wave_task.phase_accumulator / (uint64_t)SystemCoreClock);

    s_wave_task.phase_accumulator %= (uint64_t)SystemCoreClock;

    if (samples_due == 0U)
    {
      return s_dac8831_app_data.status;
    }

    if (samples_due > s_wave_task.size)
    {
      samples_due = s_wave_task.size;
    }
  }

  while (samples_due > 0U)
  {
    output_code = ScaleCode(s_wave_task.table[s_wave_task.index],
                            s_wave_task.amplitude);

    status = DAC8831_WriteRaw(&s_dac8831, output_code);
    s_dac8831_app_data.status = status;

    if (status != DAC8831_OK)
    {
      DAC8831_AppStop();
      return status;
    }

    s_dac8831_app_data.last_code = output_code;
    s_dac8831_app_data.last_target_mv =
      DAC8831_CodeToUnipolarMv(output_code, s_dac8831.cfg.vref_mv);

    s_wave_task.index++;
    if (s_wave_task.index >= s_wave_task.size)
    {
      s_wave_task.index = 0U;
    }

    samples_due--;
  }

  return DAC8831_OK;
}
static void UpdateSweep(void)
{
  uint32_t now_tick;
  uint32_t next_hz;

  if (s_wave_task.sweep_running == 0U)
  {
    return;
  }

  now_tick = HAL_GetTick();
  if ((now_tick - s_wave_task.last_sweep_tick) < s_wave_task.dwell_ms)
  {
    return;
  }
  s_wave_task.last_sweep_tick = now_tick;

  next_hz = s_wave_task.frequency_hz;
  if (s_wave_task.sweep_direction > 0)
  {
    if ((next_hz >= s_wave_task.high_hz) ||
        (s_wave_task.frequency_step_hz >=
         (s_wave_task.high_hz - next_hz)))
    {
      next_hz = s_wave_task.high_hz;
      s_wave_task.sweep_direction = -1;
    }
    else
    {
      next_hz += s_wave_task.frequency_step_hz;
    }
  }
  else
  {
    if ((next_hz <= s_wave_task.low_hz) ||
        (s_wave_task.frequency_step_hz >=
         (next_hz - s_wave_task.low_hz)))
    {
      next_hz = s_wave_task.low_hz;
      s_wave_task.sweep_direction = 1;
    }
    else
    {
      next_hz -= s_wave_task.frequency_step_hz;
    }
  }

  s_wave_task.frequency_hz = next_hz;
}

static uint16_t ScaleCode(uint16_t code, uint16_t amplitude)
{
  uint64_t value;

  value = ((uint64_t)code * (uint64_t)amplitude) +
          (DAC8831_MAX_CODE / 2U);

  return (uint16_t)(value / DAC8831_MAX_CODE);
}

static void DAC8831_AppTimebaseInit(void)
{
  SystemCoreClockUpdate();                       /* 更新系统主频变量 */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t DAC8831_AppGetCycle(void)
{
  return DWT->CYCCNT;
}
