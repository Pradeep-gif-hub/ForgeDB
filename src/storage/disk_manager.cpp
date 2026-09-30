#include "forgedb/storage/disk_manager.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace forgedb {

DiskManager::DiskManager(const std::filesystem::path& db_file)
    : db_file_path_(db_file),
      log_file_path_(db_file.string() + ".wal") {
  open_or_create_files();
}

DiskManager::~DiskManager() {
  close();
}

void DiskManager::open_or_create_files() {
  std::scoped_lock lock(db_io_latch_, log_io_latch_);

  // Create parent directories if they don't exist
  if (db_file_path_.has_parent_path()) {
    std::filesystem::create_directories(db_file_path_.parent_path());
  }

  bool is_new_db = !std::filesystem::exists(db_file_path_) ||
                   std::filesystem::file_size(db_file_path_) == 0;

  // Ensure database file exists before opening for r/w
  if (is_new_db) {
    std::ofstream init_file(db_file_path_, std::ios::binary | std::ios::trunc);
    if (!init_file) {
      throw std::runtime_error("Cannot create database file: " + db_file_path_.string());
    }
    init_file.close();
  }

  db_io_.open(db_file_path_, std::ios::binary | std::ios::in | std::ios::out);
  if (!db_io_.is_open()) {
    throw std::runtime_error("Cannot open database file: " + db_file_path_.string());
  }

  if (is_new_db) {
    init_db_file();
  } else {
    // Read and validate header
    uintmax_t size = std::filesystem::file_size(db_file_path_);
    if (size < kPageSize) {
      throw std::runtime_error("Corrupted database file: smaller than one page");
    }
    num_pages_ = static_cast<uint32_t>(size / kPageSize);

    std::array<std::byte, kPageSize> header_buf{};
    db_io_.seekg(0);
    db_io_.read(reinterpret_cast<char*>(header_buf.data()), kPageSize);
    if (!db_io_) {
      throw std::runtime_error("Failed to read database header");
    }

    uint32_t magic = 0;
    for (size_t i = 0; i < 4; ++i) {
      magic |= static_cast<uint32_t>(header_buf[i]) << (i * 8);
    }
    if (magic != kMagicNumber) {
      throw std::runtime_error("Invalid database file: magic number mismatch");
    }
  }

  // Open WAL file
  if (!std::filesystem::exists(log_file_path_)) {
    std::ofstream init_log(log_file_path_, std::ios::binary | std::ios::trunc);
    if (!init_log) {
      throw std::runtime_error("Cannot create WAL file: " + log_file_path_.string());
    }
    init_log.close();
  }

  log_io_.open(log_file_path_, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
  if (!log_io_.is_open()) {
    throw std::runtime_error("Cannot open WAL file: " + log_file_path_.string());
  }
}

void DiskManager::init_db_file() {
  std::array<std::byte, kPageSize> header_buf{};

  // Magic number (FGDB)
  for (size_t i = 0; i < 4; ++i) {
    header_buf[i] = static_cast<std::byte>((kMagicNumber >> (i * 8)) & 0xFF);
  }

  // Version
  for (size_t i = 0; i < 4; ++i) {
    header_buf[4 + i] = static_cast<std::byte>((kDatabaseVersion >> (i * 8)) & 0xFF);
  }

  // Num pages = 1 (header page is page 0)
  uint32_t initial_pages = 1;
  for (size_t i = 0; i < 4; ++i) {
    header_buf[8 + i] = static_cast<std::byte>((initial_pages >> (i * 8)) & 0xFF);
  }

  // Root page ID = kInvalidPageId (0xFFFFFFFF)
  for (size_t i = 0; i < 4; ++i) {
    header_buf[12 + i] = static_cast<std::byte>(0xFF);
  }

  // Data page ID = kInvalidPageId (0xFFFFFFFF)
  for (size_t i = 0; i < 4; ++i) {
    header_buf[16 + i] = static_cast<std::byte>(0xFF);
  }

  db_io_.seekp(0);
  db_io_.write(reinterpret_cast<const char*>(header_buf.data()), kPageSize);
  db_io_.flush();
  num_pages_ = 1;
  num_writes_++;
  num_flushes_++;
}

void DiskManager::write_page(PageId page_id, std::span<const std::byte, kPageSize> page_data) {
  if (!page_id.is_valid()) {
    throw std::invalid_argument("Cannot write page with invalid PageId");
  }

  std::scoped_lock lock(db_io_latch_);
  if (is_closed_) {
    throw std::runtime_error("DiskManager is closed");
  }

  const uint64_t offset = static_cast<uint64_t>(page_id.value()) * kPageSize;
  db_io_.seekp(static_cast<std::streamoff>(offset));
  db_io_.write(reinterpret_cast<const char*>(page_data.data()), kPageSize);

  if (db_io_.bad() || db_io_.fail()) {
    db_io_.clear();
    throw std::runtime_error("Failed to write page " + std::to_string(page_id.value()));
  }

  db_io_.flush();
  num_writes_++;
  num_flushes_++;

  if (page_id.value() >= num_pages_) {
    num_pages_ = page_id.value() + 1;
  }
}

void DiskManager::read_page(PageId page_id, std::span<std::byte, kPageSize> page_data) {
  if (!page_id.is_valid()) {
    throw std::invalid_argument("Cannot read page with invalid PageId");
  }

  std::scoped_lock lock(db_io_latch_);
  if (is_closed_) {
    throw std::runtime_error("DiskManager is closed");
  }

  const uint64_t offset = static_cast<uint64_t>(page_id.value()) * kPageSize;
  db_io_.seekg(static_cast<std::streamoff>(offset));
  db_io_.read(reinterpret_cast<char*>(page_data.data()), kPageSize);

  const std::streamsize bytes_read = db_io_.gcount();
  if (bytes_read < static_cast<std::streamsize>(kPageSize)) {
    // If read past EOF or partial read, zero the remainder
    db_io_.clear();
    std::memset(page_data.data() + bytes_read, 0, kPageSize - static_cast<size_t>(bytes_read));
  }

  num_reads_++;
}

PageId DiskManager::allocate_page() {
  std::scoped_lock lock(db_io_latch_);
  if (is_closed_) {
    throw std::runtime_error("DiskManager is closed");
  }

  PageId new_id{num_pages_};
  num_pages_++;

  // Write a blank page at the new position
  std::array<std::byte, kPageSize> blank_buf{};
  const uint64_t offset = static_cast<uint64_t>(new_id.value()) * kPageSize;
  db_io_.seekp(static_cast<std::streamoff>(offset));
  db_io_.write(reinterpret_cast<const char*>(blank_buf.data()), kPageSize);
  db_io_.flush();

  num_writes_++;
  num_flushes_++;

  return new_id;
}

void DiskManager::deallocate_page(PageId /*page_id*/) {
  // Page deallocation is currently tracked via free list or slotted headers
}

uint32_t DiskManager::get_num_pages() const {
  std::scoped_lock lock(db_io_latch_);
  return num_pages_;
}

uint32_t DiskManager::get_num_flushes() const {
  std::scoped_lock lock(db_io_latch_);
  return num_flushes_;
}

uint32_t DiskManager::get_num_writes() const {
  std::scoped_lock lock(db_io_latch_);
  return num_writes_;
}

uint32_t DiskManager::get_num_reads() const {
  std::scoped_lock lock(db_io_latch_);
  return num_reads_;
}

void DiskManager::write_log(std::span<const std::byte> log_data) {
  if (log_data.empty()) {
    return;
  }

  std::scoped_lock lock(log_io_latch_);
  if (is_closed_) {
    throw std::runtime_error("DiskManager is closed");
  }

  log_io_.seekp(0, std::ios::end);
  log_io_.write(reinterpret_cast<const char*>(log_data.data()), static_cast<std::streamsize>(log_data.size()));

  if (log_io_.bad() || log_io_.fail()) {
    log_io_.clear();
    throw std::runtime_error("Failed to write to WAL file");
  }
}

bool DiskManager::read_log(std::span<std::byte> buffer, size_t size, size_t offset) {
  std::scoped_lock lock(log_io_latch_);
  if (is_closed_) {
    throw std::runtime_error("DiskManager is closed");
  }

  log_io_.seekg(static_cast<std::streamoff>(offset));
  log_io_.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));

  const std::streamsize bytes_read = log_io_.gcount();
  if (bytes_read < static_cast<std::streamsize>(size)) {
    log_io_.clear();
    return false;
  }
  return true;
}

