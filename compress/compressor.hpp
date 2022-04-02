#pragma once

#include <cstring>

#include "sz/types.hpp"

namespace sz {

class Compressor {
 public:
  Compressor() : m_src(nullptr), m_src_len(0), m_finish(false) {}

  Compressor(const Compressor&) = delete;
  Compressor& operator=(const Compressor&) = delete;
  Compressor(Compressor&&) = delete;
  Compressor& operator=(Compressor&&) = delete;

  virtual ~Compressor() {
    delete[] m_src;
    m_src = nullptr;
  }

  // Feed the content to be compressed.
  virtual void feed(const Byte* data, size_t n) {
    m_src_len = n;
    m_src = new Byte[n];
    memcpy(m_src, data, sizeof(Byte) * n);
    m_finish = false;
  }

  // Compress the fed content.
  // Return the length of compressed content.
  virtual size_t compress() = 0;

  [[nodiscard]] virtual size_t get_length_compressed() const = 0;

  // Write the compressed content to dst.
  // Call compress() first if the compression has not been called.
  virtual void write_result(Byte* dst) = 0;

 protected:
  // Source content to be compressed.
  Byte* m_src;
  // Size of source content.
  size_t m_src_len;
  // Whether the compressed data has been calculated.
  // Unset when fed by new data, set when compress() is called and finished.
  bool m_finish;
};

}  // namespace sz
