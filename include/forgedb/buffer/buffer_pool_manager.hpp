#pragma once

#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "forgedb/buffer/lru_replacer.hpp"
#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"
#include "forgedb/storage/disk_manager.hpp"
#include "forgedb/storage/page.hpp"

namespace forgedb {

class BufferPoolManager {
 public:
  BufferPoolManager(size_t pool_size, DiskManager* disk_manager);
  ~BufferPoolManager();

  BufferPoolManager(const BufferPoolManager&) = delete;
  BufferPoolManager& operator=(const BufferPoolManager&) = delete;

  // Page management
  Page* fetch_page(PageId page_id);
  bool unpin_page(PageId page_id, bool is_dirty);
  Page* new_page(PageId& out_page_id);
  bool delete_page(PageId page_id);

  // Flush operations
  bool flush_page(PageId page_id);
  void flush_all_pages();

  [[nodiscard]] size_t get_pool_size() const noexcept { return pool_size_; }

 private:
  bool find_available_frame(FrameId& out_frame_id);

  size_t pool_size_;
  DiskManager* disk_manager_;
  std::unique_ptr<LRUReplacer> replacer_;
  std::vector<Page> pages_;
  std::unordered_map<PageId, FrameId> page_table_;
  std::list<FrameId> free_list_;
  mutable std::mutex latch_;
};

}  // namespace forgedb
