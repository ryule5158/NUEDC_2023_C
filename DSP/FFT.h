#ifndef FFT_H
#define FFT_H

#include "main.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define FFT_length 512U
#define FFT_MAX_HARMONICS 12U

extern float FFT_Fs;
extern float FFT_input[FFT_length];
extern float FFT_Buffer[FFT_length * 2U];
extern float FFT_output[FFT_length];
extern float FFT_mag_max;
extern uint32_t mag_max_index;
extern volatile float FFT_Amplitude;
extern volatile float FFT_Frequency;

void FFT_Input(float arr[]);
void Sin_Simulation(void);
void FFT_Start(void);
void Process_FFT_mag(void);
void Normalize_FFT_To_Single_Side(void);
void FFT_Data_Print(void);
float FFT_AmpAtFreq(float data[], uint32_t len, float fs, float freq);
uint8_t FFT_GetHarmonics(float data[], uint32_t len, float fs, float f0,
                         float max_freq, float freq_out[], float amp_out[]);
uint8_t FFT_GetHarmonics_FromFFT(float fs, float f0, float max_freq,
                                 float freq_out[], float amp_out[]);

typedef enum {
    WIN_RECT = 0,
    WIN_HANN,
    WIN_HAMMING,
    WIN_BLACKMAN,
    WIN_BLACKMAN_HARRIS,
    WIN_FLATTOP
} Window_Type_t;

void Window_Generate(float *w, uint32_t n, Window_Type_t type,
                     float *cg, float *enbw);
void Window_Apply(float *x, const float *w, uint32_t n);

#endif /* FFT_H */
