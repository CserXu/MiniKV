# MiniKV

MiniKV is a small educational key-value storage engine implemented in C++17. It follows a simplified LSM-Tree design and uses only STL, CMake, and local file I/O.

The project focuses on clarity and modularity. It includes a write-ahead log, in-memory MemTable, immutable SSTable files, Bloom Filters, LRU Cache, basic compaction, configurable runtime parameters, and a standalone benchmark executable.

## Project Overview

MiniKV provides a simple command-line KV database with three primary operations:

```text
put <key> <value>
get <key>
delete <key>
```

Data is first written to a WAL, then stored in memory. When the MemTable reaches the configured flush threshold, it is flushed to sorted SSTable files under the project `data` directory. Reads check the cache first, then MemTable, then SSTables.

This project is intentionally compact. It is suitable for learning the core mechanics behind LSM-style storage systems without introducing advanced production concerns such as concurrent writes, snapshots, background compaction, checksums, or binary block formats.

## Architecture

MiniKV uses a layered storage architecture:

```text
              +-------------------+
              |   CLI / Benchmark |
              +---------+---------+
                        |
                        v
              +-------------------+
              |        DB         |
              +----+---------+----+
                   |         |
          get path |         | write path
                   |         v
                   |   +-----------+
                   |   |    WAL    |  data/wal.log
                   |   +-----------+
                   |
        +----------+----------+
        |                     |
        v                     v
+---------------+     +---------------+
|   LRU Cache   |     |   MemTable    |
+---------------+     +---------------+
                              |
                              | flush threshold reached
                              v
                     +----------------+
                     |    SSTable     |  data/sst_N.data
                     +----------------+
                              |
                              | >= compaction threshold
                              v
                     +----------------+
                     |   Compaction   |  data/compact_1.data
                     +----------------+
```

Read path:

```text
get(key)
  |
  +--> LRU Cache
  |
  +--> MemTable
  |
  +--> compact_1.data, then newer SSTables
          |
          +--> Bloom Filter check before file scan
```

Write path:

```text
put/delete
  |
  +--> append WAL record
  |
  +--> update MemTable
  |
  +--> update/remove Cache entry
  |
  +--> flush MemTable to SSTable when threshold is reached
```

## Features

- C++17 implementation using STL containers and file I/O
- Command-line interface for `put`, `get`, and `delete`
- Write-Ahead Logging in `data/wal.log`
- WAL recovery on DB startup
- MemTable backed by `std::unordered_map`
- SSTable flush to sorted text files
- Tombstone-based delete using `__DELETE__`
- Basic compaction into `data/compact_1.data`
- Bloom Filter per SSTable for skipping unnecessary file scans
- LRU Cache implemented with `std::list` and `std::unordered_map`
- Configurable system parameters via `data/config.txt`
- Standalone benchmark target with throughput and storage statistics
- Debug read logs:
  - `[Cache Hit]`
  - `[MemTable Hit]`
  - `[SSTable Hit]`
  - `[Not Found]`

## Storage Flow

### Put

1. Append `PUT key value` to `data/wal.log`.
2. Insert or overwrite the key in MemTable.
3. Update the LRU Cache entry.
4. If MemTable key count reaches `flush_threshold`, flush to an SSTable.
5. After a successful flush, clear MemTable and truncate WAL.

### Get

1. Check LRU Cache.
2. Check MemTable.
3. Check SSTables.
4. Before scanning an SSTable file, check its Bloom Filter.
5. If Bloom Filter says the key is absent, skip that SSTable.
6. If a tombstone value is found, return `NOT FOUND`.

### Delete

1. Append `DEL key` to `data/wal.log`.
2. Write a tombstone value `__DELETE__` to MemTable.
3. Remove the key from LRU Cache.
4. Tombstones are persisted during flush and removed during compaction.

### Flush

When MemTable reaches the configured threshold, MiniKV writes all current MemTable entries to a new sorted SSTable:

```text
data/sst_1.data
data/sst_2.data
data/sst_3.data
```

SSTable file format:

```text
key:value
```

### Compaction

