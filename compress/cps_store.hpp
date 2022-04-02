#pragma once

#include <cstring>

#include "compress/compressor.hpp"

namespace sz {

class StoreCompressor final : public Compressor {
 public:
  StoreCompressor() = default;
  StoreCompressor(const StoreCompressor&) = delete;
  StoreCompressor& operator=(const StoreCompressor&) = delete;
  StoreCompressor(StoreCompressor&&) = delete;
  StoreCompressor& operator=(StoreCompressor&&) = delete;
  ~StoreCompressor() override = default;

  size_t compress() override { return m_src_len; }

  size_t get_length_compressed() const override { return m_src_len; }

  void write_result(Byte* dst) override {
    memcpy(dst, m_src, sizeof(Byte) * m_src_len);
  }
};

}  // namespace sz
