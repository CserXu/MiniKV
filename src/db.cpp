#include "db.h"

#include <iostream>
#include <utility>

DB::DB(std::filesystem::path dataDirectory)
    : DB(dataDirectory, dataDirectory / "config.txt") {
}

DB::DB(std::filesystem::path dataDirectory, std::filesystem::path configPath)
    : dataDirectory_(std::move(dataDirectory)),
      config_(Config::load(configPath)),
      wal_(dataDirectory_ / "wal.log"),
      sstable_(dataDirectory_, config_.compactionThreshold),
      cache_(config_.cacheSize),
      flushCount_(0) {
    recover();
    flushIfNeeded();
}

void DB::put(const std::string& key, const std::string& value) {
    wal_.appendPut(key, value);
    memtable_.put(key, value);
    cache_.put(key, value);
    flushIfNeeded();
}

std::optional<std::string> DB::get(const std::string& key) const {
    return getInternal(key, true);
}

bool DB::remove(const std::string& key) {
    const bool existed = getInternal(key, false).has_value();

    wal_.appendDelete(key);
    memtable_.remove(key);
    cache_.remove(key);
    flushIfNeeded();

    return existed;
}

std::size_t DB::flushCount() const {
    return flushCount_;
}

std::size_t DB::sstableCount() const {
    return sstable_.fileCount();
}

std::size_t DB::cacheHitCount() const {
    return cache_.hitCount();
}

std::size_t DB::cacheMissCount() const {
    return cache_.missCount();
}

double DB::cacheHitRatio() const {
    return cache_.hitRatio();
}

std::size_t DB::bloomFilterSkipCount() const {
    return sstable_.bloomFilterSkipCount();
}

std::size_t DB::bloomFilterFalsePositiveCount() const {
    return sstable_.bloomFilterFalsePositiveCount();
}

std::optional<std::string> DB::getInternal(const std::string& key, bool debugLog) const {
    const auto cachedValue = cache_.get(key);
    if (cachedValue.has_value()) {
        if (isTombstone(*cachedValue)) {
            if (debugLog) {
                std::cout << "[Not Found]\n";
            }
            return std::nullopt;
        }
        if (debugLog) {
            std::cout << "[Cache Hit]\n";
        }
        return cachedValue;
    }

    const auto memtableValue = memtable_.get(key);
    if (memtableValue.has_value()) {
        cache_.put(key, *memtableValue);
        if (isTombstone(*memtableValue)) {
            if (debugLog) {
                std::cout << "[Not Found]\n";
            }
            return std::nullopt;
        }
        if (debugLog) {
            std::cout << "[MemTable Hit]\n";
        }
        return memtableValue;
    }

    const auto sstableValue = sstable_.get(key);
    if (sstableValue.has_value() && isTombstone(*sstableValue)) {
        cache_.put(key, *sstableValue);
        if (debugLog) {
            std::cout << "[Not Found]\n";
        }
        return std::nullopt;
    }

    if (sstableValue.has_value()) {
        cache_.put(key, *sstableValue);
        if (debugLog) {
            std::cout << "[SSTable Hit]\n";
        }
    } else if (debugLog) {
        std::cout << "[Not Found]\n";
    }

    return sstableValue;
}

void DB::recover() {
    wal_.replay([this](const WAL::Record& record) {
        if (record.type == WAL::OperationType::Put) {
            memtable_.put(record.key, record.value);
            return;
        }

        memtable_.remove(record.key);
    });
}

void DB::flushIfNeeded() {
    if (memtable_.size() >= config_.flushThreshold) {
        flushMemTable();
    }
}

void DB::flushMemTable() {
    if (memtable_.empty()) {
        return;
    }

    sstable_.flush(memtable_.entries());
    wal_.clear();
    memtable_.clear();
    ++flushCount_;
}

bool DB::isTombstone(const std::string& value) {
    return value == MemTable::TombstoneValue;
}