void DiskManager::flush_log() {
  std::scoped_lock lock(log_io_latch_);
  if (is_closed_) {
    return;
  }
  log_io_.flush();
}

size_t DiskManager::get_log_file_size() {
  std::scoped_lock lock(log_io_latch_);
  if (!std::filesystem::exists(log_file_path_)) {
    return 0;
  }
  return std::filesystem::file_size(log_file_path_);
}

PageId DiskManager::get_root_page_id() const {
  std::scoped_lock lock(db_io_latch_);
  if (!std::filesystem::exists(db_file_path_) || num_pages_ == 0) {
    return kInvalidPageId;
  }
  db_io_.seekg(12);
  std::array<std::byte, 4> buf{};
  db_io_.read(reinterpret_cast<char*>(buf.data()), 4);
  uint32_t val = 0;
  for (size_t i = 0; i < 4; ++i) {
    val |= static_cast<uint32_t>(static_cast<uint8_t>(buf[i])) << (i * 8);
  }
  return PageId{val};
}

void DiskManager::set_root_page_id(PageId root_id) {
  std::scoped_lock lock(db_io_latch_);
  if (is_closed_) return;

  std::array<std::byte, 4> buf{};
  uint32_t val = root_id.value();
  for (size_t i = 0; i < 4; ++i) {
    buf[i] = static_cast<std::byte>((val >> (i * 8)) & 0xFF);
  }

  db_io_.seekp(12);
  db_io_.write(reinterpret_cast<const char*>(buf.data()), 4);
  db_io_.flush();
  num_flushes_++;
}

