#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "forgedb/engine/storage_engine.hpp"
#include "forgedb/network/parser.hpp"
#include "forgedb/network/server.hpp"

namespace forgedb {
namespace {

TEST(ParserTest, ParseCommands) {
  Command cmd_ping = Parser::parse("PING");
  EXPECT_EQ(cmd_ping.type, CommandType::kPing);

  Command cmd_set = Parser::parse("SET mykey myvalue");
  EXPECT_EQ(cmd_set.type, CommandType::kSet);
  ASSERT_EQ(cmd_set.args.size(), 2U);
  EXPECT_EQ(cmd_set.args[0], "mykey");
  EXPECT_EQ(cmd_set.args[1], "myvalue");

  Command cmd_get = Parser::parse("GET mykey");
  EXPECT_EQ(cmd_get.type, CommandType::kGet);
  ASSERT_EQ(cmd_get.args.size(), 1U);
  EXPECT_EQ(cmd_get.args[0], "mykey");

  Command cmd_scan = Parser::parse("SCAN a z");
  EXPECT_EQ(cmd_scan.type, CommandType::kScan);
  ASSERT_EQ(cmd_scan.args.size(), 2U);

  Command cmd_del = Parser::parse("DEL mykey");
  EXPECT_EQ(cmd_del.type, CommandType::kDel);

  Command cmd_stats = Parser::parse("STATS");
  EXPECT_EQ(cmd_stats.type, CommandType::kStats);
}

class NetworkServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "forgedb_net_test";
    std::filesystem::remove_all(test_dir_);
    std::filesystem::create_directories(test_dir_);
    db_path_ = test_dir_ / "net.db";
  }

  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
  std::filesystem::path db_path_;
};

TEST_F(NetworkServerTest, CommandExecutionProtocol) {
  StorageEngine engine(db_path_, 16);
  Server server(&engine, 0);

  EXPECT_EQ(server.execute_command("PING"), "+PONG\r\n");
  EXPECT_EQ(server.execute_command("SET user:1 John"), "+OK\r\n");
  EXPECT_EQ(server.execute_command("GET user:1"), "$4\r\nJohn\r\n");
  EXPECT_EQ(server.execute_command("GET user:unknown"), "$-1\r\n");
  EXPECT_EQ(server.execute_command("DEL user:1"), ":1\r\n");
  EXPECT_EQ(server.execute_command("DEL user:1"), ":0\r\n");
}

}  // namespace
}  // namespace forgedb
