#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <unordered_map>

#include "forgedb/core/types.hpp"

namespace forgedb {

class LRUReplacer {
 public:
  explicit LRUReplacer(size_t num_pages);
  ~LRUReplacer() = default;

  LRUReplacer(const LRUReplacer&) = delete;
  LRUReplacer& operator=(const LRUReplacer&) = delete;

  // Select the least recently used frame to evict
  bool victim(FrameId& frame_id);

  // Pin a frame, removing it from the replacer
  void pin(FrameId frame_id);

  // Unpin a frame, adding it to the replacer as an eviction candidate
  void unpin(FrameId frame_id);

  // Number of frames currently in the replacer
  [[nodiscard]] size_t size() const;

 private:
  mutable std::mutex latch_;
  std::list<FrameId> lru_list_;
  std::unordered_map<FrameId, std::list<FrameId>::iterator> lru_map_;
};

}  // namespace forgedb
