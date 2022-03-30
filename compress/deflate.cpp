#include <vector>

#include "compress/algo_deflate.hpp"
#include "sz/log.hpp"

namespace sz {

DeflateCompressor::DeflateCompressor(DeflateCodingType coding_type)
    : m_coding_type(coding_type) {
  if (m_coding_type == DeflateCodingType::dynamic_coding) {
    log::log("Dynamic coding not supported yet, use static coding instead.");
    m_coding_type = DeflateCodingType::static_coding;
  }
}

size_t DeflateCompressor::compress() {
  if (m_finish) {
    return get_length_compressed();
  }
  const Byte* p = m_src;
  const Byte* q = m_src + m_src_len;
}

}  // namespace sz
