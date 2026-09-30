#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.hpp"
#include "forgedb/buffer/lru_replacer.hpp"
#include "forgedb/storage/disk_manager.hpp"

namespace forgedb {
namespace {

TEST(LRUReplacerTest, BasicVictimAndPin) {
  LRUReplacer replacer(5);

  replacer.unpin(FrameId{1});
  replacer.unpin(FrameId{2});
  replacer.unpin(FrameId{3});
  replacer.unpin(FrameId{4});
  replacer.unpin(FrameId{5});
  EXPECT_EQ(replacer.size(), 5);

  // Pin frame 3, size becomes 4
  replacer.pin(FrameId{3});
  EXPECT_EQ(replacer.size(), 4);

  // Victim should be 1 (least recently unpinned)
  FrameId victim;
  EXPECT_TRUE(replacer.victim(victim));
  EXPECT_EQ(victim, FrameId{1});
  EXPECT_EQ(replacer.size(), 3);

  // Victim should be 2
  EXPECT_TRUE(replacer.victim(victim));
  EXPECT_EQ(victim, FrameId{2});
}

class BufferPoolManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_bpm_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "test_bpm.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(BufferPoolManagerTest, NewPageAndFetch) {
  auto disk_mgr = std::make_unique<DiskManager>(db_path_);
  BufferPoolManager bpm(3, disk_mgr.get());

  PageId p0, p1, p2, p3;
  Page* page0 = bpm.new_page(p0);
  ASSERT_NE(page0, nullptr);
  EXPECT_EQ(page0->get_page_id(), p0);
  page0->write_u32(0, 100);

  Page* page1 = bpm.new_page(p1);
  ASSERT_NE(page1, nullptr);
  page1->write_u32(0, 200);

  Page* page2 = bpm.new_page(p2);
  ASSERT_NE(page2, nullptr);
  page2->write_u32(0, 300);

  // Pool is full (size 3) and all 3 pages are pinned. new_page should fail.
  Page* page_fail = bpm.new_page(p3);
  EXPECT_EQ(page_fail, nullptr);

  // Unpin p0 as dirty
  EXPECT_TRUE(bpm.unpin_page(p0, true));

  // Now new_page should succeed by evicting p0
  Page* page3 = bpm.new_page(p3);
  ASSERT_NE(page3, nullptr);
  page3->write_u32(0, 400);

  // Unpin p1, p2, p3
  EXPECT_TRUE(bpm.unpin_page(p1, true));
  EXPECT_TRUE(bpm.unpin_page(p2, true));
  EXPECT_TRUE(bpm.unpin_page(p3, true));

  // Fetch p0 back from disk
  Page* fetched_p0 = bpm.fetch_page(p0);
  ASSERT_NE(fetched_p0, nullptr);
  EXPECT_EQ(fetched_p0->read_u32(0), 100);

  EXPECT_TRUE(bpm.unpin_page(p0, false));
}

}  // namespace
}  // namespace forgedb
