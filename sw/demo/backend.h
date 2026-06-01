#pragma once

// backend_init  — set up hardware-access layer.
//   sim target : instantiates Verilator model, resets the DUT.
//   hw  target : no-op (UIO/DMA drivers are opened by their own APIs).
// Returns true on success.
bool backend_init(int argc, char *argv[]);

// backend_shutdown — tear down hardware-access layer.
//   sim target : closes FST trace, finalises and deletes Verilator model.
//   hw  target : no-op.
void backend_shutdown();
