#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"
#include "forgedb/recovery/log_record.hpp"
#include "forgedb/storage/disk_manager.hpp"

namespace forgedb {

class LogManager {
 public:
  static constexpr size_t kLogBufferSize = 64 * 1024;  // 64 KB

  explicit LogManager(DiskManager* disk_manager);
  ~LogManager();

  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;

  LSN append_record(LogRecord& log_record);
  void flush(LSN lsn = kInvalidLSN);

  [[nodiscard]] LSN get_next_lsn() const noexcept { return LSN{next_lsn_.load()}; }
  [[nodiscard]] LSN get_persisted_lsn() const noexcept { return LSN{persisted_lsn_.load()}; }

  void start_flush_thread();
  void stop_flush_thread();

 private:
  void swap_and_flush_buffers();
  void flush_loop();

  DiskManager* disk_manager_;
  std::atomic<uint64_t> next_lsn_{1};
  std::atomic<uint64_t> persisted_lsn_{0};

  std::vector<std::byte> log_buffer_;
  std::vector<std::byte> flush_buffer_;
  size_t log_buffer_offset_{0};

  mutable std::mutex latch_;
  std::condition_variable cv_;
  std::condition_variable append_cv_;

  std::thread flush_thread_;
  std::atomic<bool> is_running_{false};
};

}  // namespace forgedb
