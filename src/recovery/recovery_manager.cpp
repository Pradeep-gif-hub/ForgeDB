#include "forgedb/recovery/recovery_manager.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace forgedb {

RecoveryManager::RecoveryManager(DiskManager* disk_manager,
                                 BufferPoolManager* buffer_pool_manager,
                                 LogManager* log_manager)
    : disk_manager_(disk_manager),
      bpm_(buffer_pool_manager),
      log_manager_(log_manager) {
  if (disk_manager_ == nullptr || bpm_ == nullptr || log_manager_ == nullptr) {
    throw std::invalid_argument("Subsystems cannot be null in RecoveryManager");
  }
}

void RecoveryManager::checkpoint() {
  // 1. Flush all dirty pages to disk
  bpm_->flush_all_pages();

  // 2. Write checkpoint record to WAL
  LogRecord chk_rec(kInvalidTxnId, kInvalidLSN, LogRecordType::kCheckpoint);
  LSN chk_lsn = log_manager_->append_record(chk_rec);
  log_manager_->flush(chk_lsn);
}

void RecoveryManager::recover() {
  // Read all log records from disk WAL
  std::vector<LogRecord> log_records;
  size_t log_size = disk_manager_->get_log_file_size();
  if (log_size == 0) {
    return;
  }

  std::vector<std::byte> buffer(log_size);
  if (!disk_manager_->read_log(buffer, log_size, 0)) {
    return;
  }

  size_t offset = 0;
  while (offset + LogRecord::kHeaderSize <= log_size) {
    uint32_t rec_size = 0;
    for (size_t i = 0; i < 4; ++i) {
      rec_size |= static_cast<uint32_t>(buffer[offset + i]) << (i * 8);
    }

    if (rec_size == 0 || offset + rec_size > log_size) {
      break;
    }

    std::span<const std::byte> rec_span(buffer.data() + offset, rec_size);
    LogRecord rec = LogRecord::deserialize(rec_span);
    log_records.push_back(std::move(rec));
    offset += rec_size;
  }

  if (log_records.empty()) {
    return;
  }

  // Phase 1: Analysis Phase
  analysis_phase(log_records);

  // Phase 2: Redo Phase
  redo_phase(log_records);

  // Phase 3: Undo Phase
  undo_phase(log_records);

  // Flush recovered state to disk
  bpm_->flush_all_pages();
}

void RecoveryManager::analysis_phase(std::vector<LogRecord>& log_records) {
  active_txn_table_.clear();
  dirty_page_table_.clear();
  redo_start_lsn_ = log_records.front().get_lsn();

  for (const auto& rec : log_records) {
    TransactionId tid = rec.get_txn_id();
    switch (rec.get_type()) {
      case LogRecordType::kBegin:
        if (tid.is_valid()) {
          active_txn_table_[tid] = rec.get_lsn();
        }
        break;
      case LogRecordType::kCommit:
      case LogRecordType::kAbort:
        if (tid.is_valid()) {
          active_txn_table_.erase(tid);
        }
        break;
      case LogRecordType::kInsert:
      case LogRecordType::kUpdate:
      case LogRecordType::kDelete: {
        if (tid.is_valid()) {
          active_txn_table_[tid] = rec.get_lsn();
        }
        PageId pid = rec.get_rid().page_id;
        if (pid.is_valid() && dirty_page_table_.find(pid) == dirty_page_table_.end()) {
          dirty_page_table_[pid] = rec.get_lsn();
        }
        break;
      }
      case LogRecordType::kCheckpoint:
        break;
      default:
        break;
    }
  }

  if (!dirty_page_table_.empty()) {
    LSN min_lsn = LSN{UINT64_MAX};
    for (const auto& [pid, lsn] : dirty_page_table_) {
      if (lsn < min_lsn) {
        min_lsn = lsn;
      }
    }
    redo_start_lsn_ = min_lsn;
  }
}

