#pragma once

#include <memory>

#include "util/bit_util.hpp"

#include "sz/compressor.hpp"

namespace sz {

// Deflate dictionary size (32768 Bytes).
constexpr size_t DeflateDictionarySize = 1 << 15;

enum class DeflateCodingType { static_coding, dynamic_coding };

enum class DeflateItemType { literal, distance, length, stop };

struct DeflateItem {
  DeflateItemType type;
  uint16 val;
};

extern void bb_write_deflate_item(const std::shared_ptr<BitFlowBuilder>& bb,
                                  const DeflateItem& item);

class DeflateCompressor final : public Compressor {
 public:
  DeflateCompressor(DeflateCodingType coding_type);
  DeflateCompressor(const DeflateCompressor&) = delete;
  DeflateCompressor& operator=(const DeflateCompressor&) = delete;
  DeflateCompressor(DeflateCompressor&&) = delete;
  DeflateCompressor& operator=(DeflateCompressor&&) = delete;
  ~DeflateCompressor() override = default;

  size_t compress() override;

  [[nodiscard]] size_t get_length_compressed() const override;
  void write_result(Byte* dst) override;

 private:
  // Coding type: static / dynamic
  DeflateCodingType m_coding_type;

  // Compressed content.
  Byte* m_res;
  // Size of compressed content.
  size_t m_res_len;
};

}  // namespace sz
