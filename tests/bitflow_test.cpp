#include <algorithm>
#include <cstdlib>
#include <memory>

#include "sz/sz.hpp"

#include "util/bit_util.hpp"

#include "gtest/gtest.h"

constexpr int BitFlowLen = 1 << 25;
constexpr int SingleDataMaxLen = 18;

TEST(util, BitFlowBuilder_size) {
  auto bb = std::make_shared<sz::BitFlowBuilder>();

  EXPECT_EQ(bb->get_bits_size(), 0);
  EXPECT_EQ(bb->get_bytes_size(), 0);

  bb->write_bit(1);
  EXPECT_EQ(bb->get_bits_size(), 1);
  EXPECT_EQ(bb->get_bytes_size(), 1);
  bb->write_bit(0);
  EXPECT_EQ(bb->get_bits_size(), 2);
  EXPECT_EQ(bb->get_bytes_size(), 1);
  for (int i = 2; i < 8; ++i) {
    bb->write_bit(i & 1);
  }
  EXPECT_EQ(bb->get_bits_size(), 8);
  EXPECT_EQ(bb->get_bytes_size(), 1);

  bb->write_bit(0);
  EXPECT_EQ(bb->get_bits_size(), 9);
  EXPECT_EQ(bb->get_bytes_size(), 2);
}

TEST(util, BitFlow_content) {
  auto bb = std::make_shared<sz::BitFlowBuilder>(BitFlowLen >>
                                                 6);  // also test expanding
  int* arr = new int[BitFlowLen];
  for (int i = 0; i < BitFlowLen; ++i) {
    arr[i] = rand() % 2;
  }
  for (int i = 0, j; i < BitFlowLen; i = j + 1) {
    j = i + rand() % std::min(BitFlowLen - i, SingleDataMaxLen);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (k - i);
    }
    bb->write_bits(val, j - i + 1);
  }

  EXPECT_EQ(bb->get_bits_size(), BitFlowLen);
  EXPECT_EQ(bb->get_bytes_size(), (BitFlowLen + 7) >> 3);

  const auto res = new sz::Byte[bb->get_bytes_size()];
  bb->export_bitflow(res);
  for (int i = 0; i < BitFlowLen; ++i) {
    EXPECT_EQ((res[i >> 3] >> (i & 7)) & 1, arr[i]);
  }

  delete[] res;
  delete[] arr;
}

TEST(util, BitFlow_contentrev) {
  auto bb = std::make_shared<sz::BitFlowBuilder>(BitFlowLen >>
                                                 8);  // also test expanding
  int* arr = new int[BitFlowLen];
  for (int i = 0; i < BitFlowLen; ++i) {
    arr[i] = rand() % 2;
  }
  for (int i = 0, j; i < BitFlowLen; i = j + 1) {
    j = i + rand() % std::min(BitFlowLen - i, SingleDataMaxLen);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (j - k);
    }
    bb->write_bits(val, j - i + 1, false);
  }

  EXPECT_EQ(bb->get_bits_size(), BitFlowLen);
  EXPECT_EQ(bb->get_bytes_size(), (BitFlowLen + 7) >> 3);

  const auto res = new sz::Byte[bb->get_bytes_size()];
  bb->export_bitflow(res);
  for (int i = 0; i < BitFlowLen; ++i) {
    EXPECT_EQ((res[i >> 3] >> (i & 7)) & 1, arr[i]);
  }

  delete[] res;
  delete[] arr;
}

TEST(util, BitFlow_append) {
  constexpr int Len1 = 48;
  constexpr int Len2 = 47;
  auto bb1 = std::make_shared<sz::BitFlowBuilder>(Len1);
  auto bb2 = std::make_shared<sz::BitFlowBuilder>(Len2);

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
    bb1->write_bits(val, j - i + 1);
  }
  for (int i = Len1, j; i < Len1 + Len2; i = j + 1) {
    j = i + rand() % std::min(Len1 + Len2 - i, SingleDataMaxLen);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (k - i);
    }
    bb2->write_bits(val, j - i + 1);
  }

  bb1->append(*bb2);

  const auto res = new sz::Byte[bb1->get_bytes_size()];
  bb1->export_bitflow(res);
  for (int i = 0; i < Len1 + Len2; ++i) {
    EXPECT_EQ((res[i >> 3] >> (i & 7)) & 1, arr[i]);
  }

  delete[] res;
  delete[] arr;
}
