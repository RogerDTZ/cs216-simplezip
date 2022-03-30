#pragma once

#include "sz/compressor.hpp"

namespace sz {

// Deflate dictionary size (32768 Bytes).
constexpr size_t DeflateDictionarySize = 1 << 15;

enum class DeflateCodingType { static_coding, dynamic_coding };

class DeflateCompressor final : public Compressor {
 public:
  DeflateCompressor(DeflateCodingType coding_type);
  DeflateCompressor(const DeflateCompressor&) = delete;
  DeflateCompressor& operator=(const DeflateCompressor&) = delete;
  DeflateCompressor(DeflateCompressor&&) = delete;
  DeflateCompressor& operator=(DeflateCompressor&&) = delete;
  ~DeflateCompressor() override = default;

  size_t compress() override;

 private:
  DeflateCodingType m_coding_type;
};

}  // namespace sz
