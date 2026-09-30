#include "forgedb/engine/storage_engine.hpp"

#include <stdexcept>

namespace forgedb {

StorageEngine::StorageEngine(const std::filesystem::path& db_path, size_t buffer_pool_size)
    : db_path_(db_path) {
  disk_manager_ = std::make_unique<DiskManager>(db_path_);
  bpm_ = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager_.get());
  log_manager_ = std::make_unique<LogManager>(disk_manager_.get());
  recovery_manager_ = std::make_unique<RecoveryManager>(disk_manager_.get(), bpm_.get(), log_manager_.get());
  txn_manager_ = std::make_unique<TransactionManager>(log_manager_.get());

  PageId saved_root = disk_manager_->get_root_page_id();
  index_ = std::make_unique<BPlusTree>(bpm_.get(), saved_root);

  // Run crash recovery if WAL exists
  recovery_manager_->recover();

  // Start background log flush thread
  log_manager_->start_flush_thread();

  // Initialize data page if needed
  PageId saved_data = disk_manager_->get_data_page_id();
  if (saved_data.is_valid()) {
    current_data_page_id_ = saved_data;
  } else if (disk_manager_->get_num_pages() <= 1) {
    PageId data_pid;
    Page* data_page = bpm_->new_page(data_pid);
    if (data_page != nullptr) {
      SlottedPage::init(*data_page, data_pid);
      current_data_page_id_ = data_pid;
      disk_manager_->set_data_page_id(data_pid);
      bpm_->unpin_page(data_pid, true);
    }
  } else {
    current_data_page_id_ = PageId{1};
  }
}

StorageEngine::~StorageEngine() {
  flush();
  log_manager_->stop_flush_thread();
}

RID StorageEngine::insert_record_into_data_pages(const std::string& key, const std::string& value) {
  Record rec(key, value);
  RID rid;

  if (!current_data_page_id_.is_valid()) {
    Page* page = bpm_->new_page(current_data_page_id_);
    if (page == nullptr) {
      throw std::runtime_error("Failed to allocate data page");
    }
    SlottedPage::init(*page, current_data_page_id_);
    SlottedPage::insert_record(*page, rec, rid);
    bpm_->unpin_page(current_data_page_id_, true);
    return rid;
  }

  Page* page = bpm_->fetch_page(current_data_page_id_);
  if (page == nullptr) {
    throw std::runtime_error("Failed to fetch current data page");
  }

  if (SlottedPage::insert_record(*page, rec, rid)) {
    bpm_->unpin_page(current_data_page_id_, true);
    return rid;
  }

  // Current data page is full, allocate a new page
  PageId new_pid;
  Page* new_page = bpm_->new_page(new_pid);
  if (new_page == nullptr) {
    bpm_->unpin_page(current_data_page_id_, false);
    throw std::runtime_error("Failed to allocate new data page when full");
  }

  SlottedPage::init(*new_page, new_pid, current_data_page_id_, kInvalidPageId);
  SlottedPage::set_next_page_id(*page, new_pid);

  SlottedPage::insert_record(*new_page, rec, rid);

  bpm_->unpin_page(current_data_page_id_, true);
  bpm_->unpin_page(new_pid, true);
  current_data_page_id_ = new_pid;

  return rid;
}

std::optional<std::string> StorageEngine::get(const std::string& key) {
  std::scoped_lock lock(engine_latch_);
  stats_.total_reads.fetch_add(1, std::memory_order_relaxed);

  std::vector<RID> rids;
  if (!index_->get_value(key, rids) || rids.empty()) {
    stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
    return std::nullopt;
  }

  RID rid = rids[0];
  Page* page = bpm_->fetch_page(rid.page_id);
  if (page == nullptr) {
    return std::nullopt;
  }

  Record rec;
  bool found = SlottedPage::get_record(*page, rid, rec);
  bpm_->unpin_page(rid.page_id, false);

  if (found) {
    stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);
    return rec.get_value();
  }

  stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
  return std::nullopt;
}

