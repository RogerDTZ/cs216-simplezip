#include "sz/zipper.hpp"

#include "util/byte_util.hpp"

#include "sz/fs.hpp"
#include "wrapper/constants.hpp"

namespace sz {

void Zipper::update_buffer() {
  m_buffer.clear();
  const size_t n = n_entries();
  std::vector<size_t> header_offset(n);

  // local file headers & data
  for (size_t i = 0; i < n; ++i) {
    header_offset[i] = m_buffer.size();
    m_entries[i].write_local_file_header(m_buffer);
    m_entries[i].write_file_block(m_buffer);
  }

  const size_t offset_cd =
      m_buffer.size();  // offset to the start of central directory
  // central directory headers
  for (size_t i = 0; i < n; ++i) {
    m_entries[i].write_central_directory_file_header(
        m_buffer, static_cast<Offset>(header_offset[i]));
  }

  write_eocd(offset_cd);

  m_buffer_ready = true;
}

bool Zipper::write(const char* filename) const {
  if (!m_buffer_ready) {
    return false;
  }
  io::write_bytes(filename, m_buffer);
  return true;
}

bool Zipper::write(const std::string& filename) const {
  return write(filename.c_str());
}

void Zipper::write_eocd(size_t off_cd) {
  const size_t cd_end = m_buffer.size();
  const size_t header_length = static_cast<size_t>(22) + get_comment_length();
  Byte* content = new Byte[header_length];
  Byte* p = &content[0];
  marshal_32(p, endof_central_directory_file_header_signature);
  marshal_16(p, 0);  // Number of this disk
  marshal_16(p, 0);  // Disk where central directory starts
  // Use the number of entries directly as number of central directory records
  marshal_16(p, static_cast<uint16>(n_entries()));
  marshal_16(p, static_cast<uint16>(n_entries()));
  marshal_32(p, static_cast<uint32>(cd_end - off_cd));
  marshal_32(p, static_cast<uint32>(off_cd));
  marshal_16(p, static_cast<uint16>(get_comment_length()));
  marshal_string(p, m_comment);

  m_buffer.insert(m_buffer.end(), content, content + header_length);
  delete[] content;
}

}  // namespace sz
