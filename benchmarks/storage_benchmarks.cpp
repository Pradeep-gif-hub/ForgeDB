#include <benchmark/benchmark.h>

#include <filesystem>
#include <random>
#include <string>

#include "forgedb/engine/storage_engine.hpp"
#include "forgedb/storage/record.hpp"

namespace forgedb {
namespace {

static void BM_RecordSerialization(benchmark::State& state) {
  Record rec("benchmark_key_12345", "benchmark_value_payload_data_67890");
  size_t sz = rec.get_serialized_size();
  std::vector<std::byte> buf(sz);

  for (auto _ : state) {
    rec.serialize(buf);
    benchmark::DoNotOptimize(buf.data());
  }
}
BENCHMARK(BM_RecordSerialization);

static void BM_RecordDeserialization(benchmark::State& state) {
  Record rec("benchmark_key_12345", "benchmark_value_payload_data_67890");
  size_t sz = rec.get_serialized_size();
  std::vector<std::byte> buf(sz);
  rec.serialize(buf);

  for (auto _ : state) {
    Record out = Record::deserialize(buf);
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_RecordDeserialization);

static void BM_SequentialPuts(benchmark::State& state) {
  std::filesystem::path test_db = std::filesystem::temp_directory_path() / "bm_seq.db";
  std::filesystem::remove(test_db);
  std::filesystem::remove(test_db.string() + ".wal");

  StorageEngine engine(test_db, 128);
  int i = 0;

  for (auto _ : state) {
    std::string key = "key_" + std::to_string(i++);
    std::string val = "value_" + std::to_string(i);
    engine.put(key, val);
  }

  std::filesystem::remove(test_db);
  std::filesystem::remove(test_db.string() + ".wal");
}
BENCHMARK(BM_SequentialPuts)->Iterations(5000)->Unit(benchmark::kMicrosecond);

static void BM_RandomGets(benchmark::State& state) {
  std::filesystem::path test_db = std::filesystem::temp_directory_path() / "bm_gets.db";
  std::filesystem::remove(test_db);
  std::filesystem::remove(test_db.string() + ".wal");

  StorageEngine engine(test_db, 128);
  for (int i = 0; i < 1000; ++i) {
    engine.put("key_" + std::to_string(i), "value_" + std::to_string(i));
  }

  std::mt19937 gen(42);
  std::uniform_int_distribution<int> dist(0, 999);

  for (auto _ : state) {
    std::string key = "key_" + std::to_string(dist(gen));
    auto val = engine.get(key);
    benchmark::DoNotOptimize(val);
  }

  std::filesystem::remove(test_db);
  std::filesystem::remove(test_db.string() + ".wal");
}
BENCHMARK(BM_RandomGets)->Iterations(10000)->Unit(benchmark::kMicrosecond);

}  // namespace
}  // namespace forgedb
