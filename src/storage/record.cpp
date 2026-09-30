#include "forgedb/storage/record.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace forgedb {

Record::Record(std::string key, std::string value, bool is_deleted)
    : key_(std::move(key)), value_(std::move(value)), is_deleted_(is_deleted) {}

Record::Record(std::span<const std::byte> key, std::span<const std::byte> value, bool is_deleted)
    : key_(reinterpret_cast<const char*>(key.data()), key.size()),
      value_(reinterpret_cast<const char*>(value.data()), value.size()),
      is_deleted_(is_deleted) {}

size_t Record::get_serialized_size() const noexcept {
  // 2 bytes key_len + 4 bytes val_len + 1 byte is_deleted + payload
  return sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t) + key_.size() + value_.size();
}

void Record::serialize(std::span<std::byte> dst) const {
  if (dst.size() < get_serialized_size()) {
    throw std::out_of_range("Destination buffer too small for record serialization");
  }

  size_t offset = 0;
  const uint16_t key_len = static_cast<uint16_t>(key_.size());
  dst[offset++] = static_cast<std::byte>(key_len & 0xFF);
  dst[offset++] = static_cast<std::byte>((key_len >> 8) & 0xFF);

  const uint32_t val_len = static_cast<uint32_t>(value_.size());
  for (size_t i = 0; i < 4; ++i) {
    dst[offset++] = static_cast<std::byte>((val_len >> (i * 8)) & 0xFF);
  }

  dst[offset++] = static_cast<std::byte>(is_deleted_ ? 1 : 0);

  if (key_len > 0) {
    std::memcpy(dst.data() + offset, key_.data(), key_len);
    offset += key_len;
  }

  if (val_len > 0) {
    std::memcpy(dst.data() + offset, value_.data(), val_len);
    offset += val_len;
  }
}

Record Record::deserialize(std::span<const std::byte> src) {
  if (src.size() < sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t)) {
    throw std::out_of_range("Source buffer too small for record deserialization");
  }

  size_t offset = 0;
  uint16_t key_len = static_cast<uint16_t>(src[offset]);
  key_len |= static_cast<uint16_t>(src[offset + 1]) << 8;
  offset += 2;

  uint32_t val_len = 0;
  for (size_t i = 0; i < 4; ++i) {
    val_len |= static_cast<uint32_t>(src[offset + i]) << (i * 8);
  }
  offset += 4;

  const bool is_deleted = static_cast<uint8_t>(src[offset++]) != 0;

  if (src.size() < offset + key_len + val_len) {
    throw std::out_of_range("Corrupted record payload size in deserialization");
  }

  std::string key(reinterpret_cast<const char*>(src.data() + offset), key_len);
  offset += key_len;

  std::string value(reinterpret_cast<const char*>(src.data() + offset), val_len);

  return Record(std::move(key), std::move(value), is_deleted);
}

// Slotted Page Implementation

void SlottedPage::init(Page& page, PageId page_id, PageId prev_page, PageId next_page) {
  page.reset_memory();
  page.set_page_id(page_id);

  page.write_u32(0, page_id.value());
  page.write_u32(4, prev_page.value());
  page.write_u32(8, next_page.value());
  page.write_u16(12, 0);                       // num_records
  page.write_u16(14, static_cast<uint16_t>(kPageSize));  // free_space_pointer
  page.set_dirty(true);
}

PageId SlottedPage::get_prev_page_id(const Page& page) {
  return PageId{page.read_u32(4)};
}

void SlottedPage::set_prev_page_id(Page& page, PageId prev_page) {
  page.write_u32(4, prev_page.value());
  page.set_dirty(true);
}

PageId SlottedPage::get_next_page_id(const Page& page) {
  return PageId{page.read_u32(8)};
}

void SlottedPage::set_next_page_id(Page& page, PageId next_page) {
  page.write_u32(8, next_page.value());
  page.set_dirty(true);
}

uint16_t SlottedPage::get_record_count(const Page& page) {
  return page.read_u16(12);
}

size_t SlottedPage::get_free_space(const Page& page) {
  const uint16_t num_slots = page.read_u16(12);
  const uint16_t free_ptr = page.read_u16(14);
  const size_t slot_array_end = kHeaderSize + num_slots * kSlotSize;

  if (free_ptr < slot_array_end) {
    return 0;
  }
  return free_ptr - slot_array_end;
}

bool SlottedPage::insert_record(Page& page, const Record& record, RID& out_rid) {
  const size_t rec_size = record.get_serialized_size();
  const size_t required_space = rec_size + kSlotSize;

  if (get_free_space(page) < required_space) {
    // Attempt defragmentation in case of deleted slot fragmentation
    defragment(page);
    if (get_free_space(page) < required_space) {
      return false;
    }
  }

  const uint16_t num_slots = page.read_u16(12);
  uint16_t free_ptr = page.read_u16(14);

  free_ptr = static_cast<uint16_t>(free_ptr - rec_size);

  // Serialize record to the newly reserved space
  std::span<std::byte> page_span = page.get_data_span();
  record.serialize(page_span.subspan(free_ptr, rec_size));

  // Write slot metadata
  const size_t slot_offset = kHeaderSize + num_slots * kSlotSize;
  page.write_u16(slot_offset, free_ptr);
  page.write_u16(slot_offset + 2, static_cast<uint16_t>(rec_size));
  page.write_u8(slot_offset + 4, record.is_deleted() ? 1 : 0);
  page.write_u8(slot_offset + 5, 0);  // padding

  // Update page header
  page.write_u16(12, num_slots + 1);
  page.write_u16(14, free_ptr);
  page.set_dirty(true);

  out_rid = RID{page.get_page_id(), num_slots};
  return true;
}

