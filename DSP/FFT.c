#include "FFT.h"
#include <stdio.h>
#define pi 3.1415926
float FFT_Fs=20000.0f;
float f=1000.0f;
float FFT_input[FFT_length]={0};
float FFT_Buffer[FFT_length*2]={0};
float FFT_normalized_output[FFT_length / 2 + 1] = {0};
//DSP������⣬һ��Ҫ������������飬������һ��FFT_length*2��input�������
float FFT_output[FFT_length]={0};
float FFT_mag_max=0;
uint32_t mag_max_index=0;
volatile float FFT_Amplitude=0;
volatile float FFT_Frequency=0;


void FFT_Input(float arr[]) {
	for(int i=0;i<FFT_length;i++){
		FFT_input[i]=arr[i];
	}
}

void Sin_Simulation(){
	for(int i=0;i<FFT_length;i++){
		FFT_input[i] = sin(2.0f*pi*f*(float)i/FFT_Fs);
	}
};


//void FFT_Start() {
//	for (int i = 0; i < FFT_length; i++) {
//        FFT_Buffer[2*i] = FFT_input[i];   // ʵ��
//        FFT_Buffer[2*i+1] = 0.0f;      // �鲿
//    }
//	
//    arm_cfft_f32(&arm_cfft_sR_f32_len4096, FFT_Buffer, 0, 1);
//    
//    arm_cmplx_mag_f32(FFT_Buffer, FFT_output, FFT_length);

//}

void FFT_Start() {
    float win_sum = 0.0f;

	for (int i = 0; i < FFT_length; i++) {
        float win = 0.5f - 0.5f * cosf(2.0f * pi * (float)i / (float)(FFT_length - 1));
        win_sum += win;

        FFT_Buffer[2*i] = FFT_input[i] * win;
        FFT_Buffer[2*i+1] = 0.0f;
    }
	
    arm_cfft_f32(&arm_cfft_sR_f32_len512, FFT_Buffer, 0, 1);
    
    arm_cmplx_mag_f32(FFT_Buffer, FFT_output, FFT_length);

    for (int i = 0; i < FFT_length; i++) {
        FFT_output[i] = FFT_output[i] * (float)FFT_length / win_sum;
    }
}

void Process_FFT_mag(){
	FFT_Frequency=0;
	FFT_Amplitude=0;
	
	arm_max_f32(FFT_output,FFT_length/2,&FFT_mag_max,&mag_max_index);
	
	//���������ж������޳�ֱ�����������Ҫ��ֱ������Ҫע��
	if(mag_max_index==0){
		FFT_output[0]=0;
		arm_max_f32(FFT_output,FFT_length/2,&FFT_mag_max,&mag_max_index);
	}
	
	FFT_Frequency=(float)mag_max_index*FFT_Fs/(float)FFT_length;

	FFT_Amplitude=FFT_mag_max*2.0f/(float)FFT_length;
	printf("001:%.3f\r\n",FFT_Frequency);
	printf("002:%.3f\r\n",FFT_mag_max);
	printf("003:%.3f\r\n",FFT_Amplitude);
}

/**
 * ��ԭʼFFT�����׹�һ����ת��Ϊ������
 * ԭʼFFT_output���鱣�ֲ���
 * ��һ����ĵ����״洢��FFT_normalized_output��
 */
void Normalize_FFT_To_Single_Side(void)
{
    float scale = 1.0f / (float)FFT_length;
    int single_side_length = FFT_length / 2 + 1;

    for (int i = 0; i < single_side_length; i++)
    {
        if (i == 0)
        {
            FFT_normalized_output[i] = FFT_output[i] * scale;
        }
        else if (i == FFT_length / 2)
        {
            FFT_normalized_output[i] = FFT_output[i] * scale;
        }
        else
        {
            FFT_normalized_output[i] = FFT_output[i] * 2.0f * scale;
        }
    }
}

float FFT_AmpAtFreq(float data[], uint32_t len, float fs, float freq)
{
    float w = 2.0f * pi * freq / fs;
    float coeff = 2.0f * cosf(w);

    float q0 = 0.0f, q1 = 0.0f, q2 = 0.0f;
    float win_sum = 0.0f;

    for (uint32_t i = 0; i < len; i++)
    {
        float win = 0.5f - 0.5f * cosf(2.0f * pi * (float)i / (float)(len - 1));

        win_sum += win;

        q0 = data[i] * win + coeff * q1 - q2;
        q2 = q1;
        q1 = q0;
    }

    float power = q1 * q1 + q2 * q2 - coeff * q1 * q2;

    if (power < 0.0f)
    {
        power = 0.0f;
    }

    return 2.0f * sqrtf(power) / win_sum;
}

