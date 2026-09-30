#include "forgedb/index/b_plus_tree_page.hpp"

#include <algorithm>
#include <stdexcept>

namespace forgedb {

// Internal Page Implementation

void BPlusTreeInternalPage::init(PageId page_id, PageId parent_id, uint32_t max_size) {
  set_page_type(BPlusTreePageType::kInternal);
  set_size(0);
  set_max_size(max_size);
  set_parent_page_id(parent_id);
  set_page_id(page_id);
}

BPlusKey BPlusTreeInternalPage::key_at(size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("Internal page key index out of bounds");
  }
  return keys_[index];
}

void BPlusTreeInternalPage::set_key_at(size_t index, const BPlusKey& key) {
  if (index >= kMaxEntries) {
    throw std::out_of_range("Internal page key index out of bounds");
  }
  keys_[index] = key;
}

PageId BPlusTreeInternalPage::value_at(size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("Internal page value index out of bounds");
  }
  return values_[index];
}

void BPlusTreeInternalPage::set_value_at(size_t index, PageId value) {
  if (index >= kMaxEntries) {
    throw std::out_of_range("Internal page value index out of bounds");
  }
  values_[index] = value;
}

PageId BPlusTreeInternalPage::lookup(const BPlusKey& key) const {
  if (size_ == 0) {
    return kInvalidPageId;
  }

  // Linear scan / binary search across keys_[1..size_-1]
  size_t low = 1;
  size_t high = size_ - 1;
  size_t best = 0;

  while (low <= high && high < size_) {
    size_t mid = low + (high - low) / 2;
    if (keys_[mid] <= key) {
      best = mid;
      low = mid + 1;
    } else {
      if (mid == 0) break;
      high = mid - 1;
    }
  }

  return values_[best];
}

void BPlusTreeInternalPage::populate(PageId root_page_id, PageId child_page_id) {
  set_value_at(0, root_page_id);
  set_value_at(1, child_page_id);
  set_size(2);
}

void BPlusTreeInternalPage::insert_node_after(PageId old_value, const BPlusKey& new_key, PageId new_value) {
  size_t idx = 0;
  for (; idx < size_; ++idx) {
    if (values_[idx] == old_value) {
      break;
    }
  }

  if (idx == size_) {
    throw std::invalid_argument("Old value not found in internal page");
  }

  for (size_t i = size_; i > idx + 1; --i) {
    keys_[i] = keys_[i - 1];
    values_[i] = values_[i - 1];
  }

  keys_[idx + 1] = new_key;
  values_[idx + 1] = new_value;
  increase_size(1);
}

void BPlusTreeInternalPage::split(BPlusTreeInternalPage* recipient) {
  const size_t split_index = size_ / 2;
  const size_t move_count = size_ - split_index;

  for (size_t i = 0; i < move_count; ++i) {
    recipient->keys_[i] = keys_[split_index + i];
    recipient->values_[i] = values_[split_index + i];
  }

  recipient->set_size(static_cast<uint32_t>(move_count));
  set_size(static_cast<uint32_t>(split_index));
}

void BPlusTreeInternalPage::move_half_to(BPlusTreeInternalPage* recipient) {
  split(recipient);
}

void BPlusTreeInternalPage::move_all_to(BPlusTreeInternalPage* recipient, const BPlusKey& middle_key) {
  recipient->set_key_at(recipient->get_size(), middle_key);
  recipient->set_value_at(recipient->get_size(), values_[0]);
  recipient->increase_size(1);

  for (size_t i = 1; i < size_; ++i) {
    recipient->set_key_at(recipient->get_size(), keys_[i]);
    recipient->set_value_at(recipient->get_size(), values_[i]);
    recipient->increase_size(1);
  }

  set_size(0);
}

