#pragma once

#include "config.h"
#include "lru_cache.h"
#include "memtable.h"
#include "sstable.h"
#include "wal.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

class DB {
public:
    explicit DB(std::filesystem::path dataDirectory = "data");
    DB(std::filesystem::path dataDirectory, std::filesystem::path configPath);

    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool remove(const std::string& key);
    std::size_t flushCount() const;
    std::size_t sstableCount() const;
    std::size_t cacheHitCount() const;
    std::size_t cacheMissCount() const;
    double cacheHitRatio() const;
    std::size_t bloomFilterSkipCount() const;
    std::size_t bloomFilterFalsePositiveCount() const;

private:
    std::optional<std::string> getInternal(const std::string& key, bool debugLog) const;
    void recover();
    void flushIfNeeded();
    void flushMemTable();
    static bool isTombstone(const std::string& value);

    std::filesystem::path dataDirectory_;
    Config config_;
    MemTable memtable_;
    WAL wal_;
    SSTable sstable_;
    mutable LRUCache cache_;
    std::size_t flushCount_;
};
