#include "DSP_ProMax.h"
#include <stdio.h>
#include <string.h>

#define DSP_PROMAX_QUALITY_HARMONICS  8U /* 质量分析统计到第8次谐波。 */
#define DSP_PROMAX_HANN_LEAK_BINS     2U /* Hann窗主瓣两侧计入的频点数。 */

static DSP_ProMaxResultTypeDef s_promax_result; /* 最近一次完整分析结果。 */

/* 一次完成常用时域测量、FFT主峰、谐波和信号质量分析。 */
DSP_ProMaxStatusTypeDef DSP_ProMax_Analyze(const float *data,
                                           uint32_t len,
                                           float fs_hz,
                                           DSP_ProMaxResultTypeDef *result)
{
  uint32_t peak_index;

  if ((data == NULL) || (result == NULL) || (len == 0U) || !(fs_hz > 0.0f))
  {
    return DSP_PROMAX_ERROR_PARAM;
  }

  memset(result, 0, sizeof(*result));
  Measure_TimeStats(data,
                    len,
                    &result->dc,
                    &result->rms,
                    &result->acrms,
                    &result->vpp);
  Measure_Waveform(data, len, fs_hz, &result->wave);
  result->time_valid = 1U;

  if (len != FFT_length)
  {
    s_promax_result = *result;
    return DSP_PROMAX_ERROR_LENGTH;
  }

  FFT_Fs = fs_hz;
  FFT_StartEx(data);
  Normalize_FFT_To_Single_Side();

  peak_index = Measure_FindPeak(FFT_normalized_output,
                                FFT_length,
                                fs_hz,
                                &result->peak);
  if ((peak_index != 0U) && (result->peak.amplitude > 0.0f))
  {
    result->harmonic_count = FFT_GetHarmonics_FromFFT(
        fs_hz,
        result->peak.freq,
        fs_hz * 0.5f,
        result->harmonic_freq,
        result->harmonic_amp);
    Measure_Quality(FFT_normalized_output,
                    FFT_length,
                    fs_hz,
                    DSP_PROMAX_QUALITY_HARMONICS,
                    DSP_PROMAX_HANN_LEAK_BINS,
                    &result->quality);
    result->fft_valid = 1U;
  }

  s_promax_result = *result;
  return DSP_PROMAX_OK;
}

/* 获取最近一次分析结果的只读指针。 */
const DSP_ProMaxResultTypeDef *DSP_ProMax_GetLastResult(void)
{
  return &s_promax_result;
}

/* 通过printf输出关键分析结果。 */
void DSP_ProMax_PrintResult(const DSP_ProMaxResultTypeDef *result)
{
  if (result == NULL)
  {
    result = &s_promax_result;
  }

  printf("DSP ProMax\r\n");
  printf("DC=%.4f RMS=%.4f AC_RMS=%.4f VPP=%.4f\r\n",
         result->dc,
         result->rms,
         result->acrms,
         result->vpp);
  printf("Wave freq=%.3fHz duty=%.3f rise=%.6fs fall=%.6fs\r\n",
         result->wave.freq,
         result->wave.duty,
         result->wave.rise_time,
         result->wave.fall_time);

  if (result->fft_valid != 0U)
  {
    printf("FFT peak=%.3fHz amp=%.4f bin=%.3f\r\n",
           result->peak.freq,
           result->peak.amplitude,
           result->peak.bin);
    printf("THD=%.4f THD_dB=%.2f SNR=%.2f SINAD=%.2f ENOB=%.2f\r\n",
           result->quality.thd,
           result->quality.thd_db,
           result->quality.snr_db,
           result->quality.sinad_db,
           result->quality.enob);
  }
}
