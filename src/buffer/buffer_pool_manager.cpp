#include "forgedb/buffer/buffer_pool_manager.hpp"

#include <stdexcept>

namespace forgedb {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size),
      disk_manager_(disk_manager),
      replacer_(std::make_unique<LRUReplacer>(pool_size)),
      pages_(pool_size) {
  if (disk_manager_ == nullptr) {
    throw std::invalid_argument("DiskManager cannot be null in BufferPoolManager");
  }

  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.push_back(FrameId{static_cast<int32_t>(i)});
  }
}

BufferPoolManager::~BufferPoolManager() {
  flush_all_pages();
}

bool BufferPoolManager::find_available_frame(FrameId& out_frame_id) {
  if (!free_list_.empty()) {
    out_frame_id = free_list_.front();
    free_list_.pop_front();
    return true;
  }

  if (replacer_->victim(out_frame_id)) {
    Page& victim_page = pages_[static_cast<size_t>(out_frame_id.value())];
    if (victim_page.is_dirty()) {
      disk_manager_->write_page(victim_page.get_page_id(), victim_page.get_data_span());
      victim_page.set_dirty(false);
    }
    page_table_.erase(victim_page.get_page_id());
    return true;
  }

  return false;
}

Page* BufferPoolManager::fetch_page(PageId page_id) {
  if (!page_id.is_valid()) {
    return nullptr;
  }

  std::scoped_lock lock(latch_);

  // Check if page is already cached in memory
  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    FrameId frame_id = it->second;
    Page& page = pages_[static_cast<size_t>(frame_id.value())];
    page.increment_pin_count();
    replacer_->pin(frame_id);
    return &page;
  }

  // Page not cached, allocate a frame
  FrameId frame_id;
  if (!find_available_frame(frame_id)) {
    return nullptr;
  }

  Page& page = pages_[static_cast<size_t>(frame_id.value())];
  page.reset_memory();
  page.set_page_id(page_id);
  page.increment_pin_count();

  disk_manager_->read_page(page_id, page.get_data_span());

  page_table_[page_id] = frame_id;
  replacer_->pin(frame_id);

  return &page;
}

bool BufferPoolManager::unpin_page(PageId page_id, bool is_dirty) {
  if (!page_id.is_valid()) {
    return false;
  }

  std::scoped_lock lock(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }

  FrameId frame_id = it->second;
  Page& page = pages_[static_cast<size_t>(frame_id.value())];

  if (is_dirty) {
    page.set_dirty(true);
  }

  if (page.get_pin_count() <= 0) {
    return false;
  }

  page.decrement_pin_count();
  if (page.get_pin_count() == 0) {
    replacer_->unpin(frame_id);
  }

  return true;
}

Page* BufferPoolManager::new_page(PageId& out_page_id) {
  std::scoped_lock lock(latch_);

  FrameId frame_id;
  if (!find_available_frame(frame_id)) {
    out_page_id = kInvalidPageId;
    return nullptr;
  }

  out_page_id = disk_manager_->allocate_page();

  Page& page = pages_[static_cast<size_t>(frame_id.value())];
  page.reset_memory();
  page.set_page_id(out_page_id);
  page.increment_pin_count();
  page.set_dirty(true);

  page_table_[out_page_id] = frame_id;
  replacer_->pin(frame_id);

  return &page;
}

bool BufferPoolManager::delete_page(PageId page_id) {
  if (!page_id.is_valid()) {
    return true;
  }

  std::scoped_lock lock(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    disk_manager_->deallocate_page(page_id);
    return true;
  }

  FrameId frame_id = it->second;
  Page& page = pages_[static_cast<size_t>(frame_id.value())];

  if (page.get_pin_count() > 0) {
    return false;
  }

  page_table_.erase(it);
  replacer_->pin(frame_id);
  page.reset_memory();
  free_list_.push_back(frame_id);

  disk_manager_->deallocate_page(page_id);
  return true;
}

bool BufferPoolManager::flush_page(PageId page_id) {
  if (!page_id.is_valid()) {
    return false;
  }

  std::scoped_lock lock(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }

  FrameId frame_id = it->second;
  Page& page = pages_[static_cast<size_t>(frame_id.value())];
  if (page.is_dirty()) {
    disk_manager_->write_page(page.get_page_id(), page.get_data_span());
    page.set_dirty(false);
  }

  return true;
}

void BufferPoolManager::flush_all_pages() {
  std::scoped_lock lock(latch_);

  for (auto& [pid, fid] : page_table_) {
    Page& page = pages_[static_cast<size_t>(fid.value())];
    if (page.is_dirty()) {
      disk_manager_->write_page(page.get_page_id(), page.get_data_span());
      page.set_dirty(false);
    }
  }
}

}  // namespace forgedb
