/**
 * @file    SoftPll.h
 * @brief   Generic software PLL controller for embedded DSP applications.
 *
 * The module does not touch ADC, DDS, timer, or HAL state.  It only compares
 * a reference phase and a feedback phase, then produces small frequency and
 * phase correction commands.  The caller decides how to apply those commands
 * to a DDS/NCO/PWM/timer.
 */

#ifndef __SOFT_PLL_H
#define __SOFT_PLL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "IQ.h"

#ifndef SOFTPLL_PI
#define SOFTPLL_PI 3.14159265358979323846f
#endif

/** Phase unit used by the public API. */
typedef enum {
    SOFTPLL_UNIT_RAD = 0,
    SOFTPLL_UNIT_DEG = 1
} SoftPll_PhaseUnit_t;

/** Input phase pair for one PLL update. */
typedef struct {
    float reference_phase;  /**< Reference/input phase, rad or deg. */
    float feedback_phase;   /**< Feedback/output phase, rad or deg. */
    float dt_s;             /**< Time since previous update, seconds. */
    float reference_amp;    /**< Reference amplitude; use 1.0f if unknown. */
    float feedback_amp;     /**< Feedback amplitude; use 1.0f if unknown. */
    float target_offset;    /**< Desired feedback-reference offset, rad or deg. */
    float fixed_cal;        /**< Fixed phase calibration, rad or deg. */
} SoftPll_PhaseInput_t;

/** Tunable PLL configuration.  All phase fields use degrees internally. */
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

/** Runtime state and last computed diagnostics. */
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

/** Fill a conservative default config suitable for DDS phase/frequency trim. */
void SoftPll_DefaultConfig(SoftPll_Config_t *cfg, float nominal_freq_hz);

/** Initialize PLL state from config. */
void SoftPll_Init(SoftPll_t *pll, const SoftPll_Config_t *cfg);

/** Reset phase/frequency command state while keeping the existing config. */
void SoftPll_Reset(SoftPll_t *pll, float initial_phase_deg, float nominal_freq_hz);

/** Update using phases in radians or degrees.  Returns 1 when an update was accepted. */
uint8_t SoftPll_UpdatePhase(SoftPll_t *pll,
                            const SoftPll_PhaseInput_t *input,
                            SoftPll_PhaseUnit_t unit);

/** Update directly from IQ demodulation results.  target/calibration are in degrees. */
uint8_t SoftPll_UpdateIq(SoftPll_t *pll,
                         const IQ_Result_t *reference,
                         const IQ_Result_t *feedback,
                         float dt_s,
                         float target_offset_deg,
                         float fixed_cal_deg);

/** Utility helpers. */
float SoftPll_Wrap360Deg(float phase_deg);
float SoftPll_Normalize180Deg(float phase_deg);
float SoftPll_RadToDeg(float phase_rad);
float SoftPll_DegToRad(float phase_deg);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_PLL_H */
