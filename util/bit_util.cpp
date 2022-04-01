#include "util/bit_util.hpp"

#ifdef SZ_USE_REVERSEBIT_TABLE
#include "util/table_reversebits.hpp"
#endif

namespace sz {

constexpr uint32 BYTE_MASK = (1u << 8) - 1;
constexpr uint32 GetLowBits(const uint32 x, const int n) {
  return x & ((1 << n) - 1);
}

uint32 reverse_bits(uint32 payload, const int n) {
  payload &= (1u << n) - 1;
#ifdef SZ_USE_REVERSEBIT_TABLE
  if (n <= BIT_REVERSE_TABLE_LEN) {
    return GetReverseBitTable(n)[payload];
  }
#endif
  uint32 res = 0;
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

void BitFlowBuilder::write_bits(uint32 payload, int n, const bool little_end) {
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

void BitFlowBuilder::align_to_byte(const int bit) {
  int t = 8 - m_cur_bit;
  while (t--) {
    write_bit(bit & 1);
  }
}

#if defined(BUILD_TEST) && defined(SZ_USE_REVERSEBIT_TABLE)
void BitFlowBuilder::write_bits_no_rev_table(uint32 payload, int n, const bool little_end) {
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
