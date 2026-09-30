#include "forgedb/storage/page.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

namespace forgedb {
namespace {

TEST(PageTest, InitialState) {
  Page page;
  EXPECT_EQ(page.get_page_id(), kInvalidPageId);
  EXPECT_EQ(page.get_pin_count(), 0);
  EXPECT_FALSE(page.is_dirty());
  EXPECT_EQ(page.get_page_lsn(), kInvalidLSN);

  for (size_t i = 0; i < kPageSize; ++i) {
    EXPECT_EQ(page.read_u8(i), 0);
  }
}

TEST(PageTest, PinCountOperations) {
  Page page;
  EXPECT_EQ(page.get_pin_count(), 0);

  page.increment_pin_count();
  EXPECT_EQ(page.get_pin_count(), 1);

  page.increment_pin_count();
  EXPECT_EQ(page.get_pin_count(), 2);

  page.decrement_pin_count();
  EXPECT_EQ(page.get_pin_count(), 1);

  page.reset_pin_count();
  EXPECT_EQ(page.get_pin_count(), 0);
}

TEST(PageTest, DirtyFlag) {
  Page page;
  EXPECT_FALSE(page.is_dirty());

  page.set_dirty(true);
  EXPECT_TRUE(page.is_dirty());

  page.set_dirty(false);
  EXPECT_FALSE(page.is_dirty());
}

TEST(PageTest, PageIdAndLSN) {
  Page page;
  page.set_page_id(PageId{42});
  EXPECT_EQ(page.get_page_id(), PageId{42});

  page.set_page_lsn(LSN{1024});
  EXPECT_EQ(page.get_page_lsn(), LSN{1024});
}

TEST(PageTest, IntegerReadWriteLittleEndian) {
  Page page;

  // U8
  page.write_u8(0, 0xAB);
  EXPECT_EQ(page.read_u8(0), 0xAB);

  // U16
  page.write_u16(2, 0x1234);
  EXPECT_EQ(page.read_u16(2), 0x1234);
  EXPECT_EQ(page.read_u8(2), 0x34);
  EXPECT_EQ(page.read_u8(3), 0x12);

  // U32
  page.write_u32(8, 0x89ABCDEF);
  EXPECT_EQ(page.read_u32(8), 0x89ABCDEF);
  EXPECT_EQ(page.read_u8(8), 0xEF);
  EXPECT_EQ(page.read_u8(9), 0xCD);
  EXPECT_EQ(page.read_u8(10), 0xAB);
  EXPECT_EQ(page.read_u8(11), 0x89);

  // U64
  page.write_u64(16, 0x0123456789ABCDEFULL);
  EXPECT_EQ(page.read_u64(16), 0x0123456789ABCDEFULL);
}

TEST(PageTest, BoundsChecking) {
  Page page;

  EXPECT_THROW((void)page.write_u8(kPageSize, 0xFF), std::out_of_range);
  EXPECT_THROW((void)page.read_u8(kPageSize), std::out_of_range);

  EXPECT_THROW((void)page.write_u16(kPageSize - 1, 0x1234), std::out_of_range);
  EXPECT_THROW((void)page.read_u16(kPageSize - 1), std::out_of_range);

  EXPECT_THROW((void)page.write_u32(kPageSize - 3, 0x12345678), std::out_of_range);
  EXPECT_THROW((void)page.read_u32(kPageSize - 3), std::out_of_range);

  EXPECT_THROW((void)page.write_u64(kPageSize - 7, 0x123456789ABCDEFULL), std::out_of_range);
  EXPECT_THROW((void)page.read_u64(kPageSize - 7), std::out_of_range);
}

TEST(PageTest, ByteRangeReadWrite) {
  Page page;
  std::string message = "ForgeDB Storage Engine";
  std::vector<std::byte> src_bytes(message.size());
  for (size_t i = 0; i < message.size(); ++i) {
    src_bytes[i] = static_cast<std::byte>(message[i]);
  }

  page.write_bytes(100, src_bytes);

  std::vector<std::byte> dst_bytes(message.size());
  page.read_bytes(100, dst_bytes);
  EXPECT_EQ(src_bytes, dst_bytes);

  std::string read_str(reinterpret_cast<const char*>(dst_bytes.data()), dst_bytes.size());
  EXPECT_EQ(read_str, message);

  // Out of range check
  std::vector<std::byte> large_buf(10);
  EXPECT_THROW(page.write_bytes(kPageSize - 5, large_buf), std::out_of_range);
  EXPECT_THROW(page.read_bytes(kPageSize - 5, large_buf), std::out_of_range);
}

TEST(PageTest, ResetMemory) {
  Page page;
  page.set_page_id(PageId{10});
  page.increment_pin_count();
  page.set_dirty(true);
  page.set_page_lsn(LSN{999});
  page.write_u32(0, 0xCAFEBABE);

  page.reset_memory();
  EXPECT_EQ(page.get_page_id(), kInvalidPageId);
  EXPECT_EQ(page.get_pin_count(), 0);
  EXPECT_FALSE(page.is_dirty());
  EXPECT_EQ(page.get_page_lsn(), kInvalidLSN);
  EXPECT_EQ(page.read_u32(0), 0U);
}

}  // namespace
}  // namespace forgedb
