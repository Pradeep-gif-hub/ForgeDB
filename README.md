# ForgeDB

ForgeDB is a high-performance transactional database storage engine built in modern C++20.

## Features

- **Storage Engine:** 4KB fixed-size pages with bounds checking and slotted page layout for variable-length key-value records.
- **Buffer Pool Management:** In-memory page frame buffer pool with LRU replacement policy and thread-safe concurrency.
- **B+ Tree Index:** Disk-backed B+ Tree supporting point lookups, insertions, deletions, and sequential range scans.
- **WAL & ARIES Recovery:** Write-Ahead Logging (WAL) with group commit and ARIES 3-phase crash recovery (Analysis, Redo, Undo with CLRs).
- **ACID Transactions:** Strict 2-Phase Locking support, transaction rollback, and commit durability.
- **Network Server & CLI:** Multi-threaded TCP network server with RESP-compatible protocol and an interactive CLI client.
- **Test & Benchmark Suite:** Complete GoogleTest suite (100% pass rate) and Google Benchmark suite.

## Building and Running

### Prerequisites
- CMake 3.20+
- C++20 compliant compiler (Apple Clang, GCC 11+, or Clang 13+)

### Build
```bash
cmake -B build -S .
cmake --build build -j
```

### Running Tests
```bash
./build/forgedb_tests
# or using ctest:
ctest --test-dir build --output-on-failure
```

### Running Benchmarks
```bash
./build/forgedb_benchmarks
```

### Starting the Server
```bash
./build/forgedb-server --port 7654 --db data/forgedb.db --pool 64
```

### Connecting with the CLI Client
```bash
./build/forgedb-client --host 127.0.0.1 --port 7654
```
Example commands:
```text
forgedb> PING
+PONG
forgedb> SET user:1 Alice
+OK
forgedb> GET user:1
$5
Alice
forgedb> SCAN user:0 user:9
*2
$6
user:1
$5
Alice
forgedb> STATS
${"total_reads":1,"total_writes":1,...}
forgedb> QUIT
+OK
```
