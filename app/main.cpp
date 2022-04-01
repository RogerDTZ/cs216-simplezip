#include <string>

#include "sz/sz.hpp"

inline std::pair<std::string, std::string> arg_read(int argc, char** args) {
  std::string src_filename;
  std::string dst_filename;
  if (argc < 2) {
    sz::log::panic("usage: ", args[0], " <source_filename> [target_filename] [-v | --verbose]");
  }
  src_filename = std::string{args[1]};
  if (argc >= 3) {
    dst_filename = std::string{args[2]};
  } else {
    auto suffix_pos = src_filename.find_last_of('.');
    if (suffix_pos == std::string::npos) {
      suffix_pos = src_filename.length();
    }
    dst_filename = src_filename.substr(0, suffix_pos) + std::string{".zip"};
  }
  for (int i = 1; i < argc; ++i) {
    auto a = std::string{args[i]};
    if (a == "-v" || a == "--verbose") {
      sz::log::log_info_switch = true;
    }
  }
  return std::make_pair(src_filename, dst_filename);
}


int main(int argc, char **args) {
  // Process command arguments.
  auto filename_pair = arg_read(argc, args);
  std::string src_filename{filename_pair.first};
  std::string dst_filename{filename_pair.second};

  sz::Zipper zipper;
  sz::FileEntry file(src_filename, sz::CompressionMethod::deflate);
  zipper.add_entry(file);

  zipper.update_buffer();

  std::cerr << "writing zip ... ";
  if (!zipper.write(dst_filename)) {
    std::cerr << "fail" << std::endl;
  } else {
    std::cerr << "success" << std::endl;
  }

  return 0;
}
