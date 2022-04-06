#include "compress/cps_deflate.hpp"

#include <iomanip>
#include <thread>

#include "util/progress_bar.hpp"

#include "compress/table_deflate.hpp"
#include "sz/log.hpp"

namespace sz {

DeflateCompressor::DeflateCompressor(DeflateCodingType coding_type)
    : m_coding_type(coding_type), m_res(nullptr), m_res_len(0) {
  if (m_coding_type == DeflateCodingType::dynamic_coding) {
    log::log("Dynamic coding not supported yet, use static coding instead.");
    m_coding_type = DeflateCodingType::static_coding;
  }
}

size_t DeflateCompressor::compress() {
  if (m_finish) {
    return get_length_compressed();
  }
  delete[] m_res;
  m_res_len = 0;

  ProgressBar bar(std::string("deflate: "), &std::cerr, m_src_len, 30, ' ', '=',
                  '>');
  bar.set_display(false);

  auto work_thread =
      [this](const Byte* st, const Byte* ed, const bool last_section,
             std::shared_ptr<BitFlowBuilder>& bb, ProgressBar& bar) {
        std::vector<DeflateItem> items;
        items.reserve(DeflateBlockSize << 1);
        auto dict = std::make_shared<DeflateDictionary>();
        bb = std::make_shared<BitFlowBuilder>((ed - st) << 3);
        for (const Byte *p = st, *q; p < ed; p = q) {
          q = p + DeflateBlockSize <= ed ? p + DeflateBlockSize : ed;
          if (static_cast<size_t>(ed - q) < DeflateBlockSize) {
            q = ed;
          }

          auto start = std::chrono::system_clock::now();
          dict->calc(p, q - p, items, bar);
          auto end = std::chrono::system_clock::now();
          std::chrono::duration<double> elapsed_seconds = end - start;
          std::cerr << "elapsed time (" << std::this_thread::get_id()
                    << "): " << elapsed_seconds.count() << " for " << q - p
                    << "bytes: " << p - m_src << " ~ " << q - m_src << std::endl;

          switch (m_coding_type) {
            case DeflateCodingType::static_coding: {
              size_t bit_cnt = 3;
              for (const auto& item : items) {
                bit_cnt += bb_write_deflate_item_static(bb, item, false);
              }
              if (((bit_cnt + 7) >> 3) >= static_cast<size_t>(q - p)) {
                bb_write_store(bb, p, q - p, q == ed && last_section);
                log::log((bit_cnt + 7) >> 3, "(deflate) v.s. ", q - p,
                         "(original), use store");
              } else {
                bb->write_bit(q == ed && last_section);
                bb->write_bits(0b01, 2);
                for (const auto& item : items) {
                  bb_write_deflate_item_static(bb, item);
                }
                bb_write_deflate_item_static(
                    bb, DeflateItem{DeflateItemType::stop, 0});
                log::log((bit_cnt + 7) >> 3, "(deflate) v.s. ", q - p,
                         "(original), use deflate");
              }
            } break;
            case DeflateCodingType::dynamic_coding:
              log::panic("Dynamic coding not implemented.");
              break;
          }
          items.clear();
        }
      };

  const int block_cnt =
      static_cast<int>((m_src_len + DeflateBlockSize - 1) / DeflateBlockSize);
  const int thread_cnt = std::min(block_cnt, DeflateThreadNum);
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

  for (size_t i = 0, skip = 0; i < n; ++i) {
    if (i >= DeflateDictionarySize) {
      m_left = i - DeflateDictionarySize;
    }
    const auto hash3b = get_hash3b(src[i], i < n - 1 ? src[i + 1] : -1,
                                   i < n - 2 ? src[i + 2] : -1);
    if (skip == 0) {
      LLNode* th = nullptr;
      for (LLNode *p = m_head3b[hash3b], *q; p; p = q) {
        q = p->next;
        if (p->pos >= m_left) {
          p->next = th;
          th = p;
        } else {
          delete p;
        }
      }
      m_head3b[hash3b] = th;
      if (m_head3b[hash3b]) {
        th = nullptr;
        for (LLNode* p = m_head3b[hash3b]; p; p = p->next) {
          th = new LLNode{th, p->pos};
        }

        auto classify =
            [&](LLNode* head, const size_t len,
                const size_t equal_len) -> std::pair<LLNode*, LLNode*> {
          const uint64 std_val = get_hash(i, len);
          LLNode* yes = nullptr;
          LLNode* no = nullptr;
          const bool single = equal_len == len - 1;
          for (LLNode *p = head, *q; p; p = q) {
            q = p->next;
            if ((single && src[p->pos + equal_len] == src[i + equal_len]) ||
                (!single && get_hash(p->pos, len) == std_val &&
                 memcmp(src + p->pos + equal_len, src + i + equal_len,
                        len - equal_len) == 0)) {
              p->next = yes;
              yes = p;
            } else {
              p->next = no;
              no = p;
            }
          }
          return std::make_pair(yes, no);
        };

        const size_t bf_range = std::min(SmallFilter, n - i);
        size_t l = 3;
        size_t final_len = 3;
        while (l < bf_range) {
          auto [yes, no] = classify(th, l + 1, l);
          if (yes) {
            ++l;
            for (LLNode* q; no; no = q) {
              q = no->next;
              delete no;
            }
            th = yes;
          } else {
            final_len = l;
            th = no;
            break;
          }
        }
        if (l == bf_range && l < n - i) {
          ++l;
          size_t r = static_cast<int>(std::min(DeflateRepeatLenMax, n - i));
          while (l <= r) {
            const size_t mid = (l + r) >> 1;
            auto [yes, no] = classify(th, mid, l - 1);
            if (yes) {
              l = mid + 1;
              for (LLNode* q; no; no = q) {
                q = no->next;
                delete no;
              }
              th = yes;
            } else {
              r = mid - 1;
              th = no;
            }
          }
          final_len = r;
        }
        skip = final_len - 1;
        res.push_back(DeflateItem{DeflateItemType::length,
                                  static_cast<uint16>(final_len)});
        res.push_back(DeflateItem{DeflateItemType::distance,
                                  static_cast<uint16>(i - th->pos)});
        for (LLNode* q; th; th = q) {
          q = th->next;
          delete th;
        }
        bar.increase_progress(final_len);
      } else {
        res.push_back(DeflateItem{DeflateItemType::literal, src[i]});
        bar.increase_progress(1);
      }
    } else {
      --skip;
    }

    if (auto j = i + DeflateRepeatLenMax; j < n) {
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
  m_head3b.fill(nullptr);
  m_hash.fill(0ull);
}

size_t bb_write_store(const std::shared_ptr<BitFlowBuilder>& bb,
                      const Byte* src, const size_t n, const bool last_block) {
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
  return n;
}

size_t bb_write_deflate_item_static(const std::shared_ptr<BitFlowBuilder>& bb,
                                    const DeflateItem& item,
                                    const bool do_write) {
  size_t bit_cnt = 0;
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
      if (do_write) {
        bb->write_bits(base, 5, false);
        if (n_extra) {
          bb->write_bits(extra, n_extra, true);
        }
      }
      bit_cnt += 5;
      bit_cnt += n_extra;
      return bit_cnt;
    case DeflateItemType::length:
      base = DeflateLengthTable[item.val][0];
      n_extra = DeflateLengthTable[item.val][1];
      extra = DeflateLengthTable[item.val][2];
      break;
    case DeflateItemType::stop:
      base = 256;
      n_extra = 0;
      extra = 0;
      break;
  }
  // literal & stop & length
  if (base < 144) {
    if (do_write) {
      bb->write_bits(0x30 + base, 8, false);
    }
    bit_cnt += 8;
  } else if (base < 256) {
    if (do_write) {
      bb->write_bits(0x190 + base - 144, 9, false);
    }
    bit_cnt += 9;
  } else if (base < 280) {
    if (do_write) {
      bb->write_bits(base - 256, 7, false);
    }
    bit_cnt += 7;
  } else {
    if (do_write) {
      bb->write_bits(0xc0 + base - 280, 8, false);
    }
    bit_cnt += 8;
  }
  if (n_extra) {
    if (do_write) {
      bb->write_bits(extra, n_extra, true);
    }
    bit_cnt += n_extra;
  }
  return bit_cnt;
}

}  // namespace sz
