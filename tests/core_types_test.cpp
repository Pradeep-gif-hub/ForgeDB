#include <gtest/gtest.h>

#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"

namespace forgedb {
namespace {

TEST(CoreTypesTest, PageIdBasics) {
  PageId invalid_id;
  EXPECT_FALSE(invalid_id.is_valid());
  EXPECT_EQ(invalid_id, kInvalidPageId);

  PageId valid_id{42};
  EXPECT_TRUE(valid_id.is_valid());
  EXPECT_EQ(valid_id.value(), 42U);

  PageId id1{10};
  PageId id2{20};
  EXPECT_LT(id1, id2);
  EXPECT_LE(id1, id2);
  EXPECT_GT(id2, id1);
  EXPECT_GE(id2, id1);
  EXPECT_NE(id1, id2);

  std::stringstream ss;
  ss << valid_id;
  EXPECT_EQ(ss.str(), "PageId(42)");

  std::stringstream ss_inv;
  ss_inv << invalid_id;
  EXPECT_EQ(ss_inv.str(), "PageId(INVALID)");
}

TEST(CoreTypesTest, FrameIdBasics) {
  FrameId invalid_id;
  EXPECT_FALSE(invalid_id.is_valid());
  EXPECT_EQ(invalid_id, kInvalidFrameId);

  FrameId valid_id{3};
  EXPECT_TRUE(valid_id.is_valid());
  EXPECT_EQ(valid_id.value(), 3);

  std::stringstream ss;
  ss << valid_id;
  EXPECT_EQ(ss.str(), "FrameId(3)");
}

TEST(CoreTypesTest, TransactionIdAndLSN) {
  TransactionId txn{100};
  EXPECT_TRUE(txn.is_valid());
  EXPECT_EQ(txn.value(), 100ULL);

  LSN lsn{500};
  EXPECT_TRUE(lsn.is_valid());
  EXPECT_EQ(lsn.value(), 500ULL);

  std::stringstream ss_txn;
  ss_txn << txn;
  EXPECT_EQ(ss_txn.str(), "TxnId(100)");

  std::stringstream ss_lsn;
  ss_lsn << lsn;
  EXPECT_EQ(ss_lsn.str(), "LSN(500)");
}

TEST(CoreTypesTest, HashingInContainers) {
  std::unordered_set<PageId> page_set;
  page_set.insert(PageId{1});
  page_set.insert(PageId{2});
  page_set.insert(PageId{1});
  EXPECT_EQ(page_set.size(), 2U);

  std::unordered_map<PageId, FrameId> page_table;
  page_table[PageId{10}] = FrameId{0};
  page_table[PageId{20}] = FrameId{1};
  EXPECT_EQ(page_table[PageId{10}], FrameId{0});
  EXPECT_EQ(page_table[PageId{20}], FrameId{1});
}

}  // namespace
}  // namespace forgedb
