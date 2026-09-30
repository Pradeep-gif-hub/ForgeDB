#include "forgedb/recovery/log_manager.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace forgedb {

LogManager::LogManager(DiskManager* disk_manager)
    : disk_manager_(disk_manager),
      log_buffer_(kLogBufferSize),
      flush_buffer_(kLogBufferSize) {
  if (disk_manager_ == nullptr) {
    throw std::invalid_argument("DiskManager cannot be null in LogManager");
  }
}

LogManager::~LogManager() {
  stop_flush_thread();
  flush();
}

void LogManager::start_flush_thread() {
  if (is_running_.exchange(true)) {
    return;
  }
  flush_thread_ = std::thread(&LogManager::flush_loop, this);
}

void LogManager::stop_flush_thread() {
  if (!is_running_.exchange(false)) {
    return;
  }
  cv_.notify_all();
  if (flush_thread_.joinable()) {
    flush_thread_.join();
  }
}

void LogManager::flush_loop() {
  while (is_running_) {
    std::unique_lock lock(latch_);
    cv_.wait_for(lock, std::chrono::milliseconds(10), [this] {
      return !is_running_ || log_buffer_offset_ > 0;
    });

    if (log_buffer_offset_ > 0) {
      swap_and_flush_buffers();
    }
  }
}

LSN LogManager::append_record(LogRecord& log_record) {
  std::unique_lock lock(latch_);
  const size_t rec_size = log_record.get_serialized_size();

  if (log_buffer_offset_ + rec_size > kLogBufferSize) {
    swap_and_flush_buffers();
  }

  uint64_t current_lsn_val = next_lsn_.fetch_add(1);
  LSN current_lsn{current_lsn_val};
  log_record.set_lsn(current_lsn);

  std::span<std::byte> dst_span(log_buffer_.data() + log_buffer_offset_, rec_size);
  log_record.serialize(dst_span);
  log_buffer_offset_ += rec_size;

  return current_lsn;
}

void LogManager::swap_and_flush_buffers() {
  if (log_buffer_offset_ == 0) {
    return;
  }

  std::memcpy(flush_buffer_.data(), log_buffer_.data(), log_buffer_offset_);
  const size_t flush_size = log_buffer_offset_;
  log_buffer_offset_ = 0;

  disk_manager_->write_log(std::span<const std::byte>(flush_buffer_.data(), flush_size));
  disk_manager_->flush_log();

  persisted_lsn_.store(next_lsn_.load() - 1);
  cv_.notify_all();
}

void LogManager::flush(LSN lsn) {
  std::unique_lock lock(latch_);
  if (lsn.is_valid() && persisted_lsn_.load() >= lsn.value()) {
    return;
  }

  swap_and_flush_buffers();
}

}  // namespace forgedb
