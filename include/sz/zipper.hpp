#pragma once

#include <vector>

#include "sz/file_entry.hpp"

namespace sz {

class Zipper {
 public:
  Zipper() : m_buffer_ready(false) {}

  size_t n_entries() const { return m_entries.size(); }
  size_t get_comment_length() const { return m_comment.length(); }

  void add_entry(const FileEntry& entry) { m_entries.push_back(entry); }
  void update_buffer();
  bool ready() const { return m_buffer_ready; }
  bool write(const char* filename) const;
  bool write(const std::string& filename) const;

 private:
  std::vector<FileEntry> m_entries;
  std::string m_comment;
  std::vector<Byte> m_buffer;
  bool m_buffer_ready;

  void write_eocd(size_t off_cd);
};

}  // namespace sz
