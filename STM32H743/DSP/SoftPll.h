/**
 * @file    SoftPll.h
 * @brief   Generic software PLL controller for embedded DSP applications.
 *
 * The module does not touch ADC, DDS, timer, or HAL state.  It only compares
 * a reference phase and a feedback phase, then produces small frequency and
 * phase correction commands.  The caller decides how to apply those commands
 * to a DDS/NCO/PWM/timer.
 */

/* 软件锁相环模块头文件保护宏。 */
#ifndef __SOFT_PLL_H
#define __SOFT_PLL_H /* 软件锁相环模块头文件保护宏。 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "IQ.h"

/* 单精度圆周率，允许工程在外部覆盖。 */
#ifndef SOFTPLL_PI
#define SOFTPLL_PI 3.14159265358979323846f /* 单精度圆周率。 */
#endif

/* 公共接口使用的相位单位。 */
typedef enum {
    SOFTPLL_UNIT_RAD = 0,
    SOFTPLL_UNIT_DEG = 1
} SoftPll_PhaseUnit_t;

/* 单次锁相环更新所需的参考与反馈输入。 */
typedef struct {
    float reference_phase;  /**< Reference/input phase, rad or deg. */
    float feedback_phase;   /**< Feedback/output phase, rad or deg. */
    float dt_s;             /**< Time since previous update, seconds. */
    float reference_amp;    /**< Reference amplitude; use 1.0f if unknown. */
    float feedback_amp;     /**< Feedback amplitude; use 1.0f if unknown. */
    float target_offset;    /**< Desired feedback-reference offset, rad or deg. */
    float fixed_cal;        /**< Fixed phase calibration, rad or deg. */
} SoftPll_PhaseInput_t;

/* 软件锁相环可调参数，内部相位统一使用度。 */
typedef struct {
    float nominal_freq_hz;       /**< Locked nominal frequency. */
    float initial_phase_deg;     /**< Initial phase command. */
    float freq_cal_hz;           /**< Fixed frequency calibration added to command. */
    float max_freq_offset_hz;    /**< Clamp around nominal frequency. 0 disables clamp. */

    float min_reference_amp;     /**< Reject update when reference amplitude is too small. */
    float min_feedback_amp;      /**< Reject update when feedback amplitude is too small. */

    float phase_kp;              /**< Phase-command proportional gain, deg/deg. */
    float phase_ki;              /**< Phase-command integral gain, deg/update/deg. */
    float max_phase_step_deg;    /**< Max phase command change per update. */
    float max_integral_deg;      /**< Integral clamp. */
    float phase_deadband_deg;    /**< No phase command update inside this error band. */

    float freq_kp_hz_per_deg;    /**< Direct phase-error to frequency gain. */
    float freq_kd;               /**< Phase-drift to frequency gain. */
    float phase_rate_alpha;      /**< Low-pass factor for phase drift, 0..1. */
    float max_freq_step_hz;      /**< Max frequency command change per update. */
    float freq_step_deadband_hz; /**< Small frequency step suppression. */
    float hold_error_deg;        /**< Apply freq deadband only when abs(error) is below this. */

    float freq_direction;        /**< Set to -1 if frequency correction direction is inverted. */
    float phase_direction;       /**< Set to -1 if phase correction direction is inverted. */
} SoftPll_Config_t;

/* 软件锁相环运行状态与最近一次诊断结果。 */
typedef struct {
    SoftPll_Config_t cfg;

    float phase_cmd_deg;       /**< Current phase command, 0..360 deg. */
    float freq_cmd_hz;         /**< Current frequency command before fixed calibration. */
    float dds_freq_cmd_hz;     /**< Current frequency command after fixed calibration. */
    float integral_deg;        /**< Phase integral accumulator. */

    float error_deg;           /**< Last normalized phase error, -180..180 deg. */
    float last_error_deg;      /**< Previous normalized phase error. */
    float delta_error_deg;     /**< Last error delta, -180..180 deg. */
    float phase_rate_hz;       /**< Low-passed phase drift converted to Hz. */
    float freq_step_hz;        /**< Last frequency correction step. */
    float phase_step_deg;      /**< Last phase correction step. */

    uint8_t ready;             /**< Set after init/reset. */
    uint8_t last_error_valid;  /**< Internal derivative validity flag. */
    uint8_t feedback_valid;    /**< Last update accepted amplitudes. */
} SoftPll_t;

/* 填充适合 DDS 频率和相位微调的保守默认配置。 */
void SoftPll_DefaultConfig(SoftPll_Config_t *cfg, float nominal_freq_hz);

/* 按配置初始化锁相环状态。 */
void SoftPll_Init(SoftPll_t *pll, const SoftPll_Config_t *cfg);

/* 保留配置并复位频率与相位指令状态。 */
void SoftPll_Reset(SoftPll_t *pll, float initial_phase_deg, float nominal_freq_hz);

/* 使用弧度或度相位更新锁相环，接受更新时返回 1。 */
uint8_t SoftPll_UpdatePhase(SoftPll_t *pll,
                            const SoftPll_PhaseInput_t *input,
                            SoftPll_PhaseUnit_t unit);

/* 使用 IQ 解调结果更新锁相环，目标和校准相位单位为度。 */
uint8_t SoftPll_UpdateIq(SoftPll_t *pll,
                         const IQ_Result_t *reference,
                         const IQ_Result_t *feedback,
                         float dt_s,
                         float target_offset_deg,
                         float fixed_cal_deg);

/* 将角度折返到 [0, 360) 度。 */
float SoftPll_Wrap360Deg(float phase_deg);
/* 将角度归一化到 (-180, 180] 度。 */
float SoftPll_Normalize180Deg(float phase_deg);
/* 将弧度转换为度。 */
float SoftPll_RadToDeg(float phase_rad);
/* 将度转换为弧度。 */
float SoftPll_DegToRad(float phase_deg);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_PLL_H */
