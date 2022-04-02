#pragma once

#include <vector>

#include "sz/types.hpp"

namespace sz {

#ifdef SZ_USE_REVERSEBIT_TABLE
constexpr int BIT_REVERSE_TABLE_LEN = 15;
#endif

extern uint32 reverse_bits(uint32 payload, int n);
#ifdef BUILD_TEST
extern uint32 reverse_bits_no_table(uint32 payload, int n);
#endif

class BitFlowBuilder {
 public:
  BitFlowBuilder() : BitFlowBuilder(16 << 3) {}

  // Init the bit flow with initial size (in bits).
  BitFlowBuilder(size_t init_size)
      : m_bytes((init_size >> 3) + ((init_size & 7) != 0)), m_cur_bit(0) {
    m_cap = 1;
    while (m_cap < m_bytes.capacity()) {
      m_cap <<= 1;
    }
    m_bytes.resize(m_cap);
    m_cur_byte = &m_bytes[0];
    m_buffer_end = m_cur_byte + m_bytes.size();
  }

  // Append the lowest bit of payload to the bit flow.
  void write_bit(int payload);
  // Append the low n bits of payload to the bit flow.
  // Little end is used by default.
  void write_bits(uint32 payload, int n, bool little_end = true);
#if defined(BUILD_TEST) && defined(SZ_USE_REVERSEBIT_TABLE)
  // Comparison of not using reverse bit table.
  void write_bits_no_rev_table(uint32 payload, int n, bool little_end = true);
#endif

  // Align to byte using the bit.
  void align_to_byte(int bit);

  // Get the number of bits.
  [[nodiscard]] size_t get_bits_size() const;
  // Get the number of bytes containing all bits.
  [[nodiscard]] size_t get_bytes_size() const;

  // Export bit flow of length n to destination byte flow.
  // If n is 0, export the whole bit flow; otherwise, export n bits.
  void export_bitflow(Byte* dst, size_t n = 0);

 private:
  std::vector<Byte> m_bytes;
  size_t m_cap;
  Byte* m_cur_byte;
  Byte* m_buffer_end;
  int m_cur_bit;

  void next_byte();
  void expand();
};

}  // namespace sz
