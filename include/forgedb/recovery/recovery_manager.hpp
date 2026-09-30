#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.hpp"
#include "forgedb/core/types.hpp"
#include "forgedb/recovery/log_manager.hpp"
#include "forgedb/recovery/log_record.hpp"
#include "forgedb/storage/disk_manager.hpp"

namespace forgedb {

class RecoveryManager {
 public:
  RecoveryManager(DiskManager* disk_manager,
                  BufferPoolManager* buffer_pool_manager,
                  LogManager* log_manager);
  ~RecoveryManager() = default;

  RecoveryManager(const RecoveryManager&) = delete;
  RecoveryManager& operator=(const RecoveryManager&) = delete;

  // ARIES 3-Phase Recovery
  void recover();

  // Checkpointing
  void checkpoint();

  [[nodiscard]] const std::unordered_map<TransactionId, LSN>& get_active_txns() const noexcept {
    return active_txn_table_;
  }

 private:
  void analysis_phase(std::vector<LogRecord>& log_records);
  void redo_phase(const std::vector<LogRecord>& log_records);
  void undo_phase(const std::vector<LogRecord>& log_records);

  DiskManager* disk_manager_;
  BufferPoolManager* bpm_;
  LogManager* log_manager_;

  std::unordered_map<TransactionId, LSN> active_txn_table_;
  std::unordered_map<PageId, LSN> dirty_page_table_;
  LSN redo_start_lsn_{kInvalidLSN};
};

}  // namespace forgedb
