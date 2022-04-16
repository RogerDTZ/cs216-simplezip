#include <fstream>

#include "compress/cps_deflate.hpp"

#include "gtest/gtest.h"

TEST(defalte, dictionary) {
  size_t tot_len[] = {100, 10000, 32767, 1 << 20};
  size_t c[] = {2, 20, 256, 20};
  for (int t = 0; t < 4; ++t) {
    std::vector<sz::Byte> src(tot_len[t]);
    for (size_t i = 0; i < tot_len[t]; ++i) {
      src[i] = static_cast<sz::Byte>(static_cast<size_t>(rand()) % c[t]);
    }

    auto dict = std::make_shared<sz::LZ77Dictionary>();
    auto res = std::make_shared<std::vector<sz::LZ77Item>>();
    sz::ProgressBar bar(std::string("Deflate: "), &std::cerr, src.size(), 40,
                        ' ', '=', '>');
    bar.set_display(true);
    dict->calc(&src[0], src.size(), *res, bar);
    bar.set_full();
    bar.set_display(false);

    int cur = 0;
    for (size_t i = 0; i < res->size(); ++i) {
      const auto& item = (*res)[i];
      if (item.type == sz::LZ77ItemType::literal) {
        EXPECT_EQ(src[cur], item.val);
        ++cur;
      } else {
        EXPECT_EQ(item.type, sz::LZ77ItemType::length);
        EXPECT_LT(i + 1, res->size());
        const auto& next_item = (*res)[i + 1];
        EXPECT_EQ(item.type, sz::LZ77ItemType::length);
        const auto len = item.val;
        const auto distance = next_item.val;
        EXPECT_GE(cur, distance);
        EXPECT_LE(cur + len, src.size());
        EXPECT_GE(len, 3);
        EXPECT_LE(len, sz::DeflateRepeatLenMax);
        const int off = cur - distance;
        for (int j = 0; j < len; ++j) {
          EXPECT_EQ(src[off + j], src[cur + j]);
        }
        cur += len;
        ++i;
      }
    }
    EXPECT_EQ(cur, src.size());
  }
}

class RunLengthCodeTest : public testing::TestWithParam<int> {
 protected:
  int n;
};

TEST_P(RunLengthCodeTest, encode_decode) {
  n = GetParam();
  std::vector<int> data(n);
  for (int i = 0, j; i < n; i = j + 1) {
    j = i + rand() % std::min(n - i, 200);
    const int x = rand() % 16;
    for (int k = i; k <= j; ++k) {
      data[k] = x;
    }
  }

  auto code = sz::run_length_encode(data);

  std::vector<int> decoded;
  for (auto&& item : code) {
    auto [x, y] = sz::run_length_decode(item);
    if (x <= 15) {
      decoded.push_back(x);
    } else if (x == 16) {
      EXPECT_FALSE(decoded.empty());
      const auto std_val = decoded.back();
      EXPECT_TRUE(3 <= y && y <= 6);
      while (y--) {
        decoded.push_back(std_val);
      }
    } else if (x == 17) {
      EXPECT_TRUE(3 <= y && y <= 10);
      while (y--) {
        decoded.push_back(0);
      }
    } else {
      EXPECT_EQ(x, 18);
      EXPECT_TRUE(11 <= y && y <= 138);
      while (y--) {
        decoded.push_back(0);
      }
    }
  }

  EXPECT_EQ(data.size(), decoded.size());
  for (size_t i = 0; i < data.size(); ++i) {
    EXPECT_EQ(data[i], decoded[i]);
  }
}

INSTANTIATE_TEST_CASE_P(DeflateTest, RunLengthCodeTest,
                        testing::Values(100, 1000, 10000, 100000));