void RecoveryManager::redo_phase(const std::vector<LogRecord>& log_records) {
  for (const auto& rec : log_records) {
    if (rec.get_lsn() < redo_start_lsn_) {
      continue;
    }

    switch (rec.get_type()) {
      case LogRecordType::kInsert: {
        RID rid = rec.get_rid();
        Page* page = bpm_->fetch_page(rid.page_id);
        if (page != nullptr) {
          if (page->get_page_lsn() < rec.get_lsn()) {
            Record r(rec.get_key(), rec.get_value());
            RID out_rid;
            SlottedPage::insert_record(*page, r, out_rid);
            page->set_page_lsn(rec.get_lsn());
          }
          bpm_->unpin_page(rid.page_id, true);
        }
        break;
      }
      case LogRecordType::kUpdate: {
        RID rid = rec.get_rid();
        Page* page = bpm_->fetch_page(rid.page_id);
        if (page != nullptr) {
          if (page->get_page_lsn() < rec.get_lsn()) {
            Record r(rec.get_key(), rec.get_new_value());
            SlottedPage::update_record(*page, rid, r);
            page->set_page_lsn(rec.get_lsn());
          }
          bpm_->unpin_page(rid.page_id, true);
        }
        break;
      }
      case LogRecordType::kDelete: {
        RID rid = rec.get_rid();
        Page* page = bpm_->fetch_page(rid.page_id);
        if (page != nullptr) {
          if (page->get_page_lsn() < rec.get_lsn()) {
            SlottedPage::delete_record(*page, rid);
            page->set_page_lsn(rec.get_lsn());
          }
          bpm_->unpin_page(rid.page_id, true);
        }
        break;
      }
      default:
        break;
    }
  }
}

void RecoveryManager::undo_phase(const std::vector<LogRecord>& log_records) {
  if (active_txn_table_.empty()) {
    return;
  }

  // Iterate backwards through log records to rollback active uncommitted transactions
  for (auto it = log_records.rbegin(); it != log_records.rend(); ++it) {
    const LogRecord& rec = *it;
    TransactionId tid = rec.get_txn_id();

    if (!tid.is_valid() || active_txn_table_.find(tid) == active_txn_table_.end()) {
      continue;
    }

    switch (rec.get_type()) {
      case LogRecordType::kInsert: {
        // Inverse of Insert is Delete
        RID rid = rec.get_rid();
        Page* page = bpm_->fetch_page(rid.page_id);
        if (page != nullptr) {
          SlottedPage::delete_record(*page, rid);
          bpm_->unpin_page(rid.page_id, true);
        }
        // Append CLR
        LogRecord clr(tid, rec.get_lsn(), LogRecordType::kCLR, rec.get_prev_lsn());
        log_manager_->append_record(clr);
        break;
      }
      case LogRecordType::kUpdate: {
        // Inverse of Update is restoring old value
        RID rid = rec.get_rid();
        Page* page = bpm_->fetch_page(rid.page_id);
        if (page != nullptr) {
          Record old_r(rec.get_key(), rec.get_old_value());
          SlottedPage::update_record(*page, rid, old_r);
          bpm_->unpin_page(rid.page_id, true);
        }
        LogRecord clr(tid, rec.get_lsn(), LogRecordType::kCLR, rec.get_prev_lsn());
        log_manager_->append_record(clr);
        break;
      }
      case LogRecordType::kDelete: {
        // Inverse of Delete is re-inserting record
        RID rid = rec.get_rid();
        Page* page = bpm_->fetch_page(rid.page_id);
        if (page != nullptr) {
          Record r(rec.get_key(), rec.get_value());
          RID out_rid;
          SlottedPage::insert_record(*page, r, out_rid);
          bpm_->unpin_page(rid.page_id, true);
        }
        LogRecord clr(tid, rec.get_lsn(), LogRecordType::kCLR, rec.get_prev_lsn());
        log_manager_->append_record(clr);
        break;
      }
      case LogRecordType::kBegin: {
        // Transaction fully aborted
        LogRecord abort_rec(tid, rec.get_lsn(), LogRecordType::kAbort);
        log_manager_->append_record(abort_rec);
        active_txn_table_.erase(tid);
        break;
      }
      default:
        break;
    }
  }

  log_manager_->flush();
}

}  // namespace forgedb
