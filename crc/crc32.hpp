#pragma once

#include "sz/types.hpp"

namespace sz {

namespace crc32 {

CRC32Value extend(CRC32Value init_val, const Byte* data, size_t n);

inline CRC32Value calculate(const Byte* data, size_t n) {
  return extend(0, data, n);
}

}  // namespace crc32

}  // namespace sz
