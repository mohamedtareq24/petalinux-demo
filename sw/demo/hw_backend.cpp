// hw_backend.cpp — no-op hardware backend for PetaLinux / aarch64.
//
// The UIO and AXI DMA resources are opened by axilite_init() and
// axidma_init() respectively; no additional lifecycle management is needed.

#include "backend.h"

bool backend_init(int /*argc*/, char * /*argv*/[])
{
    return true;
}

void backend_shutdown() {}
