#pragma once

#include <array>

#include "sz/types.hpp"

#include "compress/compressor.hpp"
#include "util/bit_util.hpp"
#include "util/progress_bar.hpp"

namespace sz {

// LZ77 dictionary size (32 KBytes).
constexpr size_t LZ77DictionarySize = 32 << 10;

// Deflate max repeat length.
constexpr size_t DeflateRepeatLenMax = 258;

// Deflate Block Size (1024 KBytes).
constexpr size_t DeflateBlockSize = 1024 << 10;

// End of block code.
constexpr int DeflateEOBCode = 256;

// Max code of two huffman tree.
constexpr int DeflateRLCMaxCode = 18;
constexpr int DeflateLELMaxCode = 285;
constexpr int DeflateDisMaxCode = 29;

// Max length of run length code.
constexpr int DeflateRLCMaxLen = 7;
// Max length of huffman encoding.
constexpr int DeflateHuffmanMaxLen = 15;

constexpr int DeflateHLITMin = 257;
constexpr int DeflateHDISTMin = 1;
constexpr int DeflateHCLENMin = 4;

constexpr int DeflateRLCPermutation[] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                         11, 4,  12, 3, 13, 2, 14, 1, 15};

constexpr size_t LZ77DictionaryConfigNum = 4;
constexpr size_t LZ77DictionaryConfig[LZ77DictionaryConfigNum][4]{
    /* max chain length, good(/4), nice(/16), perfect(stop) */
    {64, 4, 8, 16},
    {128, 8, 16, 128},
    {512, 16, 128, 258},
    {4096, 258, 258, 258},
};

size_t lz77_get_config(const size_t level);

enum class DeflateCodingType { static_coding, dynamic_coding };

enum class LZ77ItemType { literal, distance, length, eob };

struct LZ77Item {
  LZ77ItemType type;
  uint16 val;
};

void deflate_encode_store_block(const std::shared_ptr<BitStream>& bs,
                                const Byte* src, size_t n, bool last_block);

void deflate_encode_static_block(const std::shared_ptr<BitStream>& bs,
                                 const std::vector<LZ77Item>& items,
                                 bool last_block);

void deflate_encode_dynamic_block(const std::shared_ptr<BitStream>& bs,
                                  const std::vector<LZ77Item>& items,
                                  bool last_block);

// Return the run length code of src.
// Use low 5 bits as [0, 18]. The next higher bits are extra bits.
std::vector<uint32> run_length_encode(const std::vector<int>& src);

std::pair<int, int> run_length_decode(uint32 code);

class DeflateCompressor final : public Compressor {
 public:
  DeflateCompressor(DeflateCodingType coding_type);
  DeflateCompressor(DeflateCodingType coding_type, size_t thread_cnt);
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
  // Thread count.
  size_t m_thread_cnt;
  // Compressed content.
  Byte* m_res;
  // Size of compressed content.
  size_t m_res_len;
};

constexpr size_t HashBufferSize = LZ77DictionarySize + DeflateRepeatLenMax;
constexpr uint64 HashRoot = 13333ull;
constexpr size_t Hash3bHeadSize = 257 * 257 * 257;

class LZ77Dictionary final {
 public:
  LZ77Dictionary() : m_head3b(), m_hash(), m_left(0) {}

  LZ77Dictionary(const LZ77Dictionary&) = delete;
  LZ77Dictionary& operator=(const LZ77Dictionary&) = delete;
  LZ77Dictionary(LZ77Dictionary&&) = delete;
  LZ77Dictionary& operator=(LZ77Dictionary&&) = delete;
  ~LZ77Dictionary() = default;

  void calc(const Byte* src, size_t n, std::vector<LZ77Item>& res,
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

class HuffmanTree final {
 public:
  HuffmanTree() = delete;
  // Create a huffman tree of [0, max_code) with limited length (1 <= max_len
  // <= 64).
  HuffmanTree(int max_code, int max_len);

  // Prepare the huffman coding.
  // Return the deflate code length array.
  std::vector<int> calculate(const std::vector<uint64>& src);

  // Return the code and length of the input entry.
  [[nodiscard]] std::pair<uint64, int> encode(uint64 x) const;

  // Write the code of x into BitStream (use big endian).
  void write_to_bs(const std::shared_ptr<BitStream>& bs, uint64 x) const;

  // Return the last existed code.
  [[nodiscard]] int get_last_code() const;

 private:
  int m_max_code;
  int m_max_len;
  std::vector<size_t> m_bucket;
  std::vector<int> m_dep;
  std::vector<uint64> m_code;
};

}  // namespace sz
