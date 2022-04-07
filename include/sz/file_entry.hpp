#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sz/types.hpp"

namespace sz {

class Compressor;

class FileEntry {
 public:
  FileEntry() = delete;
  FileEntry(const char* filename, CompressionMethod method, size_t thread_cnt);
  FileEntry(const std::string& str, CompressionMethod method, size_t thread_cnt)
      : FileEntry(str.c_str(), method, thread_cnt) {}

  void compress();

  [[nodiscard]] LengthType get_name_length() const {
    return static_cast<LengthType>(m_filename.length());
  }

  [[nodiscard]] LengthType get_comment_length() const {
    return static_cast<LengthType>(m_comment.length());
  }

  [[nodiscard]] SizeType get_uncompressed_size() const {
    return static_cast<SizeType>(m_raw.size());
  }

  [[nodiscard]] SizeType get_compressed_size() const;

  void write_local_file_header(std::vector<Byte>& buffer) const;
  void write_file_block(std::vector<Byte>& buffer) const;
  void write_central_directory_file_header(std::vector<Byte>& buffer,
                                           Offset off_local_file_header) const;

 private:
  std::vector<Byte> m_raw;

  OptVersion m_ver_made;
  OptVersion m_ver_extract;
  GeneralPurpose m_general_purpose;
  CompressionMethod m_method;
  Timestamp m_last_modify_time;
  CRC32Value m_crc32;
  LengthType m_length_extra;
  DiskNumber m_disk_number;
  InternalAttr m_internal_attr;
  ExternalAttr m_external_attr;
  std::string m_filename;
  std::string m_comment;

  std::shared_ptr<Compressor> m_compressor;
};

}  // namespace sz
