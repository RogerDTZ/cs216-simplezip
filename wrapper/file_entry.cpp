#include "sz/file_entry.hpp"

#include <cassert>

#include "util/byte_util.hpp"

#include "compress/algo_deflate.hpp"
#include "compress/algo_store.hpp"
#include "crc/crc32.hpp"
#include "sz/fs.hpp"
#include "sz/log.hpp"
#include "wrapper/constants.hpp"
#include "wrapper/version.hpp"

namespace sz {

FileEntry::FileEntry(const char* filename, CompressionMethod method)
    : m_raw(io::read_bytes(filename)),
      m_ver_made{Version},
      m_ver_extract{ExtractVersion},
      m_general_purpose{0},
      m_method{method},
      m_last_modify_time{io::get_last_modify_time(filename)},
      m_length_extra{0},
      m_disk_number{0},
      m_internal_attr{0},
      m_external_attr{0},
      m_filename{filename} {
  m_crc32 = crc32::calculate(&m_raw[0], m_raw.size());
  switch (m_method) {
    case CompressionMethod::none:
      m_compressor = std::make_shared<StoreCompressor>();
      break;
    case CompressionMethod::deflate:
      m_compressor =
          std::make_shared<DeflateCompressor>(DeflateCodingType::static_coding);
      break;
  }
  m_compressor->feed(&m_raw[0], m_raw.size());
  m_compressor->compress();
}

void FileEntry::write_local_file_header(std::vector<Byte>& buffer) const {
  const size_t header_length =
      static_cast<size_t>(30) + get_name_length() + m_length_extra;
  Byte* content = new Byte[header_length];
  Byte* p = &content[0];

  marshal_32(p, local_file_header_signature);
  marshal_16(p, m_ver_extract);
  marshal_16(p, m_general_purpose);
  marshal_16(p, static_cast<uint16>(m_method));
  marshal_16(p, m_last_modify_time.time);
  marshal_16(p, m_last_modify_time.date);
  marshal_32(p, m_crc32);
  marshal_32(p, get_compressed_size());
  marshal_32(p, get_uncompressed_size());
  marshal_16(p, get_name_length());
  marshal_16(p, m_length_extra);
  marshal_string(p, m_filename);
  assert(m_length_extra == 0);  // No extra field in current implementation

  buffer.insert(buffer.end(), content, content + header_length);
  delete[] content;
}

void FileEntry::write_file_block(std::vector<Byte>& buffer) const {
  size_t ed = buffer.size();
  buffer.resize(ed + m_compressor->get_length_compressed());
  m_compressor->write_result(&buffer[ed]);
}

void FileEntry::write_central_directory_file_header(
    std::vector<Byte>& buffer, Offset off_local_file_header) const {
  const size_t header_length = static_cast<size_t>(46) + get_name_length() +
                               m_length_extra + get_comment_length();
  Byte* content = new Byte[header_length];
  Byte* p = &content[0];

  marshal_32(p, central_directory_file_header_signature);
  marshal_16(p, m_ver_made);
  marshal_16(p, m_ver_extract);
  marshal_16(p, m_general_purpose);
  marshal_16(p, static_cast<uint16>(m_method));
  marshal_16(p, m_last_modify_time.time);
  marshal_16(p, m_last_modify_time.date);
  marshal_32(p, m_crc32);
  marshal_32(p, get_compressed_size());
  marshal_32(p, get_uncompressed_size());
  marshal_16(p, get_name_length());
  marshal_16(p, m_length_extra);
  marshal_16(p, get_comment_length());
  marshal_16(p, m_disk_number);
  marshal_16(p, m_internal_attr);
  marshal_32(p, m_external_attr);
  marshal_32(p, off_local_file_header);
  marshal_string(p, m_filename);
  assert(m_length_extra == 0);  // No extra field in current implementation
  marshal_string(p, m_comment);

  buffer.insert(buffer.end(), content, content + header_length);
  delete[] content;
}

}  // namespace sz
