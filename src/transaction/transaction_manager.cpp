#include "forgedb/transaction/transaction_manager.hpp"

#include <stdexcept>

namespace forgedb {

TransactionManager::TransactionManager(LogManager* log_manager) : log_manager_(log_manager) {
  if (log_manager_ == nullptr) {
    throw std::invalid_argument("LogManager cannot be null in TransactionManager");
  }
}

std::shared_ptr<Transaction> TransactionManager::begin(IsolationLevel level) {
  TransactionId tid{next_txn_id_.fetch_add(1)};
  auto txn = std::make_shared<Transaction>(tid, level);

  LogRecord begin_rec(tid, kInvalidLSN, LogRecordType::kBegin);
  LSN begin_lsn = log_manager_->append_record(begin_rec);
  txn->set_prev_lsn(begin_lsn);

  {
    std::scoped_lock lock(latch_);
    txn_map_[tid] = txn;
  }

  return txn;
}

bool TransactionManager::commit(Transaction* txn) {
  if (txn == nullptr || txn->get_state() != TransactionState::kGrowing) {
    return false;
  }

  txn->set_state(TransactionState::kCommitted);

  LogRecord commit_rec(txn->get_transaction_id(), txn->get_prev_lsn(), LogRecordType::kCommit);
  LSN commit_lsn = log_manager_->append_record(commit_rec);
  log_manager_->flush(commit_lsn);

  {
    std::scoped_lock lock(latch_);
    txn_map_.erase(txn->get_transaction_id());
  }

  return true;
}

bool TransactionManager::abort(Transaction* txn) {
  if (txn == nullptr || (txn->get_state() != TransactionState::kGrowing && txn->get_state() != TransactionState::kShrinking)) {
    return false;
  }

  txn->set_state(TransactionState::kAborted);

  LogRecord abort_rec(txn->get_transaction_id(), txn->get_prev_lsn(), LogRecordType::kAbort);
  LSN abort_lsn = log_manager_->append_record(abort_rec);
  log_manager_->flush(abort_lsn);

  {
    std::scoped_lock lock(latch_);
    txn_map_.erase(txn->get_transaction_id());
  }

  return true;
}

std::shared_ptr<Transaction> TransactionManager::get_transaction(TransactionId txn_id) {
  std::scoped_lock lock(latch_);
  auto it = txn_map_.find(txn_id);
  if (it != txn_map_.end()) {
    return it->second;
  }
  return nullptr;
}

size_t TransactionManager::get_active_transaction_count() const {
  std::scoped_lock lock(latch_);
  return txn_map_.size();
}

}  // namespace forgedb
