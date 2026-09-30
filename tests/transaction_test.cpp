#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

#include "forgedb/recovery/log_manager.hpp"
#include "forgedb/storage/disk_manager.hpp"
#include "forgedb/transaction/transaction_manager.hpp"

namespace forgedb {
namespace {

class TransactionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_txn_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "test_txn.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(TransactionTest, BeginAndCommit) {
  auto disk_mgr = std::make_unique<DiskManager>(db_path_);
  auto log_mgr = std::make_unique<LogManager>(disk_mgr.get());
  TransactionManager txn_mgr(log_mgr.get());

  EXPECT_EQ(txn_mgr.get_active_transaction_count(), 0);

  auto txn = txn_mgr.begin();
  ASSERT_NE(txn, nullptr);
  EXPECT_EQ(txn->get_state(), TransactionState::kGrowing);
  EXPECT_EQ(txn_mgr.get_active_transaction_count(), 1);

  EXPECT_TRUE(txn_mgr.commit(txn.get()));
  EXPECT_EQ(txn->get_state(), TransactionState::kCommitted);
  EXPECT_EQ(txn_mgr.get_active_transaction_count(), 0);
}

TEST_F(TransactionTest, BeginAndAbort) {
  auto disk_mgr = std::make_unique<DiskManager>(db_path_);
  auto log_mgr = std::make_unique<LogManager>(disk_mgr.get());
  TransactionManager txn_mgr(log_mgr.get());

  auto txn = txn_mgr.begin();
  ASSERT_NE(txn, nullptr);
  EXPECT_EQ(txn_mgr.get_active_transaction_count(), 1);

  EXPECT_TRUE(txn_mgr.abort(txn.get()));
  EXPECT_EQ(txn->get_state(), TransactionState::kAborted);
  EXPECT_EQ(txn_mgr.get_active_transaction_count(), 0);
}

}  // namespace
}  // namespace forgedb
