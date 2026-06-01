#include "libaxidma.h"

#include "Vfir_top.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern Vfir_top* top;
extern void tick();

struct axidma_dev {
    std::uint32_t reserved;
};

axidma_dev_t axidma_init()
{
    return static_cast<axidma_dev_t>(std::calloc(1, sizeof(struct axidma_dev)));
}

void *axidma_malloc(axidma_dev_t dev, size_t size)
{
    (void)dev;
    if (size == 0) {
        return nullptr;
    }
    return std::malloc(size);
}

void axidma_free(axidma_dev_t dev, void *ptr, size_t size)
{
    (void)dev;
    (void)size;
    std::free(ptr);
}

int axidma_twoway_transfer(axidma_dev_t dev,
                           int tx_channel,
                           void *tx_buf,
                           size_t tx_len,
                           struct axidma_video_frame *tx_frame,
                           int rx_channel,
                           void *rx_buf,
                           size_t rx_len,
                           struct axidma_video_frame *rx_frame,
                           bool wait)
{
    (void)dev;
    (void)tx_channel;
    (void)tx_frame;
    (void)rx_channel;
    (void)rx_frame;
    (void)wait;

    if (top == nullptr || tx_buf == nullptr || rx_buf == nullptr) {
        return -1;
    }

    const size_t tx_words = tx_len / sizeof(std::uint32_t);
    const size_t rx_words = rx_len / sizeof(std::uint32_t);
    const size_t transfer_words = std::min(tx_words, rx_words);

    if (transfer_words == 0) {
        return 0;
    }

    const std::uint32_t *input = static_cast<const std::uint32_t *>(tx_buf);
    std::uint32_t *output = static_cast<std::uint32_t *>(rx_buf);

    top->s_axis_tvalid = 0;
    top->s_axis_tdata = 0;
    top->s_axis_tlast = 0;
    top->m_axis_tready = 1;

    size_t accepted = 0;
    size_t received = 0;
    size_t cycles = 0;
    size_t backpressure_cycles = 0;
    size_t drain_cycles_remaining = 0;
    const size_t kDrainCycles = 256U;
    const size_t max_cycles = transfer_words * 64U + 1024U + kDrainCycles;
    const size_t kBackpressureStart = 4;
    const size_t kBackpressureLength = 10;

    while (accepted < transfer_words || received < transfer_words || drain_cycles_remaining > 0) {
        if (accepted < transfer_words) {
            top->s_axis_tvalid = 1;
            top->s_axis_tdata = input[accepted];
            top->s_axis_tlast = (accepted + 1U == transfer_words) ? 1 : 0;
        } else {
            top->s_axis_tvalid = 0;
            top->s_axis_tdata = 0;
            top->s_axis_tlast = 0;
        }

        top->m_axis_tready = (backpressure_cycles == 0) ? 1 : 0;
        tick();

        if (top->s_axis_tvalid && top->s_axis_tready) {
            ++accepted;
            if (accepted == transfer_words) {
                drain_cycles_remaining = kDrainCycles;
            }
        }

        if (top->m_axis_tvalid && top->m_axis_tready && received < transfer_words) {
            output[received] = static_cast<std::uint32_t>(top->m_axis_tdata);
            ++received;
            if (received == kBackpressureStart && backpressure_cycles == 0) {
                backpressure_cycles = kBackpressureLength;
                top->m_axis_tready = 0;
            }
        }

        if (accepted >= transfer_words && drain_cycles_remaining > 0) {
            --drain_cycles_remaining;
        }

        if (backpressure_cycles > 0) {
            --backpressure_cycles;
        }

        if (++cycles > max_cycles) {
            top->s_axis_tvalid = 0;
            top->s_axis_tlast = 0;
            top->m_axis_tready = 0;
            std::fprintf(stderr,
                         "mock_axidma timeout: accepted=%zu received=%zu target=%zu cycles=%zu\n",
                         accepted,
                         received,
                         transfer_words,
                         cycles);
            return -1;
        }
    }

    top->s_axis_tvalid = 0;
    top->s_axis_tlast = 0;
    top->m_axis_tready = 0;

    return 0;
}

void axidma_destroy(axidma_dev_t dev)
{
    std::free(dev);
}

const array_t *axidma_get_dma_tx(axidma_dev_t dev)
{
    static int      tx_data[]  = {0};
    static array_t  tx_chans   = {1, tx_data};
    (void)dev;
    return &tx_chans;
}

const array_t *axidma_get_dma_rx(axidma_dev_t dev)
{
    static int      rx_data[]  = {1};
    static array_t  rx_chans   = {1, rx_data};
    (void)dev;
    return &rx_chans;
}