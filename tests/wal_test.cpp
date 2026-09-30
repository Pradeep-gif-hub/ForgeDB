#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "forgedb/recovery/log_manager.hpp"
#include "forgedb/recovery/log_record.hpp"
#include "forgedb/storage/disk_manager.hpp"

namespace forgedb {
namespace {

class WALTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_wal_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "test_wal.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(WALTest, LogRecordSerialization) {
  LogRecord insert_rec(TransactionId{1}, LSN{0}, LogRecordType::kInsert, "user:1", "Alice", RID{PageId{2}, 3});
  insert_rec.set_lsn(LSN{10});

  size_t size = insert_rec.get_serialized_size();
  std::vector<std::byte> buf(size);
  insert_rec.serialize(buf);

  LogRecord restored = LogRecord::deserialize(buf);
  EXPECT_EQ(restored.get_lsn(), LSN{10});
  EXPECT_EQ(restored.get_txn_id(), TransactionId{1});
  EXPECT_EQ(restored.get_type(), LogRecordType::kInsert);
  EXPECT_EQ(restored.get_key(), "user:1");
  EXPECT_EQ(restored.get_value(), "Alice");
  EXPECT_EQ(restored.get_rid(), (RID{PageId{2}, 3}));
}

TEST_F(WALTest, AppendAndFlushLogs) {
  auto disk_mgr = std::make_unique<DiskManager>(db_path_);
  LogManager log_mgr(disk_mgr.get());

  LogRecord b1(TransactionId{1}, kInvalidLSN, LogRecordType::kBegin);
  LSN lsn1 = log_mgr.append_record(b1);
  EXPECT_EQ(lsn1, LSN{1});

  LogRecord r1(TransactionId{1}, lsn1, LogRecordType::kInsert, "k1", "v1", RID{PageId{1}, 0});
  LSN lsn2 = log_mgr.append_record(r1);
  EXPECT_EQ(lsn2, LSN{2});

  LogRecord c1(TransactionId{1}, lsn2, LogRecordType::kCommit);
  LSN lsn3 = log_mgr.append_record(c1);
  EXPECT_EQ(lsn3, LSN{3});

  log_mgr.flush(lsn3);
  EXPECT_GE(log_mgr.get_persisted_lsn(), lsn3);
}

}  // namespace
}  // namespace forgedb
