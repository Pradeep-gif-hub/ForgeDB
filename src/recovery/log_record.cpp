#include "forgedb/recovery/log_record.hpp"

#include <cstring>
#include <stdexcept>

namespace forgedb {

LogRecord::LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type)
    : prev_lsn_(prev_lsn), txn_id_(txn_id), type_(type) {}

LogRecord::LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type,
                     const std::string& key, const std::string& val, RID rid)
    : prev_lsn_(prev_lsn), txn_id_(txn_id), type_(type), key_(key), val_(val), rid_(rid) {}

LogRecord::LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type,
                     const std::string& key, const std::string& old_val, const std::string& new_val, RID rid)
    : prev_lsn_(prev_lsn), txn_id_(txn_id), type_(type), key_(key), val_(new_val), old_val_(old_val), rid_(rid) {}

LogRecord::LogRecord(TransactionId txn_id, LSN prev_lsn, LogRecordType type, LSN undo_next_lsn)
    : prev_lsn_(prev_lsn), txn_id_(txn_id), type_(type), undo_next_lsn_(undo_next_lsn) {}

size_t LogRecord::get_serialized_size() const noexcept {
  size_t total = kHeaderSize;
  switch (type_) {
    case LogRecordType::kInsert:
    case LogRecordType::kDelete:
      total += sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) + key_.size() + sizeof(uint32_t) + val_.size();
      break;
    case LogRecordType::kUpdate:
      total += sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) + key_.size() +
               sizeof(uint32_t) + old_val_.size() + sizeof(uint32_t) + val_.size();
      break;
    case LogRecordType::kCLR:
      total += sizeof(uint64_t);
      break;
    default:
      break;
  }
  return total;
}

void LogRecord::serialize(std::span<std::byte> dst) const {
  const size_t total_size = get_serialized_size();
  if (dst.size() < total_size) {
    throw std::out_of_range("Destination buffer too small for log record serialization");
  }

  size_t offset = 0;

  // Header: size(4), lsn(8), prev_lsn(8), txn_id(8), type(4)
  const uint32_t size_val = static_cast<uint32_t>(total_size);
  for (size_t i = 0; i < 4; ++i) dst[offset++] = static_cast<std::byte>((size_val >> (i * 8)) & 0xFF);

  const uint64_t lsn_val = lsn_.value();
  for (size_t i = 0; i < 8; ++i) dst[offset++] = static_cast<std::byte>((lsn_val >> (i * 8)) & 0xFF);

  const uint64_t prev_lsn_val = prev_lsn_.value();
  for (size_t i = 0; i < 8; ++i) dst[offset++] = static_cast<std::byte>((prev_lsn_val >> (i * 8)) & 0xFF);

  const uint64_t txn_id_val = txn_id_.value();
  for (size_t i = 0; i < 8; ++i) dst[offset++] = static_cast<std::byte>((txn_id_val >> (i * 8)) & 0xFF);

  const uint32_t type_val = static_cast<uint32_t>(type_);
  for (size_t i = 0; i < 4; ++i) dst[offset++] = static_cast<std::byte>((type_val >> (i * 8)) & 0xFF);

  // Body
  if (type_ == LogRecordType::kInsert || type_ == LogRecordType::kDelete) {
    const uint32_t page_id = rid_.page_id.value();
    for (size_t i = 0; i < 4; ++i) dst[offset++] = static_cast<std::byte>((page_id >> (i * 8)) & 0xFF);

    const uint16_t slot_num = rid_.slot_num;
    for (size_t i = 0; i < 2; ++i) dst[offset++] = static_cast<std::byte>((slot_num >> (i * 8)) & 0xFF);

    const uint16_t key_len = static_cast<uint16_t>(key_.size());
    for (size_t i = 0; i < 2; ++i) dst[offset++] = static_cast<std::byte>((key_len >> (i * 8)) & 0xFF);
    std::memcpy(dst.data() + offset, key_.data(), key_len);
    offset += key_len;

    const uint32_t val_len = static_cast<uint32_t>(val_.size());
    for (size_t i = 0; i < 4; ++i) dst[offset++] = static_cast<std::byte>((val_len >> (i * 8)) & 0xFF);
    std::memcpy(dst.data() + offset, val_.data(), val_len);
    offset += val_len;
  } else if (type_ == LogRecordType::kUpdate) {
    const uint32_t page_id = rid_.page_id.value();
    for (size_t i = 0; i < 4; ++i) dst[offset++] = static_cast<std::byte>((page_id >> (i * 8)) & 0xFF);

    const uint16_t slot_num = rid_.slot_num;
    for (size_t i = 0; i < 2; ++i) dst[offset++] = static_cast<std::byte>((slot_num >> (i * 8)) & 0xFF);

    const uint16_t key_len = static_cast<uint16_t>(key_.size());
    for (size_t i = 0; i < 2; ++i) dst[offset++] = static_cast<std::byte>((key_len >> (i * 8)) & 0xFF);
    std::memcpy(dst.data() + offset, key_.data(), key_len);
    offset += key_len;

    const uint32_t old_val_len = static_cast<uint32_t>(old_val_.size());
    for (size_t i = 0; i < 4; ++i) dst[offset++] = static_cast<std::byte>((old_val_len >> (i * 8)) & 0xFF);
    std::memcpy(dst.data() + offset, old_val_.data(), old_val_len);
    offset += old_val_len;

    const uint32_t val_len = static_cast<uint32_t>(val_.size());
    for (size_t i = 0; i < 4; ++i) dst[offset++] = static_cast<std::byte>((val_len >> (i * 8)) & 0xFF);
    std::memcpy(dst.data() + offset, val_.data(), val_len);
    offset += val_len;
  } else if (type_ == LogRecordType::kCLR) {
    const uint64_t undo_lsn = undo_next_lsn_.value();
    for (size_t i = 0; i < 8; ++i) dst[offset++] = static_cast<std::byte>((undo_lsn >> (i * 8)) & 0xFF);
  }
}

