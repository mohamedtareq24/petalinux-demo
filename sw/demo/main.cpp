// main.cpp — AXI-DMA FIR filter verification
//
// DMA handling mirrors the pattern of axidma_transfer.c (bmperez/jaewonch):
//   • axidma_get_dma_tx / axidma_get_dma_rx  — enumerate available channels
//   • struct dma_transfer                     — groups transfer parameters
//   • transfer_and_verify()                   — alloc → transfer → verify → free
//   • main()                                  — goto-chain cleanup
//
// This file has no #ifdef guards; all platform differences are in the backends:
//   sim target :  sim_backend.cpp + mock_axidma.cpp  (Verilator)
//   hw  target :  hw_backend.cpp  + libaxidma.so     (PetaLinux)

#include "backend.h"
#include "libaxidma.h"
#include "axilite_ctrl.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Test parameters
// ---------------------------------------------------------------------------
static constexpr int kDefaultSampleCount = 32;
static constexpr int kPipelineLatency   = 0;

// ---------------------------------------------------------------------------
// Transfer descriptor — mirrors struct dma_transfer in axidma_transfer.c
// ---------------------------------------------------------------------------
struct dma_transfer {
    int   input_channel;
    int   input_size;      // bytes
    void *input_buf;
    int   output_channel;
    int   output_size;     // bytes
    void *output_buf;
};

static void dump_buffers(const std::uint32_t *tx,
                         const std::uint32_t *rx,
                         int sample_count)
{
    std::printf("idx\tinput\texpected\tactual\n");
    for (int i = 0; i < sample_count; ++i) {
        int src_idx = i + kPipelineLatency;
        const std::uint32_t expected = tx[src_idx];

        std::printf("%d\t0x%08X\t0x%08X\t0x%08X%s\n",
                    i,
                    tx[i],
                    expected,
                    rx[i],
                    (rx[i] == expected) ? "" : "  <==");
    }
}

// ---------------------------------------------------------------------------
// transfer_and_verify — mirrors transfer_file() in axidma_transfer.c
//
// Allocates both buffers, fills input with a ramp, performs the two-way DMA
// transfer, verifies output against the expected pipeline-shifted ramp, then
// frees both buffers.
//
// Returns 0 on success, -1 on any error.
// ---------------------------------------------------------------------------
static int transfer_and_verify(axidma_dev_t dev, struct dma_transfer *trans, int sample_count)
{
    int rc = 0;

    trans->input_buf = axidma_malloc(dev, trans->input_size);
    if (trans->input_buf == nullptr) {
        std::fprintf(stderr, "Error: failed to allocate input DMA buffer.\n");
        return -1;
    }

    trans->output_buf = axidma_malloc(dev, trans->output_size);
    if (trans->output_buf == nullptr) {
        std::fprintf(stderr, "Error: failed to allocate output DMA buffer.\n");
        axidma_free(dev, trans->input_buf, trans->input_size);
        return -1;
    }

    // Fill input with a simple ramp: sample[i] = i
    auto *tx = static_cast<std::uint32_t *>(trans->input_buf);
    for (int i = 0; i < sample_count; ++i)
        tx[i] = static_cast<std::uint32_t>(i);

    std::memset(trans->output_buf, 0, trans->output_size);

    rc = axidma_twoway_transfer(dev,
                                trans->input_channel,
                                trans->input_buf,
                                static_cast<std::size_t>(trans->input_size),
                                nullptr,
                                trans->output_channel,
                                trans->output_buf,
                                static_cast<std::size_t>(trans->output_size),
                                nullptr,
                                true);
    if (rc < 0) {
        std::fprintf(stderr, "Error: AXI DMA two-way transfer failed.\n");
        dump_buffers(static_cast<const std::uint32_t *>(trans->input_buf),
                     static_cast<const std::uint32_t *>(trans->output_buf),
                     sample_count);
        goto free_bufs;
    }

    // Verify against the current RTL behavior: identity ramp, ending with a
    // single 0x1F for the default 32-sample test.
    {
        const auto *rx = static_cast<const std::uint32_t *>(trans->output_buf);
        const auto *tx = static_cast<const std::uint32_t *>(trans->input_buf);
        bool pass = true;
        for (int i = 0; i < sample_count; ++i) {
            int src_idx = i + kPipelineLatency;
            std::uint32_t expected = static_cast<std::uint32_t>(src_idx);
            if (rx[i] != expected) {
                std::fprintf(stderr,
                             "FAIL: output[%d] = %u, expected %u\n",
                             i, rx[i], expected);
                pass = false;
            }
        }
        if (pass) {
            std::printf("PASS: all %d output samples match.\n", sample_count);
        } else {
            dump_buffers(tx, rx, sample_count);
            rc = -1;
        }
    }

free_bufs:
    axidma_free(dev, trans->output_buf, trans->output_size);
    axidma_free(dev, trans->input_buf,  trans->input_size);
    return rc;
}

