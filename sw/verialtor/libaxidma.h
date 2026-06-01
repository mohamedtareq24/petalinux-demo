#ifndef LIBAXIDMA_H
#define LIBAXIDMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct axidma_dev;
struct axidma_video_frame;

typedef struct axidma_dev* axidma_dev_t;

typedef struct {
    int  len;
    int *data;
} array_t;

const array_t *axidma_get_dma_tx(axidma_dev_t dev);
const array_t *axidma_get_dma_rx(axidma_dev_t dev);

axidma_dev_t axidma_init();
void *axidma_malloc(axidma_dev_t dev, size_t size);
void axidma_free(axidma_dev_t dev, void *ptr, size_t size);
int axidma_twoway_transfer(axidma_dev_t dev,
                           int tx_channel,
                           void *tx_buf,
                           size_t tx_len,
                           struct axidma_video_frame *tx_frame,
                           int rx_channel,
                           void *rx_buf,
                           size_t rx_len,
                           struct axidma_video_frame *rx_frame,
                           bool wait);
void axidma_destroy(axidma_dev_t dev);

#ifdef __cplusplus
}
#endif

#endif