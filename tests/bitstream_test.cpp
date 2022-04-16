#include <algorithm>
#include <cstdlib>
#include <memory>

#include "sz/sz.hpp"

#include "util/bit_util.hpp"

#include "gtest/gtest.h"

constexpr int BitStreamLen = 1 << 25;
constexpr int SingleDataMaxLen = 18;

TEST(util, BitStreamBuilder_size) {
  auto bs = std::make_shared<sz::BitStream>();

  EXPECT_EQ(bs->get_bits_size(), 0);
  EXPECT_EQ(bs->get_bytes_size(), 0);

  bs->write_bit(1);
  EXPECT_EQ(bs->get_bits_size(), 1);
  EXPECT_EQ(bs->get_bytes_size(), 1);
  bs->write_bit(0);
  EXPECT_EQ(bs->get_bits_size(), 2);
  EXPECT_EQ(bs->get_bytes_size(), 1);
  for (int i = 2; i < 8; ++i) {
    bs->write_bit(i & 1);
  }
  EXPECT_EQ(bs->get_bits_size(), 8);
  EXPECT_EQ(bs->get_bytes_size(), 1);

  bs->write_bit(0);
  EXPECT_EQ(bs->get_bits_size(), 9);
  EXPECT_EQ(bs->get_bytes_size(), 2);
}

TEST(util, BitStream_content) {
  auto bs = std::make_shared<sz::BitStream>(BitStreamLen >>
                                                 6);  // also test expanding
  int* arr = new int[BitStreamLen];
  for (int i = 0; i < BitStreamLen; ++i) {
    arr[i] = rand() % 2;
  }
  for (int i = 0, j; i < BitStreamLen; i = j + 1) {
    j = i + rand() % std::min(BitStreamLen - i, SingleDataMaxLen);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (k - i);
    }
    bs->write_bits(val, j - i + 1);
  }

  EXPECT_EQ(bs->get_bits_size(), BitStreamLen);
  EXPECT_EQ(bs->get_bytes_size(), (BitStreamLen + 7) >> 3);

  const auto res = new sz::Byte[bs->get_bytes_size()];
  bs->export_bitstream(res);
  for (int i = 0; i < BitStreamLen; ++i) {
    EXPECT_EQ((res[i >> 3] >> (i & 7)) & 1, arr[i]);
  }

  delete[] res;
  delete[] arr;
}

TEST(util, BitStream_contentrev) {
  auto bs = std::make_shared<sz::BitStream>(BitStreamLen >>
                                                 8);  // also test expanding
  int* arr = new int[BitStreamLen];
  for (int i = 0; i < BitStreamLen; ++i) {
    arr[i] = rand() % 2;
  }
  for (int i = 0, j; i < BitStreamLen; i = j + 1) {
    j = i + rand() % std::min(BitStreamLen - i, SingleDataMaxLen);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (j - k);
    }
    bs->write_bits(val, j - i + 1, false);
  }

  EXPECT_EQ(bs->get_bits_size(), BitStreamLen);
  EXPECT_EQ(bs->get_bytes_size(), (BitStreamLen + 7) >> 3);

  const auto res = new sz::Byte[bs->get_bytes_size()];
  bs->export_bitstream(res);
  for (int i = 0; i < BitStreamLen; ++i) {
    EXPECT_EQ((res[i >> 3] >> (i & 7)) & 1, arr[i]);
  }

  delete[] res;
  delete[] arr;
}

TEST(util, BitStream_append) {
  constexpr int Len1 = 48;
  constexpr int Len2 = 47;
  auto bs1 = std::make_shared<sz::BitStream>(Len1);
  auto bs2 = std::make_shared<sz::BitStream>(Len2);

  int* arr = new int[Len1 + Len2];
  for (int i = 0; i < Len1 + Len2; ++i) {
    arr[i] = rand() % 2;
  }
  for (int i = 0, j; i < Len1; i = j + 1) {
    j = i + rand() % std::min(Len1 - i, SingleDataMaxLen);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (k - i);
    }
    bs1->write_bits(val, j - i + 1);
  }
  for (int i = Len1, j; i < Len1 + Len2; i = j + 1) {
    j = i + rand() % std::min(Len1 + Len2 - i, SingleDataMaxLen);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (k - i);
    }
    bs2->write_bits(val, j - i + 1);
  }

  bs1->append(*bs2);

  const auto res = new sz::Byte[bs1->get_bytes_size()];
  bs1->export_bitstream(res);
  for (int i = 0; i < Len1 + Len2; ++i) {
    EXPECT_EQ((res[i >> 3] >> (i & 7)) & 1, arr[i]);
  }

  delete[] res;
  delete[] arr;
}
