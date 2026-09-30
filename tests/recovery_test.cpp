#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.hpp"
#include "forgedb/recovery/log_manager.hpp"
#include "forgedb/recovery/recovery_manager.hpp"
#include "forgedb/storage/disk_manager.hpp"
#include "forgedb/storage/record.hpp"

namespace forgedb {
namespace {

class RecoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_recovery_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "test_recovery.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(RecoveryTest, CrashAndRedoCommittedTransaction) {
  // Step 1: Simulate transaction commit with WAL logging before crash
  {
    auto disk_mgr = std::make_unique<DiskManager>(db_path_);
    auto bpm = std::make_unique<BufferPoolManager>(10, disk_mgr.get());
    auto log_mgr = std::make_unique<LogManager>(disk_mgr.get());

    PageId p1;
    Page* page = bpm->new_page(p1);
    ASSERT_NE(page, nullptr);
    SlottedPage::init(*page, p1);

    LogRecord begin_rec(TransactionId{1}, kInvalidLSN, LogRecordType::kBegin);
    LSN lsn1 = log_mgr->append_record(begin_rec);

    Record rec("user:10", "John Doe");
    RID rid;
    SlottedPage::insert_record(*page, rec, rid);

    LogRecord insert_rec(TransactionId{1}, lsn1, LogRecordType::kInsert, "user:10", "John Doe", rid);
    LSN lsn2 = log_mgr->append_record(insert_rec);
    page->set_page_lsn(lsn2);

    LogRecord commit_rec(TransactionId{1}, lsn2, LogRecordType::kCommit);
    LSN lsn3 = log_mgr->append_record(commit_rec);
    log_mgr->flush(lsn3);

    bpm->unpin_page(p1, true);
    // Simulate crash without flushing buffer pool to disk (destroy without flush)
  }

  // Step 2: Restart and recover
  {
    auto disk_mgr = std::make_unique<DiskManager>(db_path_);
    auto bpm = std::make_unique<BufferPoolManager>(10, disk_mgr.get());
    auto log_mgr = std::make_unique<LogManager>(disk_mgr.get());

    RecoveryManager recovery_mgr(disk_mgr.get(), bpm.get(), log_mgr.get());
    recovery_mgr.recover();

    Page* page = bpm->fetch_page(PageId{1});
    ASSERT_NE(page, nullptr);

    Record read_rec;
    bool found = SlottedPage::get_record(*page, RID{PageId{1}, 0}, read_rec);
    EXPECT_TRUE(found);
    EXPECT_EQ(read_rec.get_key(), "user:10");
    EXPECT_EQ(read_rec.get_value(), "John Doe");

    bpm->unpin_page(PageId{1}, false);
  }
}

TEST_F(RecoveryTest, UndoUncommittedTransaction) {
  // Step 1: Write uncommitted transaction to page and WAL
  {
    auto disk_mgr = std::make_unique<DiskManager>(db_path_);
    auto bpm = std::make_unique<BufferPoolManager>(10, disk_mgr.get());
    auto log_mgr = std::make_unique<LogManager>(disk_mgr.get());

    PageId p1;
    Page* page = bpm->new_page(p1);
    ASSERT_NE(page, nullptr);
    SlottedPage::init(*page, p1);

    LogRecord begin_rec(TransactionId{2}, kInvalidLSN, LogRecordType::kBegin);
    LSN lsn1 = log_mgr->append_record(begin_rec);

    Record rec("uncommitted_key", "secret");
    RID rid;
    SlottedPage::insert_record(*page, rec, rid);

    LogRecord insert_rec(TransactionId{2}, lsn1, LogRecordType::kInsert, "uncommitted_key", "secret", rid);
    LSN lsn2 = log_mgr->append_record(insert_rec);
    page->set_page_lsn(lsn2);

    log_mgr->flush(lsn2);
    bpm->unpin_page(p1, true);
    bpm->flush_all_pages();  // Uncommitted dirty page flushed to disk
  }

  // Step 2: Restart and verify Undo phase rolls back uncommitted transaction
  {
    auto disk_mgr = std::make_unique<DiskManager>(db_path_);
    auto bpm = std::make_unique<BufferPoolManager>(10, disk_mgr.get());
    auto log_mgr = std::make_unique<LogManager>(disk_mgr.get());

    RecoveryManager recovery_mgr(disk_mgr.get(), bpm.get(), log_mgr.get());
    recovery_mgr.recover();

    Page* page = bpm->fetch_page(PageId{1});
    ASSERT_NE(page, nullptr);

    Record read_rec;
    bool found = SlottedPage::get_record(*page, RID{PageId{1}, 0}, read_rec);
    // Uncommitted inserted record should be deleted/undone
    EXPECT_FALSE(found);

    bpm->unpin_page(PageId{1}, false);
  }
}

}  // namespace
}  // namespace forgedb
