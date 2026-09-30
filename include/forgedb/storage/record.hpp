#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"
#include "forgedb/storage/page.hpp"

namespace forgedb {

// Record identifier (RID) pointing to a specific slot within a page
struct RID {
  PageId page_id{kInvalidPageId};
  uint16_t slot_num{0};

  constexpr RID() noexcept = default;
  constexpr RID(PageId pid, uint16_t snum) noexcept : page_id(pid), slot_num(snum) {}

  constexpr bool is_valid() const noexcept {
    return page_id.is_valid();
  }

  constexpr bool operator==(const RID& other) const noexcept {
    return page_id == other.page_id && slot_num == other.slot_num;
  }

  constexpr bool operator!=(const RID& other) const noexcept {
    return !(*this == other);
  }
};

inline constexpr RID kInvalidRID = RID();

class Record {
 public:
  Record() = default;
  Record(std::string key, std::string value, bool is_deleted = false);
  Record(std::span<const std::byte> key, std::span<const std::byte> value, bool is_deleted = false);

  [[nodiscard]] const std::string& get_key() const noexcept { return key_; }
  [[nodiscard]] const std::string& get_value() const noexcept { return value_; }
  [[nodiscard]] bool is_deleted() const noexcept { return is_deleted_; }
  void set_deleted(bool deleted) noexcept { is_deleted_ = deleted; }

  [[nodiscard]] size_t get_serialized_size() const noexcept;
  void serialize(std::span<std::byte> dst) const;
  static Record deserialize(std::span<const std::byte> src);

 private:
  std::string key_;
  std::string value_;
  bool is_deleted_{false};
};

// Slot metadata inside the slotted page header
struct Slot {
  uint16_t offset{0};
  uint16_t length{0};
  bool is_deleted{false};
};

class SlottedPage {
 public:
  static constexpr size_t kHeaderSize = 24;  // Header metadata offset
  static constexpr size_t kSlotSize = 6;     // offset(2) + length(2) + is_deleted(1) + pad(1)

  static void init(Page& page, PageId page_id, PageId prev_page = kInvalidPageId, PageId next_page = kInvalidPageId);

  static bool insert_record(Page& page, const Record& record, RID& out_rid);
  static bool get_record(const Page& page, const RID& rid, Record& out_record);
  static bool update_record(Page& page, const RID& rid, const Record& new_record);
  static bool delete_record(Page& page, const RID& rid);

  static size_t get_free_space(const Page& page);
  static uint16_t get_record_count(const Page& page);
  static void defragment(Page& page);

  static PageId get_prev_page_id(const Page& page);
  static void set_prev_page_id(Page& page, PageId prev_page);
  static PageId get_next_page_id(const Page& page);
  static void set_next_page_id(Page& page, PageId next_page);
};

}  // namespace forgedb
