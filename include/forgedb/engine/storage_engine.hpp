#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.hpp"
#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"
#include "forgedb/engine/stats.hpp"
#include "forgedb/index/b_plus_tree.hpp"
#include "forgedb/recovery/log_manager.hpp"
#include "forgedb/recovery/recovery_manager.hpp"
#include "forgedb/storage/disk_manager.hpp"
#include "forgedb/transaction/transaction.hpp"
#include "forgedb/transaction/transaction_manager.hpp"

namespace forgedb {

class StorageEngine {
 public:
  explicit StorageEngine(const std::filesystem::path& db_path,
                         size_t buffer_pool_size = kDefaultBufferPoolSize);
  ~StorageEngine();

  StorageEngine(const StorageEngine&) = delete;
  StorageEngine& operator=(const StorageEngine&) = delete;

  // Single-key operations (autocommit)
  std::optional<std::string> get(const std::string& key);
  bool put(const std::string& key, const std::string& value);
  bool delete_key(const std::string& key);

  // Range scan
  std::vector<std::pair<std::string, std::string>> scan(const std::string& start_key,
                                                        const std::string& end_key);

  // Transaction API
  std::shared_ptr<Transaction> begin_transaction(IsolationLevel level = IsolationLevel::kRepeatableRead);
  bool commit_transaction(Transaction* txn);
  bool abort_transaction(Transaction* txn);

  std::optional<std::string> get(Transaction* txn, const std::string& key);
  bool put(Transaction* txn, const std::string& key, const std::string& value);
  bool delete_key(Transaction* txn, const std::string& key);

  // Maintenance & Metrics
  void checkpoint();
  void flush();
  [[nodiscard]] const EngineStats& get_stats() const noexcept { return stats_; }

 private:
  RID insert_record_into_data_pages(const std::string& key, const std::string& value);

  std::filesystem::path db_path_;
  std::unique_ptr<DiskManager> disk_manager_;
  std::unique_ptr<BufferPoolManager> bpm_;
  std::unique_ptr<LogManager> log_manager_;
  std::unique_ptr<RecoveryManager> recovery_manager_;
  std::unique_ptr<TransactionManager> txn_manager_;
  std::unique_ptr<BPlusTree> index_;

  PageId current_data_page_id_{kInvalidPageId};
  EngineStats stats_;
  mutable std::recursive_mutex engine_latch_;
};

}  // namespace forgedb
