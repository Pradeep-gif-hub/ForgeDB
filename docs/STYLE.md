# ForgeDB Code Style Guide

## 1. Naming Conventions

- **Types and Classes:** `PascalCase` (e.g., `BufferPoolManager`, `PageId`, `LogRecord`).
- **Functions and Methods:** `snake_case` (e.g., `fetch_page`, `serialize_record`, `get_lsn`).
- **Local Variables and Parameters:** `snake_case` (e.g., `page_id`, `frame_index`, `byte_offset`).
- **Member Variables (Private and Protected):** `snake_case` with a trailing underscore (e.g., `disk_manager_`, `pool_size_`, `page_table_`).
- **Constants and Enum Values:** `kPascalCase` (e.g., `kPageSize`, `kInvalidPageId`, `kHeaderSize`).
- **Namespaces:** `forgedb` (all lower case). Sub-namespaces avoided unless isolating implementation details.

## 2. Directory Structure

- `include/forgedb/<subsystem>/`: Public and subsystem header files.
- `src/<subsystem>/`: Implementation files (`.cpp`).
- `tests/`: Automated unit and integration tests using GoogleTest.
- `benchmarks/`: Performance benchmarks using Google Benchmark.
- `docs/`: Technical and architectural documentation.
- `examples/`: Minimal standalone examples.

## 3. Commenting Rules

- Comment **why** a decision was made or explain tricky mathematical offsets / invariants.
- Never write comments that merely restate the code.
- No banner comments, no decorative separators (`=====`), and no section dividers.
- No emojis anywhere in the codebase (code, comments, docs, git commit messages).
- No first-person narration ("Let's", "Here we...").
- Mark legitimate remaining work with concise `TODO` or `NOTE` comments.

## 4. Error Handling Strategy

- **Programmer and I/O Failures:** Throw standard exceptions (`std::runtime_error`, `std::invalid_argument`, `std::out_of_range`) for unrecoverable errors such as disk read failures, corrupted file headers, checksum mismatches, or invalid byte boundaries.
- **Expected Application Outcomes:** Use `std::optional<T>` for queries that may legitimately not find a result (e.g., `get(key)` returning `std::nullopt` when a key does not exist).
- **Resource Management:** Rely strictly on RAII guards for file descriptors, mutex locks, and buffer pool page pins.
