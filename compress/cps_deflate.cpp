#include "compress/cps_deflate.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iomanip>
#include <queue>
#include <thread>

#include "sz/log.hpp"

#include "compress/table_deflate.hpp"
#include "util/progress_bar.hpp"

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

  ProgressBar bar(std::string("deflate: "), &std::cerr, m_src_len, 30, ' ', '=',
                  '>');
  bar.set_display(true);

  auto work_thread = [this](const Byte* st, const Byte* ed,
                            const bool last_section,
                            std::shared_ptr<BitFlowBuilder>& bb,
                            ProgressBar& bar) {
    std::vector<DeflateItem> items;
    items.reserve(DeflateBlockSize << 1);
    auto dict = std::make_shared<DeflateDictionary>();
    bb = std::make_shared<BitFlowBuilder>((ed - st) << 3);
    for (const Byte *p = st, *q; p < ed; p = q) {
      q = p + DeflateBlockSize <= ed ? p + DeflateBlockSize : ed;
      if (static_cast<size_t>(ed - q) < DeflateBlockSize) {
        q = ed;
      }

      // auto start = std::chrono::system_clock::now();
      dict->calc(p, q - p, items, bar);
      // auto end = std::chrono::system_clock::now();
      // std::chrono::duration<double> elapsed_seconds = end - start;
      // std::cerr << "elapsed time (" << std::this_thread::get_id()
      //           << "): " << elapsed_seconds.count() << " for " << q - p
      //           << "bytes: " << p - m_src << " ~ " << q - m_src << std::endl;

      items.push_back(DeflateItem{DeflateItemType::stop, DeflateEOBCode});
      auto bb_cps = std::make_shared<BitFlowBuilder>(items.size() << 3);

      switch (m_coding_type) {
        case DeflateCodingType::static_coding: {
          bb_cps->write_bit(q == ed && last_section);
          bb_cps->write_bits(0b01, 2);
          for (const auto& item : items) {
            bb_write_deflate_item_static(bb_cps, item);
          }
        } break;
        case DeflateCodingType::dynamic_coding: {
          std::vector<uint64> lit_len_eob;
          std::vector<uint64> dis;
          for (auto&& item : items) {
            switch (item.type) {
              case DeflateItemType::literal:
                lit_len_eob.push_back(DeflateLiteralTable[item.val][0]);
                break;
              case DeflateItemType::length:
                lit_len_eob.push_back(DeflateLengthTable[item.val][0]);
                break;
              case DeflateItemType::stop:
                lit_len_eob.push_back(DeflateEOBCode);
                break;
              case DeflateItemType::distance:
                dis.push_back(DeflateDistanceTable[item.val][0]);
            }
          }
          lit_len_eob.push_back(DeflateEOBCode);
          // If no distance code, push an arbitrary one.
          if (dis.empty()) {
            dis.push_back(0);
          }
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
          auto cl3 = h3.calculate(
              std::vector<uint64>(rlc_combine.begin(), rlc_combine.end()));

          // header
          bb_cps->write_bit(q == ed && last_section);
          bb_cps->write_bits(0b10, 2);
          bb_cps->write_bits(cl1.size() - DeflateHLITMin, 5);
          bb_cps->write_bits(cl2.size() - DeflateHDISTMin, 5);
          int rlc_last_code_index = DeflateRLCMaxCode;
          for (int i = DeflateRLCMaxCode; i >= 0; --i) {
            if (cl3[DeflateRLCPermutation[i]]) {
              rlc_last_code_index = i;
              break;
            }
          }
          bb_cps->write_bits(
              static_cast<uint64>(rlc_last_code_index) + 1 - DeflateHCLENMin,
              4);
          // (HCLEN + 4) x 3 bits: code lengths for the code length
          // alphabet given just above, in the order: 16, 17, 18,
          // 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
          for (int i = 0; i <= rlc_last_code_index; ++i) {
            bb_cps->write_bits(cl3[DeflateRLCPermutation[i]], 3);
          }
          // literal/length/eob huffman tree and distance huffman tree.
          auto write_huffman_code_len = [&bb_cps,
                                         &h3](const std::vector<uint32>& rlc) {
            for (auto&& item : rlc) {
              const auto [x, y] = run_length_decode(item);
              h3.write_to_bb(bb_cps, x);
              if (x == 16) {
                bb_cps->write_bits(static_cast<uint64>(y) - 3, 2);
              } else if (x == 17) {
                bb_cps->write_bits(static_cast<uint64>(y) - 3, 3);
              } else if (x == 18) {
                bb_cps->write_bits(static_cast<uint64>(y) - 11, 7);
              }
            }
          };
          write_huffman_code_len(rlc1);
          write_huffman_code_len(rlc2);
          // encoded data
          for (auto&& item : items) {
            switch (item.type) {
              case DeflateItemType::literal:
                h1.write_to_bb(bb_cps, DeflateLiteralTable[item.val][0]);
                break;
              case DeflateItemType::length:
                h1.write_to_bb(bb_cps, DeflateLengthTable[item.val][0]);
                if (DeflateLengthTable[item.val][1]) {
                  bb_cps->write_bits(DeflateLengthTable[item.val][2],
                                     DeflateLengthTable[item.val][1]);
                }
                break;
              case DeflateItemType::stop:
                h1.write_to_bb(bb_cps, DeflateEOBCode);
                break;
              case DeflateItemType::distance:
                h2.write_to_bb(bb_cps, DeflateDistanceTable[item.val][0]);
                if (DeflateDistanceTable[item.val][1]) {
                  bb_cps->write_bits(DeflateDistanceTable[item.val][2],
                                     DeflateDistanceTable[item.val][1]);
                }
                break;
            }
          }
        } break;
      }

      // Determine which method to use: store / static coding
      if (bb_cps->get_bytes_size() >= static_cast<size_t>(q - p)) {
        // Use store.
        bb_write_store(bb, p, q - p, q == ed && last_section);
        // log::log(bb_cps->get_bytes_size(), "(deflate) v.s. ", q - p,
        //          "(original), use store");
      } else {
        // Use static coding.
        bb->append(*bb_cps);
        // log::log(bb_cps->get_bytes_size(), "(deflate) v.s. ", q - p,
        //          "(original), use deflate");
      }
      items.clear();
    }
  };

  const int block_cnt =
      static_cast<int>((m_src_len + DeflateBlockSize - 1) / DeflateBlockSize);
  const int thread_cnt = std::min(block_cnt, static_cast<int>(m_thread_cnt));
  const int each_cnt = (block_cnt + thread_cnt - 1) / thread_cnt;
  std::vector<std::shared_ptr<BitFlowBuilder>> bbs(thread_cnt);
  std::vector<std::shared_ptr<std::thread>> threads(thread_cnt);
  const Byte* p = m_src;
  const Byte* ed = m_src + m_src_len;
  for (int i = 0; i < thread_cnt; ++i) {
    const Byte* q = std::min(p + each_cnt * DeflateBlockSize, ed);
    threads[i] = std::make_shared<std::thread>(work_thread, p, q, q == ed,
                                               std::ref(bbs[i]), std::ref(bar));
    p = q;
  }
  for (int i = 0; i < thread_cnt; ++i) {
    threads[i]->join();
  }
  bar.set_full();
  bar.set_display(false);

  auto bb = std::make_shared<BitFlowBuilder>(m_src_len << 3);
  for (int i = 0; i < thread_cnt; ++i) {
    bb->append(*bbs[i]);
  }

  m_res_len = bb->get_bytes_size();
  m_res = new Byte[m_res_len];
  bb->export_bitflow(m_res);
  m_finish = true;
  return m_res_len;
}