bool SlottedPage::get_record(const Page& page, const RID& rid, Record& out_record) {
  const uint16_t num_slots = page.read_u16(12);
  if (rid.slot_num >= num_slots) {
    return false;
  }

  const size_t slot_offset = kHeaderSize + rid.slot_num * kSlotSize;
  const uint16_t rec_offset = page.read_u16(slot_offset);
  const uint16_t rec_len = page.read_u16(slot_offset + 2);
  const bool is_deleted = (page.read_u8(slot_offset + 4) != 0);

  if (is_deleted || rec_len == 0) {
    return false;
  }

  std::span<const std::byte, kPageSize> page_span = page.get_data_span();
  out_record = Record::deserialize(page_span.subspan(rec_offset, rec_len));
  return true;
}

bool SlottedPage::update_record(Page& page, const RID& rid, const Record& new_record) {
  const uint16_t num_slots = page.read_u16(12);
  if (rid.slot_num >= num_slots) {
    return false;
  }

  const size_t slot_offset = kHeaderSize + rid.slot_num * kSlotSize;
  const uint16_t old_offset = page.read_u16(slot_offset);
  const uint16_t old_len = page.read_u16(slot_offset + 2);
  const size_t new_len = new_record.get_serialized_size();

  // If new record fits in existing slot space in-place
  if (new_len <= old_len) {
    std::span<std::byte> page_span = page.get_data_span();
    new_record.serialize(page_span.subspan(old_offset, new_len));
    page.write_u16(slot_offset + 2, static_cast<uint16_t>(new_len));
    page.write_u8(slot_offset + 4, new_record.is_deleted() ? 1 : 0);
    page.set_dirty(true);
    return true;
  }

  // Otherwise, check if enough free space to relocate record
  if (get_free_space(page) < new_len) {
    defragment(page);
    if (get_free_space(page) < new_len) {
      return false;
    }
  }

  uint16_t free_ptr = page.read_u16(14);
  free_ptr = static_cast<uint16_t>(free_ptr - new_len);

  std::span<std::byte> page_span = page.get_data_span();
  new_record.serialize(page_span.subspan(free_ptr, new_len));

  page.write_u16(slot_offset, free_ptr);
  page.write_u16(slot_offset + 2, static_cast<uint16_t>(new_len));
  page.write_u8(slot_offset + 4, new_record.is_deleted() ? 1 : 0);
  page.write_u16(14, free_ptr);
  page.set_dirty(true);

  return true;
}

bool SlottedPage::delete_record(Page& page, const RID& rid) {
  const uint16_t num_slots = page.read_u16(12);
  if (rid.slot_num >= num_slots) {
    return false;
  }

  const size_t slot_offset = kHeaderSize + rid.slot_num * kSlotSize;
  page.write_u8(slot_offset + 4, 1);  // Mark deleted
  page.set_dirty(true);
  return true;
}

void SlottedPage::defragment(Page& page) {
  const uint16_t num_slots = page.read_u16(12);
  if (num_slots == 0) {
    page.write_u16(14, static_cast<uint16_t>(kPageSize));
    return;
  }

  // Collect active records
  std::vector<std::pair<uint16_t, std::vector<std::byte>>> active_records;
  std::span<const std::byte, kPageSize> page_span = page.get_data_span();

  for (uint16_t i = 0; i < num_slots; ++i) {
    const size_t slot_offset = kHeaderSize + i * kSlotSize;
    const uint16_t offset = page.read_u16(slot_offset);
    const uint16_t len = page.read_u16(slot_offset + 2);
    const bool is_deleted = (page.read_u8(slot_offset + 4) != 0);

    if (!is_deleted && len > 0) {
      std::vector<std::byte> buf(len);
      std::memcpy(buf.data(), page.get_data() + offset, len);
      active_records.emplace_back(i, std::move(buf));
    }
  }

  // Rewrite active records from top down
  uint16_t free_ptr = static_cast<uint16_t>(kPageSize);
  for (auto& [slot_idx, data] : active_records) {
    free_ptr = static_cast<uint16_t>(free_ptr - data.size());
    std::memcpy(page.get_data() + free_ptr, data.data(), data.size());

    const size_t slot_offset = kHeaderSize + slot_idx * kSlotSize;
    page.write_u16(slot_offset, free_ptr);
    page.write_u16(slot_offset + 2, static_cast<uint16_t>(data.size()));
    page.write_u8(slot_offset + 4, 0);
  }

  // Zero out the free space region
  const size_t slot_array_end = kHeaderSize + num_slots * kSlotSize;
  if (free_ptr > slot_array_end) {
    std::memset(page.get_data() + slot_array_end, 0, free_ptr - slot_array_end);
  }

  page.write_u16(14, free_ptr);
  page.set_dirty(true);
}

}  // namespace forgedb
