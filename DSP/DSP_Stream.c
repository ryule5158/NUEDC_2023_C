/** @file DSP_Stream.c */
#include "DSP_Stream.h"
#include <stddef.h>
#include <string.h>

#define DSP_STREAM_NO_INDEX 0xFFu

static void DSP_StreamBarrier(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __sync_synchronize();
#else
    volatile uint32_t barrier = 0U;
    (void)barrier;
#endif
}

DSP_StreamStatus_t DSP_Stream_Init(DSP_Stream_t *stream,
                                   void *rx_buffer0,
                                   void *rx_buffer1,
                                   uint32_t rx_frame_bytes,
                                   void *tx_buffer0,
                                   void *tx_buffer1,
                                   uint32_t tx_frame_bytes,
                                   uint32_t frame_count)
{
    if (stream == NULL || rx_buffer0 == NULL || rx_buffer1 == NULL ||
        tx_buffer0 == NULL || tx_buffer1 == NULL) {
        return DSP_STREAM_ERROR_NULL;
    }
    if (rx_frame_bytes == 0U || tx_frame_bytes == 0U || frame_count == 0U ||
        rx_buffer0 == rx_buffer1 || tx_buffer0 == tx_buffer1) {
        return DSP_STREAM_ERROR_CONFIG;
    }

    memset(stream, 0, sizeof(*stream));
    stream->rx[0] = rx_buffer0;
    stream->rx[1] = rx_buffer1;
    stream->tx[0] = tx_buffer0;
    stream->tx[1] = tx_buffer1;
    stream->rx_frame_bytes = rx_frame_bytes;
    stream->tx_frame_bytes = tx_frame_bytes;
    stream->frame_count = frame_count;
    stream->rx_dma_index = DSP_STREAM_NO_INDEX;
    stream->tx_dma_index = DSP_STREAM_NO_INDEX;
    stream->next_rx_sequence = 1U;
    stream->next_tx_sequence = 1U;
    stream->ready = 1U;
    return DSP_STREAM_OK;
}

DSP_StreamStatus_t DSP_Stream_RxBeginDma(DSP_Stream_t *stream,
                                         uint8_t *index,
                                         void **buffer)
{
    uint8_t selected = DSP_STREAM_NO_INDEX;

    if (stream == NULL || index == NULL || buffer == NULL) return DSP_STREAM_ERROR_NULL;
    if (stream->ready == 0U) return DSP_STREAM_ERROR_CONFIG;
    if (stream->rx_dma_index != DSP_STREAM_NO_INDEX) return DSP_STREAM_ERROR_STATE;
    for (uint8_t i = 0U; i < 2U; ++i) {
        if (stream->rx_state[i] == DSP_STREAM_BUFFER_FREE) {
            selected = i;
            break;
        }
    }
    if (selected == DSP_STREAM_NO_INDEX) {
        stream->rx_overruns++;
        return DSP_STREAM_ERROR_OVERRUN;
    }
    stream->rx_state[selected] = DSP_STREAM_BUFFER_DMA;
    stream->rx_dma_index = selected;
    DSP_StreamBarrier();
    *index = selected;
    *buffer = stream->rx[selected];
    return DSP_STREAM_OK;
}

DSP_StreamStatus_t DSP_Stream_RxDmaComplete(DSP_Stream_t *stream,
                                            uint8_t index)
{
    if (stream == NULL) return DSP_STREAM_ERROR_NULL;
    if (stream->ready == 0U || index >= 2U || stream->rx_dma_index != index ||
        stream->rx_state[index] != DSP_STREAM_BUFFER_DMA) {
        if (stream->ready != 0U) stream->rx_overruns++;
        return DSP_STREAM_ERROR_STATE;
    }
    DSP_StreamBarrier();
    stream->rx_ready_sequence[index] = stream->next_rx_sequence++;
    stream->rx_state[index] = DSP_STREAM_BUFFER_READY;
    stream->rx_dma_index = DSP_STREAM_NO_INDEX;
    stream->rx_completed++;
    DSP_StreamBarrier();
    return DSP_STREAM_OK;
}