size_t DeflateCompressor::get_length_compressed() const { return m_res_len; }

void DeflateCompressor::write_result(Byte* dst) {
  memcpy(dst, m_res, sizeof(Byte) * m_res_len);
}

static uint64 get_pow_hash_root(size_t q) {
  static bool calculated = false;
  static uint64 table[HashBufferSize];
  if (!calculated) {
    table[0] = 1;
    for (size_t i = 1; i < HashBufferSize; ++i) {
      table[i] = table[i - 1] * HashRoot;
    }
    calculated = true;
  }
  return table[q];
}

void DeflateDictionary::calc(const Byte* src, size_t n,
                             std::vector<DeflateItem>& res, ProgressBar& bar) {
  reset();

  // Directly store literals if too short.
  if (n <= 10) {
    for (const Byte *p = src, *q = src + n; p < q; ++p) {
      res.push_back(DeflateItem{DeflateItemType::literal, *p});
    }
    return;
  }

  constexpr auto L = HashBufferSize;
  for (size_t i = 0; i < n && i < DeflateRepeatLenMax; ++i) {
    m_hash[i % L] = m_hash[(i + L - 1) % L] * HashRoot + src[i];
  }

  size_t finished_bytes = 0;
  for (size_t i = 0, skip = 0; i < n; ++i) {
    if (i >= DeflateDictionarySize) {
      m_left = i - DeflateDictionarySize;
    }
    const auto hash3b = get_hash3b(src[i], i < n - 1 ? src[i + 1] : -1,
                                   i < n - 2 ? src[i + 2] : -1);
    if (skip == 0) {
      if (!m_head3b[hash3b] || m_head3b[hash3b]->pos < m_left) {
        res.push_back(DeflateItem{DeflateItemType::literal, src[i]});
        ++finished_bytes;
      } else {
        size_t checked = 0;
        size_t max_check = DeflateDictionaryConfig[deflate_lz77_level][0];
        size_t max_match_len = 0;
        size_t max_match_pos = 0;
        for (const LLNode* p = m_head3b[hash3b]; p && p->pos >= m_left;
             p = p->next) {
          size_t match_len = max_match_len;
          if (match_len == 0) {
            match_len = 3;
          }
          bool base_ok = true;
          if (match_len > 3) {
            if (get_hash(p->pos, match_len) != get_hash(i, match_len) ||
                memcmp(src + p->pos + 3, src + i + 3, match_len - 3) != 0) {
              base_ok = false;
            }
          }

          if (base_ok) {
            while (i + match_len < n && match_len < DeflateRepeatLenMax &&
                   src[p->pos + match_len] == src[i + match_len]) {
              ++match_len;
            }
            if (match_len > max_match_len) {
              max_match_len = match_len;
              max_match_pos = p->pos;
              if (max_match_len >=
                  DeflateDictionaryConfig[deflate_lz77_level][3]) {
                break;
              } else if (max_match_len >=
                         DeflateDictionaryConfig[deflate_lz77_level][2]) {
                max_check = DeflateDictionaryConfig[deflate_lz77_level][0] >> 4;
              } else if (max_match_len >=
                         DeflateDictionaryConfig[deflate_lz77_level][1]) {
                max_check = DeflateDictionaryConfig[deflate_lz77_level][0] >> 2;
              }
            }
          }

          ++checked;
          if (checked >= max_check) {
            break;
          }
        }

        skip = max_match_len - 1;
        res.push_back(DeflateItem{DeflateItemType::length,
                                  static_cast<uint16>(max_match_len)});
        res.push_back(DeflateItem{DeflateItemType::distance,
                                  static_cast<uint16>(i - max_match_pos)});
        finished_bytes += max_match_len;
      }
      if (finished_bytes > 64) {
        bar.increase_progress(finished_bytes);
        finished_bytes = 0;
      }
    } else {
      --skip;
    }

    if (const auto j = i + DeflateRepeatLenMax; j < n) {
      m_hash[j % L] = m_hash[(j - 1) % L] * HashRoot + src[j];
    }
    m_head3b[hash3b] = new LLNode{m_head3b[hash3b], i};
  }
}

