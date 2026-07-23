/**
 * @file    SoftPll.c
 * @brief   Generic software PLL controller implementation.
 */

#include "SoftPll.h"
#include <stddef.h>

#define SOFTPLL_DEFAULT_PHASE_KP              0.35f
#define SOFTPLL_DEFAULT_PHASE_KI              0.0f
#define SOFTPLL_DEFAULT_MAX_PHASE_STEP_DEG    20.0f
#define SOFTPLL_DEFAULT_MAX_INTEGRAL_DEG      2.0f
#define SOFTPLL_DEFAULT_PHASE_DEADBAND_DEG    6.0f
#define SOFTPLL_DEFAULT_FREQ_KP_HZ_PER_DEG    0.0f
#define SOFTPLL_DEFAULT_FREQ_KD               0.08f
#define SOFTPLL_DEFAULT_RATE_ALPHA            0.20f
#define SOFTPLL_DEFAULT_MAX_FREQ_STEP_HZ      0.010f
#define SOFTPLL_DEFAULT_FREQ_DEADBAND_HZ      0.006f
#define SOFTPLL_DEFAULT_HOLD_ERROR_DEG        30.0f
#define SOFTPLL_DEFAULT_MAX_FREQ_OFFSET_HZ    10.0f
#define SOFTPLL_DEFAULT_MIN_REFERENCE_AMP     0.05f
#define SOFTPLL_DEFAULT_MIN_FEEDBACK_AMP      0.03f

static float SoftPll_Abs(float x)
{
    return (x < 0.0f) ? -x : x;
}

static float SoftPll_Clamp(float x, float lo, float hi)
{
    if (x > hi) return hi;
    if (x < lo) return lo;
    return x;
}

static float SoftPll_LimitSym(float x, float limit)
{
    if (limit <= 0.0f) return x;
    return SoftPll_Clamp(x, -limit, limit);
}

float SoftPll_Wrap360Deg(float phase_deg)
{
    while (phase_deg >= 360.0f) phase_deg -= 360.0f;
    while (phase_deg < 0.0f) phase_deg += 360.0f;
    return phase_deg;
}

float SoftPll_Normalize180Deg(float phase_deg)
{
    while (phase_deg > 180.0f) phase_deg -= 360.0f;
    while (phase_deg <= -180.0f) phase_deg += 360.0f;
    return phase_deg;
}

float SoftPll_RadToDeg(float phase_rad)
{
    return phase_rad * (180.0f / SOFTPLL_PI);
}

float SoftPll_DegToRad(float phase_deg)
{
    return phase_deg * (SOFTPLL_PI / 180.0f);
}

void SoftPll_DefaultConfig(SoftPll_Config_t *cfg, float nominal_freq_hz)
{
    if (cfg == NULL) return;

    cfg->nominal_freq_hz = nominal_freq_hz;
    cfg->initial_phase_deg = 0.0f;
    cfg->freq_cal_hz = 0.0f;
    cfg->max_freq_offset_hz = SOFTPLL_DEFAULT_MAX_FREQ_OFFSET_HZ;

    cfg->min_reference_amp = SOFTPLL_DEFAULT_MIN_REFERENCE_AMP;
    cfg->min_feedback_amp = SOFTPLL_DEFAULT_MIN_FEEDBACK_AMP;

    cfg->phase_kp = SOFTPLL_DEFAULT_PHASE_KP;
    cfg->phase_ki = SOFTPLL_DEFAULT_PHASE_KI;
    cfg->max_phase_step_deg = SOFTPLL_DEFAULT_MAX_PHASE_STEP_DEG;
    cfg->max_integral_deg = SOFTPLL_DEFAULT_MAX_INTEGRAL_DEG;
    cfg->phase_deadband_deg = SOFTPLL_DEFAULT_PHASE_DEADBAND_DEG;

    cfg->freq_kp_hz_per_deg = SOFTPLL_DEFAULT_FREQ_KP_HZ_PER_DEG;
    cfg->freq_kd = SOFTPLL_DEFAULT_FREQ_KD;
    cfg->phase_rate_alpha = SOFTPLL_DEFAULT_RATE_ALPHA;
    cfg->max_freq_step_hz = SOFTPLL_DEFAULT_MAX_FREQ_STEP_HZ;
    cfg->freq_step_deadband_hz = SOFTPLL_DEFAULT_FREQ_DEADBAND_HZ;
    cfg->hold_error_deg = SOFTPLL_DEFAULT_HOLD_ERROR_DEG;

    cfg->freq_direction = 1.0f;
    cfg->phase_direction = 1.0f;
}

void SoftPll_Init(SoftPll_t *pll, const SoftPll_Config_t *cfg)
{
    if ((pll == NULL) || (cfg == NULL)) return;

    pll->cfg = *cfg;
    SoftPll_Reset(pll, cfg->initial_phase_deg, cfg->nominal_freq_hz);
}

void SoftPll_Reset(SoftPll_t *pll, float initial_phase_deg, float nominal_freq_hz)
{
    if (pll == NULL) return;

    pll->cfg.initial_phase_deg = initial_phase_deg;
    pll->cfg.nominal_freq_hz = nominal_freq_hz;

    pll->phase_cmd_deg = SoftPll_Wrap360Deg(initial_phase_deg);
    pll->freq_cmd_hz = nominal_freq_hz;
    pll->dds_freq_cmd_hz = nominal_freq_hz + pll->cfg.freq_cal_hz;
    pll->integral_deg = 0.0f;

    pll->error_deg = 0.0f;
    pll->last_error_deg = 0.0f;
    pll->delta_error_deg = 0.0f;
    pll->phase_rate_hz = 0.0f;
    pll->freq_step_hz = 0.0f;
    pll->phase_step_deg = 0.0f;

    pll->ready = 1U;
    pll->last_error_valid = 0U;
    pll->feedback_valid = 0U;
}

