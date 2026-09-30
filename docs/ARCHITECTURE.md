# ForgeDB Architecture & Technical Design

ForgeDB is a high-performance, crash-resilient, transactional database storage engine built in modern C++20. It implements standard relational/embedded engine internals including slotted-page storage, buffer pool caching with LRU eviction, disk-backed B+ Tree indexing, ARIES-style write-ahead logging (WAL) with 3-phase crash recovery, strict ACID transaction management, and a multi-threaded TCP network server.

## 1. Subsystem Overview

```
+-------------------------------------------------------------+
|                     Client Applications                     |
|           (TCP Protocol / CLI Interactive Shell)            |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                   Network & Protocol Layer                  |
|                 (Parser, Server, ThreadPool)                |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                    StorageEngine (Facade)                   |
|     (CRUD API, Range Scan, Stats, Checkpoint Manager)       |
+-------------------------------------------------------------+
         |                                  |
         v                                  v
+------------------------+      +-----------------------------+
|   TransactionManager   |      |        BPlusTree Index      |
| (ACID, Undo/Redo Logs) |      |   (Internal & Leaf Nodes)   |
+------------------------+      +-----------------------------+
         |                                  |
         +-----------------+----------------+
                           |
                           v
+-------------------------------------------------------------+
|                     BufferPoolManager                       |
|           (Page Frame Cache, Page Table, LRUReplacer)       |
+-------------------------------------------------------------+
         |                                  |
         v                                  v
+------------------------+      +-----------------------------+
|       DiskManager      |      |          LogManager         |
|   (4KB Page File I/O)  |      |   (Append-only WAL Buffer)  |
+------------------------+      +-----------------------------+
         |                                  |
         v                                  v
+------------------------+      +-----------------------------+
|    database.db File    |      |       database.db.wal       |
|      (Page Data)       |      |     (Write-Ahead Log)       |
+------------------------+      +-----------------------------+
```

---

## 2. Storage & Page Architecture

- **Page Structure (`Page`):** Fixed 4KB memory blocks with little-endian bounds-checked integer accessors (`read_u8`, `read_u16`, `read_u32`, `read_u64`), byte range accessors, dirty tracking, pin counting, and Log Sequence Number (`LSN`) headers.
- **Slotted Page Architecture (`SlottedPage`):** Variable-length record storage within 4KB pages. Slot descriptors (`offset`, `length`, `is_deleted`) grow upward from the header, while tuple payloads grow downward from the top of the page. Automatic defragmentation reclaims space when deleted records fragment memory.
- **Disk Management (`DiskManager`):** Manages file persistence for `.db` and `.db.wal` files. Page 0 serves as the database metadata header containing magic bytes (`0x46474442` / `FGDB`), database version, total page allocations, B+ Tree root page ID, and active data page IDs.

---

## 3. Buffer Pool & Caching

- **LRU Replacer (`LRUReplacer`):** Tracks unpinned frames eligible for eviction using an $O(1)$ doubly-linked list and hash map index.
- **Buffer Pool Manager (`BufferPoolManager`):** Coordinates page allocation, disk reads/writes, frame pinning, and page-table indexing. Ensures thread-safe synchronization across concurrent workers.

---

## 4. B+ Tree Indexing

- **Nodes (`BPlusTreeInternalPage` & `BPlusTreeLeafPage`):** Disk-backed nodes stored directly in 4KB buffer pool pages.
- **Tree Operations (`BPlusTree`):**
  - **Search:** Traverse internal routing keys to the appropriate leaf in $O(\log N)$ time.
  - **Insert:** Handles node splits (propagating split keys up to internal parents and creating new roots when necessary), while updating child parent pointers.
  - **Remove:** Deletes keys and cleans up empty roots.
  - **Range Scan:** Fast leaf-chain iteration following `next_page_id` sibling pointers.

---

## 5. Write-Ahead Logging (WAL) & ARIES Crash Recovery

- **Log Record (`LogRecord`):** Binary serialized entries for `BEGIN`, `COMMIT`, `ABORT`, `INSERT`, `UPDATE`, `DELETE`, `CHECKPOINT`, and `CLR` (Compensation Log Records).
- **Log Manager (`LogManager`):** Double-buffered append-only logging pipeline with group committing, force flushing, and background flush threads.
- **Recovery Manager (`RecoveryManager`):** Implements the classic ARIES 3-phase algorithm:
  1. **Analysis Phase:** Reconstructs the Active Transaction Table (`ATT`) and Dirty Page Table (`DPT`) by scanning WAL records forward from the earliest checkpoint.
  2. **Redo Phase:** Repeats history from `redo_start_lsn` forward to bring disk pages up to date with logged mutations.
  3. **Undo Phase:** Scans backwards to rollback all active/uncommitted transactions using inverse operations and appending CLRs to the WAL.

---

## 6. Concurrency & Networking

- **Thread Pool (`ThreadPool`):** Task queue thread pool with future/promise return values for parallel query execution.
- **Protocol & Network (`Server`, `Parser`):** Multi-client TCP socket server supporting line/RESP protocol commands (`PING`, `GET`, `SET`, `DEL`, `SCAN`, `STATS`, `CHECKPOINT`, `QUIT`).
