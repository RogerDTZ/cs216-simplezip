#include <string>
#include <thread>

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"
#include "sz/sz.hpp"

int main(int argc, char** argv) {
  CLI::App app{"simplezip"};

  std::string target_filename;
  app.add_option("target", target_filename, "The filename of the result.")
      ->required();

  std::vector<std::string> source_filenames;
  app.add_option<std::vector<std::string>>("source", source_filenames,
                                           "The source file to be compressed.")
      ->required();

  app.add_flag("-v,--verbose", sz::log_info_switch, "Verbose mode");

  std::string arg_compress_method;
  app.add_option("-m,--method", arg_compress_method, "store | deflate");

  app.add_flag("--deflate_static", sz::deflate_use_static,
               "Use static encoding in Deflate");
  app.add_option<int>("-l,--level", sz::deflate_lz77_level,
                      "Level of LZ77 (0..3)")
      ->check(CLI::Range(0, 3));

  size_t thread_cnt = std::thread::hardware_concurrency();
  app.add_option<size_t>("-t,--thread", thread_cnt,
                         "number of threads used (for deflate)");

  CLI11_PARSE(app, argc, argv)

  sz::CompressionMethod compress_method = sz::CompressionMethod::deflate;
  try {
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

  if (compress_method == sz::CompressionMethod::deflate) {
    sz::log::log("Deflate: use ", thread_cnt, " thread(s)");
    sz::log::log("LZ77 level: ", sz::deflate_lz77_level);
  }

  auto start = std::chrono::system_clock::now();

  sz::Zipper zipper;
  for (auto&& source_filename : source_filenames) {
    sz::FileEntry file(source_filename, compress_method, thread_cnt);
    file.compress();
    zipper.add_entry(std::move(file));
  }
  zipper.update_buffer();

  auto end = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
  sz::log::log("Time used: ", std::fixed, std::setprecision(2),
               elapsed_seconds.count());

  std::cerr << "writing zip ... ";
  if (!zipper.write(target_filename)) {
    std::cerr << "fail" << std::endl;
  } else {
    std::cerr << "success" << std::endl;
  }

  return 0;
}
