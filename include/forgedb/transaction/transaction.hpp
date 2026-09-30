#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "forgedb/core/types.hpp"
#include "forgedb/storage/record.hpp"

namespace forgedb {

enum class TransactionState {
  kGrowing = 0,
  kShrinking = 1,
  kCommitted = 2,
  kAborted = 3,
};

enum class IsolationLevel {
  kReadUncommitted = 0,
  kReadCommitted = 1,
  kRepeatableRead = 2,
  kSerializable = 3,
};

class Transaction {
 public:
  explicit Transaction(TransactionId txn_id, IsolationLevel level = IsolationLevel::kRepeatableRead);
  ~Transaction() = default;

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  [[nodiscard]] TransactionId get_transaction_id() const noexcept { return txn_id_; }
  [[nodiscard]] TransactionState get_state() const noexcept { return state_; }
  void set_state(TransactionState state) noexcept { state_ = state; }

  [[nodiscard]] IsolationLevel get_isolation_level() const noexcept { return isolation_level_; }
  [[nodiscard]] LSN get_prev_lsn() const noexcept { return prev_lsn_; }
  void set_prev_lsn(LSN lsn) noexcept { prev_lsn_ = lsn; }

  void add_write_record(const RID& rid) { write_set_.push_back(rid); }
  [[nodiscard]] const std::vector<RID>& get_write_set() const noexcept { return write_set_; }

  void add_undo_action(std::string key, std::optional<std::string> old_value) {
    undo_set_.emplace_back(std::move(key), std::move(old_value));
  }
  [[nodiscard]] const std::vector<std::pair<std::string, std::optional<std::string>>>& get_undo_set() const noexcept {
    return undo_set_;
  }

 private:
  TransactionId txn_id_;
  TransactionState state_{TransactionState::kGrowing};
  IsolationLevel isolation_level_;
  LSN prev_lsn_{kInvalidLSN};

  std::vector<RID> write_set_;
  std::vector<std::pair<std::string, std::optional<std::string>>> undo_set_;
};

}  // namespace forgedb
