#include "forgedb/storage/record.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "forgedb/storage/page.hpp"

namespace forgedb {
namespace {

TEST(RecordTest, SerializationAndDeserialization) {
  Record original("user:1001", "{\"name\": \"Alice\", \"role\": \"admin\"}");
  EXPECT_FALSE(original.is_deleted());
  EXPECT_EQ(original.get_key(), "user:1001");
  EXPECT_EQ(original.get_value(), "{\"name\": \"Alice\", \"role\": \"admin\"}");

  size_t size = original.get_serialized_size();
  std::vector<std::byte> buffer(size);
  original.serialize(buffer);

  Record restored = Record::deserialize(buffer);
  EXPECT_EQ(restored.get_key(), original.get_key());
  EXPECT_EQ(restored.get_value(), original.get_value());
  EXPECT_EQ(restored.is_deleted(), original.is_deleted());
}

TEST(SlottedPageTest, InsertAndGetRecords) {
  Page page;
  SlottedPage::init(page, PageId{1});

  EXPECT_EQ(SlottedPage::get_record_count(page), 0);
  EXPECT_GT(SlottedPage::get_free_space(page), 4000U);

  Record r1("key1", "value1");
  Record r2("key2", "value2_with_longer_content");

  RID rid1, rid2;
  EXPECT_TRUE(SlottedPage::insert_record(page, r1, rid1));
  EXPECT_TRUE(SlottedPage::insert_record(page, r2, rid2));

  EXPECT_EQ(rid1.page_id, PageId{1});
  EXPECT_EQ(rid1.slot_num, 0);
  EXPECT_EQ(rid2.page_id, PageId{1});
  EXPECT_EQ(rid2.slot_num, 1);
  EXPECT_EQ(SlottedPage::get_record_count(page), 2);

  Record out_r1, out_r2;
  EXPECT_TRUE(SlottedPage::get_record(page, rid1, out_r1));
  EXPECT_TRUE(SlottedPage::get_record(page, rid2, out_r2));

  EXPECT_EQ(out_r1.get_key(), "key1");
  EXPECT_EQ(out_r1.get_value(), "value1");
  EXPECT_EQ(out_r2.get_key(), "key2");
  EXPECT_EQ(out_r2.get_value(), "value2_with_longer_content");
}

TEST(SlottedPageTest, UpdateAndDelete) {
  Page page;
  SlottedPage::init(page, PageId{5});

  Record r1("account:1", "100");
  Record r2("account:2", "200");

  RID rid1, rid2;
  ASSERT_TRUE(SlottedPage::insert_record(page, r1, rid1));
  ASSERT_TRUE(SlottedPage::insert_record(page, r2, rid2));

  // Update r1 with smaller or equal size
  Record r1_updated("account:1", "150");
  EXPECT_TRUE(SlottedPage::update_record(page, rid1, r1_updated));

  Record out_r1;
  EXPECT_TRUE(SlottedPage::get_record(page, rid1, out_r1));
  EXPECT_EQ(out_r1.get_value(), "150");

  // Delete r2
  EXPECT_TRUE(SlottedPage::delete_record(page, rid2));

  Record out_r2;
  EXPECT_FALSE(SlottedPage::get_record(page, rid2, out_r2));

  // Defragment page and verify r1 is still accessible
  SlottedPage::defragment(page);
  Record out_r1_after_defrag;
  EXPECT_TRUE(SlottedPage::get_record(page, rid1, out_r1_after_defrag));
  EXPECT_EQ(out_r1_after_defrag.get_value(), "150");
}

TEST(SlottedPageTest, PageCapacityExceeded) {
  Page page;
  SlottedPage::init(page, PageId{10});

  std::string large_val(1500, 'x');
  Record rec("k", large_val);

  RID rid;
  EXPECT_TRUE(SlottedPage::insert_record(page, rec, rid));
  EXPECT_TRUE(SlottedPage::insert_record(page, rec, rid));
  // Third 1.5KB record will not fit in 4KB page
  EXPECT_FALSE(SlottedPage::insert_record(page, rec, rid));
}

}  // namespace
}  // namespace forgedb
