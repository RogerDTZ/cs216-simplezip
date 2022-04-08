#pragma once

#include <cstring>
#include <string>

#include "sz/types.hpp"

namespace sz {

inline void marshal_8(Byte*& p, uint8 x) { *p++ = x; }

inline void marshal_16(Byte*& p, uint16 x) {
  *p++ = static_cast<Byte>((x >> 0) & 0xFF);
  *p++ = static_cast<Byte>((x >> 8) & 0xFF);
}

inline void marshal_32(Byte*& p, uint32 x) {
  *p++ = static_cast<Byte>((x >> 0) & 0xFF);
  *p++ = static_cast<Byte>((x >> 8) & 0xFF);
  *p++ = static_cast<Byte>((x >> 16) & 0xFF);
  *p++ = static_cast<Byte>((x >> 24) & 0xFF);
}

inline void marshal_string(Byte*& p, const char* src, size_t n) {
  memcpy(p, src, n);
  p += n;
}

inline void marshal_string(Byte*& p, const std::string& str) {
  marshal_string(p, str.c_str(), str.size());
}

}  // namespace sz
