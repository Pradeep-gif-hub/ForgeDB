#include "forgedb/index/b_plus_tree.hpp"

#include <stdexcept>

namespace forgedb {

BPlusTree::BPlusTree(BufferPoolManager* buffer_pool_manager,
                     PageId root_page_id,
                     size_t max_leaf_size,
                     size_t max_internal_size)
    : bpm_(buffer_pool_manager),
      root_page_id_(root_page_id),
      max_leaf_size_(max_leaf_size),
      max_internal_size_(max_internal_size) {
  if (bpm_ == nullptr) {
    throw std::invalid_argument("BufferPoolManager cannot be null in BPlusTree");
  }
}

bool BPlusTree::is_empty() const {
  std::scoped_lock lock(tree_latch_);
  return !root_page_id_.is_valid();
}

PageId BPlusTree::get_root_page_id() const {
  std::scoped_lock lock(tree_latch_);
  return root_page_id_;
}

Page* BPlusTree::find_leaf_page(const BPlusKey& key) {
  if (!root_page_id_.is_valid()) {
    return nullptr;
  }

  Page* curr_page = bpm_->fetch_page(root_page_id_);
  if (curr_page == nullptr) {
    return nullptr;
  }

  auto* tree_page = reinterpret_cast<BPlusTreePage*>(curr_page->get_data());
  while (!tree_page->is_leaf_page()) {
    auto* internal_page = reinterpret_cast<BPlusTreeInternalPage*>(tree_page);
    PageId next_page_id = internal_page->lookup(key);

    Page* next_page = bpm_->fetch_page(next_page_id);
    bpm_->unpin_page(curr_page->get_page_id(), false);
    curr_page = next_page;
    if (curr_page == nullptr) {
      return nullptr;
    }
    tree_page = reinterpret_cast<BPlusTreePage*>(curr_page->get_data());
  }

  return curr_page;
}

bool BPlusTree::get_value(const std::string& key_str, std::vector<RID>& result) {
  std::scoped_lock lock(tree_latch_);
  if (!root_page_id_.is_valid()) {
    return false;
  }

  BPlusKey key(key_str);
  Page* leaf_raw = find_leaf_page(key);
  if (leaf_raw == nullptr) {
    return false;
  }

  auto* leaf = reinterpret_cast<BPlusTreeLeafPage*>(leaf_raw->get_data());
  RID rid;
  bool found = leaf->lookup(key, rid);
  if (found) {
    result.push_back(rid);
  }

  bpm_->unpin_page(leaf_raw->get_page_id(), false);
  return found;
}

void BPlusTree::create_new_tree(const BPlusKey& key, const RID& value) {
  PageId root_id;
  Page* root_page = bpm_->new_page(root_id);
  if (root_page == nullptr) {
    throw std::runtime_error("Failed to allocate new root page for B+ Tree");
  }

  auto* leaf = reinterpret_cast<BPlusTreeLeafPage*>(root_page->get_data());
  leaf->init(root_id, kInvalidPageId, static_cast<uint32_t>(max_leaf_size_));
  leaf->insert(key, value);

  root_page_id_ = root_id;
  bpm_->unpin_page(root_id, true);
}

void BPlusTree::insert_into_leaf(BPlusTreeLeafPage* leaf, const BPlusKey& key, const RID& value) {
  if (leaf->get_size() < max_leaf_size_) {
    leaf->insert(key, value);
    bpm_->unpin_page(leaf->get_page_id(), true);
    return;
  }

  // Leaf split required
  PageId new_leaf_id;
  Page* new_leaf_page = bpm_->new_page(new_leaf_id);
  if (new_leaf_page == nullptr) {
    bpm_->unpin_page(leaf->get_page_id(), false);
    throw std::runtime_error("Failed to allocate leaf page during split");
  }

  auto* new_leaf = reinterpret_cast<BPlusTreeLeafPage*>(new_leaf_page->get_data());
  new_leaf->init(new_leaf_id, leaf->get_parent_page_id(), static_cast<uint32_t>(max_leaf_size_));

  leaf->split(new_leaf);

  if (key < new_leaf->key_at(0)) {
    leaf->insert(key, value);
  } else {
    new_leaf->insert(key, value);
  }

  BPlusKey parent_key = new_leaf->key_at(0);
  insert_into_parent(leaf, parent_key, new_leaf);

  bpm_->unpin_page(leaf->get_page_id(), true);
  bpm_->unpin_page(new_leaf_id, true);
}

void BPlusTree::insert_into_parent(BPlusTreePage* old_node, const BPlusKey& key, BPlusTreePage* new_node) {
  if (old_node->is_root_page()) {
    PageId new_root_id;
    Page* new_root_page = bpm_->new_page(new_root_id);
    if (new_root_page == nullptr) {
      throw std::runtime_error("Failed to allocate internal root page during split");
    }

    auto* new_root = reinterpret_cast<BPlusTreeInternalPage*>(new_root_page->get_data());
    new_root->init(new_root_id, kInvalidPageId, static_cast<uint32_t>(max_internal_size_));
    new_root->set_value_at(0, old_node->get_page_id());
    new_root->set_key_at(1, key);
    new_root->set_value_at(1, new_node->get_page_id());
    new_root->set_size(2);

    old_node->set_parent_page_id(new_root_id);
    new_node->set_parent_page_id(new_root_id);
    root_page_id_ = new_root_id;

    bpm_->unpin_page(new_root_id, true);
    return;
  }

  PageId parent_id = old_node->get_parent_page_id();
  Page* parent_raw = bpm_->fetch_page(parent_id);
  if (parent_raw == nullptr) {
    throw std::runtime_error("Failed to fetch parent page during split");
  }

  auto* parent = reinterpret_cast<BPlusTreeInternalPage*>(parent_raw->get_data());
  new_node->set_parent_page_id(parent_id);

  if (parent->get_size() < max_internal_size_) {
    parent->insert_node_after(old_node->get_page_id(), key, new_node->get_page_id());
    bpm_->unpin_page(parent_id, true);
    return;
  }

  // Parent split
  PageId new_parent_id;
  Page* new_parent_raw = bpm_->new_page(new_parent_id);
  if (new_parent_raw == nullptr) {
    bpm_->unpin_page(parent_id, false);
    throw std::runtime_error("Failed to allocate new internal page during split");
  }

  auto* new_parent = reinterpret_cast<BPlusTreeInternalPage*>(new_parent_raw->get_data());
  new_parent->init(new_parent_id, parent->get_parent_page_id(), static_cast<uint32_t>(max_internal_size_));

  parent->insert_node_after(old_node->get_page_id(), key, new_node->get_page_id());
  parent->split(new_parent);

  // Update parent pointers of all children moved to new_parent
  for (size_t i = 0; i < new_parent->get_size(); ++i) {
    PageId child_id = new_parent->value_at(i);
    Page* child_page = bpm_->fetch_page(child_id);
    if (child_page != nullptr) {
      auto* child_node = reinterpret_cast<BPlusTreePage*>(child_page->get_data());
      child_node->set_parent_page_id(new_parent_id);
      bpm_->unpin_page(child_id, true);
    }
  }

  // Update parent pointers of all children remaining in parent
  for (size_t i = 0; i < parent->get_size(); ++i) {
    PageId child_id = parent->value_at(i);
    Page* child_page = bpm_->fetch_page(child_id);
    if (child_page != nullptr) {
      auto* child_node = reinterpret_cast<BPlusTreePage*>(child_page->get_data());
      child_node->set_parent_page_id(parent_id);
      bpm_->unpin_page(child_id, true);
    }
  }

  BPlusKey parent_up_key = new_parent->key_at(0);
  insert_into_parent(parent, parent_up_key, new_parent);

  bpm_->unpin_page(parent_id, true);
  bpm_->unpin_page(new_parent_id, true);
}

bool BPlusTree::insert(const std::string& key_str, const RID& value) {
  std::scoped_lock lock(tree_latch_);
  BPlusKey key(key_str);

  if (!root_page_id_.is_valid()) {
    create_new_tree(key, value);
    return true;
  }

  Page* leaf_raw = find_leaf_page(key);
  if (leaf_raw == nullptr) {
    return false;
  }

  auto* leaf = reinterpret_cast<BPlusTreeLeafPage*>(leaf_raw->get_data());
  insert_into_leaf(leaf, key, value);
  return true;
}

bool BPlusTree::remove(const std::string& key_str) {
  std::scoped_lock lock(tree_latch_);
  if (!root_page_id_.is_valid()) {
    return false;
  }

  BPlusKey key(key_str);
  Page* leaf_raw = find_leaf_page(key);
  if (leaf_raw == nullptr) {
    return false;
  }

  auto* leaf = reinterpret_cast<BPlusTreeLeafPage*>(leaf_raw->get_data());
  bool removed = leaf->remove_and_delete_record(key);

  if (removed && leaf->is_root_page() && leaf->get_size() == 0) {
    bpm_->unpin_page(leaf_raw->get_page_id(), true);
    bpm_->delete_page(leaf_raw->get_page_id());
    root_page_id_ = kInvalidPageId;
    return true;
  }

  bpm_->unpin_page(leaf_raw->get_page_id(), removed);
  return removed;
}

void BPlusTree::scan(const std::string& start_key_str,
                     const std::string& end_key_str,
                     std::vector<std::pair<std::string, RID>>& results) {
  std::scoped_lock lock(tree_latch_);
  if (!root_page_id_.is_valid()) {
    return;
  }

  BPlusKey start_key(start_key_str);
  BPlusKey end_key(end_key_str);

  Page* leaf_raw = find_leaf_page(start_key);
  if (leaf_raw == nullptr) {
    return;
  }

  PageId current_page_id = leaf_raw->get_page_id();
  auto* leaf = reinterpret_cast<BPlusTreeLeafPage*>(leaf_raw->get_data());
  size_t start_idx = leaf->key_index(start_key);

  bool stop = false;
  while (current_page_id.is_valid() && !stop) {
    for (size_t i = start_idx; i < leaf->get_size(); ++i) {
      BPlusKey curr_key = leaf->key_at(i);
      if (!end_key_str.empty() && curr_key > end_key) {
        stop = true;
        break;
      }
      results.emplace_back(curr_key.to_string(), leaf->value_at(i));
    }

    PageId next_page_id = leaf->get_next_page_id();
    bpm_->unpin_page(current_page_id, false);

    if (stop || !next_page_id.is_valid()) {
      break;
    }

    current_page_id = next_page_id;
    leaf_raw = bpm_->fetch_page(current_page_id);
    if (leaf_raw == nullptr) {
      break;
    }
    leaf = reinterpret_cast<BPlusTreeLeafPage*>(leaf_raw->get_data());
    start_idx = 0;
  }
}

}  // namespace forgedb
