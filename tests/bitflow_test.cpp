#include <algorithm>
#include <cstdlib>
#include <memory>

#include "util/bit_util.hpp"

#include "gtest/gtest.h"
#include "sz/sz.hpp"

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
