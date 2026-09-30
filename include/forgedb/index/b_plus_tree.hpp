#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.hpp"
#include "forgedb/core/types.hpp"
#include "forgedb/index/b_plus_tree_page.hpp"
#include "forgedb/storage/record.hpp"

namespace forgedb {

class BPlusTree {
 public:
  explicit BPlusTree(BufferPoolManager* buffer_pool_manager,
                     PageId root_page_id = kInvalidPageId,
                     size_t max_leaf_size = BPlusTreeLeafPage::kMaxEntries,
                     size_t max_internal_size = BPlusTreeInternalPage::kMaxEntries);
  ~BPlusTree() = default;

  BPlusTree(const BPlusTree&) = delete;
  BPlusTree& operator=(const BPlusTree&) = delete;

  [[nodiscard]] bool is_empty() const;
  [[nodiscard]] PageId get_root_page_id() const;

  // Search
  bool get_value(const std::string& key, std::vector<RID>& result);

  // Modification
  bool insert(const std::string& key, const RID& value);
  bool remove(const std::string& key);

  // Range Query
  void scan(const std::string& start_key,
            const std::string& end_key,
            std::vector<std::pair<std::string, RID>>& results);

 private:
  Page* find_leaf_page(const BPlusKey& key);
  void insert_into_leaf(BPlusTreeLeafPage* leaf, const BPlusKey& key, const RID& value);
  void insert_into_parent(BPlusTreePage* old_node, const BPlusKey& key, BPlusTreePage* new_node);
  void create_new_tree(const BPlusKey& key, const RID& value);

  BufferPoolManager* bpm_;
  PageId root_page_id_{kInvalidPageId};
  size_t max_leaf_size_;
  size_t max_internal_size_;
  mutable std::mutex tree_latch_;
};

}  // namespace forgedb
