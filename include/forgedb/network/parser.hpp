#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace forgedb {

enum class CommandType {
  kPing,
  kGet,
  kSet,
  kDel,
  kScan,
  kStats,
  kCheckpoint,
  kQuit,
  kUnknown,
};

struct Command {
  CommandType type{CommandType::kUnknown};
  std::vector<std::string> args;
};

class Parser {
 public:
  static Command parse(std::string_view input);
  static std::string format_simple_string(std::string_view str);
  static std::string format_bulk_string(std::string_view str);
  static std::string format_null();
  static std::string format_integer(int64_t val);
  static std::string format_error(std::string_view err);
  static std::string format_array(const std::vector<std::string>& items);
};

}  // namespace forgedb
