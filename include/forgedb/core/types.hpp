#pragma once

#include <cstdint>
#include <functional>
#include <ostream>

namespace forgedb {

class PageId {
 public:
  static constexpr uint32_t kInvalidValue = 0xFFFFFFFFU;

  constexpr PageId() noexcept : value_(kInvalidValue) {}
  explicit constexpr PageId(uint32_t val) noexcept : value_(val) {}

  [[nodiscard]] constexpr uint32_t value() const noexcept { return value_; }
  [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != kInvalidValue; }

  constexpr bool operator==(const PageId& other) const noexcept { return value_ == other.value_; }
  constexpr bool operator!=(const PageId& other) const noexcept { return value_ != other.value_; }
  constexpr bool operator<(const PageId& other) const noexcept { return value_ < other.value_; }
  constexpr bool operator<=(const PageId& other) const noexcept { return value_ <= other.value_; }
  constexpr bool operator>(const PageId& other) const noexcept { return value_ > other.value_; }
  constexpr bool operator>=(const PageId& other) const noexcept { return value_ >= other.value_; }

  friend std::ostream& operator<<(std::ostream& os, const PageId& id) {
    if (id.is_valid()) {
      os << "PageId(" << id.value_ << ")";
    } else {
      os << "PageId(INVALID)";
    }
    return os;
  }

 private:
  uint32_t value_;
};

inline constexpr PageId kInvalidPageId = PageId();

class FrameId {
 public:
  static constexpr int32_t kInvalidValue = -1;

  constexpr FrameId() noexcept : value_(kInvalidValue) {}
  explicit constexpr FrameId(int32_t val) noexcept : value_(val) {}

  [[nodiscard]] constexpr int32_t value() const noexcept { return value_; }
  [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != kInvalidValue; }

  constexpr bool operator==(const FrameId& other) const noexcept { return value_ == other.value_; }
  constexpr bool operator!=(const FrameId& other) const noexcept { return value_ != other.value_; }
  constexpr bool operator<(const FrameId& other) const noexcept { return value_ < other.value_; }
  constexpr bool operator<=(const FrameId& other) const noexcept { return value_ <= other.value_; }
  constexpr bool operator>(const FrameId& other) const noexcept { return value_ > other.value_; }
  constexpr bool operator>=(const FrameId& other) const noexcept { return value_ >= other.value_; }

  friend std::ostream& operator<<(std::ostream& os, const FrameId& id) {
    if (id.is_valid()) {
      os << "FrameId(" << id.value_ << ")";
    } else {
      os << "FrameId(INVALID)";
    }
    return os;
  }

 private:
  int32_t value_;
};

inline constexpr FrameId kInvalidFrameId = FrameId();

class TransactionId {
 public:
  static constexpr uint64_t kInvalidValue = 0ULL;

  constexpr TransactionId() noexcept : value_(kInvalidValue) {}
  explicit constexpr TransactionId(uint64_t val) noexcept : value_(val) {}

  [[nodiscard]] constexpr uint64_t value() const noexcept { return value_; }
  [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != kInvalidValue; }

  constexpr bool operator==(const TransactionId& other) const noexcept { return value_ == other.value_; }
  constexpr bool operator!=(const TransactionId& other) const noexcept { return value_ != other.value_; }
  constexpr bool operator<(const TransactionId& other) const noexcept { return value_ < other.value_; }
  constexpr bool operator<=(const TransactionId& other) const noexcept { return value_ <= other.value_; }
  constexpr bool operator>(const TransactionId& other) const noexcept { return value_ > other.value_; }
  constexpr bool operator>=(const TransactionId& other) const noexcept { return value_ >= other.value_; }

  friend std::ostream& operator<<(std::ostream& os, const TransactionId& id) {
    if (id.is_valid()) {
      os << "TxnId(" << id.value_ << ")";
    } else {
      os << "TxnId(INVALID)";
    }
    return os;
  }

 private:
  uint64_t value_;
};

inline constexpr TransactionId kInvalidTxnId = TransactionId();

class LSN {
 public:
  static constexpr uint64_t kInvalidValue = 0ULL;

  constexpr LSN() noexcept : value_(kInvalidValue) {}
  explicit constexpr LSN(uint64_t val) noexcept : value_(val) {}

  [[nodiscard]] constexpr uint64_t value() const noexcept { return value_; }
  [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != kInvalidValue; }

  constexpr bool operator==(const LSN& other) const noexcept { return value_ == other.value_; }
  constexpr bool operator!=(const LSN& other) const noexcept { return value_ != other.value_; }
  constexpr bool operator<(const LSN& other) const noexcept { return value_ < other.value_; }
  constexpr bool operator<=(const LSN& other) const noexcept { return value_ <= other.value_; }
  constexpr bool operator>(const LSN& other) const noexcept { return value_ > other.value_; }
  constexpr bool operator>=(const LSN& other) const noexcept { return value_ >= other.value_; }

  friend std::ostream& operator<<(std::ostream& os, const LSN& lsn) {
    if (lsn.is_valid()) {
      os << "LSN(" << lsn.value_ << ")";
    } else {
      os << "LSN(INVALID)";
    }
    return os;
  }

 private:
  uint64_t value_;
};

inline constexpr LSN kInvalidLSN = LSN();

}  // namespace forgedb

namespace std {

template <>
struct hash<forgedb::PageId> {
  size_t operator()(const forgedb::PageId& id) const noexcept {
    return hash<uint32_t>{}(id.value());
  }
};

template <>
struct hash<forgedb::FrameId> {
  size_t operator()(const forgedb::FrameId& id) const noexcept {
    return hash<int32_t>{}(id.value());
  }
};

template <>
struct hash<forgedb::TransactionId> {
  size_t operator()(const forgedb::TransactionId& id) const noexcept {
    return hash<uint64_t>{}(id.value());
  }
};

template <>
struct hash<forgedb::LSN> {
  size_t operator()(const forgedb::LSN& lsn) const noexcept {
    return hash<uint64_t>{}(lsn.value());
  }
};

}  // namespace std
