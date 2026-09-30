#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "forgedb/core/types.hpp"
#include "forgedb/storage/record.hpp"

namespace forgedb {

enum class LogRecordType : uint32_t {
  kInvalid = 0,
  kBegin = 1,
  kCommit = 2,
  kAbort = 3,
  kInsert = 4,
  kUpdate = 5,
  kDelete = 6,
  kCheckpoint = 7,
  kCLR = 8,
};

class LogRecord {
 public:
  LogRecord() = default;

  // Header constructor (Begin, Commit, Abort, Checkpoint)
  LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type);

  // Insert / Delete constructor
  LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type, const std::string& key, const std::string& val, RID rid);

  // Update constructor
  LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type, const std::string& key, const std::string& old_val, const std::string& new_val, RID rid);

  // CLR constructor
  LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type, LSN undo_next_lsn);

  [[nodiscard]] LSN get_lsn() const noexcept { return lsn_; }
  void set_lsn(LSN lsn) noexcept { lsn_ = lsn; }

  [[nodiscard]] LSN get_prev_lsn() const noexcept { return prev_lsn_; }
  [[nodiscard]] TransactionId get_txn_id() const noexcept { return txn_id_; }
  [[nodiscard]] LogRecordType get_type() const noexcept { return type_; }
  [[nodiscard]] const std::string& get_key() const noexcept { return key_; }
  [[nodiscard]] const std::string& get_value() const noexcept { return val_; }
  [[nodiscard]] const std::string& get_old_value() const noexcept { return old_val_; }
  [[nodiscard]] const std::string& get_new_value() const noexcept { return val_; }
  [[nodiscard]] RID get_rid() const noexcept { return rid_; }
  [[nodiscard]] LSN get_undo_next_lsn() const noexcept { return undo_next_lsn_; }

  [[nodiscard]] size_t get_serialized_size() const noexcept;
  void serialize(std::span<std::byte> dst) const;
  static LogRecord deserialize(std::span<const std::byte> src);

  static constexpr size_t kHeaderSize = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint32_t);

 private:
  uint32_t size_{0};
  LSN lsn_{kInvalidLSN};
  LSN prev_lsn_{kInvalidLSN};
  TransactionId txn_id_{kInvalidTxnId};
  LogRecordType type_{LogRecordType::kInvalid};

  std::string key_;
  std::string val_;
  std::string old_val_;
  RID rid_{kInvalidRID};
  LSN undo_next_lsn_{kInvalidLSN};
};

}  // namespace forgedb
