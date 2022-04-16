#include <cassert>
#include <iomanip>
#include <thread>

#include "sz/log.hpp"

#include "compress/cps_deflate.hpp"
#include "compress/table_deflate.hpp"

namespace sz {

DeflateCompressor::DeflateCompressor(DeflateCodingType coding_type)
    : DeflateCompressor(coding_type, std::thread::hardware_concurrency()) {}

DeflateCompressor::DeflateCompressor(DeflateCodingType coding_type,
                                     size_t thread_cnt)
    : m_coding_type(coding_type),
      m_thread_cnt(thread_cnt),
      m_res(nullptr),
      m_res_len(0) {}

size_t DeflateCompressor::compress() {
  if (m_finish) {
    return get_length_compressed();
  }
  delete[] m_res;
  m_res_len = 0;

  log::log("File size: ", std::setprecision(2), std::fixed,
           static_cast<float>(m_src_len) / 1024.f, " KB");

  // Initialize the progress bar.
  ProgressBar bar(std::string("deflate: "), &std::cerr, m_src_len, 30, ' ', '=',
                  '>');
  bar.set_display(true);

  // The work thread that runs LZ77 on [st, ed) and encodes the result.
  auto work_thread = [this](const Byte* st, const Byte* ed,
                            const bool last_section,
                            std::shared_ptr<BitStream>& bs, ProgressBar& bar) {
    bs = std::make_shared<BitStream>((ed - st) << 3);
    // LZ77 dictionary.
    auto dict = std::make_shared<LZ77Dictionary>();
    // The vector that stores LZ77's result. Though the input bytes is actually
    // O(2 * DeflateBlockSize), it is not necessary to reserve that many.
    std::vector<LZ77Item> items;
    items.reserve(DeflateBlockSize);

    for (const Byte *p = st, *q; p < ed; p = q) {
      q = p + DeflateBlockSize <= ed ? p + DeflateBlockSize : ed;
      if (static_cast<size_t>(ed - q) < DeflateBlockSize) {
        q = ed;
      }
      const bool last_block = q == ed && last_section;

      // Run LZ77 to obtain the deflate items.
      // auto start = std::chrono::system_clock::now();
      items.clear();
      dict->calc(p, q - p, items, bar);
      // auto end = std::chrono::system_clock::now();
      // std::chrono::duration<double> elapsed_seconds = end - start;
      // std::cerr << "elapsed time (" << std::this_thread::get_id()
      //           << "): " << elapsed_seconds.count() << " for " << q - p
      //           << "bytes: " << p - m_src << " ~ " << q - m_src << std::endl;
      items.push_back(LZ77Item{LZ77ItemType::eob, DeflateEOBCode});

      // Encode the deflate items into bit stream.
      // The size of the bit stream is then compared to the one of a store
      // block. The better one is adopted.
      auto bs_cps = std::make_shared<BitStream>(items.size() << 3);
      switch (m_coding_type) {
        case DeflateCodingType::static_coding:
          deflate_encode_static_block(bs_cps, items, last_block);
          break;
        case DeflateCodingType::dynamic_coding:
          deflate_encode_dynamic_block(bs_cps, items, last_block);
          break;
      }

      if (bs_cps->get_bytes_size() >= static_cast<size_t>(q - p)) {
        // Use store.
        deflate_encode_store_block(bs, p, q - p, last_block);
        // log::log(bs_cps->get_bytes_size(), "(deflate) v.s. ", q - p,
        //          "(original), use store");
      } else {
        // Use static/dynamic coding.
        bs->append(*bs_cps);
        // log::log(bs_cps->get_bytes_size(), "(deflate) v.s. ", q - p,
        //          "(original), use deflate");
      }
    }
  };

  // Parallelism scheduler.
  const int block_cnt =
      static_cast<int>((m_src_len + DeflateBlockSize - 1) / DeflateBlockSize);
  const int thread_cnt = std::min(block_cnt, static_cast<int>(m_thread_cnt));
  const int each_cnt = (block_cnt + thread_cnt - 1) / thread_cnt;
  std::vector<std::shared_ptr<BitStream>> bss(thread_cnt);
  std::vector<std::shared_ptr<std::thread>> threads(thread_cnt);
  const Byte* p = m_src;
  const Byte* ed = m_src + m_src_len;
  for (int i = 0; i < thread_cnt; ++i) {
    const Byte* q = std::min(p + each_cnt * DeflateBlockSize, ed);
    threads[i] = std::make_shared<std::thread>(work_thread, p, q, q == ed,
                                               std::ref(bss[i]), std::ref(bar));
    p = q;
  }
  for (int i = 0; i < thread_cnt; ++i) {
    threads[i]->join();
  }
  bar.set_full();
  bar.set_display(false);

  auto bs = std::make_shared<BitStream>(m_src_len << 3);
  for (int i = 0; i < thread_cnt; ++i) {
    bs->append(*bss[i]);
  }

  m_res_len = bs->get_bytes_size();
  m_res = new Byte[m_res_len];
  bs->export_bitstream(m_res);
  m_finish = true;

  log::log("Compressed size: ", std::setprecision(2), std::fixed,
           static_cast<float>(m_res_len) / 1024.f, " KB");
  log::log(
      "Compression rate: ", std::setprecision(2), std::fixed,
      static_cast<float>(m_res_len) / static_cast<float>(m_src_len) * 100.f,
      "%");

  return m_res_len;
}

size_t DeflateCompressor::get_length_compressed() const { return m_res_len; }

void DeflateCompressor::write_result(Byte* dst) {
  memcpy(dst, m_res, sizeof(Byte) * m_res_len);
}

size_t lz77_get_config(const size_t level) {
  if (static_cast<size_t>(deflate_lz77_level) >= LZ77DictionaryConfigNum) {
    log::panic("Unrecognized LZ77 level: ", level);
  }
  assert(level < 4);
  return LZ77DictionaryConfig[deflate_lz77_level][level];
}

void deflate_encode_store_block(const std::shared_ptr<BitStream>& bs,
                                const Byte* src, const size_t n,
                                const bool last_block) {
  size_t rest = n;
  while (rest > 0) {
    constexpr size_t SubBlockSize = (1 << 16) - 1;
    const size_t sub_block = std::min(SubBlockSize, rest);
    bs->write_bit(last_block && sub_block == rest);
    bs->write_bits(0b00, 2);
    bs->align_to_byte(0);
    bs->write_bits(static_cast<uint16>(sub_block), 16);
    bs->write_bits(static_cast<uint16>(~sub_block), 16);
    for (const Byte* q = src + sub_block; src < q; ++src) {
      bs->write_bits(*src, 8);
    }
    rest -= sub_block;
  }
}

void deflate_encode_static_block(const std::shared_ptr<BitStream>& bs,
                                 const std::vector<LZ77Item>& items,
                                 const bool last_block) {
  bs->write_bit(last_block);
  bs->write_bits(0b01, 2);
  for (const auto& item : items) {
    int base = -1;
    int n_extra = 0;
    int extra = 0;
    switch (item.type) {
      case LZ77ItemType::literal:
        base = DeflateLiteralTable[item.val][0];
        n_extra = DeflateLiteralTable[item.val][1];
        extra = DeflateLiteralTable[item.val][2];
        break;
      case LZ77ItemType::distance:
        base = DeflateDistanceTable[item.val][0];
        n_extra = DeflateDistanceTable[item.val][1];
        extra = DeflateDistanceTable[item.val][2];
        bs->write_bits(base, 5, false);
        if (n_extra) {
          bs->write_bits(extra, n_extra, true);
        }
        continue;
      case LZ77ItemType::length:
        base = DeflateLengthTable[item.val][0];
        n_extra = DeflateLengthTable[item.val][1];
        extra = DeflateLengthTable[item.val][2];
        break;
      case LZ77ItemType::eob:
        base = DeflateEOBCode;
        n_extra = 0;
        extra = 0;
        break;
    }
    // for literal & eob & length
    if (base < 144) {
      bs->write_bits(0x30ull + base, 8, false);
    } else if (base < 256) {
      bs->write_bits(0x190ull + base - 144, 9, false);
    } else if (base < 280) {
      bs->write_bits(base - 256ull, 7, false);
    } else {
      bs->write_bits(0xc0ull + base - 280, 8, false);
    }
    if (n_extra) {
      bs->write_bits(extra, n_extra, true);
    }
  }
}

void deflate_encode_dynamic_block(const std::shared_ptr<BitStream>& bs,
                                  const std::vector<LZ77Item>& items,
                                  bool last_block) {
  // Collect all possible values to build Huffman trees.
  std::vector<uint64> lit_len_eob;
  std::vector<uint64> dis;
  for (auto&& item : items) {
    switch (item.type) {
      case LZ77ItemType::literal:
        lit_len_eob.push_back(DeflateLiteralTable[item.val][0]);
        break;
      case LZ77ItemType::length:
        lit_len_eob.push_back(DeflateLengthTable[item.val][0]);
        break;
      case LZ77ItemType::eob:
        lit_len_eob.push_back(DeflateEOBCode);
        break;
      case LZ77ItemType::distance:
        dis.push_back(DeflateDistanceTable[item.val][0]);
    }
  }
  lit_len_eob.push_back(DeflateEOBCode);
  // If no distance code is presented, push an arbitrary one.
  if (dis.empty()) {
    dis.push_back(0);
  }

  // Calculate run length code, build 3 Huffman trees.
  HuffmanTree h1(DeflateLELMaxCode + 1, DeflateHuffmanMaxLen);
  HuffmanTree h2(DeflateDisMaxCode + 1, DeflateHuffmanMaxLen);
  HuffmanTree h3(DeflateRLCMaxCode + 1, DeflateRLCMaxLen);
  auto cl1 = h1.calculate(lit_len_eob);
  cl1.resize(static_cast<size_t>(h1.get_last_code()) + 1);
  auto cl2 = h2.calculate(dis);
  cl2.resize(static_cast<size_t>(h2.get_last_code()) + 1);
  auto rlc1 = run_length_encode(cl1);
  auto rlc2 = run_length_encode(cl2);
  auto rlc_combine = rlc1;
  rlc_combine.insert(rlc_combine.end(), rlc2.begin(), rlc2.end());
  for (auto&& x : rlc_combine) {
    x &= 31;
  }
  auto cl3 =
      h3.calculate(std::vector<uint64>(rlc_combine.begin(), rlc_combine.end()));

  // Encode into bit stream.
  // header
  bs->write_bit(last_block);
  bs->write_bits(0b10, 2);
  bs->write_bits(cl1.size() - DeflateHLITMin, 5);
  bs->write_bits(cl2.size() - DeflateHDISTMin, 5);
  int rlc_last_code_index = DeflateRLCMaxCode;
  for (int i = DeflateRLCMaxCode; i >= 0; --i) {
    if (cl3[DeflateRLCPermutation[i]]) {
      rlc_last_code_index = i;
      break;
    }
  }
  bs->write_bits(static_cast<uint64>(rlc_last_code_index) + 1 - DeflateHCLENMin,
                 4);
  // (HCLEN + 4) x 3 bits: code lengths for the code length
  // alphabet given just above, in the order: 16, 17, 18,
  // 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
  for (int i = 0; i <= rlc_last_code_index; ++i) {
    bs->write_bits(cl3[DeflateRLCPermutation[i]], 3);
  }
  // literal/length/eob huffman tree and distance huffman tree.
  auto write_huffman_code_len = [&bs, &h3](const std::vector<uint32>& rlc) {
    for (auto&& item : rlc) {
      const auto [x, y] = run_length_decode(item);
      h3.write_to_bs(bs, x);
      if (x == 16) {
        bs->write_bits(static_cast<uint64>(y) - 3, 2);
      } else if (x == 17) {
        bs->write_bits(static_cast<uint64>(y) - 3, 3);
      } else if (x == 18) {
        bs->write_bits(static_cast<uint64>(y) - 11, 7);
      }
    }
  };
  write_huffman_code_len(rlc1);
  write_huffman_code_len(rlc2);
  // encoded data
  for (auto&& item : items) {
    switch (item.type) {
      case LZ77ItemType::literal:
        h1.write_to_bs(bs, DeflateLiteralTable[item.val][0]);
        break;
      case LZ77ItemType::length:
        h1.write_to_bs(bs, DeflateLengthTable[item.val][0]);
        if (DeflateLengthTable[item.val][1]) {
          bs->write_bits(DeflateLengthTable[item.val][2],
                         DeflateLengthTable[item.val][1]);
        }
        break;
      case LZ77ItemType::eob:
        h1.write_to_bs(bs, DeflateEOBCode);
        break;
      case LZ77ItemType::distance:
        h2.write_to_bs(bs, DeflateDistanceTable[item.val][0]);
        if (DeflateDistanceTable[item.val][1]) {
          bs->write_bits(DeflateDistanceTable[item.val][2],
                         DeflateDistanceTable[item.val][1]);
        }
        break;
    }
  }
}

std::vector<uint32> run_length_encode(const std::vector<int>& src) {
  std::vector<uint32> res;
  for (size_t i = 0; i < src.size();) {
    size_t j = i;
    while (j < src.size() && src[j] == src[i]) {
      ++j;
    }
    if (src[i]) {
      res.push_back(src[i++]);
      while (j - i >= 3) {
        const auto k = std::min(j - i, 6ull);
        res.push_back(16 | (static_cast<uint32>(k - 3) << 5));
        i += k;
      }
      for (; i < j; ++i) {
        res.push_back(src[i]);
      }
    } else {
      while (j - i >= 11) {
        const auto k = std::min(j - i, 138ull);
        res.push_back(18 | (static_cast<uint32>(k - 11) << 5));
        i += k;
      }
      while (j - i >= 3) {
        const auto k = std::min(j - i, 10ull);
        res.push_back(17 | (static_cast<uint32>(k - 3) << 5));
        i += k;
      }
      for (; i < j; ++i) {
        res.push_back(0);
      }
    }
  }
  return res;
}

std::pair<int, int> run_length_decode(const uint32 code) {
  const int x = static_cast<int>(code & 31);
  const int y = static_cast<int>(code >> 5);
  if (x <= 15) {
    return std::make_pair(x, y);
  }
  if (x == 16) {
    return std::make_pair(x, y + 3);
  }
  if (x == 17) {
    return std::make_pair(x, y + 3);
  }
  return std::make_pair(x, y + 11);
}

}  // namespace sz