uint8_t FFT_GetHarmonics(float data[], uint32_t len, float fs, float f0,
                         float max_freq, float freq_out[], float amp_out[])
{
    uint8_t count = 0;

    if (f0 < 1.0f) return 0;

    for (int h = 1; h * f0 <= max_freq && count < FFT_MAX_HARMONICS; h++)
    {
        freq_out[count] = h * f0;
        amp_out[count] = FFT_AmpAtFreq(data, len, fs, h * f0);
        count++;
    }

    return count;
}

uint8_t FFT_GetHarmonics_FromFFT(float fs, float f0, float max_freq,
                                 float freq_out[], float amp_out[])
{
    uint8_t count = 0;
    float df = fs / (float)FFT_length;

    if (f0 < 1.0f) return 0;

    for (int h = 1; h * f0 <= max_freq && count < FFT_MAX_HARMONICS; h++)
    {
        float freq = h * f0;
        uint32_t k = (uint32_t)(freq / df + 0.5f);

        if (k == 0 || k >= FFT_length / 2) break;

        uint32_t k_best = k;
        float mag = FFT_output[k];

        if (k > 1 && FFT_output[k - 1] > mag)
        {
            mag = FFT_output[k - 1];
            k_best = k - 1;
        }

        if (k + 1 < FFT_length / 2 && FFT_output[k + 1] > mag)
        {
            mag = FFT_output[k + 1];
            k_best = k + 1;
        }

        freq_out[count] = (float)k_best * df;
        amp_out[count] = mag * 2.0f / (float)FFT_length;

        count++;
    }

    return count;
}

void FFT_Data_Print() {
	printf("\r\nRaw Results\r\n");
    for (int i = 0; i < FFT_length; i++) {
        printf("%.3f,", FFT_output[i]);
    }
	printf("\r\nFreq:%.3f\r\n",FFT_Frequency);
	printf("mag_max:%.3f\r\n",FFT_mag_max);
	printf("Amp:%.3f\r\n",FFT_Amplitude);

}

/* ==================================================================== */
/*  窗函数库实现                                                         */
/* ==================================================================== */
void Window_Generate(float *w, uint32_t n, Window_Type_t type,
                     float *cg, float *enbw)
{
    if (w == NULL || n == 0u) return;

    const float Nm1 = (n > 1u) ? (float)(n - 1u) : 1.0f;  /* 对称窗分母 */

    for (uint32_t i = 0; i < n; i++) {
        float a = 2.0f * pi * (float)i / Nm1;   /* 2*pi*i/(N-1) */
        float val;
        switch (type) {
        case WIN_HANN:
            val = 0.5f - 0.5f * cosf(a);
            break;
        case WIN_HAMMING:
            val = 0.54f - 0.46f * cosf(a);
            break;
        case WIN_BLACKMAN:
            val = 0.42f - 0.5f * cosf(a) + 0.08f * cosf(2.0f * a);
            break;
        case WIN_BLACKMAN_HARRIS:               /* 旁瓣 -92dB */
            val = 0.35875f - 0.48829f * cosf(a)
                + 0.14128f * cosf(2.0f * a) - 0.01168f * cosf(3.0f * a);
            break;
        case WIN_FLATTOP:                        /* Matlab flattopwin */
            val = 0.21557895f - 0.41663158f * cosf(a)
                + 0.277263158f * cosf(2.0f * a) - 0.083578947f * cosf(3.0f * a)
                + 0.006947368f * cosf(4.0f * a);
            break;
        case WIN_RECT:
        default:
            val = 1.0f;
            break;
        }
        w[i] = val;
    }

    if (cg != NULL || enbw != NULL) {
        float s1 = 0.0f, s2 = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            s1 += w[i];
            s2 += w[i] * w[i];
        }
        if (cg)   *cg   = s1 / (float)n;
        if (enbw) *enbw = (s1 > 0.0f) ? ((float)n * s2 / (s1 * s1)) : 1.0f;
    }
}

void Window_Apply(float *x, const float *w, uint32_t n)
{
    if (x == NULL || w == NULL) return;
    for (uint32_t i = 0; i < n; i++) {
        x[i] *= w[i];
    }
}
