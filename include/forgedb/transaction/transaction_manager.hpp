#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "forgedb/core/types.hpp"
#include "forgedb/recovery/log_manager.hpp"
#include "forgedb/transaction/transaction.hpp"

namespace forgedb {

class TransactionManager {
 public:
  explicit TransactionManager(LogManager* log_manager);
  ~TransactionManager() = default;

  TransactionManager(const TransactionManager&) = delete;
  TransactionManager& operator=(const TransactionManager&) = delete;

  std::shared_ptr<Transaction> begin(IsolationLevel level = IsolationLevel::kRepeatableRead);
  bool commit(Transaction* txn);
  bool abort(Transaction* txn);

  std::shared_ptr<Transaction> get_transaction(TransactionId txn_id);
  [[nodiscard]] size_t get_active_transaction_count() const;

 private:
  LogManager* log_manager_;
  std::atomic<uint64_t> next_txn_id_{1};
  std::unordered_map<TransactionId, std::shared_ptr<Transaction>> txn_map_;
  mutable std::mutex latch_;
};

}  // namespace forgedb
