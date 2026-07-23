/**
 * @file    DSP_Stream.h
 * @brief   Allocation-free RX/process/TX ping-pong buffer ownership.
 *
 * The module does not start ADC/DAC DMA itself.  ISR code only completes one
 * ownership handoff and reserves the next buffer; the main loop/RTOS task calls
 * DSP_Stream_ProcessOne().  Thus ADC DMA may fill frame A while the CPU processes
 * frame B and DAC DMA transmits the previously produced frame.
 */
#ifndef DSP_STREAM_H
#define DSP_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    DSP_STREAM_OK = 0,
    DSP_STREAM_IDLE,
    DSP_STREAM_ERROR_NULL,
    DSP_STREAM_ERROR_CONFIG,
    DSP_STREAM_ERROR_STATE,
    DSP_STREAM_ERROR_OVERRUN,
    DSP_STREAM_ERROR_UNDERRUN,
    DSP_STREAM_ERROR_PROCESS
} DSP_StreamStatus_t;

typedef enum {
    DSP_STREAM_BUFFER_FREE = 0,
    DSP_STREAM_BUFFER_DMA,
    DSP_STREAM_BUFFER_READY,
    DSP_STREAM_BUFFER_CPU
} DSP_StreamBufferState_t;

typedef DSP_StreamStatus_t (*DSP_StreamProcessFn)(const void *rx_frame,
                                                  void *tx_frame,
                                                  uint32_t frame_count,
                                                  void *user);

typedef struct {
    void *rx[2];
    void *tx[2];
    uint32_t rx_frame_bytes;
    uint32_t tx_frame_bytes;
    uint32_t frame_count;

    volatile uint8_t rx_state[2];
    volatile uint8_t tx_state[2];
    volatile uint8_t rx_dma_index;
    volatile uint8_t tx_dma_index;
    volatile uint32_t rx_ready_sequence[2];
    volatile uint32_t tx_ready_sequence[2];
    volatile uint32_t next_rx_sequence;
    volatile uint32_t next_tx_sequence;

    uint32_t rx_completed;
    uint32_t frames_processed;
    uint32_t tx_completed;
    uint32_t rx_overruns;
    uint32_t tx_underruns;
    uint32_t process_errors;
    uint8_t ready;
} DSP_Stream_t;

DSP_StreamStatus_t DSP_Stream_Init(DSP_Stream_t *stream,
                                   void *rx_buffer0,
                                   void *rx_buffer1,
                                   uint32_t rx_frame_bytes,
                                   void *tx_buffer0,
                                   void *tx_buffer1,
                                   uint32_t tx_frame_bytes,
                                   uint32_t frame_count);

/* ADC side: reserve before DMA, complete from the matching ISR/callback. */
DSP_StreamStatus_t DSP_Stream_RxBeginDma(DSP_Stream_t *stream,
                                         uint8_t *index,
                                         void **buffer);
DSP_StreamStatus_t DSP_Stream_RxDmaComplete(DSP_Stream_t *stream,
                                            uint8_t index);

/* DAC side: ProcessOne submits READY frames.  TxBeginDma reserves the oldest
 * ready frame and TxDmaComplete releases it after transmission. */
DSP_StreamStatus_t DSP_Stream_TxBeginDma(DSP_Stream_t *stream,
                                         uint8_t *index,
                                         void **buffer);
DSP_StreamStatus_t DSP_Stream_TxDmaComplete(DSP_Stream_t *stream,
                                            uint8_t index);

/* Process exactly one RX frame when both an RX-ready and TX-free buffer exist. */
DSP_StreamStatus_t DSP_Stream_ProcessOne(DSP_Stream_t *stream,
                                         DSP_StreamProcessFn process,
                                         void *user);

#ifdef __cplusplus
}
#endif

#endif /* DSP_STREAM_H */
