#pragma once

#include <cstdint>

bool axilite_init(const char *uio_path_override);
void axilite_write_reg(std::uint32_t offset, std::uint32_t value);
void axilite_shutdown();
