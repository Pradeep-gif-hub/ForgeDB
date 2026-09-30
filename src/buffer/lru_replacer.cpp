#include "forgedb/buffer/lru_replacer.hpp"

namespace forgedb {

LRUReplacer::LRUReplacer(size_t /*num_pages*/) {}

bool LRUReplacer::victim(FrameId& frame_id) {
  std::scoped_lock lock(latch_);
  if (lru_list_.empty()) {
    return false;
  }

  // Oldest frame is at back
  frame_id = lru_list_.back();
  lru_map_.erase(frame_id);
  lru_list_.pop_back();
  return true;
}

void LRUReplacer::pin(FrameId frame_id) {
  std::scoped_lock lock(latch_);
  auto it = lru_map_.find(frame_id);
  if (it != lru_map_.end()) {
    lru_list_.erase(it->second);
    lru_map_.erase(it);
  }
}

void LRUReplacer::unpin(FrameId frame_id) {
  std::scoped_lock lock(latch_);
  if (lru_map_.find(frame_id) != lru_map_.end()) {
    return;
  }

  // Push to front (most recently used)
  lru_list_.push_front(frame_id);
  lru_map_[frame_id] = lru_list_.begin();
}

size_t LRUReplacer::size() const {
  std::scoped_lock lock(latch_);
  return lru_list_.size();
}

}  // namespace forgedb
