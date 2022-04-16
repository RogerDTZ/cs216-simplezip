#include <algorithm>
#include <cassert>
#include <vector>

#include "sz/log.hpp"

#include "compress/cps_deflate.hpp"

namespace sz {

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

void HuffmanTree::write_to_bs(const std::shared_ptr<BitStream>& bs,
                              const uint64 x) const {
  auto [payload, n] = encode(x);
  bs->write_bits(payload, n, false);
}

int HuffmanTree::get_last_code() const {
  for (int i = m_max_code - 1; i >= 0; --i) {
    if (m_dep[i]) {
      return i;
    }
  }
  return -1;
}

}  // namespace sz