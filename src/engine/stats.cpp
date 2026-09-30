#include "forgedb/engine/stats.hpp"

#include <sstream>

namespace forgedb {

void EngineStats::reset() noexcept {
  total_reads = 0;
  total_writes = 0;
  total_deletes = 0;
  total_scans = 0;
  total_commits = 0;
  total_aborts = 0;
  cache_hits = 0;
  cache_misses = 0;
}

std::string EngineStats::to_json() const {
  std::stringstream ss;
  ss << "{"
     << "\"total_reads\":" << total_reads.load() << ","
     << "\"total_writes\":" << total_writes.load() << ","
     << "\"total_deletes\":" << total_deletes.load() << ","
     << "\"total_scans\":" << total_scans.load() << ","
     << "\"total_commits\":" << total_commits.load() << ","
     << "\"total_aborts\":" << total_aborts.load() << ","
     << "\"cache_hits\":" << cache_hits.load() << ","
     << "\"cache_misses\":" << cache_misses.load()
     << "}";
  return ss.str();
}

}  // namespace forgedb