void BPlusTreeInternalPage::remove(size_t index) {
  if (index >= size_) {
    return;
  }
  for (size_t i = index; i + 1 < size_; ++i) {
    keys_[i] = keys_[i + 1];
    values_[i] = values_[i + 1];
  }
  set_size(size_ - 1);
}

// Leaf Page Implementation

void BPlusTreeLeafPage::init(PageId page_id, PageId parent_id, uint32_t max_size) {
  set_page_type(BPlusTreePageType::kLeaf);
  set_size(0);
  set_max_size(max_size);
  set_parent_page_id(parent_id);
  set_page_id(page_id);
  next_page_id_ = kInvalidPageId;
  prev_page_id_ = kInvalidPageId;
}

BPlusKey BPlusTreeLeafPage::key_at(size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("Leaf page key index out of bounds");
  }
  return keys_[index];
}

RID BPlusTreeLeafPage::value_at(size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("Leaf page value index out of bounds");
  }
  return values_[index];
}

void BPlusTreeLeafPage::set_key_at(size_t index, const BPlusKey& key) {
  if (index >= kMaxEntries) {
    throw std::out_of_range("Leaf page key index out of bounds");
  }
  keys_[index] = key;
}

void BPlusTreeLeafPage::set_value_at(size_t index, const RID& value) {
  if (index >= kMaxEntries) {
    throw std::out_of_range("Leaf page value index out of bounds");
  }
  values_[index] = value;
}

size_t BPlusTreeLeafPage::key_index(const BPlusKey& key) const {
  auto it = std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
  return static_cast<size_t>(std::distance(keys_.begin(), it));
}

bool BPlusTreeLeafPage::lookup(const BPlusKey& key, RID& out_rid) const {
  size_t idx = key_index(key);
  if (idx < size_ && keys_[idx] == key) {
    out_rid = values_[idx];
    return true;
  }
  return false;
}

uint32_t BPlusTreeLeafPage::insert(const BPlusKey& key, const RID& value) {
  size_t idx = key_index(key);
  if (idx < size_ && keys_[idx] == key) {
    // Key already exists: update RID
    values_[idx] = value;
    return size_;
  }

  for (size_t i = size_; i > idx; --i) {
    keys_[i] = keys_[i - 1];
    values_[i] = values_[i - 1];
  }

  keys_[idx] = key;
  values_[idx] = value;
  increase_size(1);
  return size_;
}

void BPlusTreeLeafPage::split(BPlusTreeLeafPage* recipient) {
  const size_t split_index = size_ / 2;
  const size_t move_count = size_ - split_index;

  for (size_t i = 0; i < move_count; ++i) {
    recipient->keys_[i] = keys_[split_index + i];
    recipient->values_[i] = values_[split_index + i];
  }

  recipient->set_size(static_cast<uint32_t>(move_count));
  set_size(static_cast<uint32_t>(split_index));

  recipient->set_next_page_id(next_page_id_);
  recipient->set_prev_page_id(get_page_id());
  set_next_page_id(recipient->get_page_id());
}

void BPlusTreeLeafPage::move_half_to(BPlusTreeLeafPage* recipient) {
  split(recipient);
}

void BPlusTreeLeafPage::move_all_to(BPlusTreeLeafPage* recipient) {
  for (size_t i = 0; i < size_; ++i) {
    recipient->keys_[recipient->get_size() + i] = keys_[i];
    recipient->values_[recipient->get_size() + i] = values_[i];
  }
  recipient->increase_size(size_);
  recipient->set_next_page_id(next_page_id_);
  set_size(0);
}

bool BPlusTreeLeafPage::remove_and_delete_record(const BPlusKey& key) {
  size_t idx = key_index(key);
  if (idx >= size_ || !(keys_[idx] == key)) {
    return false;
  }

  for (size_t i = idx; i + 1 < size_; ++i) {
    keys_[i] = keys_[i + 1];
    values_[i] = values_[i + 1];
  }
  set_size(size_ - 1);
  return true;
}

}  // namespace forgedb