// ---------------------------------------------------------------------------
// main — goto-based cleanup mirrors axidma_transfer.c
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int rc = 0;
    int sample_count = kDefaultSampleCount;
    axidma_dev_t       axidma_dev = nullptr;
    const array_t     *tx_chans;
    const array_t     *rx_chans;
    struct dma_transfer trans;

    if (argc > 1) {
        char *endptr = nullptr;
        long value = std::strtol(argv[1], &endptr, 0);
        if (endptr == argv[1] || *endptr != '\0' || value <= 0) {
            std::fprintf(stderr,
                         "Usage: %s [sample_count>0]\n",
                         argv[0]);
            return EXIT_FAILURE;
        }
        sample_count = static_cast<int>(value);
    }

    // --- Backend (Verilator on x86; no-op on PetaLinux) --------------------
    if (!backend_init(argc, argv)) {
        std::fprintf(stderr, "Error: backend_init failed.\n");
        return EXIT_FAILURE;
    }

    // --- AXI-Lite: open register window ------------------------------------
    if (!axilite_init(nullptr)) {
        std::fprintf(stderr, "Error: axilite_init failed.\n");
        rc = EXIT_FAILURE;
        goto shutdown_backend;
    }
    std::printf("FIR control window opened.\n");

    // --- AXI-Lite: enable FIR and load coefficients ------------------------
    //   Reg 0x00 ctrl    bit[0] = enable  → s_axis_tready = 1
    //   Reg 0x04 coeff[1] = h[0] = 1     → identity (pure delay)
    //   Reg 0x08-0x14 coeff[2..5] = 0
    axilite_write_reg(0x00U, 0x00000001U);
    axilite_write_reg(0x04U, 0x00000001U);
    axilite_write_reg(0x08U, 0x00000000U);
    axilite_write_reg(0x0CU, 0x00000000U);
    axilite_write_reg(0x10U, 0x00000000U);
    axilite_write_reg(0x14U, 0x00000000U);
    std::printf("FIR coefficients programmed.\n");

    // --- AXI DMA: initialise device ----------------------------------------
    axidma_dev = axidma_init();
    if (axidma_dev == nullptr) {
        std::fprintf(stderr, "Error: axidma_init failed.\n");
        rc = EXIT_FAILURE;
        goto shutdown_axilite;
    }

    // --- AXI DMA: enumerate channels ---------------------------------------
    tx_chans = axidma_get_dma_tx(axidma_dev);
    if (tx_chans->len < 1) {
        std::fprintf(stderr, "Error: no transmit DMA channels available.\n");
        rc = EXIT_FAILURE;
        goto destroy_axidma;
    }

    rx_chans = axidma_get_dma_rx(axidma_dev);
    if (rx_chans->len < 1) {
        std::fprintf(stderr, "Error: no receive DMA channels available.\n");
        rc = EXIT_FAILURE;
        goto destroy_axidma;
    }
    std::printf("Discovered %d TX DMA channel(s) and %d RX DMA channel(s).\n",
                tx_chans->len,
                rx_chans->len);

    // --- Build transfer descriptor -----------------------------------------
    std::memset(&trans, 0, sizeof(trans));
    trans.input_channel  = tx_chans->data[0];
    trans.output_channel = rx_chans->data[0];
    trans.input_size     = sample_count * static_cast<int>(sizeof(std::uint32_t));
    trans.output_size    = sample_count * static_cast<int>(sizeof(std::uint32_t));

    std::printf("FIR DMA transfer:\n");
    std::printf("  TX channel : %d\n", trans.input_channel);
    std::printf("  RX channel : %d\n", trans.output_channel);
    std::printf("  Samples    : %d (%d bytes)\n", sample_count, trans.input_size);
    std::printf("  Latency    : %d sample(s) expected\n", kPipelineLatency);

    // --- Perform and verify transfer ---------------------------------------
    if (transfer_and_verify(axidma_dev, &trans, sample_count) < 0) {
        rc = EXIT_FAILURE;
    }

destroy_axidma:
    axidma_destroy(axidma_dev);
shutdown_axilite:
    axilite_shutdown();
shutdown_backend:
    backend_shutdown();
    return rc;
}