bool StorageEngine::put(const std::string& key, const std::string& value) {
  std::scoped_lock lock(engine_latch_);
  stats_.total_writes.fetch_add(1, std::memory_order_relaxed);

  // Check if key exists
  std::vector<RID> existing_rids;
  if (index_->get_value(key, existing_rids) && !existing_rids.empty()) {
    RID old_rid = existing_rids[0];
    Page* page = bpm_->fetch_page(old_rid.page_id);
    if (page != nullptr) {
      Record new_rec(key, value);
      if (SlottedPage::update_record(*page, old_rid, new_rec)) {
        LogRecord update_log(kInvalidTxnId, kInvalidLSN, LogRecordType::kUpdate, key, "", value, old_rid);
        LSN lsn = log_manager_->append_record(update_log);
        page->set_page_lsn(lsn);
        bpm_->unpin_page(old_rid.page_id, true);
        return true;
      }
      SlottedPage::delete_record(*page, old_rid);
      bpm_->unpin_page(old_rid.page_id, true);
      index_->remove(key);
    }
  }

  RID rid = insert_record_into_data_pages(key, value);
  index_->insert(key, rid);

  LogRecord insert_log(kInvalidTxnId, kInvalidLSN, LogRecordType::kInsert, key, value, rid);
  log_manager_->append_record(insert_log);

  return true;
}

bool StorageEngine::delete_key(const std::string& key) {
  std::scoped_lock lock(engine_latch_);
  stats_.total_deletes.fetch_add(1, std::memory_order_relaxed);

  std::vector<RID> rids;
  if (!index_->get_value(key, rids) || rids.empty()) {
    return false;
  }

  RID rid = rids[0];
  index_->remove(key);

  Page* page = bpm_->fetch_page(rid.page_id);
  if (page != nullptr) {
    SlottedPage::delete_record(*page, rid);
    LogRecord del_log(kInvalidTxnId, kInvalidLSN, LogRecordType::kDelete, key, "", rid);
    LSN lsn = log_manager_->append_record(del_log);
    page->set_page_lsn(lsn);
    bpm_->unpin_page(rid.page_id, true);
  }

  return true;
}

std::vector<std::pair<std::string, std::string>> StorageEngine::scan(const std::string& start_key,
                                                                    const std::string& end_key) {
  std::scoped_lock lock(engine_latch_);
  stats_.total_scans.fetch_add(1, std::memory_order_relaxed);

  std::vector<std::pair<std::string, RID>> index_results;
  index_->scan(start_key, end_key, index_results);

  std::vector<std::pair<std::string, std::string>> results;
  results.reserve(index_results.size());

  for (const auto& [k, rid] : index_results) {
    Page* page = bpm_->fetch_page(rid.page_id);
    if (page != nullptr) {
      Record rec;
      if (SlottedPage::get_record(*page, rid, rec)) {
        results.emplace_back(k, rec.get_value());
      }
      bpm_->unpin_page(rid.page_id, false);
    }
  }

  return results;
}

std::shared_ptr<Transaction> StorageEngine::begin_transaction(IsolationLevel level) {
  return txn_manager_->begin(level);
}

bool StorageEngine::commit_transaction(Transaction* txn) {
  bool ok = txn_manager_->commit(txn);
  if (ok) {
    stats_.total_commits.fetch_add(1, std::memory_order_relaxed);
  }
  return ok;
}

bool StorageEngine::abort_transaction(Transaction* txn) {
  std::scoped_lock lock(engine_latch_);
  if (txn == nullptr) {
    return false;
  }

  // Rollback in reverse order
  const auto& undo_set = txn->get_undo_set();
  for (auto it = undo_set.rbegin(); it != undo_set.rend(); ++it) {
    const auto& [key, old_val] = *it;
    if (old_val.has_value()) {
      put(key, old_val.value());
    } else {
      delete_key(key);
    }
  }

  bool ok = txn_manager_->abort(txn);
  if (ok) {
    stats_.total_aborts.fetch_add(1, std::memory_order_relaxed);
  }
  return ok;
}

std::optional<std::string> StorageEngine::get(Transaction* /*txn*/, const std::string& key) {
  return get(key);
}

bool StorageEngine::put(Transaction* txn, const std::string& key, const std::string& value) {
  if (txn != nullptr) {
    auto old_val = get(key);
    txn->add_undo_action(key, old_val);
  }
  return put(key, value);
}

bool StorageEngine::delete_key(Transaction* txn, const std::string& key) {
  if (txn != nullptr) {
    auto old_val = get(key);
    if (old_val.has_value()) {
      txn->add_undo_action(key, old_val);
    }
  }
  return delete_key(key);
}

void StorageEngine::checkpoint() {
  std::scoped_lock lock(engine_latch_);
  recovery_manager_->checkpoint();
}

void StorageEngine::flush() {
  std::scoped_lock lock(engine_latch_);
  if (index_) {
    disk_manager_->set_root_page_id(index_->get_root_page_id());
  }
  if (current_data_page_id_.is_valid()) {
    disk_manager_->set_data_page_id(current_data_page_id_);
  }
  bpm_->flush_all_pages();
  log_manager_->flush();
}

}  // namespace forgedb
