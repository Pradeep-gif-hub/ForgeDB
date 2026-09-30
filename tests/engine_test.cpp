#include "forgedb/engine/storage_engine.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace forgedb {
namespace {

class EngineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_engine_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "engine.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(EngineTest, BasicCrud) {
  {
    StorageEngine engine(db_path_, 16);

    EXPECT_FALSE(engine.get("key1").has_value());

    EXPECT_TRUE(engine.put("key1", "val1"));
    auto val = engine.get("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "val1");

    EXPECT_TRUE(engine.put("key1", "val1_updated"));
    auto val_updated = engine.get("key1");
    ASSERT_TRUE(val_updated.has_value());
    EXPECT_EQ(*val_updated, "val1_updated");

    EXPECT_TRUE(engine.delete_key("key1"));
    EXPECT_FALSE(engine.get("key1").has_value());
  }

  // Persistence check after restart
  {
    StorageEngine engine(db_path_, 16);
    EXPECT_TRUE(engine.put("persisted_key", "persisted_value"));
    engine.flush();
  }

  {
    StorageEngine engine(db_path_, 16);
    auto val = engine.get("persisted_key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "persisted_value");
  }
}

TEST_F(EngineTest, RangeScan) {
  StorageEngine engine(db_path_, 16);

  engine.put("user:001", "Alice");
  engine.put("user:002", "Bob");
  engine.put("user:003", "Charlie");
  engine.put("user:004", "David");

  auto results = engine.scan("user:002", "user:004");
  ASSERT_EQ(results.size(), 3U);
  EXPECT_EQ(results[0].first, "user:002");
  EXPECT_EQ(results[0].second, "Bob");
  EXPECT_EQ(results[1].first, "user:003");
  EXPECT_EQ(results[1].second, "Charlie");
  EXPECT_EQ(results[2].first, "user:004");
  EXPECT_EQ(results[2].second, "David");
}

TEST_F(EngineTest, TransactionCommitAndAbort) {
  StorageEngine engine(db_path_, 16);

  engine.put("balance:A", "1000");
  engine.put("balance:B", "500");

  // Committed transaction
  {
    auto txn = engine.begin_transaction();
    engine.put(txn.get(), "balance:A", "900");
    engine.put(txn.get(), "balance:B", "600");
    EXPECT_TRUE(engine.commit_transaction(txn.get()));

    EXPECT_EQ(engine.get("balance:A"), "900");
    EXPECT_EQ(engine.get("balance:B"), "600");
  }

  // Aborted transaction
  {
    auto txn = engine.begin_transaction();
    engine.put(txn.get(), "balance:A", "500");
    engine.put(txn.get(), "balance:B", "1000");
    EXPECT_TRUE(engine.abort_transaction(txn.get()));

    // Balances rolled back
    EXPECT_EQ(engine.get("balance:A"), "900");
    EXPECT_EQ(engine.get("balance:B"), "600");
  }
}

}  // namespace
}  // namespace forgedb
