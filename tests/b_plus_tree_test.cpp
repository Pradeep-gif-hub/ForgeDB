#include "forgedb/index/b_plus_tree.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.hpp"
#include "forgedb/storage/disk_manager.hpp"

namespace forgedb {
namespace {

class BPlusTreeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_bpt_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "test_bpt.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(BPlusTreeTest, InsertAndLookupSinglePage) {
  auto disk_mgr = std::make_unique<DiskManager>(db_path_);
  auto bpm = std::make_unique<BufferPoolManager>(10, disk_mgr.get());
  BPlusTree tree(bpm.get(), kInvalidPageId, 4, 4);

  EXPECT_TRUE(tree.is_empty());

  EXPECT_TRUE(tree.insert("alice", RID{PageId{1}, 0}));
  EXPECT_TRUE(tree.insert("bob", RID{PageId{1}, 1}));
  EXPECT_FALSE(tree.is_empty());

  std::vector<RID> res_alice;
  EXPECT_TRUE(tree.get_value("alice", res_alice));
  ASSERT_EQ(res_alice.size(), 1U);
  EXPECT_EQ(res_alice[0], (RID{PageId{1}, 0}));

  std::vector<RID> res_bob;
  EXPECT_TRUE(tree.get_value("bob", res_bob));
  ASSERT_EQ(res_bob.size(), 1U);
  EXPECT_EQ(res_bob[0], (RID{PageId{1}, 1}));

  std::vector<RID> res_unknown;
  EXPECT_FALSE(tree.get_value("charlie", res_unknown));
}

TEST_F(BPlusTreeTest, SplitAndScale) {
  auto disk_mgr = std::make_unique<DiskManager>(db_path_);
  auto bpm = std::make_unique<BufferPoolManager>(20, disk_mgr.get());
  // Small max_leaf_size to force multiple splits
  BPlusTree tree(bpm.get(), kInvalidPageId, 4, 4);

  for (int i = 0; i < 30; ++i) {
    std::string key = "key_" + std::to_string(i < 10 ? 0 : 0) + std::to_string(i);
    EXPECT_TRUE(tree.insert(key, RID{PageId{static_cast<uint32_t>(i + 1)}, static_cast<uint16_t>(i)}));
  }

  for (int i = 0; i < 30; ++i) {
    std::string key = "key_" + std::to_string(i < 10 ? 0 : 0) + std::to_string(i);
    std::vector<RID> res;
    EXPECT_TRUE(tree.get_value(key, res));
    ASSERT_EQ(res.size(), 1U);
    EXPECT_EQ(res[0], (RID{PageId{static_cast<uint32_t>(i + 1)}, static_cast<uint16_t>(i)}));
  }
}

TEST_F(BPlusTreeTest, RangeScan) {
  auto disk_mgr = std::make_unique<DiskManager>(db_path_);
  auto bpm = std::make_unique<BufferPoolManager>(20, disk_mgr.get());
  BPlusTree tree(bpm.get(), kInvalidPageId, 4, 4);

  tree.insert("a", RID{PageId{1}, 1});
  tree.insert("b", RID{PageId{1}, 2});
  tree.insert("c", RID{PageId{1}, 3});
  tree.insert("d", RID{PageId{1}, 4});
  tree.insert("e", RID{PageId{1}, 5});

  std::vector<std::pair<std::string, RID>> results;
  tree.scan("b", "d", results);

  ASSERT_EQ(results.size(), 3U);
  EXPECT_EQ(results[0].first, "b");
  EXPECT_EQ(results[1].first, "c");
  EXPECT_EQ(results[2].first, "d");
}

}  // namespace
}  // namespace forgedb
