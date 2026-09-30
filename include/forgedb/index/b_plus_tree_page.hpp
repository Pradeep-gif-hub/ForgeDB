#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"
#include "forgedb/storage/page.hpp"
#include "forgedb/storage/record.hpp"

namespace forgedb {

enum class BPlusTreePageType : uint32_t {
  kLeaf = 1,
  kInternal = 2,
};

inline constexpr size_t kMaxKeySize = 32;

struct BPlusKey {
  std::array<char, kMaxKeySize> data{};

  BPlusKey() noexcept = default;
  BPlusKey(std::string_view str) noexcept {
    size_t copy_len = std::min(str.size(), kMaxKeySize);
    std::memcpy(data.data(), str.data(), copy_len);
  }

  [[nodiscard]] std::string to_string() const {
    size_t len = 0;
    while (len < kMaxKeySize && data[len] != '\0') {
      ++len;
    }
    return std::string(data.data(), len);
  }

  bool operator==(const BPlusKey& other) const noexcept {
    return std::memcmp(data.data(), other.data.data(), kMaxKeySize) == 0;
  }

  bool operator<(const BPlusKey& other) const noexcept {
    return std::memcmp(data.data(), other.data.data(), kMaxKeySize) < 0;
  }

  bool operator<=(const BPlusKey& other) const noexcept {
    return *this < other || *this == other;
  }

  bool operator>(const BPlusKey& other) const noexcept {
    return !(*this <= other);
  }

  bool operator>=(const BPlusKey& other) const noexcept {
    return !(*this < other);
  }
};

class BPlusTreePage {
 public:
  [[nodiscard]] BPlusTreePageType get_page_type() const noexcept { return page_type_; }
  void set_page_type(BPlusTreePageType type) noexcept { page_type_ = type; }

  [[nodiscard]] uint32_t get_size() const noexcept { return size_; }
  void set_size(uint32_t size) noexcept { size_ = size; }
  void increase_size(uint32_t amount) noexcept { size_ += amount; }

  [[nodiscard]] uint32_t get_max_size() const noexcept { return max_size_; }
  void set_max_size(uint32_t max_size) noexcept { max_size_ = max_size; }

  [[nodiscard]] PageId get_parent_page_id() const noexcept { return parent_page_id_; }
  void set_parent_page_id(PageId parent_page_id) noexcept { parent_page_id_ = parent_page_id; }

  [[nodiscard]] PageId get_page_id() const noexcept { return page_id_; }
  void set_page_id(PageId page_id) noexcept { page_id_ = page_id; }

  [[nodiscard]] bool is_leaf_page() const noexcept { return page_type_ == BPlusTreePageType::kLeaf; }
  [[nodiscard]] bool is_root_page() const noexcept { return !parent_page_id_.is_valid(); }

  [[nodiscard]] uint32_t get_min_size() const noexcept {
    return is_root_page() ? (is_leaf_page() ? 1 : 2) : (max_size_ + 1) / 2;
  }

 protected:
  BPlusTreePageType page_type_;
  uint32_t size_{0};
  uint32_t max_size_{0};
  PageId parent_page_id_{kInvalidPageId};
  PageId page_id_{kInvalidPageId};
};

class BPlusTreeInternalPage : public BPlusTreePage {
 public:
  static constexpr size_t kMaxEntries = 64;

  void init(PageId page_id, PageId parent_id = kInvalidPageId, uint32_t max_size = kMaxEntries);

  [[nodiscard]] BPlusKey key_at(size_t index) const;
  void set_key_at(size_t index, const BPlusKey& key);
  [[nodiscard]] PageId value_at(size_t index) const;
  void set_value_at(size_t index, PageId value);

  [[nodiscard]] PageId lookup(const BPlusKey& key) const;
  void populate(PageId root_page_id, PageId child_page_id);
  void insert_node_after(PageId old_value, const BPlusKey& new_key, PageId new_value);
  void split(BPlusTreeInternalPage* recipient);
  void move_half_to(BPlusTreeInternalPage* recipient);
  void move_all_to(BPlusTreeInternalPage* recipient, const BPlusKey& middle_key);
  void remove(size_t index);

 private:
  std::array<BPlusKey, kMaxEntries> keys_{};
  std::array<PageId, kMaxEntries> values_{};
};

class BPlusTreeLeafPage : public BPlusTreePage {
 public:
  static constexpr size_t kMaxEntries = 64;

  void init(PageId page_id, PageId parent_id = kInvalidPageId, uint32_t max_size = kMaxEntries);

  [[nodiscard]] PageId get_next_page_id() const noexcept { return next_page_id_; }
  void set_next_page_id(PageId next_page_id) noexcept { next_page_id_ = next_page_id; }

  [[nodiscard]] PageId get_prev_page_id() const noexcept { return prev_page_id_; }
  void set_prev_page_id(PageId prev_page_id) noexcept { prev_page_id_ = prev_page_id; }

  [[nodiscard]] BPlusKey key_at(size_t index) const;
  [[nodiscard]] RID value_at(size_t index) const;
  void set_key_at(size_t index, const BPlusKey& key);
  void set_value_at(size_t index, const RID& value);

  bool lookup(const BPlusKey& key, RID& out_rid) const;
  size_t key_index(const BPlusKey& key) const;
  uint32_t insert(const BPlusKey& key, const RID& value);
  void split(BPlusTreeLeafPage* recipient);
  void move_half_to(BPlusTreeLeafPage* recipient);
  void move_all_to(BPlusTreeLeafPage* recipient);
  bool remove_and_delete_record(const BPlusKey& key);

 private:
  PageId next_page_id_{kInvalidPageId};
  PageId prev_page_id_{kInvalidPageId};
  std::array<BPlusKey, kMaxEntries> keys_{};
  std::array<RID, kMaxEntries> values_{};
};

}  // namespace forgedb
