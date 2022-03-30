#include <algorithm>
#include <cstdlib>
#include <memory>
#include <queue>

#include "util/bit_util.hpp"

#include "gtest/gtest.h"
#include "sz/sz.hpp"

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
  auto bb = std::make_shared<sz::BitFlowBuilder>(0);
  constexpr int BIT_LEN = 32877;
  auto arr = new int[BIT_LEN];
  for (int i = 0; i < BIT_LEN; ++i) {
    arr[i] = rand() % 2;
  }
  for (int i = 0, j; i < BIT_LEN; i = j + 1) {
    j = i + rand() % std::min(BIT_LEN - i, 18);
    sz::uint32 val = 0;
    for (int k = i; k <= j; ++k) {
      val |= arr[k] << (k - i);
    }
    bb->write_bits(val, j - i + 1);
  }

  EXPECT_EQ(bb->get_bits_size(), BIT_LEN);
  EXPECT_EQ(bb->get_bytes_size(), (BIT_LEN + 7) >> 3);

  auto res = new sz::Byte[bb->get_bytes_size()];
  bb->export_bitflow(res);
  for (int i = 0; i < BIT_LEN; ++i) {
    EXPECT_EQ((res[i >> 3] >> (i & 7)) & 1, arr[i]);
  }

  delete[] res;
  delete[] arr;
}
