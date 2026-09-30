#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <string>

#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"

namespace forgedb {

class DiskManager {
 public:
  explicit DiskManager(const std::filesystem::path& db_file);
  ~DiskManager();

  DiskManager(const DiskManager&) = delete;
  DiskManager& operator=(const DiskManager&) = delete;
  DiskManager(DiskManager&&) = delete;
  DiskManager& operator=(DiskManager&&) = delete;

  // Page storage operations
  void write_page(PageId page_id, std::span<const std::byte, kPageSize> page_data);
  void read_page(PageId page_id, std::span<std::byte, kPageSize> page_data);

  [[nodiscard]] PageId allocate_page();
  void deallocate_page(PageId page_id);

  [[nodiscard]] uint32_t get_num_pages() const;
  [[nodiscard]] uint32_t get_num_flushes() const;
  [[nodiscard]] uint32_t get_num_writes() const;
  [[nodiscard]] uint32_t get_num_reads() const;

  // Write-Ahead Log operations
  void write_log(std::span<const std::byte> log_data);
  bool read_log(std::span<std::byte> buffer, size_t size, size_t offset);
  void flush_log();
  [[nodiscard]] size_t get_log_file_size();

  [[nodiscard]] PageId get_root_page_id() const;
  void set_root_page_id(PageId root_id);

  [[nodiscard]] PageId get_data_page_id() const;
  void set_data_page_id(PageId data_id);

  // Close and flush all open file streams
  void close();

 private:
  void init_db_file();
  void open_or_create_files();

  std::filesystem::path db_file_path_;
  std::filesystem::path log_file_path_;

  mutable std::fstream db_io_;
  mutable std::fstream log_io_;

  mutable std::mutex db_io_latch_;
  mutable std::mutex log_io_latch_;

  uint32_t num_pages_{0};
  uint32_t num_flushes_{0};
  uint32_t num_writes_{0};
  uint32_t num_reads_{0};

  bool is_closed_{false};
};

}  // namespace forgedb