DSP_StreamStatus_t DSP_Stream_TxBeginDma(DSP_Stream_t *stream,
                                         uint8_t *index,
                                         void **buffer)
{
    uint8_t selected = DSP_STREAM_NO_INDEX;

    if (stream == NULL || index == NULL || buffer == NULL) return DSP_STREAM_ERROR_NULL;
    if (stream->ready == 0U) return DSP_STREAM_ERROR_CONFIG;
    if (stream->tx_dma_index != DSP_STREAM_NO_INDEX) return DSP_STREAM_ERROR_STATE;
    DSP_StreamBarrier();
    for (uint8_t i = 0U; i < 2U; ++i) {
        if (stream->tx_state[i] == DSP_STREAM_BUFFER_READY) {
            if (selected == DSP_STREAM_NO_INDEX ||
                (int32_t)(stream->tx_ready_sequence[i] -
                          stream->tx_ready_sequence[selected]) < 0) {
                selected = i;
            }
        }
    }
    if (selected == DSP_STREAM_NO_INDEX) {
        stream->tx_underruns++;
        return DSP_STREAM_ERROR_UNDERRUN;
    }
    stream->tx_state[selected] = DSP_STREAM_BUFFER_DMA;
    stream->tx_dma_index = selected;
    DSP_StreamBarrier();
    *index = selected;
    *buffer = stream->tx[selected];
    return DSP_STREAM_OK;
}

DSP_StreamStatus_t DSP_Stream_TxDmaComplete(DSP_Stream_t *stream,
                                            uint8_t index)
{
    if (stream == NULL) return DSP_STREAM_ERROR_NULL;
    if (stream->ready == 0U || index >= 2U || stream->tx_dma_index != index ||
        stream->tx_state[index] != DSP_STREAM_BUFFER_DMA) {
        return DSP_STREAM_ERROR_STATE;
    }
    DSP_StreamBarrier();
    stream->tx_state[index] = DSP_STREAM_BUFFER_FREE;
    stream->tx_dma_index = DSP_STREAM_NO_INDEX;
    stream->tx_completed++;
    DSP_StreamBarrier();
    return DSP_STREAM_OK;
}

DSP_StreamStatus_t DSP_Stream_ProcessOne(DSP_Stream_t *stream,
                                         DSP_StreamProcessFn process,
                                         void *user)
{
    uint8_t rx_index = DSP_STREAM_NO_INDEX;
    uint8_t tx_index = DSP_STREAM_NO_INDEX;
    DSP_StreamStatus_t status;

    if (stream == NULL || process == NULL) return DSP_STREAM_ERROR_NULL;
    if (stream->ready == 0U) return DSP_STREAM_ERROR_CONFIG;
    DSP_StreamBarrier();
    for (uint8_t i = 0U; i < 2U; ++i) {
        if (stream->rx_state[i] == DSP_STREAM_BUFFER_READY) {
            if (rx_index == DSP_STREAM_NO_INDEX ||
                (int32_t)(stream->rx_ready_sequence[i] -
                          stream->rx_ready_sequence[rx_index]) < 0) {
                rx_index = i;
            }
        }
    }
    for (uint8_t i = 0U; i < 2U; ++i) {
        if (stream->tx_state[i] == DSP_STREAM_BUFFER_FREE) {
            tx_index = i;
            break;
        }
    }
    if (rx_index == DSP_STREAM_NO_INDEX || tx_index == DSP_STREAM_NO_INDEX) {
        return DSP_STREAM_IDLE;
    }

    stream->rx_state[rx_index] = DSP_STREAM_BUFFER_CPU;
    stream->tx_state[tx_index] = DSP_STREAM_BUFFER_CPU;
    DSP_StreamBarrier();
    status = process(stream->rx[rx_index], stream->tx[tx_index],
                     stream->frame_count, user);
    DSP_StreamBarrier();
    stream->rx_state[rx_index] = DSP_STREAM_BUFFER_FREE;
    if (status == DSP_STREAM_OK) {
        stream->tx_ready_sequence[tx_index] = stream->next_tx_sequence++;
        stream->tx_state[tx_index] = DSP_STREAM_BUFFER_READY;
        stream->frames_processed++;
    } else {
        stream->tx_state[tx_index] = DSP_STREAM_BUFFER_FREE;
        stream->process_errors++;
    }
    DSP_StreamBarrier();
    return (status == DSP_STREAM_OK) ? DSP_STREAM_OK : DSP_STREAM_ERROR_PROCESS;
}
