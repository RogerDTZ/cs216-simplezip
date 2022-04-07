#include "util/bit_util.hpp"

#ifdef SZ_USE_REVERSEBIT_TABLE
#include "util/table_reversebits.hpp"
#endif

namespace sz {

constexpr uint32 BYTE_MASK = (1u << 8) - 1;

constexpr uint64 GetLowBits(const uint64 x, const int n) {
  if (n < 64) {
    return x & ((1ull << n) - 1);
  }
  return x;
}

uint64 reverse_bits(uint64 payload, const int n) {
  payload &= (1u << n) - 1;
#ifdef SZ_USE_REVERSEBIT_TABLE
  if (n <= BIT_REVERSE_TABLE_LEN) {
    return GetReverseBitTable(n)[payload];
  }
#endif
  uint64 res = 0;
  for (int i = n - 1; i >= 0; --i) {
    res |= (payload & 1) << i;
    payload >>= 1;
  }
  return res;
}

void BitFlowBuilder::write_bit(const int payload) {
  *m_cur_byte |= (payload & 1) << m_cur_bit++;
  if (m_cur_bit == 8) {
    next_byte();
  }
}

void BitFlowBuilder::write_bits(uint64 payload, int n, const bool little_end) {
  if (!little_end) {
    payload = reverse_bits(payload, n);
  }
  if (n < 8 - m_cur_bit) {
    *m_cur_byte |= GetLowBits(payload, n) << m_cur_bit;
    m_cur_bit += n;
    return;
  }
  *m_cur_byte |= GetLowBits(payload, 8 - m_cur_bit) << m_cur_bit;
  payload >>= 8 - m_cur_bit;
  n -= 8 - m_cur_bit;
  next_byte();
  while (n >= 8) {
    *m_cur_byte = payload & BYTE_MASK;
    next_byte();
    payload >>= 8;
    n -= 8;
  }
  if (n) {
    *m_cur_byte |= GetLowBits(payload, n);
    m_cur_bit = n;
  }
}

void BitFlowBuilder::append(const BitFlowBuilder& rhs) {
  // Fill the first byte's high 8 - offset bits with low 8 - offset bits of
  // rhs's first byte. Each following byte consists of high offset bits of
  // rhs's current byte and low 8 - offset bits of rhs's next byte.
  size_t remain = rhs.get_bits_size();
  if (!remain) {
    return;
  }
  const int offset = m_cur_bit;
  if (!offset) {
    const Byte* p = &rhs.m_bytes[0];
    while (remain >= 8) {
      write_bits(*p, 8);
      ++p;
      remain -= 8;
    }
    if (remain) {
      for (int i = 0; i < static_cast<int>(remain); ++i) {
        write_bit(*p >> i & 1);
      }
    }
  } else {
    if (remain >= static_cast<size_t>(8) - offset) {
      *m_cur_byte |= GetLowBits(rhs.m_bytes[0], 8 - offset) << offset;
      remain -= static_cast<size_t>(8) - offset;
      next_byte();
    } else {
      for (size_t i = 0; i < remain; ++i) {
        write_bit((rhs.m_bytes[0] >> i) & 1);
      }
      return;
    }
    const Byte* p = &rhs.m_bytes[0];
    for (; remain >= 8; remain -= 8) {
      *m_cur_byte = static_cast<Byte>(
          (*p >> (8 - offset)) | (GetLowBits(*(p + 1), 8 - offset) << offset));
      next_byte();
      ++p;
    }
    for (int i = 0; i < static_cast<int>(remain); ++i) {
      if (i < offset) {
        write_bit(*p >> (8 - offset + i) & 1);
      } else {
        write_bit(*(p + 1) >> (i - offset) & 1);
      }
    }
  }
}

void BitFlowBuilder::align_to_byte(const int bit) {
  int t = 8 - m_cur_bit;
  while (t--) {
    write_bit(bit & 1);
  }
}

#if defined(BUILD_TEST) && defined(SZ_USE_REVERSEBIT_TABLE)
void BitFlowBuilder::write_bits_no_rev_table(uint32 payload, int n,
                                             const bool little_end) {
  if (!little_end) {
    payload = reverse_bits(payload, n);
  }
  if (n < 8 - m_cur_bit) {
    *m_cur_byte |= GetLowBits(payload, n) << m_cur_bit;
    m_cur_bit += n;
    return;
  }
  *m_cur_byte |= GetLowBits(payload, 8 - m_cur_bit) << m_cur_bit;
  payload >>= 8 - m_cur_bit;
  n -= 8 - m_cur_bit;
  next_byte();
  while (n >= 8) {
    *m_cur_byte = payload & BYTE_MASK;
    next_byte();
    payload >>= 8;
    n -= 8;
  }
  if (n) {
    *m_cur_byte |= GetLowBits(payload, n);
    m_cur_bit = n;
  }
}
#endif

size_t BitFlowBuilder::get_bits_size() const {
  return (m_cur_byte - &m_bytes[0]) << 3 | m_cur_bit;
}

size_t BitFlowBuilder::get_bytes_size() const {
  return m_cur_byte - &m_bytes[0] + (m_cur_bit != 0);
}

void BitFlowBuilder::export_bitflow(Byte* dst, size_t n) {
  if (n == 0) {
    memcpy(dst, &m_bytes[0], sizeof(Byte) * get_bytes_size());
  } else {
    memcpy(dst, &m_bytes[0], sizeof(Byte) * ((n >> 3) + ((n & 7) != 0)));
  }
}

void BitFlowBuilder::next_byte() {
  m_cur_byte++;
  m_cur_bit = 0;
  if (m_cur_byte == m_buffer_end) {
    expand();
  }
}

void BitFlowBuilder::expand() {
  size_t size = m_cur_byte - &m_bytes[0];
  m_cap <<= 1;
  m_bytes.resize(m_cap);
  m_cur_byte = &m_bytes[0] + size;
  m_buffer_end = &m_bytes[0] + m_cap;
}

}  // namespace sz
