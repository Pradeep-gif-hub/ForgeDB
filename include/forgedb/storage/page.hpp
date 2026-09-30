#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "forgedb/core/config.hpp"
#include "forgedb/core/types.hpp"

namespace forgedb {

class Page {
 public:
  Page() noexcept;
  ~Page() = default;

  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;
  Page(Page&&) = delete;
  Page& operator=(Page&&) = delete;

  [[nodiscard]] PageId get_page_id() const noexcept { return page_id_; }
  void set_page_id(PageId page_id) noexcept { page_id_ = page_id; }

  [[nodiscard]] int32_t get_pin_count() const noexcept { return pin_count_; }
  void increment_pin_count() noexcept { ++pin_count_; }
  void decrement_pin_count() noexcept { --pin_count_; }
  void reset_pin_count() noexcept { pin_count_ = 0; }

  [[nodiscard]] bool is_dirty() const noexcept { return is_dirty_; }
  void set_dirty(bool dirty) noexcept { is_dirty_ = dirty; }

  [[nodiscard]] LSN get_page_lsn() const noexcept { return page_lsn_; }
  void set_page_lsn(LSN lsn) noexcept { page_lsn_ = lsn; }

  [[nodiscard]] std::byte* get_data() noexcept { return data_.data(); }
  [[nodiscard]] const std::byte* get_data() const noexcept { return data_.data(); }
  [[nodiscard]] std::span<std::byte, kPageSize> get_data_span() noexcept { return data_; }
  [[nodiscard]] std::span<const std::byte, kPageSize> get_data_span() const noexcept { return data_; }

  void reset_memory() noexcept;

  // Bounds-checked integer accessors (little-endian)
  [[nodiscard]] uint8_t read_u8(size_t offset) const;
  void write_u8(size_t offset, uint8_t val);

  [[nodiscard]] uint16_t read_u16(size_t offset) const;
  void write_u16(size_t offset, uint16_t val);

  [[nodiscard]] uint32_t read_u32(size_t offset) const;
  void write_u32(size_t offset, uint32_t val);

  [[nodiscard]] uint64_t read_u64(size_t offset) const;
  void write_u64(size_t offset, uint64_t val);

  // Bounds-checked byte range accessors
  void read_bytes(size_t offset, std::span<std::byte> dst) const;
  void write_bytes(size_t offset, std::span<const std::byte> src);

 private:
  PageId page_id_{kInvalidPageId};
  int32_t pin_count_{0};
  bool is_dirty_{false};
  LSN page_lsn_{kInvalidLSN};
  std::array<std::byte, kPageSize> data_{};
};

}  // namespace forgedb