uint64 DeflateDictionary::get_hash(const size_t pos, const size_t n) const {
  constexpr auto L = HashBufferSize;
  return m_hash[(pos + n - 1) % L] -
         (pos > 0 ? m_hash[(pos + L - 1) % L] * get_pow_hash_root(n) : 0ull);
}

void DeflateDictionary::reset() {
  m_left = 0;
  for (const LLNode* u : m_head3b) {
    for (const LLNode* next; u; u = next) {
      next = u->next;
      delete u;
    }
  }
  m_head3b.fill(nullptr);
  m_hash.fill(0ull);
}

HuffmanTree::HuffmanTree(const int max_code, const int max_len)
    : m_max_code(max_code),
      m_max_len(max_len),
      m_bucket(std::vector<size_t>(m_max_code)),
      m_dep(std::vector<int>(m_max_code)),
      m_code(std::vector<size_t>(m_max_code)) {
  assert(1 <= max_len && max_len <= 64);
}

std::vector<int> HuffmanTree::calculate(const std::vector<uint64>& src) {
  if (src.empty()) {
    log::panic("HuffmanTree: empty huffman source");
  }

  std::fill(m_bucket.begin(), m_bucket.end(), 0);
  for (auto&& x : src) {
    ++m_bucket[x];
  }

  // Huffman (package merge)
  std::vector<int> index;
  for (int i = 0; i < m_max_code; ++i) {
    if (m_bucket[i]) {
      index.push_back(i);
    }
  }
  for (int i = 0; i < m_max_code && index.size() < 2; ++i) {
    if (!m_bucket[i]) {
      index.push_back(i);
    }
  }
  std::sort(index.begin(), index.end(), [this](const int x, const int y) {
    return m_bucket[x] < m_bucket[y];
  });
  std::vector<std::vector<std::pair<int, size_t>>> lists;
  for (int i = 0; i < m_max_len; ++i) {
    std::vector<std::pair<int, size_t>> list;
    size_t last_p = 0;
    for (size_t j = 0; j <= index.size(); ++j) {
      if (i > 0) {
        while (last_p + 1 < lists.back().size() &&
               (j == index.size() ||
                lists.back()[last_p].second + lists.back()[last_p + 1].second <
                    m_bucket[index[j]])) {
          list.emplace_back(
              -1 - static_cast<int>(last_p),
              lists.back()[last_p].second + lists.back()[last_p + 1].second);
          last_p += 2;
        }
      }
      if (j < index.size()) {
        list.emplace_back(index[j], m_bucket[index[j]]);
      }
    }
    lists.emplace_back(std::move(list));
  }
  std::fill(m_dep.begin(), m_dep.end(), 0);
  size_t bound = (index.size() - 1) << 1;
  for (int i = static_cast<int>(lists.size()) - 1; i >= 0; --i) {
    size_t new_bound = 0;
    for (size_t j = 0; j < bound; ++j) {
      if (lists[i][j].first >= 0) {
        ++m_dep[lists[i][j].first];
      } else {
        new_bound = static_cast<size_t>(1 - lists[i][j].first);
      }
    }
    bound = new_bound;
  }

  std::vector<size_t> bl_count(static_cast<size_t>(m_max_len) + 1);
  for (int i = 0; i < m_max_code; ++i) {
    if (m_dep[i]) {
      ++bl_count[m_dep[i]];
    }
  }
  size_t code = 0;
  std::vector<size_t> start_code(static_cast<size_t>(m_max_len) + 1);
  for (int level = 1; level <= m_max_len; ++level) {
    code = (code + bl_count[static_cast<size_t>(level) - 1]) << 1;
    start_code[level] = code;
  }
  for (int i = 0; i < m_max_code; ++i) {
    if (m_dep[i]) {
      m_code[i] = start_code[m_dep[i]]++;
    }
  }

  return m_dep;
}

