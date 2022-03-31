#include "compress/algo_deflate.hpp"
#include "compress/table_deflate.hpp"
#include "sz/log.hpp"

namespace sz {

void bb_write_deflate_item(const std::shared_ptr<BitFlowBuilder>& bb,
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
      break;
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
  if (base < 144) {
    bb->write_bits(0x30 + base, 8, false);
  } else if (base < 256) {
    bb->write_bits(0x190 + base - 144, 9, false);
  } else if (base < 280) {
    bb->write_bits(base - 256, 7, false);
  } else {
    bb->write_bits(0xc0 + base - 280, 8, false);
  }
  if (n_extra) {
    bb->write_bits(extra, n_extra, true);
  }
}

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

  const Byte* p = m_src;
  const Byte* q = m_src + m_src_len;
  auto bb = std::make_shared<BitFlowBuilder>(m_src_len << 3);

  bb->write_bit(1);         // last block
  bb->write_bits(0b01, 2);  // fixed huffman
  for (; p < q; ++p) {
    bb_write_deflate_item(bb, DeflateItem{DeflateItemType::literal, *p});
  }
  bb_write_deflate_item(bb, DeflateItem{DeflateItemType::stop, 0});

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

}  // namespace sz