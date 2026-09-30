#include "forgedb/storage/disk_manager.hpp"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "forgedb/core/config.hpp"

namespace forgedb {
namespace {

class DiskManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_disk_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "test.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(DiskManagerTest, InitializationAndHeader) {
  {
    DiskManager disk_manager(db_path_);
    EXPECT_GE(disk_manager.get_num_pages(), 1U);
  }

  EXPECT_TRUE(std::filesystem::exists(db_path_));
  EXPECT_GE(std::filesystem::file_size(db_path_), kPageSize);

  // Re-open and verify persistence of header
  {
    DiskManager disk_manager(db_path_);
    EXPECT_GE(disk_manager.get_num_pages(), 1U);
  }
}

TEST_F(DiskManagerTest, AllocateAndReadWritePages) {
  DiskManager disk_manager(db_path_);

  PageId page1 = disk_manager.allocate_page();
  EXPECT_EQ(page1.value(), 1U);

  PageId page2 = disk_manager.allocate_page();
  EXPECT_EQ(page2.value(), 2U);

  std::array<std::byte, kPageSize> write_buf{};
  write_buf[0] = static_cast<std::byte>('F');
  write_buf[1] = static_cast<std::byte>('O');
  write_buf[2] = static_cast<std::byte>('R');
  write_buf[3] = static_cast<std::byte>('G');
  write_buf[4] = static_cast<std::byte>('E');

  disk_manager.write_page(page1, write_buf);

  std::array<std::byte, kPageSize> read_buf{};
  disk_manager.read_page(page1, read_buf);

  EXPECT_EQ(read_buf[0], static_cast<std::byte>('F'));
  EXPECT_EQ(read_buf[1], static_cast<std::byte>('O'));
  EXPECT_EQ(read_buf[2], static_cast<std::byte>('R'));
  EXPECT_EQ(read_buf[3], static_cast<std::byte>('G'));
  EXPECT_EQ(read_buf[4], static_cast<std::byte>('E'));

  EXPECT_GT(disk_manager.get_num_writes(), 0U);
  EXPECT_GT(disk_manager.get_num_reads(), 0U);
}

TEST_F(DiskManagerTest, WALOperations) {
  DiskManager disk_manager(db_path_);

  std::string log_entry = "LOG_RECORD_PAYLOAD_TEST_12345";
  std::vector<std::byte> log_bytes(log_entry.size());
  for (size_t i = 0; i < log_entry.size(); ++i) {
    log_bytes[i] = static_cast<std::byte>(log_entry[i]);
  }

  disk_manager.write_log(log_bytes);
  disk_manager.flush_log();

  EXPECT_EQ(disk_manager.get_log_file_size(), log_entry.size());

  std::vector<std::byte> read_log_bytes(log_entry.size());
  bool ok = disk_manager.read_log(read_log_bytes, log_entry.size(), 0);
  EXPECT_TRUE(ok);
  EXPECT_EQ(read_log_bytes, log_bytes);
}

}  // namespace
}  // namespace forgedb