When the number of normal SSTable files reaches `compaction_threshold`, MiniKV:

1. Reads compacted data if `compact_1.data` already exists.
2. Reads normal SSTables from old to new.
3. Merges keys so newer values overwrite older values.
4. Drops tombstone entries.
5. Writes the merged result to:

```text
data/compact_1.data
```

6. Deletes old `sst_*.data` files.

## Components

### DB

`DB` is the main facade used by the CLI and benchmark. It coordinates WAL, MemTable, SSTable, Cache, configuration, recovery, flush, and read routing.

### Config

`Config` loads tunable parameters from:

```text
data/config.txt
```

Supported keys:

```text
flush_threshold=5
cache_size=100
compaction_threshold=3
```

If the file is missing or a value is invalid, MiniKV uses defaults:

```text
flush_threshold = 5
cache_size = 100
compaction_threshold = 3
```

### WAL

The WAL records writes before MemTable updates:

```text
PUT key value
DEL key
```

On startup, DB replays `data/wal.log` to restore unflushed MemTable state.

### MemTable

MemTable stores active KV data in memory using:

```cpp
std::unordered_map<std::string, std::string>
```

Deletes are represented as tombstones.

### SSTable

SSTable files are immutable text files sorted by key. MiniKV scans them from newest to oldest after checking Bloom Filters.

### Bloom Filter

Each SSTable has an in-memory Bloom Filter. Filters are built:

- during flush
- during startup when existing SSTables are loaded
- after compaction creates `compact_1.data`

Bloom Filter statistics are exposed in benchmark output:

- skip count
- false positive count

### LRU Cache

The cache uses `std::list` plus `std::unordered_map` and defaults to capacity `100`, configurable via `cache_size`.

### Benchmark

`MiniKVBenchmark` writes 10,000 keys and performs 10,000 reads. The read workload uses an 80/20 distribution:

- 80% of reads target the first 100 hot keys
- 20% of reads target random keys across the full key range

## Build

MiniKV uses CMake and builds with Visual Studio 2022 CMake projects.

Configure and build with CMake presets:

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug
```

Or open the project folder directly in Visual Studio 2022 and build the CMake targets:

- `MiniKV`
- `MiniKVBenchmark`

## Run Benchmark

From the project root:

```powershell
.\out\build\x64-debug\MiniKVBenchmark.exe
```

The benchmark uses:

```text
data/benchmark
```

as an isolated data directory and removes old benchmark data before each run.

## Benchmark Example

Example output:

```text
MiniKV Benchmark
Operations: 10000
Put total time: 45.64 s
Get total time: 25.62 s
Write TPS: 219.09
Read TPS: 390.39
Flush count: 2000
SSTable count: 3
Cache hit count: 5616
Cache miss count: 4384
Cache hit ratio: 56.16%
Bloom filter skip count: 8758
Bloom filter false positive count: 7
```

Numbers vary by machine, build type, compiler settings, and existing configuration values.

## Directory Structure

```text
MiniKV/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── data/
│   ├── config.txt          optional
│   ├── wal.log             generated at runtime
│   ├── sst_N.data          generated by flush
│   └── compact_1.data      generated by compaction
├── src/
│   ├── benchmark.cpp
│   ├── bloom_filter.cpp
│   ├── bloom_filter.h
│   ├── config.cpp
│   ├── config.h
│   ├── db.cpp
│   ├── db.h
│   ├── lru_cache.cpp
│   ├── lru_cache.h
│   ├── main.cpp
│   ├── memtable.cpp
│   ├── memtable.h
│   ├── sstable.cpp
│   ├── sstable.h
│   ├── wal.cpp
│   └── wal.h
└── tests/
```

## Future Improvements

- Use a binary SSTable format with block indexes
- Persist Bloom Filters instead of rebuilding them on startup
- Add checksums for WAL and SSTable records
- Add unit tests for recovery, compaction, and cache behavior
- Add range scan support
- Add background flush and compaction
- Add thread-safety for concurrent access
- Add configurable Bloom Filter size and hash count
- Improve benchmark coverage with mixed read/write workloads
