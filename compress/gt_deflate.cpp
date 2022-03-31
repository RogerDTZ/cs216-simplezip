#include <fstream>
#include <iomanip>

constexpr int ItemPerLine = 4;
constexpr int OutputWidth = 4;

void print_tuple(std::ofstream& ofs, int a, int b, int c) {
  // clang-format off
  ofs << "{"
      << "0x" << std::setfill('0') << std::setw(OutputWidth) << std::right << std::hex << a << ", "
      << "0x" << std::setfill('0') << std::setw(OutputWidth) << std::right << std::hex << b << ", "
      << "0x" << std::setfill('0') << std::setw(OutputWidth) << std::right << std::hex << c << "}, ";
  // clang-format on
}

int main(int argc, char** argv) {
  if (argc < 2) {
    return 1;
  }

  std::ofstream ofs(argv[1], std::ios_base::out);
  const bool file_opened = ofs.is_open();
  if (file_opened) {
    ofs << "#pragma once" << std::endl << std::endl;
    ofs << "namespace sz {" << std::endl;

    // Literal
    ofs << std::endl
        << std::dec
        << "constexpr uint16 DeflateLiteralTable[256][3] = {  // Code | "
           "#ExtraBits | "
           "ExtraBits\n";
    for (int i = 0; i < 256; i += ItemPerLine) {
      ofs << "  ";
      for (int j = 0; j < ItemPerLine && i + j < 256; ++j) {
        const int x = i + j;
        print_tuple(ofs, x, 0, 0);
      }
      ofs << std::endl;
    }
    ofs << "};" << std::endl << std::endl;

    // Length
    ofs << std::endl
        << std::dec
        << "constexpr uint16 DeflateLengthTable[259][3] = {  // Code | "
           "#ExtraBits | "
           "ExtraBits\n";
    for (int i = 0; i < 259; i += 4) {
      ofs << "  ";
      for (int j = 0; j < 4 && i + j < 259; ++j) {
        const int x = i + j;
        int a, b, c;
        if (x < 3) {
          a = b = c = 0;
        } else if (x <= 10) {
          a = 257 + x - 3;
          b = 0;
          c = 0;
        } else if (x <= 18) {
          a = 265 + ((x - 11) >> 1);
          b = 1;
          c = (x - 11) & 1;
        } else if (x <= 34) {
          a = 269 + ((x - 19) >> 2);
          b = 2;
          c = (x - 19) & 3;
        } else if (x <= 66) {
          a = 273 + ((x - 35) >> 3);
          b = 3;
          c = (x - 35) & 7;
        } else if (x <= 130) {
          a = 277 + ((x - 67) >> 4);
          b = 4;
          c = (x - 67) & 15;
        } else if (x <= 257) {
          a = 281 + ((x - 131) >> 5);
          b = 5;
          c = (x - 131) & 31;
        } else {  // x == 258
          a = 285;
          b = 0;
          c = 0;
        }
        print_tuple(ofs, a, b, c);
      }
      ofs << std::endl;
    }
    ofs << "};" << std::endl << std::endl;

    // Distance
    ofs << std::endl
        << std::dec
        << "constexpr uint16 DeflateDistanceTable[32769][3] = {  // Code | "
           "#ExtraBits | "
           "ExtraBits\n";
    constexpr int distance_div[14][2] = {
        {0, 1},     {4, 5},     {6, 9},     {8, 17},     {10, 33},
        {12, 65},   {14, 129},  {16, 257},  {18, 513},   {20, 1025},
        {22, 2049}, {24, 4097}, {26, 8193}, {28, 16385},
    };
    for (int i = 0; i <= 32768; i += 4) {
      ofs << "  ";
      for (int j = 0; j < 4 && i + j <= 32768; ++j) {
        const int x = i + j;
        int a, b, c;
        a = b = c = 0;
        if (x > 0) {
          while (b < 14 && distance_div[b][1] <= x) {
            ++b;
          }
          --b;
          a = distance_div[b][0] + ((x - distance_div[b][1]) >> b);
          c = (x - distance_div[b][1]) & ((1 << b) - 1);
        }
        print_tuple(ofs, a, b, c);
      }
      ofs << std::endl;
    }
    ofs << "};" << std::endl << std::endl;

    ofs << std::endl << "}" << std::endl;
  }

  return file_opened ? 0 : 1;
}