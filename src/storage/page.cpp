#include "forgedb/storage/page.hpp"

#include <cstring>
#include <stdexcept>

namespace forgedb {

Page::Page() noexcept {
  reset_memory();
}

void Page::reset_memory() noexcept {
  page_id_ = kInvalidPageId;
  pin_count_ = 0;
  is_dirty_ = false;
  page_lsn_ = kInvalidLSN;
  data_.fill(std::byte{0});
}

uint8_t Page::read_u8(size_t offset) const {
  if (offset + sizeof(uint8_t) > kPageSize) {
    throw std::out_of_range("Page::read_u8 offset exceeds page boundary");
  }
  return static_cast<uint8_t>(data_[offset]);
}

void Page::write_u8(size_t offset, uint8_t val) {
  if (offset + sizeof(uint8_t) > kPageSize) {
    throw std::out_of_range("Page::write_u8 offset exceeds page boundary");
  }
  data_[offset] = static_cast<std::byte>(val);
}

uint16_t Page::read_u16(size_t offset) const {
  if (offset + sizeof(uint16_t) > kPageSize) {
    throw std::out_of_range("Page::read_u16 offset exceeds page boundary");
  }
  uint16_t val = 0;
  val |= static_cast<uint16_t>(data_[offset]);
  val |= static_cast<uint16_t>(data_[offset + 1]) << 8;
  return val;
}

void Page::write_u16(size_t offset, uint16_t val) {
  if (offset + sizeof(uint16_t) > kPageSize) {
    throw std::out_of_range("Page::write_u16 offset exceeds page boundary");
  }
  data_[offset] = static_cast<std::byte>(val & 0xFF);
  data_[offset + 1] = static_cast<std::byte>((val >> 8) & 0xFF);
}

uint32_t Page::read_u32(size_t offset) const {
  if (offset + sizeof(uint32_t) > kPageSize) {
    throw std::out_of_range("Page::read_u32 offset exceeds page boundary");
  }
  uint32_t val = 0;
  for (size_t i = 0; i < 4; ++i) {
    val |= static_cast<uint32_t>(data_[offset + i]) << (i * 8);
  }
  return val;
}

void Page::write_u32(size_t offset, uint32_t val) {
  if (offset + sizeof(uint32_t) > kPageSize) {
    throw std::out_of_range("Page::write_u32 offset exceeds page boundary");
  }
  for (size_t i = 0; i < 4; ++i) {
    data_[offset + i] = static_cast<std::byte>((val >> (i * 8)) & 0xFF);
  }
}

uint64_t Page::read_u64(size_t offset) const {
  if (offset + sizeof(uint64_t) > kPageSize) {
    throw std::out_of_range("Page::read_u64 offset exceeds page boundary");
  }
  uint64_t val = 0;
  for (size_t i = 0; i < 8; ++i) {
    val |= static_cast<uint64_t>(data_[offset + i]) << (i * 8);
  }
  return val;
}

void Page::write_u64(size_t offset, uint64_t val) {
  if (offset + sizeof(uint64_t) > kPageSize) {
    throw std::out_of_range("Page::write_u64 offset exceeds page boundary");
  }
  for (size_t i = 0; i < 8; ++i) {
    data_[offset + i] = static_cast<std::byte>((val >> (i * 8)) & 0xFF);
  }
}

void Page::read_bytes(size_t offset, std::span<std::byte> dst) const {
  if (offset + dst.size() > kPageSize) {
    throw std::out_of_range("Page::read_bytes range exceeds page boundary");
  }
  std::memcpy(dst.data(), data_.data() + offset, dst.size());
}

void Page::write_bytes(size_t offset, std::span<const std::byte> src) {
  if (offset + src.size() > kPageSize) {
    throw std::out_of_range("Page::write_bytes range exceeds page boundary");
  }
  std::memcpy(data_.data() + offset, src.data(), src.size());
}

}  // namespace forgedb
