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

    auto dict = std::make_shared<sz::DeflateDictionary>();
    auto res = std::make_shared<std::vector<sz::DeflateItem>>();
    sz::ProgressBar bar(std::string("Deflate: "), &std::cerr, src.size(), 40,
                        ' ', '=', '>');
    bar.set_display(true);
    dict->calc(&src[0], src.size(), *res, bar);
    bar.set_full();
    bar.set_display(false);

    int cur = 0;
    for (size_t i = 0; i < res->size(); ++i) {
      const auto& item = (*res)[i];
      if (item.type == sz::DeflateItemType::literal) {
        EXPECT_EQ(src[cur], item.val);
        ++cur;
      } else {
        EXPECT_EQ(item.type, sz::DeflateItemType::length);
        EXPECT_LT(i + 1, res->size());
        const auto& next_item = (*res)[i + 1];
        EXPECT_EQ(item.type, sz::DeflateItemType::length);
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
