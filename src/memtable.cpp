#include "memtable.h"

void MemTable::put(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::optional<std::string> MemTable::get(const std::string& key) const {
    const auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second;
}

void MemTable::remove(const std::string& key) {
    data_[key] = TombstoneValue;
}

void MemTable::clear() {
    data_.clear();
}

bool MemTable::empty() const {
    return data_.empty();
}

std::size_t MemTable::size() const {
    return data_.size();
}

const std::unordered_map<std::string, std::string>& MemTable::entries() const {
    return data_;
}