uint8_t SoftPll_UpdatePhase(SoftPll_t *pll,
                            const SoftPll_PhaseInput_t *input,
                            SoftPll_PhaseUnit_t unit)
{
    float reference_deg;
    float feedback_deg;
    float target_offset_deg;
    float fixed_cal_deg;
    float target_deg;
    float phase_delta_deg;
    float phase_rate_hz;
    float freq_step_hz;
    float freq_min_hz;
    float freq_max_hz;
    float phase_step_deg;

    if ((pll == NULL) || (input == NULL) || (pll->ready == 0U) || (input->dt_s <= 0.0f)) {
        return 0U;
    }

    pll->freq_step_hz = 0.0f;
    pll->phase_step_deg = 0.0f;

    if ((input->reference_amp < pll->cfg.min_reference_amp) ||
        (input->feedback_amp < pll->cfg.min_feedback_amp)) {
        pll->feedback_valid = 0U;
        pll->last_error_valid = 0U;
        return 0U;
    }
    pll->feedback_valid = 1U;

    if (unit == SOFTPLL_UNIT_RAD) {
        reference_deg = SoftPll_RadToDeg(input->reference_phase);
        feedback_deg = SoftPll_RadToDeg(input->feedback_phase);
        target_offset_deg = SoftPll_RadToDeg(input->target_offset);
        fixed_cal_deg = SoftPll_RadToDeg(input->fixed_cal);
    } else {
        reference_deg = input->reference_phase;
        feedback_deg = input->feedback_phase;
        target_offset_deg = input->target_offset;
        fixed_cal_deg = input->fixed_cal;
    }

    target_deg = SoftPll_Wrap360Deg(reference_deg + target_offset_deg + fixed_cal_deg);
    pll->error_deg = SoftPll_Normalize180Deg(target_deg - feedback_deg);

    if (pll->last_error_valid != 0U) {
        phase_delta_deg = SoftPll_Normalize180Deg(pll->error_deg - pll->last_error_deg);
    } else {
        phase_delta_deg = 0.0f;
        pll->last_error_valid = 1U;
    }

    pll->last_error_deg = pll->error_deg;
    pll->delta_error_deg = phase_delta_deg;

    /* Convert phase drift into frequency error: 360 deg/s equals 1 Hz. */
    phase_rate_hz = phase_delta_deg / (360.0f * input->dt_s);
    pll->phase_rate_hz += (phase_rate_hz - pll->phase_rate_hz) * pll->cfg.phase_rate_alpha;

    freq_step_hz = ((pll->error_deg * pll->cfg.freq_kp_hz_per_deg) +
                    (pll->phase_rate_hz * pll->cfg.freq_kd)) * pll->cfg.freq_direction;
    freq_step_hz = SoftPll_LimitSym(freq_step_hz, pll->cfg.max_freq_step_hz);

    if ((SoftPll_Abs(pll->error_deg) < pll->cfg.hold_error_deg) &&
        (SoftPll_Abs(freq_step_hz) < pll->cfg.freq_step_deadband_hz)) {
        freq_step_hz = 0.0f;
    }

    pll->freq_step_hz = freq_step_hz;
    pll->freq_cmd_hz += freq_step_hz;

    if (pll->cfg.max_freq_offset_hz > 0.0f) {
        freq_min_hz = pll->cfg.nominal_freq_hz - pll->cfg.max_freq_offset_hz;
        freq_max_hz = pll->cfg.nominal_freq_hz + pll->cfg.max_freq_offset_hz;
        pll->freq_cmd_hz = SoftPll_Clamp(pll->freq_cmd_hz, freq_min_hz, freq_max_hz);
    }
    pll->dds_freq_cmd_hz = pll->freq_cmd_hz + pll->cfg.freq_cal_hz;

    if (SoftPll_Abs(pll->error_deg) < pll->cfg.phase_deadband_deg) {
        pll->phase_step_deg = 0.0f;
        return 1U;
    }

    pll->integral_deg += pll->error_deg * pll->cfg.phase_ki;
    pll->integral_deg = SoftPll_LimitSym(pll->integral_deg, pll->cfg.max_integral_deg);

    phase_step_deg = ((pll->error_deg * pll->cfg.phase_kp) + pll->integral_deg) * pll->cfg.phase_direction;
    phase_step_deg = SoftPll_LimitSym(phase_step_deg, pll->cfg.max_phase_step_deg);

    pll->phase_cmd_deg = SoftPll_Wrap360Deg(pll->phase_cmd_deg + phase_step_deg);
    pll->phase_step_deg = phase_step_deg;
    return 1U;
}

uint8_t SoftPll_UpdateIq(SoftPll_t *pll,
                         const IQ_Result_t *reference,
                         const IQ_Result_t *feedback,
                         float dt_s,
                         float target_offset_deg,
                         float fixed_cal_deg)
{
    SoftPll_PhaseInput_t input;

    if ((reference == NULL) || (feedback == NULL)) return 0U;

    input.reference_phase = reference->phase;
    input.feedback_phase = feedback->phase;
    input.dt_s = dt_s;
    input.reference_amp = reference->amplitude;
    input.feedback_amp = feedback->amplitude;
    input.target_offset = SoftPll_DegToRad(target_offset_deg);
    input.fixed_cal = SoftPll_DegToRad(fixed_cal_deg);

    return SoftPll_UpdatePhase(pll, &input, SOFTPLL_UNIT_RAD);
}
