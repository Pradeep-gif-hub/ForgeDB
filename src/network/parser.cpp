#include "forgedb/network/parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace forgedb {

namespace {

std::string to_upper(std::string_view s) {
  std::string res(s);
  for (char& c : res) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return res;
}

}  // namespace

Command Parser::parse(std::string_view input) {
  Command cmd;
  std::istringstream iss{std::string(input)};
  std::string token;

  if (!(iss >> token)) {
    return cmd;
  }

  std::string op = to_upper(token);
  if (op == "PING") {
    cmd.type = CommandType::kPing;
  } else if (op == "GET") {
    cmd.type = CommandType::kGet;
  } else if (op == "SET") {
    cmd.type = CommandType::kSet;
  } else if (op == "DEL") {
    cmd.type = CommandType::kDel;
  } else if (op == "SCAN") {
    cmd.type = CommandType::kScan;
  } else if (op == "STATS") {
    cmd.type = CommandType::kStats;
  } else if (op == "CHECKPOINT") {
    cmd.type = CommandType::kCheckpoint;
  } else if (op == "QUIT") {
    cmd.type = CommandType::kQuit;
  } else {
    cmd.type = CommandType::kUnknown;
  }

  while (iss >> token) {
    cmd.args.push_back(token);
  }

  return cmd;
}

std::string Parser::format_simple_string(std::string_view str) {
  return "+" + std::string(str) + "\r\n";
}

std::string Parser::format_bulk_string(std::string_view str) {
  return "$" + std::to_string(str.size()) + "\r\n" + std::string(str) + "\r\n";
}

std::string Parser::format_null() {
  return "$-1\r\n";
}

std::string Parser::format_integer(int64_t val) {
  return ":" + std::to_string(val) + "\r\n";
}

std::string Parser::format_error(std::string_view err) {
  return "-ERR " + std::string(err) + "\r\n";
}

std::string Parser::format_array(const std::vector<std::string>& items) {
  std::string out = "*" + std::to_string(items.size()) + "\r\n";
  for (const auto& item : items) {
    out += format_bulk_string(item);
  }
  return out;
}

}  // namespace forgedb
