#include <fstream>
#include <iomanip>
#include <vector>

#include "bit_util.hpp"

constexpr int ItemPerLine = 8;
constexpr int OutputWidth = 4;

int main(int argc, char** argv) {
  if (argc < 2) {
    return 1;
  }

  static_assert((OutputWidth << 2) >= sz::BIT_REVERSE_TABLE_LEN);

  std::ofstream ofs(argv[1], std::ios_base::out);
  const bool file_opened = ofs.is_open();
  if (file_opened) {
    ofs << "#pragma once" << std::endl << std::endl;
    ofs << R"(#include "sz/types.hpp")" << std::endl << std::endl;
    ofs << "namespace sz {" << std::endl;
    for (int bit = 1; bit <= sz::BIT_REVERSE_TABLE_LEN; ++bit) {
      ofs << std::endl;
      const auto len = 1ull << bit;
      ofs << "constexpr uint32 BitReverseTable_" << std::dec << bit << "["
          << len << "] = {" << std::endl;
      std::vector<int> vec(len);
      for (size_t i = 0; i < len; ++i) {
        vec[i] = static_cast<int>(i);
      }
      for (size_t i = 1; i < len; ++i) {
        vec[i] = (vec[i >> 1] >> 1) | static_cast<int>(i & 1) << (bit - 1);
      }
      for (size_t i = 0; i < len; i += ItemPerLine) {
        ofs << "  ";
        for (int j = 0; j < ItemPerLine && i + j < len; ++j) {
          ofs << "0x" << std::setfill('0') << std::setw(OutputWidth)
              << std::right << std::hex << vec[i + j] << ", ";
        }
        ofs << std::endl;
      }
      ofs << "};" << std::endl;
    }
    ofs << std::endl;

    ofs << std::dec << "constexpr const uint32 * GetReverseBitTable(int n) {\n  switch (n) "
           "{\n";
    for (int i = 1; i <= sz::BIT_REVERSE_TABLE_LEN; ++i) {
      ofs << "    case " << i << ":\n      return BitReverseTable_" << i
          << ";\n";
    }
    ofs << "    default:\n      return nullptr;\n}\n}\n";
    ofs << std::endl << "}" << std::endl;
  }

  return file_opened ? 0 : 1;
}