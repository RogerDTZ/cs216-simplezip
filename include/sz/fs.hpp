#pragma once

#include <fstream>
#include <vector>
#include <sys/stat.h>

#include "sz/log.hpp"
#include "sz/types.hpp"

#ifdef WIN32
#define STAT _stat
#endif

namespace sz {

namespace io {

inline std::vector<Byte> read_bytes(const char* filename) {
  std::ifstream ifs;
  ifs.open(filename, std::ios::binary);
  if (ifs.fail()) {
    log::panic("cannot open file '", filename, "'");
  }

  ifs.seekg(0, std::ifstream::end);
  const auto size = ifs.tellg();
  ifs.seekg(0, std::ifstream::beg);

#ifdef READ_USE_FREAD
  ifs.close();
  FILE* file = nullptr;
  fopen_s(&file, filename, "rb");
  std::vector<Byte> res(size);
  if (!file) {
    log::panic("cannot open file '", filename, "'");
  } else {
    fread(&res[0], sizeof(Byte), size, file);
  }
  return res;
#else 
  std::vector<char> res(size);
  ifs.read(&res[0], size);
  ifs.close();
  return std::vector<Byte>(res.begin(), res.end());
#endif
}

inline size_t write_bytes(const char* filename,
                          const std::vector<Byte>& bytes) {
  std::ofstream ofs;
  ofs.open(filename, std::ios::binary);
  if (ofs.fail()) {
    log::panic("cannot open file '", filename, "'");
  }

  std::vector<char> conv_bytes(bytes.begin(), bytes.end());
  ofs.write(&conv_bytes[0], bytes.size());
  ofs.close();
  return bytes.size();
}

inline Timestamp get_last_modify_time(const char* filename) {
  struct STAT result{};
  if (STAT(filename, &result) == 0) {
    struct tm tm{};
    localtime_s(&tm, &result.st_mtime);
    int year = tm.tm_year + 1900 - 1980;
    int month = tm.tm_mon + 1;
    int day = tm.tm_mday;
    int hour = tm.tm_hour;
    int minute = tm.tm_min;
    int sec = tm.tm_sec;
    return Timestamp{static_cast<uint16>(hour << 11 | minute << 5 | (sec + 1) >> 1),
                     static_cast<uint16>(year << 9 | month << 5 | day)};
  }
  log::panic("cannot get modify timestamp of ", filename);
  return Timestamp{0, 0};
}

}  // namespace io

}  // namespace sz