PageId DiskManager::get_data_page_id() const {
  std::scoped_lock lock(db_io_latch_);
  if (!std::filesystem::exists(db_file_path_) || num_pages_ == 0) {
    return kInvalidPageId;
  }
  db_io_.seekg(16);
  std::array<std::byte, 4> buf{};
  db_io_.read(reinterpret_cast<char*>(buf.data()), 4);
  uint32_t val = 0;
  for (size_t i = 0; i < 4; ++i) {
    val |= static_cast<uint32_t>(static_cast<uint8_t>(buf[i])) << (i * 8);
  }
  return PageId{val};
}

void DiskManager::set_data_page_id(PageId data_id) {
  std::scoped_lock lock(db_io_latch_);
  if (is_closed_) return;

  std::array<std::byte, 4> buf{};
  uint32_t val = data_id.value();
  for (size_t i = 0; i < 4; ++i) {
    buf[i] = static_cast<std::byte>((val >> (i * 8)) & 0xFF);
  }

  db_io_.seekp(16);
  db_io_.write(reinterpret_cast<const char*>(buf.data()), 4);
  db_io_.flush();
  num_flushes_++;
}

void DiskManager::close() {
  std::scoped_lock lock(db_io_latch_, log_io_latch_);
  if (is_closed_) {
    return;
  }

  if (db_io_.is_open()) {
    db_io_.flush();
    db_io_.close();
  }
  if (log_io_.is_open()) {
    log_io_.flush();
    log_io_.close();
  }
  is_closed_ = true;
}

}  // namespace forgedb
