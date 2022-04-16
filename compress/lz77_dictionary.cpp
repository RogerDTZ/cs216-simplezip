#include "sz/common.hpp"

#include "compress/cps_deflate.hpp"
#include "util/progress_bar.hpp"

namespace sz {

void LZ77Dictionary::calc(const Byte* src, size_t n,
                          std::vector<LZ77Item>& res, ProgressBar& bar) {
  reset();

  // Directly store literals if too short.
  if (n <= 10) {
    for (const Byte *p = src, *q = src + n; p < q; ++p) {
      res.push_back(LZ77Item{LZ77ItemType::literal, *p});
    }
    return;
  }

  constexpr auto L = HashBufferSize;
  for (size_t i = 0; i < n && i < DeflateRepeatLenMax; ++i) {
    m_hash[i % L] = m_hash[(i + L - 1) % L] * HashRoot + src[i];
  }

  size_t finished_bytes = 0;
  for (size_t i = 0, skip = 0; i < n; ++i) {
    if (i >= LZ77DictionarySize) {
      m_left = i - LZ77DictionarySize;
    }
    const auto hash3b = get_hash3b(src[i], i < n - 1 ? src[i + 1] : -1,
                                   i < n - 2 ? src[i + 2] : -1);
    if (skip == 0) {
      if (!m_head3b[hash3b] || m_head3b[hash3b]->pos < m_left) {
        res.push_back(LZ77Item{LZ77ItemType::literal, src[i]});
        ++finished_bytes;
      } else {
        size_t checked = 0;
        size_t max_check = LZ77DictionaryConfig[deflate_lz77_level][0];
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
                  lz77_get_config(3)) {
                break;
              } else if (max_match_len >=
                         lz77_get_config(2)) {
                max_check = lz77_get_config(0) >> 4;
              } else if (max_match_len >=
                         lz77_get_config(1)) {
                max_check = lz77_get_config(0) >> 2;
              }
            }
          }

          ++checked;
          if (checked >= max_check) {
            break;
          }
        }

        skip = max_match_len - 1;
        res.push_back(LZ77Item{LZ77ItemType::length,
                                  static_cast<uint16>(max_match_len)});
        res.push_back(LZ77Item{LZ77ItemType::distance,
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

uint64 LZ77Dictionary::get_hash(const size_t pos, const size_t n) const {
  constexpr auto L = HashBufferSize;
  return m_hash[(pos + n - 1) % L] -
         (pos > 0 ? m_hash[(pos + L - 1) % L] * get_pow_hash_root(n) : 0ull);
}

void LZ77Dictionary::reset() {
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

}  // namespace sz