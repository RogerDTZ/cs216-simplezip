#pragma once

#include <array>
#include <memory>

#include "util/bit_util.hpp"
#include "util/progress_bar.hpp"

#include "compress/compressor.hpp"
#include "sz/types.hpp"

namespace sz {

// Deflate dictionary size (32 KBytes).
constexpr size_t DeflateDictionarySize = 32 << 10;

// Deflate max repeat length.
constexpr size_t DeflateRepeatLenMax = 258;

// Deflate Block Size (1024 KBytes).
constexpr size_t DeflateBlockSize = 1024 << 10;

// Deflate Thread Number.
constexpr int DeflateThreadNum = 12;

enum class DeflateCodingType { static_coding, dynamic_coding };

enum class DeflateItemType { literal, distance, length, stop };

struct DeflateItem {
  DeflateItemType type;
  uint16 val;
};

size_t bb_write_store(const std::shared_ptr<BitFlowBuilder>& bb,
                      const Byte* src, size_t n, bool last_block);

size_t bb_write_deflate_item_static(const std::shared_ptr<BitFlowBuilder>& bb,
                                    const DeflateItem& item,
                                    bool do_write = true);

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

constexpr size_t HashBufferSize = DeflateDictionarySize + DeflateRepeatLenMax;
constexpr uint64 HashRoot = 13333ull;
constexpr size_t Hash3bHeadSize = 257 * 257 * 257;

class DeflateDictionary final {
 public:
  DeflateDictionary() : m_head3b(), m_hash(), m_left(0) {}

  DeflateDictionary(const DeflateDictionary&) = delete;
  DeflateDictionary& operator=(const DeflateDictionary&) = delete;
  DeflateDictionary(DeflateDictionary&&) = delete;
  DeflateDictionary& operator=(DeflateDictionary&&) = delete;
  ~DeflateDictionary() = default;

  // Use brute comparison for small possible items.
  constexpr static size_t SmallFilter = 8;

  void calc(const Byte* src, size_t n, std::vector<DeflateItem>& res,
            ProgressBar& bar);

 private:
  struct LLNode {
    LLNode* next;
    size_t pos;
  };

  std::array<LLNode*, Hash3bHeadSize> m_head3b;
  std::array<uint64, HashBufferSize> m_hash;
  size_t m_left;

  // Get hash value of sequence of length n started from pos.
  [[nodiscard]] uint64 get_hash(size_t pos, size_t n) const;

  void reset();

  // Return the uint16 hash value of 3 bytes.
  // -1 for out of bound.
  static uint32 get_hash3b(int a, int b, int c) {
    return (a + 1) * 66049 + (b + 1) * 257 + (c + 1);
  }
};

}  // namespace sz
