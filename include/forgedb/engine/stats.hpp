#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace forgedb {

struct EngineStats {
  std::atomic<uint64_t> total_reads{0};
  std::atomic<uint64_t> total_writes{0};
  std::atomic<uint64_t> total_deletes{0};
  std::atomic<uint64_t> total_scans{0};
  std::atomic<uint64_t> total_commits{0};
  std::atomic<uint64_t> total_aborts{0};
  std::atomic<uint64_t> cache_hits{0};
  std::atomic<uint64_t> cache_misses{0};

  void reset() noexcept;
  [[nodiscard]] std::string to_json() const;
};

}  // namespace forgedb