std::pair<uint64, int> HuffmanTree::encode(const uint64 x) const {
  return std::make_pair(m_code[x], m_dep[x]);
}

void HuffmanTree::write_to_bb(const std::shared_ptr<BitFlowBuilder>& bb,
                              const uint64 x) const {
  auto [payload, n] = encode(x);
  bb->write_bits(payload, n, false);
}

int HuffmanTree::get_last_code() const {
  for (int i = m_max_code - 1; i >= 0; --i) {
    if (m_dep[i]) {
      return i;
    }
  }
  return -1;
}

void bb_write_store(const std::shared_ptr<BitFlowBuilder>& bb, const Byte* src,
                    const size_t n, const bool last_block) {
  size_t rest = n;
  while (rest > 0) {
    constexpr size_t SubBlockSize = (1 << 16) - 1;
    const size_t sub_block = std::min(SubBlockSize, rest);
    bb->write_bit(last_block && sub_block == rest);
    bb->write_bits(0b00, 2);
    bb->align_to_byte(0);
    bb->write_bits(static_cast<uint16>(sub_block), 16);
    bb->write_bits(static_cast<uint16>(~sub_block), 16);
    for (const Byte* q = src + sub_block; src < q; ++src) {
      bb->write_bits(*src, 8);
    }
    rest -= sub_block;
  }
}

void bb_write_deflate_item_static(const std::shared_ptr<BitFlowBuilder>& bb,
                                  const DeflateItem& item) {
  int base = -1;
  int n_extra = 0;
  int extra = 0;
  switch (item.type) {
    case DeflateItemType::literal:
      base = DeflateLiteralTable[item.val][0];
      n_extra = DeflateLiteralTable[item.val][1];
      extra = DeflateLiteralTable[item.val][2];
      break;
    case DeflateItemType::distance:
      base = DeflateDistanceTable[item.val][0];
      n_extra = DeflateDistanceTable[item.val][1];
      extra = DeflateDistanceTable[item.val][2];
      bb->write_bits(base, 5, false);
      if (n_extra) {
        bb->write_bits(extra, n_extra, true);
      }
      return;
    case DeflateItemType::length:
      base = DeflateLengthTable[item.val][0];
      n_extra = DeflateLengthTable[item.val][1];
      extra = DeflateLengthTable[item.val][2];
      break;
    case DeflateItemType::stop:
      base = DeflateEOBCode;
      n_extra = 0;
      extra = 0;
      break;
  }
  // literal & stop & length
  if (base < 144) {
    bb->write_bits(0x30ull + base, 8, false);
  } else if (base < 256) {
    bb->write_bits(0x190ull + base - 144, 9, false);
  } else if (base < 280) {
    bb->write_bits(base - 256ull, 7, false);
  } else {
    bb->write_bits(0xc0ull + base - 280, 8, false);
  }
  if (n_extra) {
    bb->write_bits(extra, n_extra, true);
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
