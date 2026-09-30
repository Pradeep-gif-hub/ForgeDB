#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "forgedb/concurrency/thread_pool.hpp"
#include "forgedb/engine/storage_engine.hpp"

namespace forgedb {
namespace {

class ConcurrencyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_concurrent_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "concurrent.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(ConcurrencyTest, MultithreadedPutsAndGets) {
  StorageEngine engine(db_path_, 64);
  ThreadPool pool(8);

  constexpr int kNumTasks = 200;
  std::vector<std::future<void>> futures;

  for (int i = 0; i < kNumTasks; ++i) {
    futures.push_back(pool.submit([&engine, i] {
      std::string key = "key:" + std::to_string(i);
      std::string val = "value:" + std::to_string(i * 10);
      engine.put(key, val);

      auto read_val = engine.get(key);
      EXPECT_TRUE(read_val.has_value());
      if (read_val.has_value()) {
        EXPECT_EQ(*read_val, val);
      }
    }));
  }

  for (auto& f : futures) {
    f.get();
  }

  for (int i = 0; i < kNumTasks; ++i) {
    std::string key = "key:" + std::to_string(i);
    std::string expected_val = "value:" + std::to_string(i * 10);
    auto read_val = engine.get(key);
    ASSERT_TRUE(read_val.has_value());
    EXPECT_EQ(*read_val, expected_val);
  }
}

}  // namespace
}  // namespace forgedb
