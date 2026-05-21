#include "lru_cache.h"

#include <utility>

LRUCache::LRUCache(std::size_t capacity)
    : capacity_(capacity),
      hitCount_(0),
      missCount_(0) {
}

std::optional<std::string> LRUCache::get(const std::string& key) {
    const auto it = index_.find(key);
    if (it == index_.end()) {
        ++missCount_;
        return std::nullopt;
    }

    entries_.splice(entries_.begin(), entries_, it->second);
    ++hitCount_;
    return it->second->second;
}

void LRUCache::put(const std::string& key, const std::string& value) {
    if (capacity_ == 0) {
        return;
    }

    const auto it = index_.find(key);
    if (it != index_.end()) {
        it->second->second = value;
        entries_.splice(entries_.begin(), entries_, it->second);
        return;
    }

    entries_.emplace_front(key, value);
    index_[key] = entries_.begin();

    if (entries_.size() > capacity_) {
        const auto& lruKey = entries_.back().first;
        index_.erase(lruKey);
        entries_.pop_back();
    }
}

void LRUCache::remove(const std::string& key) {
    const auto it = index_.find(key);
    if (it == index_.end()) {
        return;
    }

    entries_.erase(it->second);
    index_.erase(it);
}

std::size_t LRUCache::hitCount() const {
    return hitCount_;
}

std::size_t LRUCache::missCount() const {
    return missCount_;
}

double LRUCache::hitRatio() const {
    const auto total = hitCount_ + missCount_;
    if (total == 0) {
        return 0.0;
    }

    return static_cast<double>(hitCount_) / static_cast<double>(total);
}
