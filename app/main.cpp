#include <string>

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"
#include "sz/sz.hpp"

int main(int argc, char** argv) {
  CLI::App app{"simplezip"};

  std::string source_filename;
  app.add_option("source", source_filename, "The source file to be compressed.")
      ->required();

  std::string target_filename;
  app.add_option("target", target_filename, "The filename of the result.");

  app.add_flag("-v,--verbose", sz::log::log_info_switch, "Verbose mode");

  std::string arg_compress_method{""};
  app.add_option("-m,--method", arg_compress_method, "store | deflate");

  CLI11_PARSE(app, argc, argv)

  sz::CompressionMethod compress_method = sz::CompressionMethod::deflate;
  try {
    if (!app.get_option("target")->count()) {
      auto suffix_pos = source_filename.find_last_of('.');
      if (suffix_pos == std::string::npos) {
        suffix_pos = source_filename.length();
      }
      target_filename =
          source_filename.substr(0, suffix_pos) + std::string{".zip"};
    }

    if (arg_compress_method.empty()) {
      compress_method = sz::CompressionMethod::deflate;
    } else if (arg_compress_method == "store") {
      compress_method = sz::CompressionMethod::none;
    } else if (arg_compress_method == "deflate") {
      compress_method = sz::CompressionMethod::deflate;
    } else {
      throw CLI::ParseError(
          "unrecognized compress method: " + arg_compress_method, 1);
    }
  } catch (const CLI::ParseError& e) {
    sz::log::panic(e.what());
  }

  sz::Zipper zipper;
  const sz::FileEntry file(source_filename, compress_method);
  zipper.add_entry(file);

  zipper.update_buffer();

  std::cerr << "writing zip ... ";
  if (!zipper.write(target_filename)) {
    std::cerr << "fail" << std::endl;
  } else {
    std::cerr << "success" << std::endl;
  }

  return 0;
}
