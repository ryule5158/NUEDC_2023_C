/**
 * @file    ModelFit.h
 * @brief   Unknown RLC model fitting from measured frequency response.
 */
#ifndef __MODEL_FIT_H
#define __MODEL_FIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    MODEL_FILTER_INVALID = 255
} ModelFit_FilterType_t;

typedef struct {
    /*
     * Normalized continuous model:
     *
     *            c0 + c1*(j*x) + c2*(j*x)^2
     * H(jx) = ---------------------------------
     *            1  + d1*(j*x) + d2*(j*x)^2
     *
     * x = f_hz / norm_hz.
     */
    float c0;
    float c1;
    float c2;
    float d1;
    float d2;
    float norm_hz;
    float fit_rms;
    uint8_t type;
} ModelFit_SecondOrder_t;

typedef struct {
    float real;
    float imag;
} ModelFit_Complex_t;

int ModelFit_FitSecondOrder(const float *freq_hz,
                            const float *gain_mag,
                            const float *phase_rad,
                            uint32_t n,
                            float norm_hz,
                            ModelFit_SecondOrder_t *model);

ModelFit_Complex_t ModelFit_EvalComplex(const ModelFit_SecondOrder_t *model,
                                        float freq_hz);

float ModelFit_EvalMag(const ModelFit_SecondOrder_t *model, float freq_hz);


#ifdef __cplusplus
}
#endif

#endif /* __MODEL_FIT_H */
