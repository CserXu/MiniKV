#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

class LRUCache {
public:
    explicit LRUCache(std::size_t capacity = 100);

    std::optional<std::string> get(const std::string& key);
    void put(const std::string& key, const std::string& value);
    void remove(const std::string& key);

    std::size_t hitCount() const;
    std::size_t missCount() const;
    double hitRatio() const;

private:
    using Entry = std::pair<std::string, std::string>;
    using EntryList = std::list<Entry>;
    using EntryIterator = EntryList::iterator;

    std::size_t capacity_;
    EntryList entries_;
    std::unordered_map<std::string, EntryIterator> index_;
    std::size_t hitCount_;
    std::size_t missCount_;
};