LogRecord LogRecord::deserialize(std::span<const std::byte> src) {
  if (src.size() < kHeaderSize) {
    throw std::out_of_range("Source buffer smaller than log record header");
  }

  size_t offset = 0;

  uint32_t size_val = 0;
  for (size_t i = 0; i < 4; ++i) size_val |= static_cast<uint32_t>(src[offset++]) << (i * 8);

  uint64_t lsn_val = 0;
  for (size_t i = 0; i < 8; ++i) lsn_val |= static_cast<uint64_t>(src[offset++]) << (i * 8);

  uint64_t prev_lsn_val = 0;
  for (size_t i = 0; i < 8; ++i) prev_lsn_val |= static_cast<uint64_t>(src[offset++]) << (i * 8);

  uint64_t txn_id_val = 0;
  for (size_t i = 0; i < 8; ++i) txn_id_val |= static_cast<uint64_t>(src[offset++]) << (i * 8);

  uint32_t type_val = 0;
  for (size_t i = 0; i < 4; ++i) type_val |= static_cast<uint32_t>(src[offset++]) << (i * 8);

  LogRecord record;
  record.size_ = size_val;
  record.lsn_ = LSN{lsn_val};
  record.prev_lsn_ = LSN{prev_lsn_val};
  record.txn_id_ = TransactionId{txn_id_val};
  record.type_ = static_cast<LogRecordType>(type_val);

  if (record.type_ == LogRecordType::kInsert || record.type_ == LogRecordType::kDelete) {
    uint32_t page_id = 0;
    for (size_t i = 0; i < 4; ++i) page_id |= static_cast<uint32_t>(src[offset++]) << (i * 8);

    uint16_t slot_num = 0;
    for (size_t i = 0; i < 2; ++i) slot_num |= static_cast<uint16_t>(src[offset++]) << (i * 8);

    record.rid_ = RID{PageId{page_id}, slot_num};

    uint16_t key_len = 0;
    for (size_t i = 0; i < 2; ++i) key_len |= static_cast<uint16_t>(src[offset++]) << (i * 8);
    record.key_ = std::string(reinterpret_cast<const char*>(src.data() + offset), key_len);
    offset += key_len;

    uint32_t val_len = 0;
    for (size_t i = 0; i < 4; ++i) val_len |= static_cast<uint32_t>(src[offset++]) << (i * 8);
    record.val_ = std::string(reinterpret_cast<const char*>(src.data() + offset), val_len);
    offset += val_len;
  } else if (record.type_ == LogRecordType::kUpdate) {
    uint32_t page_id = 0;
    for (size_t i = 0; i < 4; ++i) page_id |= static_cast<uint32_t>(src[offset++]) << (i * 8);

    uint16_t slot_num = 0;
    for (size_t i = 0; i < 2; ++i) slot_num |= static_cast<uint16_t>(src[offset++]) << (i * 8);

    record.rid_ = RID{PageId{page_id}, slot_num};

    uint16_t key_len = 0;
    for (size_t i = 0; i < 2; ++i) key_len |= static_cast<uint16_t>(src[offset++]) << (i * 8);
    record.key_ = std::string(reinterpret_cast<const char*>(src.data() + offset), key_len);
    offset += key_len;

    uint32_t old_val_len = 0;
    for (size_t i = 0; i < 4; ++i) old_val_len |= static_cast<uint32_t>(src[offset++]) << (i * 8);
    record.old_val_ = std::string(reinterpret_cast<const char*>(src.data() + offset), old_val_len);
    offset += old_val_len;

    uint32_t val_len = 0;
    for (size_t i = 0; i < 4; ++i) val_len |= static_cast<uint32_t>(src[offset++]) << (i * 8);
    record.val_ = std::string(reinterpret_cast<const char*>(src.data() + offset), val_len);
    offset += val_len;
  } else if (record.type_ == LogRecordType::kCLR) {
    uint64_t undo_lsn = 0;
    for (size_t i = 0; i < 8; ++i) undo_lsn |= static_cast<uint64_t>(src[offset++]) << (i * 8);
    record.undo_next_lsn_ = LSN{undo_lsn};
  }

  return record;
}

}  // namespace forgedb
